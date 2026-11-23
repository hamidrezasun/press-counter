"""Build a .kicad_pcb from a design.Board + placement table using KiCad's pcbnew python API."""
import os
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
FP_STD = r"D:\KiCad\10.0\share\kicad\footprints"
FP_CUSTOM = os.path.normpath(os.path.join(HERE, "..", "lib", "press_counter.pretty"))

mm = pcbnew.FromMM
mm_ = pcbnew.ToMM
V = lambda x, y: pcbnew.VECTOR2I(mm(x), mm(y))


def load_fp(fpid):
    lib, name = fpid.split(":", 1)
    path = FP_CUSTOM if lib == "press_counter" else os.path.join(FP_STD, lib + ".pretty")
    fp = pcbnew.FootprintLoad(path, name)
    if fp is None:
        raise RuntimeError(f"footprint not found: {fpid}")
    return fp


class PCB:
    def __init__(self, board_def, origin=(30.0, 30.0)):
        self.d = board_def
        self.b = pcbnew.BOARD()
        self.b.SetCopperLayerCount(2)
        self.origin = origin
        self.nets = {}
        self.fps = {}

    @classmethod
    def from_board(cls, board, board_def, origin=(30.0, 30.0)):
        """Wrap an already loaded pcbnew BOARD (e.g. after importing a routing result)."""
        self = cls.__new__(cls)
        self.d = board_def
        self.b = board
        self.origin = origin
        self.nets = {}
        # (NetsByName() is not usable through the KiCad 10 SWIG bindings: collect nets from the pads instead)
        for fp in board.GetFootprints():
            for pad in fp.Pads():
                ni = pad.GetNet()
                if ni is not None and ni.GetNetname():
                    self.nets.setdefault(ni.GetNetname(), ni)
        self.fps = {fp.GetReference(): fp for fp in board.GetFootprints()}
        # geometry that later code needs is read NOW: after BOARD.Remove() the SWIG getters return untyped objects
        self._pos_cache = {ref: (pcbnew.ToMM(fp.GetPosition().x) - origin[0], pcbnew.ToMM(fp.GetPosition().y) - origin[1]) for ref, fp in self.fps.items()}
        self._tht_cache = self.tht_boxes(0.0)
        return self

    # coordinates: board-local mm (origin top-left of the outline) -> sheet mm
    def P(self, x, y):
        return V(x + self.origin[0], y + self.origin[1])

    def net(self, name):
        if name not in self.nets:
            ni = self.b.FindNet(name)
            if ni is not None:
                self.nets[name] = ni
                return ni
            ni = pcbnew.NETINFO_ITEM(self.b, name)
            self.b.Add(ni)
            self.nets[name] = ni
        return self.nets[name]

    def add_parts(self, placement):
        for p in self.d.parts:
            fp = load_fp(p.fp)
            fp.SetReference(p.ref)
            fp.SetValue(p.value)
            x, y, rot = placement[p.ref][:3]
            flip = placement[p.ref][3] if len(placement[p.ref]) > 3 else False
            self.b.Add(fp)
            if flip:
                fp.Flip(self.P(x, y), pcbnew.FLIP_DIRECTION_LEFT_RIGHT)
            fp.SetPosition(self.P(x, y))
            fp.SetOrientationDegrees(rot + (180 if flip else 0))     # KiCad's left-right flip leaves the part turned by 180 degrees
            for pad in fp.Pads():
                num = pad.GetNumber()
                net = p.pins.get(num)
                if net:
                    pad.SetNet(self.net(net))
                elif num in p.pins:      # intentionally unconnected: same pseudo-net name KiCad's netlist uses (keeps parity clean)
                    pad.SetNet(self.net(f"unconnected-({p.ref}-{self._pin_name(p, num).replace('/', '{slash}')}-Pad{num})"))
            if p.ref.startswith(("MH", "TP", "FID")):
                fp.SetAttributes(fp.GetAttributes() | pcbnew.FP_EXCLUDE_FROM_BOM)
            lib, name = p.fp.split(":", 1)
            fp.SetFPID(pcbnew.LIB_ID(lib, name))               # keeps schematic/PCB footprint parity clean
            self._tidy_texts(fp, p.ref)
            self.fps[p.ref] = fp
        for n in self.d.nets():
            self.net(n)

    @staticmethod
    def _pin_name(part, num):
        import kilib
        lib, name = part.sym.split(":")
        sym = kilib.flat_symbol(lib, name)
        for u in range(1, kilib.n_units(sym) + 1):
            for pin in kilib.unit_pins(sym, u):
                if pin["num"] == num:
                    return pin["name"]
        return f"Pin_{num}"

    def _tidy_texts(self, fp, ref):
        """Reference: 0.8 mm, horizontal, just above the part body; mounting holes: no reference. Value stays on Fab."""
        ref_f = fp.Reference()
        if ref.startswith("MH"):
            ref_f.SetVisible(False)
            return
        # body box = courtyard-free bbox of pads+graphics at rotation 0 -> use the footprint bbox (no text)
        bb = fp.GetBoundingBox(False)
        ref_f.SetTextSize(pcbnew.VECTOR2I(mm(0.8), mm(0.8)))
        ref_f.SetTextThickness(mm(0.12))
        ref_f.SetTextAngleDegrees(0)
        ref_f.SetPosition(pcbnew.VECTOR2I((bb.GetLeft() + bb.GetRight()) // 2, bb.GetTop() - mm(0.7)))

    def outline(self, x0, y0, x1, y1, r=2.0):
        """Rounded-rectangle board outline on Edge.Cuts."""
        L = pcbnew.Edge_Cuts

        def seg(a, b):
            s = pcbnew.PCB_SHAPE(self.b)
            s.SetShape(pcbnew.SHAPE_T_SEGMENT)
            s.SetLayer(L)
            s.SetStart(self.P(*a))
            s.SetEnd(self.P(*b))
            s.SetWidth(mm(0.1))
            self.b.Add(s)

        def arc(c, start, end):
            s = pcbnew.PCB_SHAPE(self.b)
            s.SetShape(pcbnew.SHAPE_T_ARC)
            s.SetLayer(L)
            import math
            # centre, start point, angle(deg) sweep 90
            s.SetCenter(self.P(*c))
            s.SetStart(self.P(*start))
            s.SetArcAngleAndEnd(pcbnew.EDA_ANGLE(90, pcbnew.DEGREES_T), False)
            s.SetWidth(mm(0.1))
            self.b.Add(s)

        if r <= 0:
            seg((x0, y0), (x1, y0)); seg((x1, y0), (x1, y1)); seg((x1, y1), (x0, y1)); seg((x0, y1), (x0, y0))
            return
        seg((x0 + r, y0), (x1 - r, y0)); seg((x1, y0 + r), (x1, y1 - r))
        seg((x1 - r, y1), (x0 + r, y1)); seg((x0, y1 - r), (x0, y0 + r))
        arc((x0 + r, y0 + r), (x0, y0 + r), None)
        arc((x1 - r, y0 + r), (x1 - r, y0), None)
        arc((x1 - r, y1 - r), (x1, y1 - r), None)
        arc((x0 + r, y1 - r), (x0 + r, y1), None)

    def text(self, s, x, y, size=1.0, layer=pcbnew.F_SilkS, rot=0, bold=False, mirror=False, justify=None):
        if layer == pcbnew.B_SilkS:                       # bottom silk is read from the back: mirrored; justification is given as seen from the back
            mirror = True
            justify = {"left": "right", "right": "left"}.get(justify, justify)
        size = max(size, 1.02)                            # fab legend limit (esys.ir level 1: characters >= 1.016 mm high, strokes >= 0.153 mm)
        t = pcbnew.PCB_TEXT(self.b)
        t.SetText(s)
        t.SetPosition(self.P(x, y))
        t.SetLayer(layer)
        t.SetTextSize(pcbnew.VECTOR2I(mm(size), mm(size)))
        t.SetTextThickness(mm(max(0.16, size * 0.16)))
        t.SetTextAngleDegrees(rot)
        if bold:
            t.SetBold(True)
        if mirror:
            t.SetMirrored(True)
        if justify == "left":
            t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT)
        elif justify == "right":
            t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_RIGHT)
        self.b.Add(t)
        return t

    def line(self, a, b, layer=pcbnew.F_SilkS, w=0.2):
        s = pcbnew.PCB_SHAPE(self.b)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetLayer(layer)
        s.SetStart(self.P(*a))
        s.SetEnd(self.P(*b))
        s.SetWidth(mm(w))
        self.b.Add(s)

    def rect_keepout(self, x0, y0, x1, y1, name="keepout", copper=True, vias=True, tracks=True, pours=True, layers=("F.Cu", "B.Cu")):
        z = pcbnew.ZONE(self.b)
        z.SetIsRuleArea(True)
        z.SetDoNotAllowZoneFills(pours)
        z.SetDoNotAllowTracks(tracks)
        z.SetDoNotAllowVias(vias)
        z.SetDoNotAllowPads(False)
        z.SetDoNotAllowFootprints(False)
        ls = pcbnew.LSET()
        for ln in layers:
            ls.AddLayer(self.b.GetLayerID(ln))
        z.SetLayerSet(ls)
        z.SetZoneName(name)
        poly = pcbnew.SHAPE_LINE_CHAIN()
        for (x, y) in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
            poly.Append(mm(x + self.origin[0]), mm(y + self.origin[1]))
        poly.SetClosed(True)
        z.Outline().AddOutline(poly)
        self.b.Add(z)
        return z

    def add_track(self, layer, pts, width, netname, locked=False):
        net = self.net(netname)
        for a, b in zip(pts[:-1], pts[1:]):
            t = pcbnew.PCB_TRACK(self.b)
            t.SetStart(self.P(*a))
            t.SetEnd(self.P(*b))
            t.SetWidth(mm(width))
            t.SetLayer(self.b.GetLayerID(layer))
            t.SetNet(net)
            if locked:
                t.SetLocked(True)
            self.b.Add(t)

    def add_via(self, x, y, netname, dia=0.6, drill=0.3, locked=False):
        v = pcbnew.PCB_VIA(self.b)
        v.SetPosition(self.P(x, y))
        v.SetWidth(mm(dia))
        v.SetDrill(mm(drill))
        v.SetViaType(pcbnew.VIATYPE_THROUGH)
        v.SetNet(self.net(netname))
        if locked:
            v.SetLocked(True)
        self.b.Add(v)

    def add_zone(self, netname, layer, x0, y0, x1, y1, clearance=0.3, min_w=0.25, poly=None, priority=0):
        z = pcbnew.ZONE(self.b)
        z.SetLayer(self.b.GetLayerID(layer))
        z.SetNet(self.net(netname))
        z.SetLocalClearance(mm(clearance))
        z.SetMinThickness(mm(min_w))
        z.SetAssignedPriority(priority)
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
        z.SetThermalReliefGap(mm(0.3))
        z.SetThermalReliefSpokeWidth(mm(0.4))
        ch = pcbnew.SHAPE_LINE_CHAIN()
        pts = poly or [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
        for (x, y) in pts:
            ch.Append(mm(x + self.origin[0]), mm(y + self.origin[1]))
        ch.SetClosed(True)
        z.Outline().AddOutline(ch)
        self.b.Add(z)
        return z

    def place(self, ref, x, y, rot=None):
        fp = self.fps[ref]
        fp.SetPosition(self.P(x, y))
        if rot is not None:
            fp.SetOrientationDegrees(rot)

    def pos(self, ref):
        if getattr(self, "_pos_cache", None) and ref in self._pos_cache:
            return self._pos_cache[ref]
        p = self.fps[ref].GetPosition()
        return pcbnew.ToMM(p.x) - self.origin[0], pcbnew.ToMM(p.y) - self.origin[1]

    def near(self, ref, anchor, pad, dx, dy, rot=None):
        """Place `ref` at (dx, dy) from pad `pad` of `anchor` (board mm)."""
        x, y = self.pad_pos(anchor, pad)[0]
        self.place(ref, x + dx, y + dy, rot)

    def hide_refs(self, refs):
        """Reference designators that do not fit next to their part: not printed (the BOM / position file / schematic still have them)."""
        for ref in refs:
            if ref in self.fps:
                self.fps[ref].Reference().SetVisible(False)

    def edit_fp_rect(self, ref, half_w, half_h, dot_center=None):
        """Resize the library body rectangle (and move the pin-1 dot) of a footprint that is not rotated - keeps the footprint identical to
        the edited library file (no 'footprint differs from library' warning)."""
        fp = self.fps[ref]
        x, y = self.pos(ref)
        for d in fp.GraphicalItems():
            if d.GetLayer() != pcbnew.F_SilkS:
                continue
            sh = d.GetShape()
            if sh == pcbnew.SHAPE_T_RECT:
                d.SetStart(self.P(x - half_w, y - half_h))
                d.SetEnd(self.P(x + half_w, y + half_h))
            elif sh == pcbnew.SHAPE_T_CIRCLE and dot_center:
                r = pcbnew.ToMM(d.GetEnd().x - d.GetStart().x) if False else 0.25
                d.SetStart(self.P(x + dot_center[0], y + dot_center[1]))
                d.SetEnd(self.P(x + dot_center[0] + r, y + dot_center[1]))

    def tht_boxes(self, margin=0.6):
        """Bounding boxes (board-local mm) of all through-hole pads: the back-side help text keeps away from them."""
        if getattr(self, "_tht_cache", None) is not None:
            return [(b[0] - margin, b[1] - margin, b[2] + margin, b[3] + margin) for b in self._tht_cache]
        ox, oy = self.origin
        out = []
        for fp in self.fps.values():
            for pad in fp.Pads():
                if pad.GetAttribute() in (pcbnew.PAD_ATTRIB_PTH, pcbnew.PAD_ATTRIB_NPTH):
                    bb = pad.GetBoundingBox()
                    out.append((pcbnew.ToMM(bb.GetLeft()) - ox - margin, pcbnew.ToMM(bb.GetTop()) - oy - margin,
                                pcbnew.ToMM(bb.GetRight()) - ox + margin, pcbnew.ToMM(bb.GetBottom()) - oy + margin))
        return out

    def ref_offsets(self, offsets):
        """Move reference designator texts: {ref: (dx, dy)} relative to the footprint position."""
        for ref, (dx, dy) in offsets.items():
            fp = self.fps[ref]
            p = fp.GetPosition()
            fp.Reference().SetPosition(pcbnew.VECTOR2I(p.x + mm(dx), p.y + mm(dy)))

    def rect_outline(self, x0, y0, x1, y1, layer=pcbnew.F_SilkS, w=0.16, dash=0.0):
        """Rectangle outline (optionally dashed) on a graphic layer."""
        self.poly_outline([(x0, y0), (x1, y0), (x1, y1), (x0, y1)], layer, w, dash)

    def poly_outline(self, pts, layer=pcbnew.F_SilkS, w=0.16, dash=0.0):
        """Closed polygon outline (optionally dashed) on a graphic layer; dashes that would run through a part are left out."""
        for a, b in zip(pts, pts[1:] + pts[:1]):
            if dash <= 0:
                self.line(a, b, layer, w)
                continue
            import math
            L = math.hypot(b[0] - a[0], b[1] - a[1])
            n = max(1, int(L / (2 * dash)))
            for i in range(n + 1):
                s0 = i * 2 * dash / L
                s1 = min((i * 2 + 1) * dash / L, 1.0)
                if s0 >= 1.0:
                    break
                p = (a[0] + (b[0] - a[0]) * s0, a[1] + (b[1] - a[1]) * s0)
                q = (a[0] + (b[0] - a[0]) * s1, a[1] + (b[1] - a[1]) * s1)
                if not self._crosses_part(p, q):
                    self.line(p, q, layer, w)

    def _crosses_part(self, p, q, margin=0.5):
        """True if the segment p-q (board-local mm) passes within `margin` of a footprint (so a dashed outline does not run through parts)."""
        if not hasattr(self, "_boxes"):
            ox, oy = self.origin
            self._boxes = []
            for ref, fp in self.fps.items():
                bb = fp.GetBoundingBox(True)
                g = 2.2 if ref.startswith("MH") else 0.0            # keep dashed outlines well away from mounting holes
                self._boxes.append((mm_(bb.GetLeft()) - ox - g, mm_(bb.GetTop()) - oy - g, mm_(bb.GetRight()) - ox + g, mm_(bb.GetBottom()) - oy + g))
        x0, x1 = min(p[0], q[0]) - margin, max(p[0], q[0]) + margin
        y0, y1 = min(p[1], q[1]) - margin, max(p[1], q[1]) + margin
        return any(not (b[2] < x0 or b[0] > x1 or b[3] < y0 or b[1] > y1) for b in self._boxes)

    def label_pads(self, ref, texts, dx=0.0, dy=0.0, size=0.8, rot=0, justify=None, bold=False, layer=pcbnew.F_SilkS):
        """Silk text next to the pads of `ref`: texts = {pad number: text}; offset (dx, dy) from the pad centre (board mm)."""
        for num, s in texts.items():
            for (x, y) in self.pad_pos(ref, num):
                self.text(s, x + dx, y + dy, size, rot=rot, justify=justify, bold=bold, layer=layer)

    def save(self, path):
        self.b.Save(path)

    # geometry helpers --------------------------------------------------------
    def pad_pos(self, ref, num):
        """All pad centres (board-local mm) for ref/pad number."""
        out = []
        for pad in self.fps[ref].Pads():
            if pad.GetNumber() == num:
                p = pad.GetPosition()
                out.append((pcbnew.ToMM(p.x) - self.origin[0], pcbnew.ToMM(p.y) - self.origin[1]))
        return out
