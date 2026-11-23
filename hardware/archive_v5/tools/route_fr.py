"""Auto-route a board with Freerouting (Specctra DSN/SES round trip through KiCad's pcbnew).

    python route_fr.py main|panel [--passes N] [--no-run]

Steps: build placement (+ silk, keepouts) -> save .kicad_pcb -> export DSN -> post-process (net classes, GND planes)
       -> freerouting-cli -> import SES -> save.  Zone fill / DRC / exports happen in finish.py.
"""
import os
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import pcbnew
import common, pcbgen, project

JAVA = "D:/Android/Android Studio/jbr/bin/java.exe"          # Java 21 (any >= 11 works for Freerouting 1.9)
FR_JAR = os.path.join(HERE, "freerouting", "freerouting-1.9.0.jar")
FR2_JAR = os.path.join(HERE, "freerouting", "freerouting-2.5.0.jar")                       # Freerouting 2.5.0 (needs Java 25: the Temurin JRE below)
JAVA25 = os.path.join(HERE, "freerouting", "jre25", "jdk-25.0.4.1+1-jre", "bin", "java.exe")


def um(v):
    return int(round(v * 1000))


def postprocess_dsn(path, bi, extra_keepouts=None):
    t = open(path, encoding="utf-8").read()
    b = bi.board
    # --- net classes: default / power / mains
    m = re.search(r"\n\s*\(class kicad_default.*?\n    \)\n", t, re.S)
    assert m, "class kicad_default not found"
    net_sec = t[t.index("(network"):t.index("(wiring")] if "(wiring" in t else t[t.index("(network"):]
    allnets = re.findall(r'^    \(net ("[^"]+"|[^\s()]+)', net_sec, re.M)

    def q(n):
        return n if n.startswith('"') else n
    power = [n for n in b.power_nets]
    mains = list(b.mains_nets) + list(b.mains_dc_nets)
    wide = {k: v for k, v in b.wide.items()}
    used = set(power) | set(mains)
    default = [n for n in allnets if n.strip('"') not in used]
    def cls(name, nets, width, clr):
        return (f"    (class {name} " + " ".join(n for n in nets) + "\n"
                f"      (circuit\n        (use_via \"Via[0-1]_600:300_um\")\n      )\n"
                f"      (rule\n        (width {um(width)})\n        (clearance {um(clr)})\n      )\n    )\n")
    out = cls("default", default, 0.25, 0.2)
    for n in power:
        out += cls("pwr_" + n.strip("+"), [n], wide.get(n, 0.5), 0.2)
    wn = [n for n in mains if n in wide]
    mn = [n for n in mains if n not in wide]
    if mn:
        out += cls("mains", mn, 0.6, 0.2)
    for n in wn:
        out += cls("wide_" + n, [n], wide[n], 0.2)
    t = t[:m.start()] + "\n" + out + t[m.end():]
    # --- structure rule: default width/clearance
    t = t.replace("(width 200)\n      (clearance 200)", "(width 250)\n      (clearance 200)", 1)
    open(path, "w", encoding="utf-8").write(t)


def main():
    key = sys.argv[1]
    passes = 40
    if "--passes" in sys.argv:
        passes = int(sys.argv[sys.argv.index("--passes") + 1])
    bi = common.info(key)
    lay = bi.layout
    cont = "--continue" in sys.argv
    if cont:                                           # keep the existing routing and let Freerouting finish / improve it
        b0 = pcbnew.LoadBoard(bi.pcb)
        pcb = pcbgen.PCB.from_board(b0, bi.board)      # (do all reading before touching zones: SWIG loses the proxies after Remove())
        boxes = []
        if bi.board.mains_nets:
            import mainsroute
            boxes = mainsroute.boxes_from_board(pcb, bi.board)
        if boxes:
            print("creepage keep-outs:", mainsroute.keepouts_from_boxes(pcb, boxes))
        old_pours = [z for z in b0.Zones() if not z.GetIsRuleArea()]
        for z in old_pours:
            b0.Remove(z)                               # the GND pours are re-made after routing (nothing may be built after this)
    else:
        pcb = lay.build_pcb(pcbgen.PCB)
    if not cont and hasattr(lay, "extras"):
        lay.extras(pcb)
    if not cont and hasattr(lay, "silk"):
        lay.silk(pcb)
    if not cont and hasattr(lay, "preroutes"):
        lay.preroutes(pcb)
    if bi.board.mains_nets and not cont:
        import mainsroute
        rt, failed = mainsroute.route(pcb, lay, bi.board)
        n, enclosed = mainsroute.keepouts(pcb, rt, bi.board)
        print(f"mains nets routed with the own router, failed: {failed}; {n} creepage keep-outs")
        print("low-voltage pads inside a creepage keep-out:", enclosed)
        if failed:
            print("!! MAINS NETS NOT ROUTED:", failed)
    work = os.path.join(bi.dir, "fr")
    os.makedirs(work, exist_ok=True)
    tmp = os.path.join(work, "place.kicad_pcb")
    pcb.save(tmp)
    project.write_project(work, "place", bi.board)
    b = pcbnew.LoadBoard(tmp)
    dsn = os.path.join(work, "in.dsn")
    ses = os.path.join(work, "out.ses")
    assert pcbnew.ExportSpecctraDSN(b, dsn)
    postprocess_dsn(dsn, bi)
    if "--no-run" in sys.argv:
        return
    if os.path.exists(ses):
        os.remove(ses)
    t0 = time.time()
    if os.path.exists(FR2_JAR) and os.path.exists(JAVA25):
        cmd = [JAVA25, "-jar", FR2_JAR, "--gui.enabled=false", "-de", dsn, "-do", ses, "-mp", str(passes)]
    else:
        cmd = [JAVA, "-jar", FR_JAR, "-de", dsn, "-do", ses, "-mp", str(passes)]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=7200)
    open(os.path.join(work, "freerouting.log"), "w", encoding="utf-8").write(r.stdout + "\n" + r.stderr)
    print(f"freerouting finished in {time.time() - t0:.0f}s rc={r.returncode}")
    print("\n".join((r.stdout + r.stderr).splitlines()[-12:]))
    if not os.path.exists(ses):
        print("no SES produced")
        sys.exit(1)
    ok = pcbnew.ImportSpecctraSES(b, ses)
    print("SES import", ok)
    for z in list(b.Zones()):                      # the creepage keep-outs only steered the router: remove them again
        if z.GetIsRuleArea() and str(z.GetZoneName()).startswith("creepage"):
            b.Remove(z)
    p2 = pcbgen.PCB.from_board(b, bi.board)
    zl = getattr(lay, "zones", None)
    if zl:
        zl(p2)
    else:
        for layer in ("F.Cu", "B.Cu"):
            p2.add_zone("GND", layer, 0.5, 0.5, lay.W - 0.5, lay.H - 0.5, clearance=0.3)
    b.Save(bi.pcb)
    project.write_project(bi.dir, bi.name, bi.board)
    n_tr = len([t for t in b.GetTracks() if t.GetClass() == "PCB_TRACK"])
    n_via = len([t for t in b.GetTracks() if t.GetClass() == "PCB_VIA"])
    print("tracks", n_tr, "vias", n_via, "->", bi.pcb)


if __name__ == "__main__":
    main()
