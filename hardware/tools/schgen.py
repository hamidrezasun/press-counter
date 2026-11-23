"""Generate a KiCad 10 schematic (.kicad_sch) from a design.Board.

Every connected pin gets a short wire stub ending in a global label / power
symbol, so the schematic is connectivity-correct by construction and readable.
"""
import math
import uuid
from collections import OrderedDict

from sexpr import Sym, dump, find, find_all
import kilib

POWER_NETS = {"GND": "power:GND", "+5V": "power:+5V", "+3V3": "power:+3V3", "+12V": "power:+12V"}
SHEET_W, SHEET_H = 420.0, 297.0     # A3


def _u():
    return str(uuid.uuid4())


def r4(v):
    return round(v + 0.0, 4)


def rot(x, y, deg):
    a = math.radians(deg)
    c, s = round(math.cos(a), 10), round(math.sin(a), 10)
    return x * c - y * s, x * s + y * c


def fx(size=1.27, justify=None, hide=False, bold=False):
    f = [Sym("font"), [Sym("size"), size, size]]
    if bold:
        f.append([Sym("bold"), Sym("yes")])
    e = [Sym("effects"), f]
    if justify:
        e.append([Sym("justify")] + [Sym(j) for j in justify.split()])
    if hide:
        e.append([Sym("hide"), Sym("yes")])
    return e


def prop_node(key, val, x, y, hide=False, justify=None, size=1.27):
    return [Sym("property"), key, val, [Sym("at"), r4(x), r4(y), 0], fx(size, justify, hide)]


# ERC: a few stock symbols declare pins as power outputs / open emitters although in this design they are simply tied to
# the same ground as the AC-DC module.  Make those pins passive in the (embedded) schematic copy of the symbol.
PIN_TYPE_OVERRIDES = {
    "Connector:USB_B_Micro": {"5": "passive"},
}


def lib_entry(libid):
    lib, name = libid.split(":")
    s = kilib.flat_symbol(lib, name)
    s[1] = libid
    ov = PIN_TYPE_OVERRIDES.get(libid)
    if ov:
        for sub in find_all(s, "symbol"):
            for p in find_all(sub, "pin"):
                num = find(p, "number")[1]
                if num in ov:
                    p[1] = Sym(ov[num])
    return s


class Sheet:
    def __init__(self, board, project):
        self.board = board
        self.project = project
        self.root_uuid = _u()
        self.items = []
        self.libsyms = OrderedDict()
        self.pwr_count = 0
        self.flag_count = 0
        self.placed = {}          # ref -> (x, y)

    # ── library handling ──
    def _sym(self, libid):
        if libid not in self.libsyms:
            self.libsyms[libid] = lib_entry(libid)
        return self.libsyms[libid]

    # ── primitives ──
    def wire(self, x1, y1, x2, y2):
        self.items.append([Sym("wire"), [Sym("pts"), [Sym("xy"), r4(x1), r4(y1)], [Sym("xy"), r4(x2), r4(y2)]],
                           [Sym("stroke"), [Sym("width"), 0], [Sym("type"), Sym("default")]], [Sym("uuid"), _u()]])

    def glabel(self, net, x, y, direction):
        # direction: outward direction of the stub in sheet coords (dx, dy)
        dx, dy = direction
        if dx > 0:
            ang, just = 0, "left"
        elif dx < 0:
            ang, just = 180, "right"
        elif dy < 0:
            ang, just = 90, "left"
        else:
            ang, just = 270, "right"
        self.items.append([Sym("global_label"), net, [Sym("shape"), Sym("bidirectional")],
                           [Sym("at"), r4(x), r4(y), ang], [Sym("fields_autoplaced"), Sym("yes")], fx(1.27, just),
                           [Sym("uuid"), _u()],
                           [Sym("property"), "Intersheetrefs", "${INTERSHEET_REFS}", [Sym("at"), r4(x), r4(y), 0],
                            fx(1.27, hide=True)]])

    def noconnect(self, x, y):
        self.items.append([Sym("no_connect"), [Sym("at"), r4(x), r4(y)], [Sym("uuid"), _u()]])

    def text(self, s, x, y, size=2.54, bold=True):
        self.items.append([Sym("text"), s, [Sym("exclude_from_sim"), Sym("no")], [Sym("at"), r4(x), r4(y), 0],
                           fx(size, "left bottom", bold=bold), [Sym("uuid"), _u()]])

    def power_symbol(self, net, x, y):
        libid = POWER_NETS[net]
        self._sym(libid)
        self.pwr_count += 1
        ref = f"#PWR{self.pwr_count:03d}"
        self._instance(libid, ref, net, x, y, 0, 1, pins=["1"], power=True)

    def pwr_flag(self, x, y):
        libid = "power:PWR_FLAG"
        self._sym(libid)
        self.flag_count += 1
        self._instance(libid, f"#FLG{self.flag_count:03d}", "PWR_FLAG", x, y, 0, 1, pins=["1"], power=True)

    def _instance(self, libid, ref, value, x, y, angle, unit, pins, fp="", mpn="", power=False, note="", bbox=None):
        s = self._sym(libid)
        n = [Sym("symbol"), [Sym("lib_id"), libid], [Sym("at"), r4(x), r4(y), angle], [Sym("unit"), unit],
             [Sym("exclude_from_sim"), Sym("no")], [Sym("in_bom"), Sym("no" if (power or ref.startswith("TP")) else (find(s, "in_bom") or [0, Sym("yes")])[1])],
             [Sym("on_board"), Sym("no" if power else "yes")], [Sym("dnp"), Sym("no")], [Sym("uuid"), _u()]]
        if power:
            n.append(prop_node("Reference", ref, x, y - 3, hide=True))
            n.append(prop_node("Value", value, x, y + (3.2 if value != "GND" else 3.2), hide=False))
        else:
            if bbox:
                bx0, by0, bx1, by1 = bbox
                n.append(prop_node("Reference", ref, x + (bx0 + bx1) / 2, y + by0 - 2.0))
                n.append(prop_node("Value", value, x + (bx0 + bx1) / 2, y + by1 + 2.0))
            else:
                n.append(prop_node("Reference", ref, x, y - 5))
                n.append(prop_node("Value", value, x, y + 5))
        n.append(prop_node("Footprint", fp, x, y, hide=True))
        n.append(prop_node("Datasheet", "", x, y, hide=True))
        for pnum in pins:
            n.append([Sym("pin"), pnum, [Sym("uuid"), _u()]])
        n.append([Sym("instances"), [Sym("project"), self.project,
                                     [Sym("path"), "/" + self.root_uuid, [Sym("reference"), ref], [Sym("unit"), unit]]]])
        self.items.append(n)

    # ── placing a part (one unit) ──
    def place(self, part, libid, unit, x, y, nets_by_pin):
        s = self._sym(libid)
        pins = kilib.unit_pins(s, unit)
        xs, ys = [0.0], [0.0]
        for p in pins:
            xs.append(p["x"]); ys.append(-p["y"])
        bbox = (min(xs), min(ys), max(xs), max(ys))
        self._instance(libid, part.ref, part.value, x, y, 0, unit, [p["num"] for p in pins], part.fp, part.mpn, bbox=bbox)
        self.placed[(part.ref, unit)] = (x, y)
        for p in pins:
            px, py = x + p["x"], y - p["y"]
            net = nets_by_pin.get(p["num"], "__missing__")
            if net == "__missing__":
                continue
            ang = (p["angle"] + 180) % 360
            ox, oy = math.cos(math.radians(ang)), -math.sin(math.radians(ang))
            ox, oy = round(ox), round(oy)
            if net is None:
                self.noconnect(px, py)
                continue
            ln = 2.54 if net in POWER_NETS else 5.08
            ex, ey = px + ox * ln, py + oy * ln
            self.wire(px, py, ex, ey)
            if net in POWER_NETS:
                self.power_symbol(net, ex, ey)
            else:
                self.glabel(net, ex, ey, (ox, oy))
        return bbox


def estimate_width(part, libid, unit, sym):
    pins = kilib.unit_pins(sym, unit)
    nets = [str(part.pins.get(p["num"], "")) for p in pins]
    mx = max([len(n) for n in nets] + [3])
    pxs = [p["x"] for p in pins] or [0]
    body = (max(pxs) - min(pxs)) if pxs else 0
    return body + 2 * (5.08 + 1.5 * mx) + 10


PAPERS = [("A3", 420.0, 297.0), ("A2", 594.0, 420.0), ("A1", 841.0, 594.0)]


def _layout(board, project, sheet_w, sheet_h, flags):
    sh = Sheet(board, project)
    margin = 15.0
    x, y = margin, margin + 10
    row_h = 0.0
    cur_group = None
    for part in board.parts:
        libid = part.sym
        sym = sh._sym(libid)
        units = kilib.n_units(sym)
        for unit in range(1, units + 1):
            upins = kilib.unit_pins(sym, unit)
            if units > 1 and not [p for p in upins if p["num"] in part.pins]:
                continue
            w = estimate_width(part, libid, unit, sym) if part.pins else 25.0
            ys = [-p["y"] for p in upins] or [0.0]
            h = (max(ys) - min(ys)) + 22.0
            new_group = part.group != cur_group
            if new_group or x + w > sheet_w - margin:
                if new_group and cur_group is not None:
                    x = margin
                    y += row_h + 12.0
                    row_h = 0.0
                elif not new_group:
                    x = margin
                    y += row_h + 4.0
                    row_h = 0.0
                if new_group:
                    sh.text(part.group, margin, y + 3.0, 3.0)
                    y += 14.0
                cur_group = part.group
            cx = round((x + w / 2.0) / 2.54) * 2.54
            cy = round((y + max(ys) + 10.0) / 2.54) * 2.54
            sh.place(part, libid, unit, cx, cy, dict(part.pins))
            x += w
            row_h = max(row_h, h)
    y_end = y + row_h
    fy = round((y_end + 20.0) / 2.54) * 2.54
    fxp = round((margin + 10) / 2.54) * 2.54
    for net in flags:
        fxp += 25.4
        if net in POWER_NETS:
            sh.power_symbol(net, fxp, fy)
            sh.pwr_flag(fxp, fy)
        else:
            sh.glabel(net, fxp, fy, (0, 1))
            sh.pwr_flag(fxp, fy)
    sh.text("PWR_FLAG markers (ERC): nets fed only through passive connectors", margin + 10, fy + 12, 1.8, False)
    return sh, fy + 20.0


def generate(board, outpath, project, flags=()):
    for paper, w, h in PAPERS:
        sh, need = _layout(board, project, w, h, flags)
        if need <= h - 10:
            break
    root = [Sym("kicad_sch"), [Sym("version"), 20250610], [Sym("generator"), "eeschema"], [Sym("generator_version"), "10.0"],
            [Sym("uuid"), sh.root_uuid], [Sym("paper"), paper],
            [Sym("title_block"), [Sym("title"), board.title], [Sym("date"), "2026-10-06"], [Sym("rev"), "4.0"],
             [Sym("company"), "Press Counter"], [Sym("comment"), 1, "Generated by hardware/tools/gen_all.py from design.py"]],
            [Sym("lib_symbols")] + list(sh.libsyms.values())]
    root += sh.items
    root.append([Sym("sheet_instances"), [Sym("path"), "/", [Sym("page"), "1"]]])
    root.append([Sym("embedded_fonts"), Sym("no")])
    with open(outpath, "w", encoding="utf-8") as f:
        f.write(dump(root, 0, 2))
        f.write("\n")
    return sh

if __name__ == "__main__":
    import design
    import sys
    b = design.BOARDS[sys.argv[1]]()
    generate(b, sys.argv[2], b.name)
