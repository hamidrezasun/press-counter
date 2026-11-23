"""Check the routed board against the esys.ir level-1 FR-4 capability (https://esys.ir/page.aspx?i=59) and order notes (i=58).

    D:/KiCad/10.0/bin/python.exe -P check_esys.py            exit code 1 if anything is outside the limits

Limits (FR-4, 2 layers, 1 oz): board <= 60 x 50 cm, 0.4-2 mm thick; track >= 0.127 mm; spacing >= 0.127 mm; drill 0.2-6.3 mm; via inner >= 0.2 and
outer >= 0.45 mm; annular ring >= 0.1016 mm; legend stroke >= 0.153 mm and character height >= 1.016 mm; cutout width >= 1 mm;
plated half holes >= 0.6 mm.  Spacing is checked by the KiCad DRC (the board rules are 0.2 mm), not here.
"""
import os
import sys
from collections import Counter

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
PCB = os.path.normpath(os.path.join(HERE, "..", "main", "press_counter.kicad_pcb"))
mm = pcbnew.ToMM
TRACK_MIN, DRILL_MIN, DRILL_MAX, VIA_IN, VIA_OUT, ANN_MIN = 0.127, 0.2, 6.3, 0.2, 0.45, 0.1016
SILK_W, SILK_H, CUTOUT = 0.153, 1.016, 1.0
bad = []


def fail(msg):
    bad.append(msg)


b = pcbnew.LoadBoard(PCB)
bb = b.GetBoardEdgesBoundingBox()
w, h = mm(bb.GetWidth()), mm(bb.GetHeight())
print(f"board {w / 10:.2f} x {h / 10:.2f} cm, {b.GetCopperLayerCount()} copper layers, thickness {mm(b.GetDesignSettings().GetBoardThickness()):.2f} mm")
if max(w, h) > 600 or min(w, h) > 500:
    fail("board larger than 60 x 50 cm")
tw = [mm(t.GetWidth()) for t in b.GetTracks() if t.GetClass() != "PCB_VIA"]
print(f"tracks: {len(tw)}, narrowest {min(tw):.3f} mm")
if min(tw) < TRACK_MIN - 1e-6:
    fail(f"track {min(tw):.3f} mm < {TRACK_MIN}")
for v in (t for t in b.GetTracks() if t.GetClass() == "PCB_VIA"):
    d, o = mm(v.GetDrillValue()), mm(v.GetWidth(pcbnew.F_Cu))
    if d < VIA_IN - 1e-6 or o < VIA_OUT - 1e-6 or (o - d) / 2 < ANN_MIN - 1e-6:
        fail(f"via {o}/{d} at {mm(v.GetPosition().x):.1f},{mm(v.GetPosition().y):.1f}")
holes = Counter()
for fp in b.GetFootprints():
    ref = fp.GetReference()
    for p in fp.Pads():
        dx, dy = mm(p.GetDrillSizeX()), mm(p.GetDrillSizeY())
        if dx <= 0:
            continue
        holes[(round(dx, 2), round(dy, 2))] += 1
        if min(dx, dy) < DRILL_MIN - 1e-6 or max(dx, dy) > DRILL_MAX + 1e-6:
            fail(f"{ref}.{p.GetNumber()} drill {dx}x{dy}")
        if p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH:
            ring = (min(mm(p.GetSizeX()), mm(p.GetSizeY())) - min(dx, dy)) / 2
            if ring < ANN_MIN - 1e-6:
                fail(f"{ref}.{p.GetNumber()} annular ring {ring:.3f} mm")
        if abs(dx - dy) > 0.01 and min(dx, dy) < CUTOUT:
            fail(f"{ref}.{p.GetNumber()} slot {dx} x {dy} mm is narrower than the 1 mm minimum cutout width")
print("hole sizes (mm):", dict(sorted(holes.items())))
# legend (silkscreen): every text and graphic on F.SilkS / B.SilkS
silk = {pcbnew.F_SilkS, pcbnew.B_SilkS}
texts = shapes = 0
small_h = Counter()
thin = Counter()


def chk_text(t, where):
    global texts
    if not t.IsVisible() or t.GetLayer() not in silk:
        return
    texts += 1
    if mm(t.GetTextHeight()) < SILK_H - 1e-6:
        small_h[round(mm(t.GetTextHeight()), 2)] += 1
    if mm(t.GetTextThickness()) < SILK_W - 1e-6:
        thin[("text", round(mm(t.GetTextThickness()), 3))] += 1


for d in b.GetDrawings():
    if d.GetLayer() in silk:
        if d.GetClass() == "PCB_TEXT":
            chk_text(d, "board")
        elif hasattr(d, "GetWidth"):
            shapes += 1
            if mm(d.GetWidth()) < SILK_W - 1e-6:
                thin[("line", round(mm(d.GetWidth()), 3))] += 1
for fp in b.GetFootprints():
    for f in fp.GetFields():
        chk_text(f, fp.GetReference())
    for g in fp.GraphicalItems():
        if g.GetLayer() in silk:
            if g.GetClass() == "PCB_TEXT":
                chk_text(g, fp.GetReference())
            elif hasattr(g, "GetWidth"):
                shapes += 1
                if mm(g.GetWidth()) < SILK_W - 1e-6:
                    thin[("line", round(mm(g.GetWidth()), 3))] += 1
print(f"silk: {texts} visible texts, {shapes} graphic items")
if small_h:
    fail(f"silk text height < {SILK_H} mm: {dict(small_h)}")
if thin:
    fail(f"silk stroke < {SILK_W} mm: {dict(thin)}")
print("RESULT:", "OK, all level-1 limits met" if not bad else "")
for m in bad:
    print(" -", m)
sys.exit(1 if bad else 0)
