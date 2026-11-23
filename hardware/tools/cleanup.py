"""Remove copper that the DRC reports as dangling (a via with copper on one layer only, a track with a free end) and save.

    python cleanup.py main|panel        (run finish.py first so that drc.rpt is current; run finish.py again afterwards)

Only items that the DRC itself calls dangling are touched; a dangling track that was the only link of something would show up as
an unconnected item in the next DRC run, so nothing can silently get lost."""
import math
import os
import re
import sys

import pcbnew
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common, project

mm = pcbnew.ToMM


def main():
    bi = common.info(sys.argv[1])
    rpt = open(os.path.join(bi.dir, "drc.rpt"), encoding="utf-8").read()
    vias, tracks = [], []
    for blk in rpt.split("\n[")[1:]:
        head = blk.split("]")[0]
        for m in re.finditer(r"@\(([\d.]+) mm, ([\d.]+) mm\): (Via|Track) \[([^\]]*)\] on ([\w.]+)(?: - [\w.]+)?(?:, length ([\d.]+) mm)?", blk):
            x, y, kind, net, layer, length = float(m.group(1)), float(m.group(2)), m.group(3), m.group(4), m.group(5), m.group(6)
            if head == "via_dangling" and kind == "Via":
                vias.append((x, y, net))
            elif head == "track_dangling" and kind == "Track":
                tracks.append((x, y, net, layer, float(length or 0)))
    b = pcbnew.LoadBoard(bi.pcb)
    nets = {}
    all_tracks = list(b.GetTracks())                           # one list: wrapper objects must be the same ones for the identity tests below
    for t in all_tracks:
        nets.setdefault(t.GetNetname(), []).append(t)
    pads = {}
    for fp in b.GetFootprints():
        for pad in fp.Pads():
            bb = pad.GetBoundingBox()
            pads.setdefault(pad.GetNetname(), []).append((mm(bb.GetLeft()), mm(bb.GetTop()), mm(bb.GetRight()), mm(bb.GetBottom())))

    def touches(net, pt, skip):
        """is point pt (mm) on other copper of this net (track end, via, pad)?"""
        for o in nets.get(net, []):
            if o is skip:
                continue
            for q in ((o.GetStart(),) if o.GetClass() == "PCB_VIA" else (o.GetStart(), o.GetEnd())):
                if abs(mm(q.x) - pt[0]) < 0.05 and abs(mm(q.y) - pt[1]) < 0.05:
                    return True
        return any(r[0] - 0.02 <= pt[0] <= r[2] + 0.02 and r[1] - 0.02 <= pt[1] <= r[3] + 0.02 for r in pads.get(net, []))

    def junction_on(t, net):
        """endpoint of another same-net track that lies on the interior of t: the point where the dead tail starts"""
        a, e = (mm(t.GetStart().x), mm(t.GetStart().y)), (mm(t.GetEnd().x), mm(t.GetEnd().y))
        out = []
        dx, dy = e[0] - a[0], e[1] - a[1]
        L2 = dx * dx + dy * dy
        for o in nets.get(net, []):
            if o is t or o.GetClass() == "PCB_VIA":
                continue
            for q in (o.GetStart(), o.GetEnd()):
                p = (mm(q.x), mm(q.y))
                u = ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / L2 if L2 else 0
                if 0.02 < u < 0.98 and math.hypot(a[0] + u * dx - p[0], a[1] + u * dy - p[1]) < 0.03:
                    out.append((u, p))
        return out

    kill, trimmed = [], 0
    for t in all_tracks:
        p = t.GetStart()
        if t.GetClass() == "PCB_VIA":
            if any(abs(mm(p.x) - x) < 0.02 and abs(mm(p.y) - y) < 0.02 and t.GetNetname() == n for x, y, n in vias):
                kill.append(t)
            continue
        e = t.GetEnd()
        L = math.hypot(mm(e.x - p.x), mm(e.y - p.y))
        if not any(t.GetNetname() == n and b.GetLayerName(t.GetLayer()) == layer and abs(L - length) < 0.02 and (
                (abs(mm(p.x) - x) < 0.05 and abs(mm(p.y) - y) < 0.05) or (abs(mm(e.x) - x) < 0.05 and abs(mm(e.y) - y) < 0.05))
                for x, y, n, layer, length in tracks):
            continue
        net = t.GetNetname()
        free_start = not touches(net, (mm(p.x), mm(p.y)), t)
        free_end = not touches(net, (mm(e.x), mm(e.y)), t)
        js = junction_on(t, net)
        if free_start != free_end and js:                       # a dead tail behind a junction: cut the track at the junction
            u, pt = (min(js) if free_start else max(js))
            if free_start:
                t.SetStart(pcbnew.VECTOR2I(pcbnew.FromMM(pt[0]), pcbnew.FromMM(pt[1])))
            else:
                t.SetEnd(pcbnew.VECTOR2I(pcbnew.FromMM(pt[0]), pcbnew.FromMM(pt[1])))
            trimmed += 1
        elif (free_start or free_end) and not js and net not in bi.board.mains_nets and not net.startswith("RLY_"):
            kill.append(t)                                      # a plain dead-end stub of a low-voltage net
    for t in kill:
        b.Remove(t)
    b.Save(bi.pcb)
    project.write_project(bi.dir, bi.name, bi.board)
    print(f"removed {len(kill)} dangling items, trimmed {trimmed} dead tails ({len(vias)} vias / {len(tracks)} tracks reported)")


if __name__ == "__main__":
    main()
