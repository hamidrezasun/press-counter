"""Island repair for nets the autorouter left incomplete.

    python repair.py main NET [--window x0 y0 x1 y1] [--apply]

Finds the copper islands of NET on the routed board (union-find over pads, vias and tracks).  With --apply: every unlocked track / via of
OTHER nets inside the window is removed (they are routed again afterwards by `route_fr.py main --continue`), the islands are joined with the
own router (locked tracks), and the board is saved.
"""
import math
import os
import sys

import pcbnew
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common, pcbgen, project, router

mm = pcbnew.ToMM


# ── geometry: every copper item as (kind, data, layers) in board-local mm ───────────────────────────────────
def seg_dist(p, a, b):
    ax, ay, bx, by = a[0], a[1], b[0], b[1]
    dx, dy = bx - ax, by - ay
    L2 = dx * dx + dy * dy
    t = 0.0 if L2 == 0 else max(0.0, min(1.0, ((p[0] - ax) * dx + (p[1] - ay) * dy) / L2))
    return math.hypot(p[0] - (ax + t * dx), p[1] - (ay + t * dy))


def seg_seg_dist(a, b, c, d):
    def inter(p1, p2, p3, p4):
        def ccw(x, y, z):
            return (z[1] - x[1]) * (y[0] - x[0]) - (y[1] - x[1]) * (z[0] - x[0])
        return (ccw(p1, p3, p4) * ccw(p2, p3, p4) < 0) and (ccw(p1, p2, p3) * ccw(p1, p2, p4) < 0)
    if inter(a, b, c, d):
        return 0.0
    return min(seg_dist(a, c, d), seg_dist(b, c, d), seg_dist(c, a, b), seg_dist(d, a, b))


def rect_point_dist(r, p):
    dx = max(r[0] - p[0], 0, p[0] - r[2])
    dy = max(r[1] - p[1], 0, p[1] - r[3])
    return math.hypot(dx, dy)


def seg_rect_dist(a, b, r):
    # sample the segment densely (items are small) - good to 0.02 mm
    n = max(2, int(math.hypot(b[0] - a[0], b[1] - a[1]) / 0.02))
    best = 1e9
    for i in range(n + 1):
        t = i / n
        best = min(best, rect_point_dist(r, (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)))
        if best == 0:
            break
    return best


def touching(A, B):
    """A, B = (kind, data, layers, half_width).  kinds: rect (x0,y0,x1,y1), seg (a, b), circle (cx, cy, r)"""
    if not set(A[2]) & set(B[2]):
        return False
    ka, da, _, ha = A
    kb, db, _, hb = B
    if ka > kb:
        A, B, ka, kb, da, db, ha, hb = B, A, kb, ka, db, da, hb, ha
    # sorted kinds: circle < rect < seg
    if ka == "circle" and kb == "circle":
        return math.hypot(da[0] - db[0], da[1] - db[1]) <= da[2] + db[2]
    if ka == "circle" and kb == "rect":
        return rect_point_dist(db, da[:2]) <= da[2]
    if ka == "circle" and kb == "seg":
        return seg_dist(da[:2], db[0], db[1]) <= da[2] + hb
    if ka == "rect" and kb == "rect":
        return not (da[2] < db[0] or db[2] < da[0] or da[3] < db[1] or db[3] < da[1])
    if ka == "rect" and kb == "seg":
        return seg_rect_dist(db[0], db[1], da) <= hb
    if ka == "seg" and kb == "seg":
        return seg_seg_dist(da[0], da[1], db[0], db[1]) <= ha + hb
    raise ValueError((ka, kb))


def collect(pcb, board, net):
    """Copper items of `net` -> list of dicts (id, geom, kind, ref)."""
    ox, oy = pcb.origin
    items = []
    for ref, fp in pcb.fps.items():
        for pad in fp.Pads():
            if pad.GetNetname() != net:
                continue
            bb = pad.GetBoundingBox()
            r = (mm(bb.GetLeft()) - ox, mm(bb.GetTop()) - oy, mm(bb.GetRight()) - ox, mm(bb.GetBottom()) - oy)
            layers = tuple(i for i, L in enumerate((pcbnew.F_Cu, pcbnew.B_Cu)) if pad.IsOnLayer(L))
            items.append(dict(kind="rect", data=r, layers=layers, hw=0.0, label=f"{ref}.{pad.GetNumber()}",
                              round=pad.GetShape() in (pcbnew.PAD_SHAPE_CIRCLE, pcbnew.PAD_SHAPE_OVAL)))
    for t in board.GetTracks():
        if t.GetNetname() != net:
            continue
        s, e = t.GetStart(), t.GetEnd()
        if t.GetClass() == "PCB_VIA":
            items.append(dict(kind="circle", data=(mm(s.x) - ox, mm(s.y) - oy, mm(t.GetWidth(pcbnew.F_Cu)) / 2), layers=(0, 1), hw=0.0, label="via"))
        else:
            li = 0 if t.GetLayer() == pcbnew.F_Cu else 1
            items.append(dict(kind="seg", data=((mm(s.x) - ox, mm(s.y) - oy), (mm(e.x) - ox, mm(e.y) - oy)), layers=(li,), hw=mm(t.GetWidth()) / 2, label="trk"))
    return items


def islands(items):
    n = len(items)
    parent = list(range(n))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i
    for i in range(n):
        Ai = (items[i]["kind"], items[i]["data"], items[i]["layers"], items[i]["hw"])
        for j in range(i + 1, n):
            Bj = (items[j]["kind"], items[j]["data"], items[j]["layers"], items[j]["hw"])
            if touching(Ai, Bj):
                parent[find(i)] = find(j)
    groups = {}
    for i in range(n):
        groups.setdefault(find(i), []).append(i)
    return list(groups.values())


def item_cells(rt, it):
    g = rt.g
    out = set()
    if it["kind"] == "rect":
        for c in rt._cells_in_pad(router.Obj("rect", it["data"], it["layers"], None, round=it.get("round", False)), None):
            out.add(c)
    elif it["kind"] == "circle":
        cx, cy, r = it["data"]
        for li in it["layers"]:
            for ix in range(int((cx - r) / g), int((cx + r) / g) + 2):
                for iy in range(int((cy - r) / g), int((cy + r) / g) + 2):
                    if math.hypot(ix * g - cx, iy * g - cy) <= r:
                        out.add((li, ix, iy))
    else:
        (ax, ay), (bx, by) = it["data"]
        n = max(1, int(math.hypot(bx - ax, by - ay) / (g / 2)))
        li = it["layers"][0]
        for k in range(n + 1):
            t = k / n
            out.add((li, int(round((ax + (bx - ax) * t) / g)), int(round((ay + (by - ay) * t) / g))))
    return out


def load_other_nets(rt, pcb, board, net):
    ox, oy = pcb.origin
    n = 0
    for t in board.GetTracks():
        nn = t.GetNetname()
        if nn == net:
            continue
        s, e = t.GetStart(), t.GetEnd()
        if t.GetClass() == "PCB_VIA":
            rt.objs.append(router.Obj("circle", (mm(s.x) - ox, mm(s.y) - oy, mm(t.GetWidth(pcbnew.F_Cu)) / 2), (0, 1), nn))
        else:
            li = 0 if t.GetLayer() == pcbnew.F_Cu else 1
            rt.objs.append(router.Obj("seg", (mm(s.x) - ox, mm(s.y) - oy, mm(e.x) - ox, mm(e.y) - oy), (li,), nn, mm(t.GetWidth()) / 2))
        n += 1
    for z in board.Zones():                                    # keep-out areas (antennas, creepage rings) are obstacles too (bounding box)
        if z.GetIsRuleArea() and z.GetDoNotAllowTracks():
            bb = z.GetBoundingBox()
            rt.objs.append(router.Obj("rect", (mm(bb.GetLeft()) - ox, mm(bb.GetTop()) - oy, mm(bb.GetRight()) - ox, mm(bb.GetBottom()) - oy),
                                      (0, 1), "__keepout__"))
            n += 1
    return n


def join_islands(rt, net, items, groups):
    """Connect the islands (lists of item indices) with A* paths; the biggest island (most pads) is the start of the tree."""
    rule = rt.rule(net)
    bt, bv = rt.build_masks(net, rule.width)
    cells = [set().union(*[item_cells(rt, items[i]) for i in g]) for g in groups]
    npads = [sum(1 for i in g if items[i]["kind"] == "rect") for g in groups]
    order = sorted(range(len(groups)), key=lambda k: -npads[k])
    tree = set(cells[order[0]])
    ok = True
    for k in order[1:]:
        src = [c for c in cells[k] if not bt[c[0]][c[2], c[1]]]
        path = rt.astar(src, list(tree), bt, bv, max_expand=3_000_000) if src else None
        if path is None:
            print(f"   island of {npads[k]} pads: NO PATH")
            ok = False
            continue
        nt0 = len(rt.tracks)
        rt._commit_path(net, rule, path, tree, [], [])
        for c in (path[0], path[-1]):                       # make sure the copper really reaches a pad centre (round pads have no copper in the bbox corners)
            x, y = c[1] * rt.g, c[2] * rt.g
            for it in items:
                if it["kind"] == "rect" and c[0] in it["layers"]:
                    x0, y0, x1, y1 = it["data"]
                    if x0 - 1e-6 <= x <= x1 + 1e-6 and y0 - 1e-6 <= y <= y1 + 1e-6:
                        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
                        if math.hypot(cx - x, cy - y) > 0.05 and not any(abs(t[1][0][0] - cx) < 1e-3 and abs(t[1][0][1] - cy) < 1e-3 for t in rt.tracks[nt0:]):
                            rt.tracks.append((c[0], [(x, y), (cx, cy)], rule.width, net))
                        break
        tree |= cells[k]
        print(f"   island of {npads[k]} pads joined with a path of {len(path)} cells")
    return ok


def main():
    key, net = sys.argv[1], sys.argv[2]
    apply = "--apply" in sys.argv
    bi = common.info(key)
    b = pcbnew.LoadBoard(bi.pcb)
    pcb = pcbgen.PCB.from_board(b, bi.board)
    items = collect(pcb, b, net)
    isl = [g for g in islands(items) if any(items[i]["kind"] == "rect" for i in g)]
    print(f"{net}: {len(items)} copper items, {len(isl)} islands with pads")
    for g in sorted(isl, key=len):
        print("  ", len(g), ", ".join(items[i]["label"] for i in g if items[i]["kind"] == "rect")[:120])
    if len(isl) < 2 or not apply:
        return
    router.MARGIN = 0.03
    bd = bi.board
    rules = router.default_rules(bd.mains_nets, bd.mains_dc_nets, bd.power_nets, bd.wide)
    rules[net] = router.NetRule(0.25, 0.2, "Power")
    g = float(sys.argv[sys.argv.index("--grid") + 1]) if "--grid" in sys.argv else 0.1
    rt = router.Router(pcb, bi.layout.W, bi.layout.H, rules, grid=g, corner_r=3.0)
    print("other-net copper loaded:", load_other_nets(rt, pcb, b, net))
    ok = join_islands(rt, net, items, isl)
    old_vias = [(mm(t.GetPosition().x) - pcb.origin[0], mm(t.GetPosition().y) - pcb.origin[1]) for t in b.GetTracks() if t.GetClass() == "PCB_VIA" and t.GetNetname() == net]
    rt.vias = [v for v in rt.vias if not any(math.hypot(v[0] - o[0], v[1] - o[1]) < 0.4 for o in old_vias)]
    print("tracks", len(rt.tracks), "vias", len(rt.vias))
    for (li, pts, w, nn) in rt.tracks:
        pcb.add_track("F.Cu" if li == 0 else "B.Cu", pts, w, nn, locked=True)
    for (x, y, nn) in rt.vias:
        pcb.add_via(x, y, nn, router.VIA_D, router.VIA_DRILL, locked=True)
    out = bi.pcb if "--inplace" in sys.argv else bi.pcb.replace(".kicad_pcb", "_repaired.kicad_pcb")
    b.Save(out)
    print("saved", out, "all joined" if ok else "INCOMPLETE")


if __name__ == "__main__":
    main()
