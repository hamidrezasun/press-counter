"""Place-only overlap check: builds the placement of a board and lists courtyard / hole conflicts.
    python placecheck.py main|panel"""
import os, re, shutil, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common, pcbgen, project

key = sys.argv[1]
bi = common.info(key)
tmp = os.path.join(tempfile.gettempdir(), "pc_placecheck_" + key)
shutil.rmtree(tmp, ignore_errors=True)
os.makedirs(tmp)
pcb = bi.layout.build_pcb(pcbgen.PCB)
f = os.path.join(tmp, "pc.kicad_pcb")
pcb.save(f)
project.write_project(tmp, "pc", bi.board)
shutil.copy(os.path.join(common.HW, "lib", "press_counter.pretty", "..", "..", "tools", "common.py"), os.path.join(tmp, "x.py")) if False else None
rpt = os.path.join(tmp, "drc.rpt")
subprocess.run([common.CLI, "pcb", "drc", "--severity-all", "--format", "report", "--output", rpt, f],
               capture_output=True, text=True, timeout=600)
t = open(rpt, encoding="utf-8").read()
n = 0
for b in re.split(r"\n(?=\[)", t):
    m = re.match(r"\[(courtyards_overlap|pth_inside_courtyard|hole_clearance|hole_to_hole|copper_edge_clearance|items_not_allowed|silk_over_copper)\]", b)
    if m:
        refs = re.findall(r"Footprint (\w+)|of (\w+)\b", b)
        names = sorted({a or c for a, c in refs})
        pos = re.findall(r"@\(([\d.]+) mm, ([\d.]+) mm\)", b)
        print(m.group(1), names, [(float(x) - pcb.origin[0], float(y) - pcb.origin[1]) for x, y in pos[:1]])
        n += 1
print("conflicts:", n)
