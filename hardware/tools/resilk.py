"""Rebuild the silkscreen of an already routed board (no re-route):   python resilk.py main|panel [--pcb file]

Removes every free text / graphic on the silkscreen layers and draws them again from layout_*.silk(), so silk changes in the layout
files (text sizes, help sheet, labels) can be applied without running the autorouter again."""
import os
import sys

import pcbnew
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common, fab_rules, pcbgen, project


def main():
    bi = common.info(sys.argv[1])
    path = sys.argv[sys.argv.index("--pcb") + 1] if "--pcb" in sys.argv else bi.pcb
    b = pcbnew.LoadBoard(path)
    pcb = pcbgen.PCB.from_board(b, bi.board)           # wrap everything BEFORE anything is removed (SWIG proxy loss after Remove)
    silk = (pcbnew.F_SilkS, pcbnew.B_SilkS)
    old = [d for d in b.GetDrawings() if d.GetLayer() in silk]
    for d in old:
        b.Remove(d)
    if hasattr(bi.layout, "silk"):
        bi.layout.silk(pcb)
    fixed = fab_rules.apply(b)                         # esys.ir legend / slot rules (see fab_rules.py)
    b.Save(path)
    if path == bi.pcb:
        project.write_project(bi.dir, bi.name, bi.board)
    print("silk rebuilt:", len(old), "old items replaced; fab rules:", fixed)


if __name__ == "__main__":
    main()
