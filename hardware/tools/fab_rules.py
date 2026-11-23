"""Fabrication-house rules applied to a routed board (esys.ir level 1, https://esys.ir/page.aspx?i=59), run by resilk.py after the silk is rebuilt.

  * legend: every silk stroke >= 0.16 mm (limit 0.153), every visible silk text >= 1.02 mm high (limit 1.016) with a 0.16 mm stroke
  * routed slots narrower than 1 mm (the shield-tab slots of the micro-USB footprint J3) become round holes of the slot's narrow dimension
    (the generic SMD micro-USB that the BOM lists has no through-hole tabs; the holes are only the footprint's anchor points)
Checked by check_esys.py.
"""
import pcbnew

mm = pcbnew.FromMM
LEGEND_W = 0.16
LEGEND_H = 1.02
CUTOUT_MIN = 1.0


def apply(b):
    silk = (pcbnew.F_SilkS, pcbnew.B_SilkS)
    n = {"lines": 0, "texts": 0, "slots": 0}

    def fix_text(t):
        if not t.IsVisible() or t.GetLayer() not in silk:
            return
        s = t.GetTextSize()
        h = max(pcbnew.ToMM(s.y), LEGEND_H)
        w = max(pcbnew.ToMM(s.x), LEGEND_H)
        if (w, h) != (pcbnew.ToMM(s.x), pcbnew.ToMM(s.y)):
            t.SetTextSize(pcbnew.VECTOR2I(mm(w), mm(h)))
            n["texts"] += 1
        if pcbnew.ToMM(t.GetTextThickness()) < LEGEND_W - 1e-6:
            t.SetTextThickness(mm(LEGEND_W))
            n["texts"] += 1

    for d in b.GetDrawings():
        if d.GetLayer() in silk:
            if d.GetClass() == "PCB_TEXT":
                fix_text(d)
            elif hasattr(d, "GetWidth") and pcbnew.ToMM(d.GetWidth()) < LEGEND_W - 1e-6:
                d.SetWidth(mm(LEGEND_W))
                n["lines"] += 1
    for fp in b.GetFootprints():
        for f in fp.GetFields():
            fix_text(f)
        for g in fp.GraphicalItems():
            if g.GetLayer() in silk:
                if g.GetClass() == "PCB_TEXT":
                    fix_text(g)
                elif hasattr(g, "GetWidth") and pcbnew.ToMM(g.GetWidth()) < LEGEND_W - 1e-6:
                    g.SetWidth(mm(LEGEND_W))
                    n["lines"] += 1
        for p in fp.Pads():
            dx, dy = pcbnew.ToMM(p.GetDrillSizeX()), pcbnew.ToMM(p.GetDrillSizeY())
            if dx > 0 and abs(dx - dy) > 0.01 and min(dx, dy) < CUTOUT_MIN:
                d = min(dx, dy)
                p.SetDrillShape(pcbnew.PAD_DRILL_SHAPE_CIRCLE)
                p.SetDrillSize(pcbnew.VECTOR2I(mm(d), mm(d)))
                n["slots"] += 1
    return n
