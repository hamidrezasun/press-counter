"""Build the upload package for the esys.ir PCB order form (level 1, https://esys.ir/pcb.aspx).

    D:/KiCad/10.0/bin/python.exe -P make_esys_package.py

Rules of the shop (https://esys.ir/page.aspx?i=58, i=59), and what this script does about them:
  * only Gerber (or Altium PcbDoc) in a ZIP / RAR, no KiCad project   -> Gerber ZIP
  * Gerber file names must start with G or end in .TXT; usual extensions GTL GBL GKO ...  -> names "Gerber_press_counter.GTL" ... (G prefix AND Protel extension)
  * an NC drill file is mandatory, extension .TXT                    -> Gerber_press_counter_PTH.TXT and _NPTH.TXT (Excellon, mm)
  * a closed outline on the keep-out / outline layer                 -> Edge.Cuts as .GKO, checked to be one closed loop
  * board size in cm with 2 decimals                                 -> printed at the end
Runs check_esys.py first (limits of the level-1 FR-4 table) and stops if the board is outside them.
"""
import glob
import os
import shutil
import subprocess
import sys
import zipfile

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
PCB = os.path.normpath(os.path.join(HERE, "..", "main", "press_counter.kicad_pcb"))
OUT = os.path.normpath(os.path.join(HERE, "..", "fabrication", "esys"))
CLI = r"D:\KiCad\10.0\bin\kicad-cli.exe"
PY = sys.executable
BASE = "Gerber_press_counter"
LAYERS = {"F.Cu": "GTL", "B.Cu": "GBL", "F.Mask": "GTS", "B.Mask": "GBS", "F.SilkS": "GTO", "B.SilkS": "GBO", "Edge.Cuts": "GKO"}


def run(args):
    r = subprocess.run([CLI] + args, capture_output=True, text=True)
    if r.returncode:
        print(r.stdout, r.stderr)
        sys.exit("kicad-cli failed: " + " ".join(args[:3]))


def main():
    if subprocess.run([PY, "-P", os.path.join(HERE, "check_esys.py")]).returncode:
        sys.exit("the board is outside the esys level-1 limits (see above)")
    b = pcbnew.LoadBoard(PCB)
    ps = pcbnew.SHAPE_POLY_SET()
    if not b.GetBoardPolygonOutlines(ps, False) or ps.OutlineCount() != 1:
        sys.exit("the Edge.Cuts outline is not exactly one closed loop")
    bb = b.GetBoardEdgesBoundingBox()
    shutil.rmtree(OUT, ignore_errors=True)
    tmp = os.path.join(OUT, "_tmp")
    os.makedirs(tmp)
    run(["pcb", "export", "gerbers", "--output", tmp + os.sep, "--layers", ",".join(LAYERS), "--no-x2", "--no-netlist", PCB])      # Protel extensions are the default
    run(["pcb", "export", "drill", "--output", tmp + os.sep, "--format", "excellon", "--drill-origin", "absolute", "--excellon-units", "mm",
         "--excellon-separate-th", PCB])
    names = []
    for f in sorted(os.listdir(tmp)):
        low = f.lower()
        src = os.path.join(tmp, f)
        ext = os.path.splitext(f)[1].upper().lstrip(".")
        if low.endswith(".drl"):
            new = f"{BASE}_NPTH.TXT" if "npth" in low else f"{BASE}_PTH.TXT"
        elif ext in LAYERS.values():
            new = f"{BASE}.{ext}"
        elif ext == "GM1":                                # KiCad writes Edge.Cuts as .GM1 (mechanical 1): the shop wants the outline as a keep-out layer, .GKO
            new = f"{BASE}.GKO"
        else:
            continue                                      # job files etc. are not sent
        shutil.copy(src, os.path.join(OUT, new))
        names.append(new)
    shutil.rmtree(tmp)
    zpath = os.path.join(OUT, "press_counter_esys_gerber.zip")
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
        for n in names:
            z.write(os.path.join(OUT, n), n)
    print("files in the ZIP:")
    for n in names:
        print(f"  {n:38s} {os.path.getsize(os.path.join(OUT, n)):8d} bytes")
    w, h = pcbnew.ToMM(bb.GetWidth()) / 10, pcbnew.ToMM(bb.GetHeight()) / 10
    print(f"\nboard size for the form: {w:.2f} x {h:.2f} cm  ->", zpath)


if __name__ == "__main__":
    main()
