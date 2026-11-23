"""Grid A* PCB router for 2-layer boards (numpy + heapq), driving a pcbgen.PCB.

Objects on the board (pads, tracks, vias, holes, keepouts) are kept as simple geometric
primitives. For each connection the blocked-cell masks are rebuilt with the clearance
rules that apply to the net being routed, so mixed rules (0.2 mm signals, 2/3 mm mains
creepage, wide relay contacts) are honoured.
"""
import heapq
import math
import time
from dataclasses import dataclass

import numpy as np
import pcbnew

MARGIN = 0.10          # extra safety (mm) on top of clearance + half track width (covers grid quantisation)
EDGE_CLR = 0.45        # copper to board edge (mm)
HOLE_CLR = 0.30        # copper to NPTH hole edge
VIA_D, VIA_DRILL = 0.6, 0.3


@dataclass
class NetRule:
    width: float = 0.25
    clearance: float = 0.2
    cls: str = "Default"     # Default | Power | Mains | MainsHeavy


def default_rules(mains_nets=(), dc_nets=(), power_nets=(), wide=None):
    r = {}
    for n in power_nets:
        r[n] = NetRule((wide or {}).get(n, 0.5), 0.2, "Power")
    for n in mains_nets:
        r[n] = NetRule((wide or {}).get(n, 0.6), 1.5, "Mains")
    for n in dc_nets:
        r[n] = NetRule(0.4, 0.4, "Mains")
    return r


def is_mains(rule):
    return rule.cls == "Mains"


@dataclass
class Obj:
    kind: str                 # rect | seg | circle
    p: tuple                  # geometry (mm, board-local)
    layers: tuple             # (0,) (1,) or (0,1)
    net: str                  # net name or None
    hw: float = 0.0           # half width (segments)
    round: bool = False       # pad is circular/oval (bbox corners are NOT copper)
    ref: str = ""             # footprint reference for pads


class Router:
    def __init__(self, pcb, W, H, rules, grid=0.2, corner_r=3.0, mains_creepage=5.0):
        self.pcb = pcb
        self.W, self.H = W, H
        self.g = grid
        self.nx = int(W / grid) + 1
        self.ny = int(H / grid) + 1
        self.rules = rules
        self.default_rule = NetRule()
        self.creep = mains_creepage
        self.objs = []
        self.keepouts = []                    # (x0,y0,x1,y1,layers)
        self.tracks = []                      # for output: (layer, pts, width, net)
        self.fixed = []                       # hand-placed tracks (never re-routed)
        self.vias = []                        # (x, y, net)
        self.corner_r = corner_r
        self.no_via_nets = set()               # high-current nets: a 0.3 mm via must never carry them
        self._edge_map = None
        self._build_edge_map()
        self.collect_pads()

    # ───────────────────────── geometry extraction ─────────────────────────
    def rule(self, net):
        return self.rules.get(net, self.default_rule)

    def clr(self, na, nb):
        ra, rb = self.rule(na), self.rule(nb)
        c = max(ra.clearance, rb.clearance)
        if na and nb and na != nb and (is_mains(ra) != is_mains(rb)):
            c = max(c, self.creep)
        return c

    def collect_pads(self):
        origin = self.pcb.origin
        for ref, fp in self.pcb.fps.items():
            for pad in fp.Pads():
                bb = pad.GetBoundingBox()
                x0 = pcbnew.ToMM(bb.GetLeft()) - origin[0]
                x1 = pcbnew.ToMM(bb.GetRight()) - origin[0]
                y0 = pcbnew.ToMM(bb.GetTop()) - origin[1]
                y1 = pcbnew.ToMM(bb.GetBottom()) - origin[1]
                net = pad.GetNetname() or None
                attr = pad.GetAttribute()
                if attr == pcbnew.PAD_ATTRIB_NPTH:
                    d = pcbnew.ToMM(pad.GetDrillSize().x) / 2
                    c = ((x0 + x1) / 2, (y0 + y1) / 2)
                    self.objs.append(Obj("circle", (c[0], c[1], d), (0, 1), "__hole__"))
                    continue
                layers = []
                if pad.IsOnLayer(pcbnew.F_Cu):
                    layers.append(0)
                if pad.IsOnLayer(pcbnew.B_Cu):
                    layers.append(1)
                o = Obj("rect", (x0, y0, x1, y1), tuple(layers), net)
                o.ref = ref
                o.round = pad.GetShape() in (pcbnew.PAD_SHAPE_CIRCLE, pcbnew.PAD_SHAPE_OVAL)
                self.objs.append(o)

    def add_keepout(self, x0, y0, x1, y1, layers=(0, 1)):
        self.keepouts.append((x0, y0, x1, y1, layers))
        self.objs.append(Obj("rect", (x0, y0, x1, y1), tuple(layers), "__keepout__"))

    # ───────────────────────── edge map ─────────────────────────
    def _build_edge_map(self):
        g = self.g
        xs = (np.arange(self.nx) * g)[None, :]
        ys = (np.arange(self.ny) * g)[:, None]
        r = self.corner_r
        cx = np.clip(xs, r, self.W - r)
        cy = np.clip(ys, r, self.H - r)
        # distance from the point to the rounded-rect boundary (positive inside)
        dx = np.abs(xs - self.W / 2) - (self.W / 2 - r)
        dy = np.abs(ys - self.H / 2) - (self.H / 2 - r)
        outside = np.hypot(np.maximum(dx, 0), np.maximum(dy, 0)) - r
        inside = np.minimum(np.maximum(dx, dy), 0) - r
        sdf = outside + inside   # <0 inside
        self._edge_map = -sdf    # distance to boundary, positive inside

    # ───────────────────────── blocked mask construction ─────────────────────────
    def _paint(self, mask, o, need):
        """mark cells whose centre is closer than `need` to the edge of object o."""
        g = self.g
        if o.kind == "rect":
            x0, y0, x1, y1 = o.p
            ext = need
            ix0 = max(0, int(math.floor((x0 - ext) / g)))
            ix1 = min(self.nx - 1, int(math.ceil((x1 + ext) / g)))
            iy0 = max(0, int(math.floor((y0 - ext) / g)))
            iy1 = min(self.ny - 1, int(math.ceil((y1 + ext) / g)))
            if ix1 < ix0 or iy1 < iy0:
                return
            xs = (np.arange(ix0, ix1 + 1) * g)[None, :]
            ys = (np.arange(iy0, iy1 + 1) * g)[:, None]
            dx = np.maximum(np.maximum(x0 - xs, xs - x1), 0)
            dy = np.maximum(np.maximum(y0 - ys, ys - y1), 0)
            d = np.hypot(dx, dy)
            sub = mask[iy0:iy1 + 1, ix0:ix1 + 1]
            sub |= d < need
        elif o.kind == "circle":
            cx, cy, r = o.p
            ext = need + r
            ix0 = max(0, int(math.floor((cx - ext) / g)))
            ix1 = min(self.nx - 1, int(math.ceil((cx + ext) / g)))
            iy0 = max(0, int(math.floor((cy - ext) / g)))
            iy1 = min(self.ny - 1, int(math.ceil((cy + ext) / g)))
            if ix1 < ix0 or iy1 < iy0:
                return
            xs = (np.arange(ix0, ix1 + 1) * g)[None, :]
            ys = (np.arange(iy0, iy1 + 1) * g)[:, None]
            d = np.hypot(xs - cx, ys - cy) - r
            mask[iy0:iy1 + 1, ix0:ix1 + 1] |= d < need
        else:  # seg
            ax, ay, bx, by = o.p
            ext = need + o.hw
            ix0 = max(0, int(math.floor((min(ax, bx) - ext) / g)))
            ix1 = min(self.nx - 1, int(math.ceil((max(ax, bx) + ext) / g)))
            iy0 = max(0, int(math.floor((min(ay, by) - ext) / g)))
            iy1 = min(self.ny - 1, int(math.ceil((max(ay, by) + ext) / g)))
            if ix1 < ix0 or iy1 < iy0:
                return
            xs = (np.arange(ix0, ix1 + 1) * g)[None, :]
            ys = (np.arange(iy0, iy1 + 1) * g)[:, None]
            vx, vy = bx - ax, by - ay
            L2 = vx * vx + vy * vy
            if L2 == 0:
                t = 0 * xs + 0 * ys
            else:
                t = np.clip(((xs - ax) * vx + (ys - ay) * vy) / L2, 0, 1)
            px, py = ax + t * vx, ay + t * vy
            d = np.hypot(xs - px, ys - py) - o.hw
            mask[iy0:iy1 + 1, ix0:ix1 + 1] |= d < need

    def build_masks(self, net, width, own_pads_block_via=True):
        """Return (blocked_track[2], blocked_via) boolean arrays for routing `net` with `width`."""
        half = width / 2
        vhalf = VIA_D / 2
        bt = [np.zeros((self.ny, self.nx), bool), np.zeros((self.ny, self.nx), bool)]
        bv = np.zeros((self.ny, self.nx), bool)
        rl = self.rule(net)
        my_refs = {o.ref for o in self.objs if o.net == net and net is not None and o.kind == "rect" and o.ref}
        for o in self.objs:
            if o.net == net and net is not None:
                if own_pads_block_via and o.kind == "rect":
                    # own pads: tracks may land on them, but vias keep a small distance
                    self._paint(bv, o, vhalf + 0.2 + MARGIN)
                continue
            if o.net == "__keepout__":
                need_t, need_v = half + MARGIN, vhalf + MARGIN
            elif o.net == "__hole__":
                need_t, need_v = half + HOLE_CLR + MARGIN - 0.0, vhalf + HOLE_CLR + MARGIN
            else:
                c = self.clr(net, o.net)
                if o.ref and o.ref in my_refs and o.kind == "rect":
                    # pads of the same part (relay): the pad pitch is the manufacturer's business -> no 5 mm creepage inflation
                    c = max(self.rule(net).clearance, self.rule(o.net).clearance)
                need_t, need_v = c + half + MARGIN, c + vhalf + MARGIN
            for li in o.layers:
                self._paint(bt[li], o, need_t)
            for li in o.layers:
                pass
            self._paint(bv, o, need_v)
        edge_t = EDGE_CLR + half + MARGIN if not is_mains(rl) else 1.5 + half + MARGIN
        for li in (0, 1):
            bt[li] |= self._edge_map < edge_t
        bv |= self._edge_map < (EDGE_CLR + vhalf + MARGIN)
        return bt, bv

    # ───────────────────────── A* search ─────────────────────────
    def _cells_in_pad(self, pad_obj, bt):
        x0, y0, x1, y1 = pad_obj.p
        g = self.g
        out = []
        ix0, ix1 = int(math.ceil(x0 / g - 1e-9)), int(math.floor(x1 / g + 1e-9))
        iy0, iy1 = int(math.ceil(y0 / g - 1e-9)), int(math.floor(y1 / g + 1e-9))
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        rx, ry = max((x1 - x0) / 2 - 0.1, 0.05), max((y1 - y0) / 2 - 0.1, 0.05)
        for li in pad_obj.layers:
            for iy in range(iy0, iy1 + 1):
                for ix in range(ix0, ix1 + 1):
                    if not (0 <= ix < self.nx and 0 <= iy < self.ny):
                        continue
                    if pad_obj.round and ((ix * g - cx) / rx) ** 2 + ((iy * g - cy) / ry) ** 2 > 1.0:
                        continue                      # bbox corner outside the copper circle / oval
                    out.append((li, ix, iy))
        if not out:                                    # tiny pad: use the cell nearest to the centre
            for li in pad_obj.layers:
                ix, iy = int(round(cx / g)), int(round(cy / g))
                if 0 <= ix < self.nx and 0 <= iy < self.ny:
                    out.append((li, ix, iy))
        return out

    def _stub_to_pad(self, net, rule, cell):
        """If `cell` lies in a pad of this net, add a short track from the cell to the pad centre (guarantees contact)."""
        li, ix, iy = cell
        x, y = ix * self.g, iy * self.g
        for o in self.objs:
            if o.kind != "rect" or o.net != net or li not in o.layers:
                continue
            x0, y0, x1, y1 = o.p
            if x0 - 1e-6 <= x <= x1 + 1e-6 and y0 - 1e-6 <= y <= y1 + 1e-6:
                cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
                if min(x1 - x0, y1 - y0) >= 1.2:
                    return                      # big pad: a cell inside it is copper already (and a stub to the centre could violate creepage)
                if abs(cx - x) > 1e-3 or abs(cy - y) > 1e-3:
                    self.tracks.append((li, [(x, y), (cx, cy)], rule.width, net))
                return

    def astar(self, sources, targets, bt, bv, pref_layer_dir=True, via_cost=9.0, max_expand=350000, allow_via=True):
        nx, ny = self.nx, self.ny
        tset = set(targets)
        if not tset or not sources:
            return None
        txs = [t[1] for t in tset]
        tys = [t[2] for t in tset]
        tx0, tx1, ty0, ty1 = min(txs), max(txs), min(tys), max(tys)
        btb = [bt[0].tobytes(), bt[1].tobytes()]
        bvb = bv.tobytes()

        def h(ix, iy):
            dx = max(tx0 - ix, 0, ix - tx1)
            dy = max(ty0 - iy, 0, iy - ty1)
            return (dx + dy) - 0.5857864 * min(dx, dy)

        INF = 1e18
        gscore = {}
        parent = {}
        heap = []
        for s in sources:
            li, ix, iy = s
            if btb[li][iy * nx + ix]:
                continue
            gscore[s] = 0.0
            heapq.heappush(heap, (h(ix, iy), 0.0, s, (0, 0)))
        steps = [(1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0),
                 (1, 1, 1.4142), (1, -1, 1.4142), (-1, 1, 1.4142), (-1, -1, 1.4142)]
        expanded = 0
        while heap:
            f, gcur, node, lastdir = heapq.heappop(heap)
            if gcur > gscore.get(node, INF) + 1e-9:
                continue
            if node in tset:
                path = [node]
                while node in parent:
                    node = parent[node]
                    path.append(node)
                path.reverse()
                return path
            expanded += 1
            if expanded > max_expand:
                return None
            li, ix, iy = node
            blk = btb[li]
            for dx, dy, c in steps:
                jx, jy = ix + dx, iy + dy
                if jx < 0 or jy < 0 or jx >= nx or jy >= ny:
                    continue
                if blk[jy * nx + jx]:
                    continue
                if dx and dy:
                    if blk[iy * nx + jx] or blk[jy * nx + ix]:
                        continue
                cost = c
                if pref_layer_dir:
                    horiz = dy == 0
                    vert = dx == 0
                    if li == 0 and vert:
                        cost *= 1.5
                    elif li == 1 and horiz:
                        cost *= 1.5
                    elif dx and dy:
                        cost *= 1.1
                if (dx, dy) != lastdir and lastdir != (0, 0):
                    cost += 0.35
                ng = gcur + cost
                nn = (li, jx, jy)
                if ng < gscore.get(nn, INF) - 1e-9:
                    gscore[nn] = ng
                    parent[nn] = node
                    heapq.heappush(heap, (ng + h(jx, jy), ng, nn, (dx, dy)))
            if allow_via:
                oli = 1 - li
                if not bvb[iy * nx + ix] and not btb[oli][iy * nx + ix]:
                    nn = (oli, ix, iy)
                    ng = gcur + via_cost
                    if ng < gscore.get(nn, INF) - 1e-9:
                        gscore[nn] = ng
                        parent[nn] = node
                        heapq.heappush(heap, (ng + h(ix, iy), ng, nn, (0, 0)))
        return None

    # ───────────────────────── net routing ─────────────────────────
    def route_net(self, net, verbose=True, pads=None):
        rule = self.rule(net)
        if pads is None:
            pads = [o for o in self.objs if o.net == net and o.kind == "rect"]
        pads = list(pads)
        if len(pads) < 2:
            return True
        bt, bv = self.build_masks(net, rule.width)
        g = self.g
        # pads belonging to the same *part* group are not merged; every pad is a terminal.
        connected = []        # cells of the growing tree
        done_pads = []
        remaining = list(pads)
        # start with the pad pair that is closest
        best = None
        for i in range(len(remaining)):
            for j in range(i + 1, len(remaining)):
                a, b = remaining[i], remaining[j]
                ca = ((a.p[0] + a.p[2]) / 2, (a.p[1] + a.p[3]) / 2)
                cb = ((b.p[0] + b.p[2]) / 2, (b.p[1] + b.p[3]) / 2)
                dd = abs(ca[0] - cb[0]) + abs(ca[1] - cb[1])
                if best is None or dd < best[0]:
                    best = (dd, i, j)
        _, i, j = best
        first = remaining[i]
        tree = set(self._cells_in_pad(first, bt))
        done_pads.append(first)
        remaining.pop(i)
        ok_all = True
        new_tracks = []
        new_vias = []
        # merge pads that are electrically one (same footprint numbering) = already in `pads` list
        while remaining:
            # choose the nearest remaining pad to the tree (manhattan to first pad cells approx)
            def pad_dist(p):
                c = ((p.p[0] + p.p[2]) / 2, (p.p[1] + p.p[3]) / 2)
                return min(abs(c[0] - (q.p[0] + q.p[2]) / 2) + abs(c[1] - (q.p[1] + q.p[3]) / 2) for q in done_pads)
            remaining.sort(key=pad_dist)
            p = remaining.pop(0)
            src = [c for c in self._cells_in_pad(p, bt) if not bt[c[0]][c[2], c[1]]]
            if not src:
                if verbose:
                    print(f"    [{net}] pad at ({p.p[0]:.1f},{p.p[1]:.1f}) has no free access cell")
                ok_all = False
                continue
            # if this pad already overlaps the tree (same THT pad etc.) nothing to do
            if any(c in tree for c in src):
                tree |= set(self._cells_in_pad(p, bt))
                done_pads.append(p)
                continue
            path = self.astar(src, list(tree), bt, bv, allow_via=net not in self.no_via_nets)
            if path is None:
                if verbose:
                    print(f"    [{net}] no path to pad at ({p.p[0]:.1f},{p.p[1]:.1f})")
                ok_all = False
                continue
            done_pads.append(p)
            tree |= set(self._cells_in_pad(p, bt))
            self._commit_path(net, rule, path, tree, new_tracks, new_vias)
        return ok_all

    def _commit_path(self, net, rule, path, tree, new_tracks, new_vias):
        g = self.g
        # split the path in per-layer runs
        run = [path[0]]
        runs = []
        for a, b in zip(path[:-1], path[1:]):
            if a[0] != b[0]:
                runs.append(run)
                self.vias.append((b[1] * g, b[2] * g, net))
                self.objs.append(Obj("circle", (b[1] * g, b[2] * g, VIA_D / 2), (0, 1), net))
                run = [b]
            else:
                run.append(b)
        runs.append(run)
        for r in runs:
            li = r[0][0]
            pts = [(c[1] * g, c[2] * g) for c in r]
            # simplify collinear
            simp = [pts[0]]
            for k in range(1, len(pts) - 1):
                a, b, c = simp[-1], pts[k], pts[k + 1]
                if abs((b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0])) < 1e-9:
                    continue
                simp.append(b)
            simp.append(pts[-1])
            if len(simp) >= 2 and simp[0] != simp[-1]:
                self.tracks.append((li, simp, rule.width, net))
                for a, b in zip(simp[:-1], simp[1:]):
                    self.objs.append(Obj("seg", (a[0], a[1], b[0], b[1]), (li,), net, rule.width / 2))
        for c in path:
            tree.add(c)
        # make sure both ends really touch pad copper
        self._stub_to_pad(net, rule, path[0])
        self._stub_to_pad(net, rule, path[-1])

    # ───────────────────────── driver ─────────────────────────
    def reset(self):
        self.objs = list(self._base_objs)
        self.tracks = []
        self.vias = []

    def route_all(self, order, verbose=True, retries=4):
        self._base_objs = list(self.objs)
        order = list(order)
        best = None
        for attempt in range(retries + 1):
            self.reset()
            failed = []
            t0 = time.time()
            for net in order:
                ok = self.route_net(net, verbose)
                if verbose:
                    print(f"  [try {attempt}] routed {net:<12} {'OK' if ok else 'FAILED'}   ({time.time() - t0:.1f}s)")
                if not ok:
                    failed.append(net)
            if best is None or len(failed) < len(best[0]):
                best = (failed, list(self.tracks), list(self.vias), list(self.objs))
            if not failed:
                break
            # promote failed nets to the front and try again
            order = failed + [n for n in order if n not in failed]
        failed, self.tracks, self.vias, self.objs = best
        return failed

    def add_fixed_track(self, layer, pts, width, net):
        """A hand-placed track (layer 0/1) that stays and counts as an obstacle for everything else."""
        self.fixed.append((layer, pts, width, net))
        for a, b in zip(pts[:-1], pts[1:]):
            self.objs.append(Obj("seg", (a[0], a[1], b[0], b[1]), (layer,), net, width / 2))

    def write_to_pcb(self, lock=False):
        for (li, pts, w, net) in self.fixed + self.tracks:
            self.pcb.add_track("F.Cu" if li == 0 else "B.Cu", pts, w, net, locked=lock)
        for (x, y, net) in self.vias:
            self.pcb.add_via(x, y, net, VIA_D, VIA_DRILL)
