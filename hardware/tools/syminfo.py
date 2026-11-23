"""Print pins of KiCad library symbols: python syminfo.py Lib:Name ..."""
import sys
from sexpr import parse, find, find_all

SYM_ROOT = r"D:\KiCad\10.0\share\kicad\symbols"
_cache = {}


def load_lib(lib):
    if lib not in _cache:
        with open(f"{SYM_ROOT}\\{lib}.kicad_sym", encoding="utf-8") as f:
            _cache[lib] = parse(f.read())
    return _cache[lib]


def get_symbol(lib, name):
    root = load_lib(lib)
    for s in find_all(root, "symbol"):
        if s[1] == name:
            return s
    return None


def pins(sym):
    out = []
    for sub in find_all(sym, "symbol"):
        for p in find_all(sub, "pin"):
            at = find(p, "at")
            nm = find(p, "name")
            num = find(p, "number")
            out.append((num[1], nm[1], p[1], p[2], float(at[1]), float(at[2]), float(at[3])))
    return out


if __name__ == "__main__":
    for arg in sys.argv[1:]:
        lib, name = arg.split(":", 1)
        s = get_symbol(lib, name)
        if s is None:
            print(f"!! {arg} not found")
            continue
        ext = find(s, "extends")
        if ext:
            print(f"== {arg} (extends {ext[1]})")
            s = get_symbol(lib, ext[1])
        else:
            print(f"== {arg}")
        for num, nm, typ, shape, x, y, a in sorted(pins(s), key=lambda t: (len(t[0]), t[0])):
            print(f"   {num:>3} {nm:<12} {typ:<14} at({x:g},{y:g}) a={a:g}")
