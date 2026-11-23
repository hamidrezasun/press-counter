"""Regenerate the schematic (+ERC) of a board.   Run with KiCad's python:

    D:\KiCad\10.0\bin\python.exe gen_all.py main|panel|all
"""
import os
import subprocess
import sys
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common, schgen, project  # noqa: E402

FLAGS = {"main": ("LF", "N")}


def build_sch(bi):
    project.write_project(bi.dir, bi.name, bi.board)
    schgen.generate(bi.board, bi.sch, bi.name, flags=FLAGS.get(bi.key, ()))
    print(bi.key, ": schematic written")


def erc(bi):
    rpt = os.path.join(bi.dir, "erc.rpt")
    subprocess.run([common.CLI, "sch", "erc", "--format", "report", "--severity-all", "--output", rpt, bi.sch],
                   capture_output=True, text=True, timeout=300)
    txt = open(rpt, encoding="utf-8").read()
    print(bi.key, ": ERC:", [l.strip() for l in txt.splitlines() if "ERC messages" in l][0])
    return txt


def verify_netlist(bi):
    """Export the schematic netlist and compare it pin-by-pin with design.py."""
    xml = os.path.join(bi.dir, "netlist.xml")
    subprocess.run([common.CLI, "sch", "export", "netlist", "--format", "kicadxml", "--output", xml, bi.sch],
                   check=True, capture_output=True, text=True, timeout=300)
    got = {}
    for net in ET.parse(xml).getroot().find("nets"):
        nm = net.get("name")
        nm = nm[1:] if nm.startswith("/") else nm
        for node in net.findall("node"):
            got[(node.get("ref"), node.get("pin"))] = nm
    bad = 0
    for p in bi.board.parts:
        for pin, net in p.pins.items():
            if net and got.get((p.ref, pin)) != net:
                bad += 1
                print(f"  MISMATCH {p.ref}.{pin}: want {net}, got {got.get((p.ref, pin))}")
    print(bi.key, f": netlist check: {bad} problems")
    return bad


if __name__ == "__main__":
    which = sys.argv[1] if len(sys.argv) > 1 else "all"
    for key in (common.KEYS if which == "all" else [which]):
        bi = common.info(key)
        build_sch(bi)
        txt = erc(bi)
        for l in txt.splitlines():
            if l.startswith("[") or "Error" in l[:12]:
                print("   ", l)
        verify_netlist(bi)
