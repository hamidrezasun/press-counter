"""Calibration piece for laser PCB etching: 1-bit BMP, 1016 dpi, 50 x 36 mm, black = burnt away (same convention as make_laser_files.py).

    D:/KiCad/10.0/bin/python.exe -P make_laser_test.py

Copper lines 0.15 ... 0.5 mm with equal gaps (the board uses 0.25 mm tracks / 0.2 mm gaps, mains tracks 1.0-1.2 mm), a pad grid with pilot pits,
a 5 mm wide isolation channel (the mains creepage) and a solid copper square (a pour).  Etch this on a scrap piece first and look at it with a loupe.
"""
import os
from PIL import Image, ImageDraw

DPI = 1016.0
PX = DPI / 25.4
W, H = 50.0, 36.0
img = Image.new("L", (int(W * PX), int(H * PX)), 0)            # 255 = copper (kept)
d = ImageDraw.Draw(img)
d.rectangle([0, 0, W * PX, H * PX], fill=0)
# everything inside a 48 x 34 mm panel starts as copper
d.rectangle([1 * PX, 1 * PX, 49 * PX, 35 * PX], fill=255)
# 1) line / gap series: carve gaps of width g between copper lines of width g
x = 3.0
for g in (0.15, 0.2, 0.25, 0.3, 0.4, 0.5):
    for k in range(5):
        d.rectangle([x * PX, 3 * PX, (x + g) * PX, 15 * PX], fill=0)      # gap
        x += 2 * g
    x += 1.5
# 2) pad grid 2.54 mm pitch: copper ring 2.0 mm with a 0.25 mm pilot pit, inside a burnt frame
for i in range(6):
    for j in range(3):
        cx, cy = (6 + 2.54 * i), (21 + 2.54 * j)
        d.rectangle([(cx - 1.65) * PX, (cy - 1.65) * PX, (cx + 1.65) * PX, (cy + 1.65) * PX], fill=0)
        d.ellipse([(cx - 1.0) * PX, (cy - 1.0) * PX, (cx + 1.0) * PX, (cy + 1.0) * PX], fill=255)
        d.ellipse([(cx - 0.125) * PX, (cy - 0.125) * PX, (cx + 0.125) * PX, (cy + 0.125) * PX], fill=0)
# 3) 5 mm isolation channel between two copper areas
d.rectangle([26 * PX, 18 * PX, 31 * PX, 34 * PX], fill=0)
# 4) text-free marker: 1 mm ruler ticks along the bottom
for k in range(0, 21):
    d.rectangle([(33 + k * 0.75) * PX, 31 * PX, (33 + k * 0.75 + 0.15) * PX, (33 + (k % 5 == 0) * 1.5) * PX], fill=0)
out = img.point(lambda v: 0 if v == 0 else 255)                # black = burn ... the frame outside the panel (0) is burnt too: keep it unburnt
mask = Image.new("L", img.size, 0)
ImageDraw.Draw(mask).rectangle([1 * PX, 1 * PX, 49 * PX, 35 * PX], fill=255)
res = Image.new("L", img.size, 255)                            # outside the panel: white = not burnt
res.paste(out, mask=mask)
res = res.convert("1")
dst = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fabrication", "laser", "test_pattern.bmp"))
res.save(dst, dpi=(DPI, DPI))
print("wrote", dst, res.size)
