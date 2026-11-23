"""Run a SPICE netlist through KiCad's shared ngspice library (no extra install) and return the result vectors.

    D:/KiCad/10.0/bin/python.exe ngspice_run.py          (used by sense_input.py)
"""
import ctypes
import os

KICAD_BIN = r"D:\KiCad\10.0\bin"


class _VecInfo(ctypes.Structure):
    _fields_ = [("v_name", ctypes.c_char_p), ("v_type", ctypes.c_int), ("v_flags", ctypes.c_short),
                ("v_realdata", ctypes.POINTER(ctypes.c_double)), ("v_compdata", ctypes.c_void_p), ("v_length", ctypes.c_int)]


_lib = None
_out = []
_CB_CHAR = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p)
_CB_STAT = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p)
_CB_EXIT = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_int, ctypes.c_bool, ctypes.c_bool, ctypes.c_int, ctypes.c_void_p)
_cbs = (_CB_CHAR(lambda s, i, p: _out.append(s.decode(errors="replace")) or 0),
        _CB_STAT(lambda s, i, p: 0),
        _CB_EXIT(lambda st, unload, quit_, i, p: 0))


def _load():
    global _lib
    if _lib is None:
        os.add_dll_directory(KICAD_BIN)
        _lib = ctypes.CDLL(os.path.join(KICAD_BIN, "ngspice.dll"))
        _lib.ngGet_Vec_Info.restype = ctypes.POINTER(_VecInfo)
        _lib.ngSpice_Init(_cbs[0], _cbs[1], _cbs[2], None, None, None, None)
    return _lib


def run(netlist, vectors):
    """Simulate `netlist` (text, with an analysis line) and return {name: [values]} for `vectors` (e.g. 'time', 'v(press)')."""
    lib = _load()
    _out.clear()
    lib.ngSpice_Command(b"destroy all")
    lines = [ln.encode() for ln in netlist.strip().splitlines()]
    arr = (ctypes.c_char_p * (len(lines) + 1))(*lines, None)
    lib.ngSpice_Circ(arr)
    lib.ngSpice_Command(b"run")
    res = {}
    for v in vectors:
        info = lib.ngGet_Vec_Info(v.encode())
        if not info:
            raise RuntimeError(f"no vector {v}:\n" + "\n".join(_out[-20:]))
        n = info.contents.v_length
        res[v] = [info.contents.v_realdata[i] for i in range(n)]
    return res
