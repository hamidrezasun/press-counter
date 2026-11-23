"""Laser-etch artwork for the board (galvo fibre-laser software such as EzCad, which reads BMP): 1-bit bitmaps + drill list.

    D:/KiCad/10.0/bin/python.exe -P make_laser_files.py [dpi]          (default 1016 dpi = 25 um per pixel)

Writes ../fabrication/laser/:
  top_remove.bmp     BLACK = copper the laser must remove (everything that is not track / pad / pour), viewed from the TOP
  bottom_remove.bmp  the same for the bottom layer, MIRRORED left-right: flip the board about its vertical axis, then it lines up
  drill_list.csv     every hole: x, y (mm from the top-left corner of the board, viewed from the top), diameter, type, net
  drill_summary.txt  tools and counts
Each hole centre has a 0.25 mm black dot inside its pad (a pilot pit for the drill bit).  The picture covers exactly the board
rectangle (202.1 x 104.1 mm): cut the blank to that size, or leave a margin, nothing is marked outside.
The bitmaps carry their resolution (DPI) in the header: import with "keep original size" or set 1016 dpi in EzCad.
"""
import csv
import math
import os
import sys

import pcbnew
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "fabrication", "laser"))
PCB = os.path.normpath(os.path.join(HERE, "..", "main", "press_counter.kicad_pcb"))
mm = pcbnew.ToMM
DPI = float(sys.argv[1]) if len(sys.argv) > 1 else 1016.0
PX = DPI / 25.4                                   # pixels per mm


def poly_to_draw(ps, draw, ox, oy, value):
    """draw every outline of a SHAPE_POLY_SET with `value`, then its holes with the opposite value"""
    for i in range(ps.OutlineCount()):
        ol = ps.Outline(i)
        pts = [((mm(ol.CPoint(k).x) - ox) * PX, (mm(ol.CPoint(k).y) - oy) * PX) for k in range(ol.PointCount())]
        if len(pts) >= 3:
            draw.polygon(pts, fill=value)
        for h in range(ps.HoleCount(i)):
            hl = ps.Hole(i, h)
            hp = [((mm(hl.CPoint(k).x) - ox) * PX, (mm(hl.CPoint(k).y) - oy) * PX) for k in range(hl.PointCount())]
            if len(hp) >= 3:
                draw.polygon(hp, fill=0 if value else 255)


def layer_image(b, layer, ox, oy, w, h):
    img = Image.new("L", (int(round(w * PX)), int(round(h * PX))), 0)          # 255 = copper
    d = ImageDraw.Draw(img)
    # zone fills first, then tracks, pads, vias on top
    for z in b.Zones():
        if z.GetIsRuleArea() or not z.IsOnLayer(layer):
            continue
        poly_to_draw(z.GetFilledPolysList(layer), d, ox, oy, 255)
    for t in b.GetTracks():
        if t.GetClass() == "PCB_VIA":
            c = t.GetPosition()
            r = mm(t.GetWidth(layer)) / 2
            d.ellipse([(mm(c.x) - ox - r) * PX, (mm(c.y) - oy - r) * PX, (mm(c.x) - ox + r) * PX, (mm(c.y) - oy + r) * PX], fill=255)
        elif t.GetLayer() == layer:
            ps = pcbnew.SHAPE_POLY_SET()
            t.TransformShapeToPolygon(ps, layer, 0, pcbnew.FromMM(0.005), pcbnew.ERROR_INSIDE)
            poly_to_draw(ps, d, ox, oy, 255)
    for fp in b.GetFootprints():
        for p in fp.Pads():
            if not p.IsOnLayer(layer):
                continue
            ps = pcbnew.SHAPE_POLY_SET()
            p.TransformShapeToPolygon(ps, layer, 0, pcbnew.FromMM(0.005), pcbnew.ERROR_INSIDE)
            poly_to_draw(ps, d, ox, oy, 255)
    return img, d


def main():
    b = pcbnew.LoadBoard(PCB)
    bb = b.GetBoardEdgesBoundingBox()
    ox, oy, w, h = mm(bb.GetLeft()), mm(bb.GetTop()), mm(bb.GetWidth()), mm(bb.GetHeight())
    os.makedirs(OUT, exist_ok=True)
    holes = []
    for fp in b.GetFootprints():
        for p in fp.Pads():
            if p.GetDrillSizeX() > 0:
                c = p.GetPosition()
                kind = "mounting hole" if p.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH else "component"
                holes.append((mm(c.x) - ox, mm(c.y) - oy, mm(p.GetDrillSizeX()), mm(p.GetDrillSizeY()), kind, f"{fp.GetReference()}.{p.GetNumber()}", p.GetNetname(),
                              p.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH))
    for t in b.GetTracks():
        if t.GetClass() == "PCB_VIA":
            c = t.GetPosition()
            holes.append((mm(c.x) - ox, mm(c.y) - oy, mm(t.GetDrillValue()), mm(t.GetDrillValue()), "via", "via", t.GetNetname(), False))
    for name, layer, mirror in (("top", pcbnew.F_Cu, False), ("bottom", pcbnew.B_Cu, True)):
        img, d = layer_image(b, layer, ox, oy, w, h)
        inv = Image.eval(img, lambda v: 255 - v)                              # 255 = remove (to be burnt)
        dd = ImageDraw.Draw(inv)
        for (x, y, dx, dy, kind, ref, net, npth) in holes:
            r = 0.125
            dd.ellipse([(x - r) * PX, (y - r) * PX, (x + r) * PX, (y + r) * PX], fill=255)          # pilot pit at every hole centre
        # board outline as a thin cutting guide
        dd.rectangle([0, 0, w * PX - 1, h * PX - 1], outline=255, width=max(1, int(round(0.1 * PX))))
        out = inv.point(lambda v: 0 if v else 255)                            # BMP convention: black = burn
        if mirror:
            out = out.transpose(Image.FLIP_LEFT_RIGHT)
        out = out.convert("1")
        out.save(os.path.join(OUT, f"{name}_remove.bmp"), dpi=(DPI, DPI))
        burn = sum(1 for v in out.getdata() if v == 0) / (out.width * out.height)
        print(f"{name}: {out.width} x {out.height} px, {burn * 100:.0f} % of the board is burnt away")
    with open(os.path.join(OUT, "drill_list.csv"), "w", newline="", encoding="utf-8-sig") as f:
        wr = csv.writer(f)
        wr.writerow(["x_mm (from the top-left corner, board seen from the top)", "y_mm", "drill_x_mm", "drill_y_mm", "type", "ref", "net"])
        for x, y, dx, dy, kind, ref, net, npth in sorted(holes, key=lambda r: (round(r[2], 2), r[1], r[0])):
            wr.writerow([f"{x:.3f}", f"{y:.3f}", f"{dx:.2f}", f"{dy:.2f}", kind, ref, net])
    from collections import Counter
    cnt = Counter((round(r[2], 2), round(r[3], 2), r[4]) for r in holes)
    with open(os.path.join(OUT, "drill_summary.txt"), "w", encoding="utf-8") as f:
        f.write(f"board {w:.1f} x {h:.1f} mm, {len(holes)} holes\n")
        for (dx, dy, kind), n in sorted(cnt.items()):
            f.write(f"{n:4d} x {dx:.2f}" + (f" x {dy:.2f} (slot)" if abs(dx - dy) > 0.01 else "") + f" mm  {kind}\n")
    print(open(os.path.join(OUT, "drill_summary.txt"), encoding="utf-8").read())


if __name__ == "__main__":
    main()
