"""Shared helpers: board registry (design + layout module + output paths).

    import common;  bi = common.info("main")   # or "panel"
"""
import importlib
import os
import types

HERE = os.path.dirname(os.path.abspath(__file__))
HW = os.path.normpath(os.path.join(HERE, ".."))
KICAD = r"D:\KiCad\10.0"
CLI = os.path.join(KICAD, "bin", "kicad-cli.exe")
PY = os.path.join(KICAD, "bin", "python.exe")
KEYS = ("main", "panel")


def info(key):
    import design
    b = design.BOARDS[key]()
    d = os.path.join(HW, key)
    os.makedirs(d, exist_ok=True)
    ns = types.SimpleNamespace()
    ns.key = key
    ns.board = b
    ns.name = b.name
    ns.dir = d
    ns.sch = os.path.join(d, b.name + ".kicad_sch")
    ns.pcb = os.path.join(d, b.name + ".kicad_pcb")
    ns.fab = os.path.join(HW, "fabrication", key)
    try:
        ns.layout = importlib.import_module("layout_" + key)
    except ModuleNotFoundError:
        ns.layout = None
    return ns
