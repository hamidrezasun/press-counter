"""Mains-class nets: own router (1.5 mm clearance, 5 mm creepage to low voltage), hand-placed relay-coil stubs and the
creepage keep-out rectangles that keep the Freerouting pass (low-voltage nets) 5 mm away from all mains copper."""
import router


def route(pcb, lay, board, verbose=False):
    rules = router.default_rules(board.mains_nets, board.mains_dc_nets, board.power_nets, board.wide)
    rt = router.Router(pcb, lay.W, lay.H, rules, grid=0.2, corner_r=3.0)
    rt.no_via_nets = {n for n, w in board.wide.items() if w >= 1.0 and n in board.mains_nets}        # mains current: copper only, no vias
    if hasattr(lay, "ANTENNA"):
        rt.add_keepout(*lay.ANTENNA)
    for (layer, pts, w, net) in (lay.coil_stubs(pcb) if hasattr(lay, "coil_stubs") else []):
        rt.add_fixed_track(layer, pts, w, net)
    order = list(board.mains_nets) + list(board.mains_dc_nets)
    failed = rt.route_all(order, verbose=verbose, retries=3)
    if hasattr(lay, "local_power"):
        lay.local_power(pcb, rt)
    rt.write_to_pcb(lock=True)
    return rt, failed


def keepouts(pcb, rt, board, margin=5.0):
    """One rule-area rectangle (no tracks, no vias; pours stay allowed) around every mains pad / track / via."""
    mains = set(board.mains_nets) | set(board.mains_dc_nets)
    boxes = []
    for o in rt.objs:
        if o.net not in mains:
            continue
        if o.kind == "rect":
            x0, y0, x1, y1 = o.p
        elif o.kind == "seg":
            ax, ay, bx, by = o.p
            x0, x1 = min(ax, bx) - o.hw, max(ax, bx) + o.hw
            y0, y1 = min(ay, by) - o.hw, max(ay, by) + o.hw
        else:
            cx, cy, r = o.p
            x0, y0, x1, y1 = cx - r, cy - r, cx + r, cy + r
        boxes.append((x0 - margin, y0 - margin, x1 + margin, y1 + margin))
    # fixed hand-placed tracks of mains nets are in rt.objs already; also the ones written by write_to_pcb come from rt.tracks
    for (li, pts, w, net) in rt.tracks:
        if net not in mains:
            continue
        for a, b in zip(pts[:-1], pts[1:]):
            boxes.append((min(a[0], b[0]) - w / 2 - margin, min(a[1], b[1]) - w / 2 - margin,
                          max(a[0], b[0]) + w / 2 + margin, max(a[1], b[1]) + w / 2 + margin))
    # drop boxes completely inside another one
    uniq = []
    for i, b in enumerate(boxes):
        inside = any(j != i and o[0] <= b[0] and o[1] <= b[1] and o[2] >= b[2] and o[3] >= b[3] and (o != b or j < i)
                     for j, o in enumerate(boxes))
        if not inside:
            uniq.append(b)
    for i, b in enumerate(uniq):
        pcb.rect_keepout(*b, name=f"creepage {i}", pours=False)
    # report low-voltage pads that ended up inside a keep-out (they need hand-placed stubs)
    enclosed = []
    for o in rt.objs:
        if o.kind != "rect" or not o.ref or o.net in mains or o.net is None or str(o.net).startswith("__"):
            continue
        cx, cy = (o.p[0] + o.p[2]) / 2, (o.p[1] + o.p[3]) / 2
        if any(b[0] < cx < b[2] and b[1] < cy < b[3] for b in uniq):
            enclosed.append((o.ref, o.net, round(cx, 1), round(cy, 1)))
    return len(uniq), enclosed


def boxes_from_board(pcb, board):
    """Bounding boxes (board-local mm) of every mains pad / track / via of an already routed board (continue mode).
    Must run before any zone is removed: after pcbnew's BOARD.Remove() the SWIG iterators return untyped objects."""
    import pcbnew
    mains = set(board.mains_nets) | set(board.mains_dc_nets)
    ox, oy = pcb.origin
    mm = pcbnew.ToMM
    boxes = []
    for fp in pcb.fps.values():
        for pad in fp.Pads():
            if pad.GetNetname() in mains:
                bb = pad.GetBoundingBox()
                boxes.append((mm(bb.GetLeft()) - ox, mm(bb.GetTop()) - oy, mm(bb.GetRight()) - ox, mm(bb.GetBottom()) - oy))
    for t in pcb.b.GetTracks():
        if t.GetNetname() not in mains:
            continue
        if t.GetClass() == "PCB_VIA":
            x, y, r = mm(t.GetX()) - ox, mm(t.GetY()) - oy, mm(t.GetWidth(pcbnew.F_Cu)) / 2
            boxes.append((x - r, y - r, x + r, y + r))
        else:
            a, b2, hw = t.GetStart(), t.GetEnd(), mm(t.GetWidth()) / 2
            ax, ay, bx, by = mm(a.x) - ox, mm(a.y) - oy, mm(b2.x) - ox, mm(b2.y) - oy
            boxes.append((min(ax, bx) - hw, min(ay, by) - hw, max(ax, bx) + hw, max(ay, by) + hw))
    return boxes


def keepouts_from_boxes(pcb, boxes, margin=5.0):
    n = 0
    for i, bx in enumerate(boxes):
        pcb.rect_keepout(bx[0] - margin, bx[1] - margin, bx[2] + margin, bx[3] + margin, name=f"creepage {i}", pours=False)
        n += 1
    return n
