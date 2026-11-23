"""Enclosure interface drawing: board outline, mounting holes, and every place the enclosure needs an opening or a light pipe.

    D:/KiCad/10.0/bin/python.exe -P make_enclosure_drawing.py

Writes ../fabrication/enclosure/enclosure_interface.svg (top view, board-local mm, origin top-left, 1 mm = 5 px) and
enclosure_interface.csv (the same coordinates for the enclosure maker).  Everything is taken from layout_main.py, so it follows the board.
The OLED window is an estimate from a typical 0.96in module (the module position over the socket varies by maker): leave +-2 mm and
check it with a printed mock-up on the first article.
"""
import csv
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import layout_main as L  # noqa: E402

P = L.PLACE
S = 5.0                                        # px per mm
OUT = os.path.normpath(os.path.join(HERE, "..", "fabrication", "enclosure"))

items = []                                     # (kind, name, x, y, w, h, note)


def add(kind, name, x, y, w, h, note=""):
    items.append((kind, name, round(x, 2), round(y, 2), w, h, note))


for ref in ("MH1", "MH2", "MH3", "MH4", "MH5", "MH6"):
    add("hole", ref, P[ref][0], P[ref][1], 3.2, 3.2, "M3 mounting hole / standoff")
add("button", "SW1 CALL SUPERVISOR", P["SW1"][0], P["SW1"][1], 5.0, 5.0, "plunger hole 5 mm, stem 3.5 mm; use a plastic cap, switch top is 5 mm above the PCB")
add("button", "SW2 CALL NET", P["SW2"][0], P["SW2"][1], 5.0, 5.0, "same")
for ref, label in (("D2", "red ALARM LED"), ("D13", "green ON LED"), ("D6", "green PWR LED")):
    add("light", f"{ref} {label}", P[ref][0], P[ref][1], 2.0, 2.0, "0805 LED, light pipe 2 mm, 1-2 mm above the PCB")
add("light", "D5 red mains-present LED", P["D5"][0], P["D5"][1], 2.0, 2.0, "OPTIONAL window: it is a 230 V area, keep the cover closed, a light pipe must be insulating")
add("opening", "J3 USB micro-B", P["J3"][0], L.H, 12.0, 5.0, "slot at the bottom edge for the plug (service port); a plug cap or screw cover in operation")
# RC522: the module lies over the board, antenna coil above the copper-free area
add("area", "RC522 tag read area", L.RC_X0, L.RC_Y0 + 23.0, 40.0, 38.0, "no cutout needed: front plastic <= 3 mm, no metal / foil / magnet within 15 mm in front of it")
add("area", "RC522 module outline", L.RC_X0, L.RC_Y0, 40.0, 60.0, "module 40 x 60 mm, 8-pin header at its top edge (P1)")
# OLED: header P2 on the top edge, module body below it
ox, oy = P["P2"][0] + 3.8, P["P2"][1]
add("window", "OLED display window (estimate)", ox - 12.0, oy + 9.0, 24.0, 13.0, "active area 21.7 x 10.9 mm + margin; position estimated, verify with the real module")
add("area", "OLED module outline (estimate)", ox - 13.7, oy - 1.3, 27.4, 27.8, "typical 0.96in module 27.3 x 27.8 mm")
# terminals: wire entries
entries = [("J1 mains in (L, N)", "left", 'J1'), ("J2 press sense 230 V", "left", 'J2'), ("J5 relay K1 contact (NO, COM, NC)", "left", 'J5'),
           ("J6 5 V DC in", "top", 'J6'), ("J4 DS18B20 probe", "bottom", 'J4'), ("J7 12/24 V sense", "bottom", 'J7'), ("J9 AUX", "bottom", 'J9'),
           ("J10 OUT1 (NO, COM, NC)", "right", 'J10'), ("J11 OUT2", "right", 'J11'), ("J12 OUT3", "right", 'J12'), ("J13 OUT4", "right", 'J13')]
for name, side, ref in entries:
    x, y, rot = P[ref]
    add("cable", name, x, y, 0, 0, f"wire entry from the {side} edge; screw terminal, 5.08 mm pitch (the board edge is the enclosure wall: use a gland / slot beside it)")

os.makedirs(OUT, exist_ok=True)
with open(os.path.join(OUT, "enclosure_interface.csv"), "w", newline="", encoding="utf-8-sig") as f:
    w = csv.writer(f)
    w.writerow(["kind", "name", "x_mm", "y_mm", "w_mm", "h_mm", "note"])
    w.writerows(items)

colors = {"hole": "#888", "button": "#d33", "light": "#e90", "opening": "#06c", "area": "#0a6", "window": "#a0a", "cable": "#555"}
svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{(L.W + 10) * S:.0f}" height="{(L.H + 14) * S:.0f}" viewBox="-5 -5 {L.W + 10} {L.H + 14}" font-family="sans-serif" font-size="2.2">',
       '<rect x="-5" y="-5" width="100%" height="100%" fill="#fff"/>',
       f'<rect x="0" y="0" width="{L.W}" height="{L.H}" rx="3" fill="#f3f3ee" stroke="#000" stroke-width="0.3"/>',
       f'<text x="0" y="{L.H + 5}" font-size="2.6" font-weight="bold">PRESS COUNTER v6.2 - enclosure interface, top view, mm, origin = top-left corner of the board (board {L.W} x {L.H})</text>']
for kind, name, x, y, w, h, note in items:
    c = colors[kind]
    if kind == "hole":
        svg.append(f'<circle cx="{x}" cy="{y}" r="{w / 2}" fill="none" stroke="{c}" stroke-width="0.3"/>')
    elif kind in ("button", "light"):
        svg.append(f'<circle cx="{x}" cy="{y}" r="{w / 2}" fill="{c}" fill-opacity="0.35" stroke="{c}" stroke-width="0.3"/>')
        dx, dy, anchor = {"D2": (-1.5, -2.5, "end"), "D13": (1.5, -2.5, "start"), "SW1": (0, 5.5, "middle"), "SW2": (0, 5.5, "middle")}.get(name.split()[0], (w / 2 + 0.8, 0.8, "start"))
        svg.append(f'<text x="{x + dx}" y="{y + dy}" fill="{c}" text-anchor="{anchor}">{name}</text>')
    elif kind == "opening":
        svg.append(f'<rect x="{x - w / 2}" y="{y - h}" width="{w}" height="{h}" fill="{c}" fill-opacity="0.35" stroke="{c}" stroke-width="0.3"/>')
        svg.append(f'<text x="{x + w / 2 + 0.8}" y="{y - 1}" fill="{c}">{name}</text>')
    elif kind in ("area", "window"):
        dash = ' stroke-dasharray="1.5 1"' if kind == "area" else ""
        svg.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{c}" fill-opacity="{0.12 if kind == "area" else 0.3}" stroke="{c}" stroke-width="0.3"{dash}/>')
        svg.append(f'<text x="{x + 1}" y="{y + 3}" fill="{c}">{name}</text>')
    else:
        svg.append(f'<circle cx="{x}" cy="{y}" r="0.8" fill="{c}"/><text x="{x + 1.5}" y="{y - 1.2}" fill="{c}" font-size="1.8">{name}</text>')
svg.append("</svg>")
with open(os.path.join(OUT, "enclosure_interface.svg"), "w", encoding="utf-8") as f:
    f.write("\n".join(svg))
print("wrote", OUT, len(items), "items")
