"""Fill zones, run DRC, export fabrication files and renders.   python finish.py main|panel [--no-export]"""
import os
import glob
import subprocess
import sys
import zipfile

import pcbnew
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common, project


def run(args, timeout=900):
    r = subprocess.run([common.CLI] + args, capture_output=True, text=True, timeout=timeout)
    if r.returncode not in (0, 5):
        print("  !!", " ".join(args[:3]), "rc", r.returncode, (r.stderr or r.stdout)[-400:])
    return r


def fill_zones(bi):
    b = pcbnew.LoadBoard(bi.pcb)
    hook = getattr(bi.layout, "extra_zones", None)
    if hook and not any(str(z.GetZoneName()) == "3V3 heat sink" for z in b.Zones()):
        import pcbgen
        hook(pcbgen.PCB.from_board(b, bi.board))
    filler = pcbnew.ZONE_FILLER(b)
    filler.Fill(b.Zones())
    b.Save(bi.pcb)
    project.write_project(bi.dir, bi.name, bi.board)          # pcbnew rewrites the .kicad_pro on save
    print("zones filled:", len(b.Zones()))


def drc(bi):
    rpt = os.path.join(bi.dir, "drc.rpt")
    run(["pcb", "drc", "--severity-all", "--schematic-parity", "--all-track-errors", "--format", "report", "--output", rpt, bi.pcb])
    txt = open(rpt, encoding="utf-8").read()
    lines = [l for l in txt.splitlines() if l.startswith("**")]
    print("DRC:", " | ".join(l.strip("* ").strip() for l in lines))
    return txt


def export(bi):
    g = os.path.join(bi.fab, "gerbers")
    os.makedirs(g, exist_ok=True)
    for old in glob.glob(os.path.join(g, "*.drl")) + glob.glob(os.path.join(g, "*drl_map.pdf")):
        os.remove(old)
    run(["pcb", "export", "gerbers", "--output", g + os.sep, "--layers",
         "F.Cu,B.Cu,F.Mask,B.Mask,F.SilkS,B.SilkS,F.Paste,B.Paste,Edge.Cuts", "--no-protel-ext", bi.pcb])
    run(["pcb", "export", "drill", "--output", g + os.sep, "--format", "excellon", "--drill-origin", "absolute",
         "--excellon-units", "mm", "--excellon-separate-th", "--generate-map", "--map-format", "pdf", bi.pcb])
    zpath = os.path.join(bi.fab, bi.name + "_gerbers.zip")          # what a board house wants: copper, mask, silk, paste, outline + drill files
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
        for f in sorted(glob.glob(os.path.join(g, "*.gbr")) + glob.glob(os.path.join(g, "*.drl")) + glob.glob(os.path.join(g, "*.gbrjob"))):
            z.write(f, os.path.basename(f))
    run(["pcb", "export", "pos", "--output", os.path.join(bi.fab, "positions.csv"), "--format", "csv", "--units", "mm", bi.pcb])
    run(["pcb", "export", "pdf", "--output", os.path.join(bi.fab, "pcb_layers.pdf"), "--layers", "F.Cu,B.Cu,F.SilkS,Edge.Cuts",
         "--mode-multipage", bi.pcb])
    run(["sch", "export", "pdf", "--output", os.path.join(bi.fab, "schematic.pdf"), bi.sch])
    for side in ("top", "bottom"):
        run(["pcb", "render", "--side", side, "--width", "2000", "--height", "1500", "--quality", "basic",
             "--output", os.path.join(bi.fab, f"render_{side}.png"), bi.pcb])
    print("exports written to", bi.fab)


if __name__ == "__main__":
    bi = common.info(sys.argv[1])
    fill_zones(bi)
    drc(bi)
    if "--no-export" not in sys.argv:
        export(bi)
