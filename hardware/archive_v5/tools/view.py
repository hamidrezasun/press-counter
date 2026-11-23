"""Place only (no routing) and render a PNG for a quick look:  python view.py main|panel [top|bottom]"""
import os, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common, pcbgen

key = sys.argv[1]
side = sys.argv[2] if len(sys.argv) > 2 else "top"
bi = common.info(key)
pcb = bi.layout.build_pcb(pcbgen.PCB)
if hasattr(bi.layout, "silk"):
    bi.layout.silk(pcb)
out = os.path.join(bi.dir, "place_only.kicad_pcb")
pcb.save(out)
png = os.path.join(bi.dir, "preview.png")
r = subprocess.run([common.CLI, "pcb", "render", "--side", side, "--width", "2200", "--height", "1600",
                    "--quality", "basic", "--output", png, out], capture_output=True, text=True, timeout=300)
print("render rc", r.returncode, r.stderr[-200:])
