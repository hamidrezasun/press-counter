"""KiCad symbol library helpers: load, flatten `extends`, per-unit pin tables."""
import copy
import os
from sexpr import parse, find, find_all, Sym

STD_SYM = r"D:\KiCad\10.0\share\kicad\symbols"
CUSTOM_SYM = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "lib", "press_counter.kicad_sym"))

_libs = {}


def _lib_path(lib):
    if lib == "press_counter":
        return CUSTOM_SYM
    return os.path.join(STD_SYM, lib + ".kicad_sym")


def load_lib(lib):
    if lib not in _libs:
        with open(_lib_path(lib), encoding="utf-8") as f:
            _libs[lib] = parse(f.read())
    return _libs[lib]


def raw_symbol(lib, name):
    for s in find_all(load_lib(lib), "symbol"):
        if s[1] == name:
            return s
    raise KeyError(f"{lib}:{name}")


def flat_symbol(lib, name):
    """Return a deep-copied symbol with `extends` resolved and renamed to `name`."""
    s = copy.deepcopy(raw_symbol(lib, name))
    ext = find(s, "extends")
    if not ext:
        return s
    parent = flat_symbol(lib, ext[1])
    # child properties override parent properties
    child_props = {p[1]: p for p in find_all(s, "property")}
    new = [Sym("symbol"), name]
    for item in parent[2:]:
        if isinstance(item, list) and item and item[0] == "property" and item[1] in child_props:
            new.append(child_props.pop(item[1]))
        elif isinstance(item, list) and item and item[0] == "symbol":
            sub = copy.deepcopy(item)
            sub[1] = name + sub[1][len(parent[1]):]
            new.append(sub)
        else:
            new.append(item)
    for p in child_props.values():
        new.append(p)
    return new


def unit_pins(sym, unit):
    """Pins of a (flattened) symbol for the given unit (unit 0 pins are common)."""
    out = []
    base = sym[1].split(":")[-1]
    for sub in find_all(sym, "symbol"):
        suffix = sub[1][len(base) + 1:]        # "<unit>_<style>"
        u, st = suffix.split("_")
        u, st = int(u), int(st)
        if u not in (0, unit) or st > 1:
            continue
        for p in find_all(sub, "pin"):
            at = find(p, "at")
            out.append({
                "num": find(p, "number")[1],
                "name": find(p, "name")[1],
                "type": p[1],
                "x": float(at[1]), "y": float(at[2]), "angle": float(at[3]),
                "hide": any(c == Sym("hide") or (isinstance(c, list) and c and c[0] == "hide" and c[1] == Sym("yes"))
                            for c in p),
            })
    return out


def n_units(sym):
    base = sym[1].split(":")[-1]
    mx = 1
    for sub in find_all(sym, "symbol"):
        u = int(sub[1][len(base) + 1:].split("_")[0])
        mx = max(mx, u)
    return mx


def prop(sym, key, default=""):
    for p in find_all(sym, "property"):
        if p[1] == key:
            return p[2]
    return default
