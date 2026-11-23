"""Print the courtyard (or pad) size of every footprint used by the boards: python fpsizes.py"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pcbnew, design, pcbgen

seen = {}
for key, f in design.BOARDS.items():
    for p in f().parts:
        seen.setdefault(p.fp, []).append(f"{key}:{p.ref}")
for fpid, refs in sorted(seen.items()):
    fp = pcbgen.load_fp(fpid)
    cy = fp.GetCourtyard(pcbnew.F_CrtYd)
    bb = cy.BBox() if cy.OutlineCount() else fp.GetBoundingBox(False)
    w, h = pcbnew.ToMM(bb.GetWidth()), pcbnew.ToMM(bb.GetHeight())
    cx, cyy = pcbnew.ToMM(bb.GetCenter().x - fp.GetPosition().x), pcbnew.ToMM(bb.GetCenter().y - fp.GetPosition().y)
    pads = [(pcbnew.ToMM(pd.GetPosition().x - fp.GetPosition().x), pcbnew.ToMM(pd.GetPosition().y - fp.GetPosition().y)) for pd in fp.Pads()]
    xs = [p[0] for p in pads] or [0]; ys = [p[1] for p in pads] or [0]
    print(f"{fpid:75s} crtyd {w:6.2f} x {h:6.2f} (centre {cx:+.2f},{cyy:+.2f}) pads x[{min(xs):+.2f},{max(xs):+.2f}] y[{min(ys):+.2f},{max(ys):+.2f}] {len(pads)}p  {','.join(refs[:4])}")
