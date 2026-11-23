"""Project-local KiCad footprint library: hardware/lib/press_counter.pretty/

 * SOP4_Wide_Universal - PC817-type 4-pin SMD optocoupler.  Sold in Iran as "PC817 SMD": either the 4.4 mm wide mini-flat SOP-4
   (lead span ~7 mm) or the gull-wing DIP-4 SMD (lead span ~10 mm).  The elongated pads accept both; the inner pad edges are
   5.2 mm apart, i.e. the 5 mm mains creepage of the input side (pins 1-2) to the output side (pins 3-4) is kept.
 * FuseHolder_5x20_PCB_P22.5 - PCB-mount holder for a 5x20 mm glass fuse (BLX-A type with cover, 26.4 x 9.4 mm body, tails 22.5 mm apart).
   The holes are slots (2.4 x 1.3 mm along the pitch axis) so holders with 22.0 ... 23.0 mm pin pitch fit.  CHECK against the holder you buy.
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
LIB = os.path.normpath(os.path.join(HERE, "..", "lib"))
PRETTY = os.path.join(LIB, "press_counter.pretty")
os.makedirs(PRETTY, exist_ok=True)


def header(name, descr, tags, ref_xy, val_xy):
    return [f'(footprint "{name}"\n\t(version 20260206)\n\t(generator "press_counter_gen")\n\t(layer "F.Cu")',
            f'\t(descr "{descr}")', f'\t(tags "{tags}")',
            f'\t(property "Reference" "REF**" (at {ref_xy[0]:g} {ref_xy[1]:g} 0) (layer "F.SilkS") (effects (font (size 1 1) (thickness 0.15))))',
            f'\t(property "Value" "{name}" (at {val_xy[0]:g} {val_xy[1]:g} 0) (layer "F.Fab") (effects (font (size 1 1) (thickness 0.15))))',
            '\t(attr smd)']


def rect(x1, y1, x2, y2, layer, w):
    return f'\t(fp_rect (start {x1:g} {y1:g}) (end {x2:g} {y2:g}) (stroke (width {w:g}) (type solid)) (fill no) (layer "{layer}"))'


def pad(num, x, y, w, h, shape="roundrect"):
    extra = " (roundrect_rratio 0.2)" if shape == "roundrect" else ""
    return (f'\t(pad "{num}" smd {shape} (at {x:g} {y:g}) (size {w:g} {h:g}) (layers "F.Cu" "F.Mask" "F.Paste"){extra})')


def opto():
    name = "SOP4_Wide_Universal"
    t = header(name, "PC817 SMD optocoupler, 4 pin, universal pads for SOP-4 (7 mm span) and gull-wing DIP-4 SMD (10 mm span). "
               "Pins 1-2 = LED (left), 3-4 = transistor (right).", "optocoupler pc817 sop4", (0, -4), (0, 4))
    t.append(rect(-3.2, -2.3, 3.2, 2.3, "F.Fab", 0.1))
    t.append(rect(-2.4, -2.4, 2.4, 2.4, "F.SilkS", 0.12))
    t.append('\t(fp_circle (center -1.7 -1.7) (end -1.45 -1.7) (stroke (width 0.12) (type solid)) (fill yes) (layer "F.SilkS"))')
    t.append(rect(-6.3, -2.6, 6.3, 2.6, "F.CrtYd", 0.05))
    x, ln = 4.3, 3.4
    t.append(pad(1, -x, -1.27, ln, 1.3))
    t.append(pad(2, -x, 1.27, ln, 1.3))
    t.append(pad(3, x, 1.27, ln, 1.3))
    t.append(pad(4, x, -1.27, ln, 1.3))
    t.append(")")
    return name, "\n".join(t) + "\n"


def fuse_holder():
    name = "FuseHolder_5x20_PCB_P22.5"
    pitch = 22.5
    t = header(name, "PCB fuse holder for 5x20 mm glass fuse (BLX-A type), tails 22.5 mm apart, slotted holes for +-0.5 mm tolerance",
               "fuse holder 5x20 pcb", (0, -6.6), (0, 6.6))
    t[-1] = "\t(attr through_hole)"
    t.append(rect(-14.0, -4.7, 14.0, 4.7, "F.SilkS", 0.15))
    t.append(rect(-13.2, -4.7, 13.2, 4.7, "F.Fab", 0.1))
    t.append(rect(-15.0, -5.7, 15.0, 5.7, "F.CrtYd", 0.05))
    for num, x in ((1, -pitch / 2), (2, pitch / 2)):
        t.append(f'\t(pad "{num}" thru_hole oval (at {x:g} 0) (size 3.8 2.6) (drill oval 2.4 1.3) (layers "*.Cu" "*.Mask"))')
    t.append(")")
    return name, "\n".join(t) + "\n"


if __name__ == "__main__":
    for f in os.listdir(PRETTY):
        if f.endswith(".kicad_mod"):
            os.remove(os.path.join(PRETTY, f))
    for fn in (opto, fuse_holder):
        name, txt = fn()
        with open(os.path.join(PRETTY, name + ".kicad_mod"), "w", encoding="utf-8") as f:
            f.write(txt)
    sym = os.path.join(LIB, "press_counter.kicad_sym")
    if os.path.exists(sym):
        os.remove(sym)            # no custom symbols any more (every part uses a stock KiCad symbol)
    print("custom footprints written to", PRETTY)
