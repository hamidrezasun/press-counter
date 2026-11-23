"""MAIN board (v5): outline, placement and silk-screen help (board-local mm, origin = top-left, y down).

Left strip (x < ~68): mains - supply input + fuse + HLK module, press-sense input, press-contactor relay contacts.
Right part: low voltage - STM32, ESP-12F (antenna at the top edge), USB, relay driver, LM35 input, panel cable (J6).
5 mm creepage between every mains copper item and every low-voltage copper item is enforced by the router and a DRC rule.
"""
import design

W, H = 188.0, 104.0              # 136 x 104 mm for the controller part + a 52 mm strip on the right with the four output relays K2-K5 and the voltage-sense parts
OLD_W = 136.0

# output relay cells (K2..K5 = OUT1..OUT4): relay COM pad at (XR, CY[n]), contacts east, terminal at the right edge, driver parts west
XR = 156.0
PITCH = 22.0
CY = [16.5 + PITCH * i for i in range(4)]
CELL = {"D": (-9.5, 0.0, 90), "Q": (-15.0, -6.5, 0), "RB": (-15.0, -2.7, 0), "RP": (-15.0, 0.8, 0), "LED": (-15.0, 4.3, 0), "RL": (-15.0, 7.8, 0)}


def cell_places():
    out = {}
    for n in range(1, 5):
        cy = CY[n - 1]
        out[f"K{n + 1}"] = (XR, cy, 0)
        out[f"J{9 + n}"] = (XR + 24.9, cy + 6.05, 90)                 # pin 1 = NO (south), pin 2 = COM (north); wire entry to the right edge
        for key, ref in (("D", f"D{19 + n}"), ("Q", f"Q{n + 2}"), ("RB", f"R{59 + n}"), ("RP", f"R{69 + n}"), ("LED", f"D{29 + n}"), ("RL", f"R{79 + n}")):
            dx, dy, rot = CELL[key]
            out[ref] = (XR + dx, cy + dy, rot)
    return out

PLACE = {
    "MH1": (4.0, 4.0, 0), "MH2": (W - 4.0, 4.0, 0), "MH3": (4.0, H - 4.0, 0), "MH4": (W - 4.0, H - 4.0, 0),
    "MH5": (OLD_W - 2.0, 4.0, 0), "MH6": (OLD_W - 2.0, H - 4.0, 0),
    "FID1": (9.0, 3.5, 0), "FID2": (W - 9.0, H - 3.5, 0), "FID3": (9.0, H - 3.5, 0),

    # ── mains in + fuse + HLK-5M05 (AC pads left, DC pads right)
    "J1": (6.0, 20.0, 270), "F1": (31.0, 8.0, 0), "RV1": (15.0, 28.0, 90), "PS1": (24.0, 24.0, 0),
    "C1": (66.0, 28.0, 90), "C2": (66.0, 38.0, 0), "TP1": (62.0, 5.0, 0), "TP3": (66.0, 5.0, 0),

    # ── press-sense input (mains)
    "J2": (6.0, 52.0, 270), "RV2": (15.0, 50.0, 90),
    "RS1": (30.0, 62.5, 0),                       # one 120k 1 W axial resistor (pads 15.24 mm apart) instead of four 1206 resistors
    "BR1": (38.0, 51.0, 180), "C5": (47.0, 47.0, 0), "D5": (47.0, 56.0, 0), "U2": (58.0, 52.0, 0), "R1": (67.0, 55.0, 90),
    "TP4": (70.0, 59.0, 0),

    # ── relay: contacts = mains (COM at the east end of the body, NO / NC at the west end next to J5); driver parts east of it (LV)
    "K1": (40.0, 85.0, 180), "J5": (6.0, 79.0, 270), "RV3": (16.0, 97.0, 0),
    "Q2": (54.0, 92.0, 0), "R9": (59.0, 89.0, 0), "R13": (59.0, 93.0, 0), "D12": (55.0, 84.0, 0),
    "R43": (53.0, 98.0, 0), "D13": (58.0, 98.0, 0),

    # ── 3.3 V LDO
    "U3": (80.0, 10.0, 0), "C3": (73.0, 13.0, 90), "C4": (88.0, 13.0, 90), "C19": (95.0, 13.0, 90),
    "R2": (83.0, 4.0, 0), "D6": (88.0, 4.0, 0), "TP2": (100.0, 5.0, 0),

    # ── ESP-12F (antenna at the top edge); small parts are positioned relative to its pads in fine()
    "U1": (116.0, 16.0, 0),
    "R50": (103.0, 12.5, 0), "R51": (103.0, 16.5, 0), "R15": (103.0, 24.5, 0),
    "R52": (127.5, 24.7, 0), "R53": (127.5, 19.5, 0), "R54": (127.5, 22.1, 0), "R55": (127.5, 27.3, 0),
    "D16": (131.5, 28.5, 0), "C16": (110.0, 33.0, 0), "C17": (115.0, 33.0, 0),
    "SW1": (109, 39.5, 0), "SW2": (119, 39.5, 0), "R14": (127.5, 31.0, 0),

    # ── STM32F103C8T6 + crystal
    "U4": (98.0, 66.0, 0), "Y1": (82.0, 66.0, 90),
    "C6": (82.0, 56.0, 0), "C7": (82.0, 76.0, 0),
    "C8": (92.0, 57.0, 0), "C9": (106.0, 59.0, 0), "C10": (104.0, 74.0, 0), "C11": (90.0, 72.0, 0), "C12": (90.0, 76.0, 0),
    "C13": (88.0, 60.0, 0), "C14": (88.0, 66.0, 90),
    "R3": (97.0, 55.0, 0), "R5": (101.0, 55.0, 0), "R4": (95.0, 77.0, 0), "R16": (74.0, 63.0, 0),

    # ── panel connector + series resistors
    "J6": (124.0, 50.0, 0),
    "R20": (112.0, 50.0, 0), "R21": (112.0, 53.0, 0), "R22": (112.0, 56.0, 0), "R23": (112.0, 59.0, 0), "R24": (112.0, 62.0, 0),
    "R25": (112.0, 65.0, 0), "R26": (112.0, 68.0, 0), "R27": (112.0, 71.0, 0), "R28": (112.0, 74.0, 0), "R29": (112.0, 77.0, 0),
    "R32": (106.0, 80.0, 0), "R33": (112.0, 80.0, 0),

    # ── USB / SWD / LM35
    "J3": (116, 98, 0), "R30": (108.5, 84.2, 0), "R31": (112.5, 84.2, 0), "D14": (113.0, 89.0, 0),
    "J9": (135.0, 34.0, 0),
    "J4": (84.0, 98.0, 0), "R10": (88.0, 90.0, 0), "C15": (92.0, 90.0, 0),
}
PLACE.update({
    # 12 V sense: terminal + divider in the strip below the relay cells (bottom edge); ZMPT101B header + filter in the free LV area between the 5 V module and the ESP
    "J7": (143.0, 97.7, 0), "R90": (154.0, 97.5, 0), "R91": (158.2, 97.5, 0), "R92": (162.4, 97.5, 0), "C20": (167.0, 97.5, 0),
    "J8": (78.0, 28.0, 0), "R93": (86.0, 30.0, 0), "C21": (91.0, 30.0, 0),
})
PLACE.update({
    # USB programming of the ESP8266 (CH340G + auto-reset) in the free area left of the STM32, micro-USB on the bottom edge;
    # STM32 RESET / BOOT buttons and the SWD header J9 in the free column right of the ESP
    "J14": (74.0, 98.0, 0), "D15": (65.3, 93.0, 0), "U5": (73.0, 86.0, 180), "Y2": (72.0, 75.5, 0), "C25": (79.5, 80.5, 90), "C26": (79.5, 84.0, 90),
    "C22": (66.0, 86.0, 90), "R97": (66.0, 81.5, 0), "Q7": (65.5, 68.0, 0), "Q8": (70.5, 68.0, 0), "R94": (65.5, 64.5, 0), "R95": (69.5, 64.5, 0),
    "SW3": (102.5, 98.0, 0), "SW4": (102.5, 89.5, 0), "R96": (96.5, 84.0, 0),
})
PLACE.update(cell_places())

# ESP antenna: no copper on any layer (the footprint carries its own keepout too)
ANTENNA = (104.0, 0.0, 128.0, 10.0)
FIDUCIALS = ("FID1", "FID2", "FID3")


def fine(pcb):
    """Positions relative to pads of the big parts (called after the coarse placement)."""
    # ESP: pull-ups on the left pads
    pcb.near("R50", "U1", "1", -4.3, 0.0, 0)
    pcb.near("R51", "U1", "3", -4.3, 0.0, 0)
    pcb.near("R15", "U1", "7", -4.3, 0.0, 0)


def coil_stubs(pcb):
    """Hand-placed first segments of the relay-coil nets (+5V and the driver collector).  The coil pads of a T73 relay are only about 6 mm
    from the COM pad (the relay maker certifies that distance, so it is exempt inside the relay's courtyard); the stubs leave the 5 mm
    creepage ring of COM straight away from it before the autorouter takes over.
    K1: 7 mm north / south.  K2-K7 (output relays, pitch 18 mm): a short stub and a diagonal that ends 8.5 mm from the relay centre line
    on the driver side (west), so that neighbouring cells do not collide."""
    ax, ay = pcb.pad_pos("K1", "2")[0]
    bx, by = pcb.pad_pos("K1", "5")[0]
    north = -1.0 if ay < by else 1.0
    out = [(0, [(ax, ay + north * 1.0), (ax, ay + north * 7.0)], 0.5, "+5V"),
           (0, [(bx, by - north * 1.0), (bx, by - north * 7.0)], 0.5, "RLY_N")]
    for n in range(1, 5):
        ref = f"K{n + 1}"
        cx, cy = pcb.pos(ref)                                     # COM pad = footprint origin
        px, py = pcb.pad_pos(ref, "2")[0]                         # +5V coil pad (south of the centre line for rot 0)
        qx, qy = pcb.pad_pos(ref, "5")[0]                         # driver-side coil pad (north)
        sgn = 1.0 if py > cy else -1.0
        out.append((0, [(px, py + sgn * 0.8), (px, py + sgn * 1.9), (px - 1.4, cy + sgn * 8.5), (px - 4.5, cy + sgn * 8.5)], 0.5, "+5V"))
        out.append((0, [(qx, qy - sgn * 0.8), (qx, qy - sgn * 1.9), (qx - 1.4, cy - sgn * 8.5), (qx - 4.5, cy - sgn * 8.5)], 0.5, f"OUT{n}_N"))
    return out


def stub_geometry(pcb):
    """Escape stubs of the STM32 supply pins: (net, track points, via point).  The pins have 0.5 mm pitch and the autorouter cannot find
    the way out of the pin field for them; each stub leaves the pad (pad length 1.48 mm -> the tip is 0.74 mm from the centre) and ends in
    a through via.  Pin 48 (+3V3) bends away from its GND neighbour (pin 47) so that both vias fit; pin 47 gets a via because the GND pour
    cannot reach the pad between the stubs and signals around it."""
    out = []
    dirs = {"1": (-1, 0), "9": (-1, 0), "36": (1, 0), "24": (0, 1)}
    for pin, (dx, dy) in dirs.items():
        x, y = pcb.pad_pos("U4", pin)[0]
        tip = (x + dx * 0.74, y + dy * 0.74)
        via = (tip[0] + dx * 0.95, tip[1] + dy * 0.95)
        out.append(("+3V3", [(x + dx * 0.4, y + dy * 0.4), via], via))
    x, y = pcb.pad_pos("U4", "48")[0]                      # top row: first go straight up, then 45 degrees towards the corner
    via = (x - 0.7, y - 0.74 - 0.45 - 0.7)
    out.append(("+3V3", [(x, y - 0.4), (x, y - 0.74 - 0.45), via], via))
    x, y = pcb.pad_pos("U4", "47")[0]
    via = (x, y - 0.74 - 0.95)
    out.append(("GND", [(x, y - 0.4), via], via))
    return out


def preroutes(pcb):
    """Locked escape stubs (see stub_geometry) - placed before the autorouter runs."""
    for net, pts, via in stub_geometry(pcb):
        pcb.add_track("F.Cu", pts, 0.25, net, locked=True)
        pcb.add_via(via[0], via[1], net, locked=True)


LOCAL_POWER_CAPS = ("C8", "C9", "C10", "C11", "C12", "C13")      # MCU decoupling capacitors that the stub vias are tied to


def local_power(pcb, rt):
    """Tie every +3V3 stub via of the STM32 to its nearest decoupling capacitor with the own router (locked tracks).  Short radial
    links, like the signal escapes - no ring around the chip.  The autorouter then only has to join the capacitors to the rest of +3V3."""
    import router
    vias = []
    for net, pts, via in stub_geometry(pcb):
        for a, b in zip(pts[:-1], pts[1:]):
            rt.objs.append(router.Obj("seg", (a[0], a[1], b[0], b[1]), (0,), net, 0.125))
        o = router.Obj("rect", (via[0] - 0.3, via[1] - 0.3, via[0] + 0.3, via[1] + 0.3), (0, 1), net, ref="VIA")
        rt.objs.append(o)
        if net == "+3V3":
            vias.append(o)
    caps = [o for o in rt.objs if o.kind == "rect" and o.net == "+3V3" and o.ref in LOCAL_POWER_CAPS]
    allok = True
    for v in vias:
        vc = ((v.p[0] + v.p[2]) / 2, (v.p[1] + v.p[3]) / 2)
        cap = min(caps, key=lambda c: abs((c.p[0] + c.p[2]) / 2 - vc[0]) + abs((c.p[1] + c.p[3]) / 2 - vc[1]))
        ok = rt.route_net("+3V3", pads=[v, cap])
        print(f"  stub via ({vc[0]:.1f},{vc[1]:.1f}) -> {cap.ref}:", "OK" if ok else "FAILED")
        allok = allok and ok
    return allok


def extras(pcb):
    """Copper-free ESP antenna area (no tracks / vias / pours) and a pour-free halo around each assembly fiducial."""
    pcb.rect_keepout(*ANTENNA, name="ESP antenna keepout")
    for ref in FIDUCIALS:
        x, y = pcb.pos(ref)
        pcb.rect_keepout(x - 2.0, y - 2.0, x + 2.0, y + 2.0, name="fiducial halo " + ref, tracks=False, vias=False)


def extra_zones(pcb):
    """+3V3 copper on the top layer around the tab of the AMS1117 (the tab IS the output pin and the heat path of the chip: a few cm2
    of copper keep the junction temperature down).  Higher priority than the GND pour; only fills where nothing else is."""
    import pcbnew
    x, y = pcb.pos("U3")
    z = pcb.add_zone("+3V3", "F.Cu", x + 1.0, y - 5.0, x + 15.0, y + 8.0, clearance=0.3, min_w=0.25, priority=2)
    z.SetZoneName("3V3 heat sink")
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)


def build_pcb(PCBCls):
    b = design.main_board()
    pcb = PCBCls(b)
    pcb.add_parts(PLACE)
    fine(pcb)
    pcb.outline(0, 0, W, H, r=3.0)
    return pcb


REF_OFFSET = {"TP1": (0.0, 2.0), "TP2": (0.0, 2.0), "TP3": (0.0, 2.0), "TP4": (0.0, 2.0), "J9": (0.0, 2.4), "J3": (0.0, 3.2)}
# reference designators that cannot be placed without touching a neighbour (tight stacks): not printed
HIDE_REF = ("J7", "J8", "J10", "J11", "J12", "J13", "R52", "R53", "R54", "R55", "C12", "SW1", "U3", "R1", "C1", "R20", "R21") + tuple(
    ref for n in range(1, 5) for ref in (f"D{19 + n}", f"Q{n + 2}", f"R{59 + n}", f"R{69 + n}", f"D{29 + n}", f"R{79 + n}"))


def silk(pcb):
    import pcbnew
    pcb.ref_offsets(REF_OFFSET)
    pcb.hide_refs(HIDE_REF)
    pcb.edit_fp_rect("U2", 2.4, 2.4, dot_center=(-1.7, -1.7))   # narrower body outline: the old one ran over the pads of this wide SOP-4
    t = pcb.text
    lp = pcb.label_pads
    # ── titles and warnings (front)
    t("PRESS COUNTER v5 - MAIN BOARD", 70.0, 42.0, 1.2, bold=True, justify="left")
    t("STM32F103 + ESP-12F WiFi + 5 relays + MQTT", 70.0, 44.8, 0.7, justify="left")
    t("DANGER 230V AC - mains zone (left) is NOT isolated from the line", 70.0, 47.6, 0.8, bold=True, justify="left")
    pcb.rect_outline(1.5, 1.5, 66.5, 66.5, dash=1.5)
    pcb.rect_outline(1.5, 69.0, 46.5, 102.5, dash=1.5)
    t("MAINS ZONE", 3.0, 61.5, 0.8, bold=True, justify="left")
    t("RELAY CONTACT (MAINS)", 13.0, 101.2, 0.7, bold=True, justify="left")
    # ── mains terminals
    lp("J1", {"1": "L", "2": "N"}, dx=6.2, size=1.0, bold=True, justify="left")
    t("MAINS IN 230VAC", 2.5, 12.3, 0.7, bold=True, justify="left")
    t("F1  T1A 250V", 31.0, 8.0, 0.8, bold=True)
    lp("J2", {"1": "SNS A", "2": "SNS B"}, dx=6.2, size=0.8, bold=True, justify="left")
    t("PRESS SENSE 230V", 2.5, 43.0, 0.7, bold=True, justify="left")
    t("-", 43.4, 50.6, 0.9, bold=True)                          # BR1 MB10S: DC pins on the east side, the + lead of the part (bevelled corner) goes on pad 1
    t("+", 43.4, 52.7, 0.9, bold=True)
    lp("J5", {"1": "NO", "2": "COM", "3": "NC"}, dx=6.2, size=0.8, bold=True, justify="left")
    t("RELAY OUT 10A", 2.5, 72.8, 0.7, bold=True, justify="left")
    # ── connectors (low voltage)
    lp("J4", {"1": "OUT", "2": "GND", "3": "+5V"}, dy=-3.6, size=0.8, bold=True)
    t("LM35", 82.0, 91.5, 0.8, bold=True, justify="left")
    lp("J9", {"1": "GND", "2": "CLK", "3": "DIO", "4": "3V3", "5": "RST"}, dx=-1.7, size=0.7, bold=True, justify="right")
    t("USB STM32", 117.0, 92.0, 0.8, bold=True, justify="left")
    t("(console / update)", 117.0, 93.7, 0.7, justify="left")
    t("USB ESP8266", 69.0, 92.5, 0.7, bold=True, justify="left")
    # ── switches, LEDs, test points
    t("ESP RESET", 109.0, 44.0, 0.7, bold=True)
    t("ESP FLASH", 119.0, 44.0, 0.7, bold=True)
    t("STM RESET", 102.5, 101.9, 0.6, bold=True)
    t("STM BOOT", 102.5, 93.4, 0.6, bold=True)
    t("SWD", 135.0, 32.0, 0.6, bold=True)
    t("3V3 OK", 90.5, 4.0, 0.7, justify="left")
    t("RELAY ON", 60.5, 98.0, 0.7, justify="left")
    t("WIFI", 131.5, 31.6, 0.7)
    for ref, s in (("TP1", "5V"), ("TP3", "GND"), ("TP2", "3V3"), ("TP4", "PRESS")):
        lp(ref, {"1": s}, dy=-2.0, size=0.7, bold=True)
    t("J6", 124.0, 47.0, 0.8, bold=True)
    # ── voltage inputs
    lp("J7", {"1": "+12V", "2": "GND"}, dy=3.4, size=0.8, bold=True, layer=pcbnew.B_SilkS)
    t("12V SENSE IN", 148.0, 91.0, 0.8, bold=True)
    lp("J8", {"1": "3V3", "2": "OUT", "3": "GND"}, dx=1.9, size=0.7, bold=True, justify="left")
    t("230V SENSOR MODULE", 78.0, 24.5, 0.8, bold=True, justify="left")
    t("(ZMPT101B: 3V3 OUT GND)", 78.0, 26.2, 0.7, justify="left")
    # ── output relays OUT1..OUT4
    for n in range(1, 5):
        cy = CY[n - 1]
        j = f"J{9 + n}"
        lp(j, {"1": "NO", "2": "COM", "3": "NC"}, dx=-4.4, size=0.8, bold=True, justify="right", layer=pcbnew.B_SilkS)
        t(f"OUT{n} (K{n + 1})", XR + 24.9, cy - 8.8, 0.8, bold=True)
    pcb.rect_outline(XR - 3.5, 1.5, W - 1.5, H - 1.5, dash=1.5)
    t("OUTPUTS 2.5A max", 162.0, 101.8, 0.7, bold=True, justify="left")
    back_sheet(pcb)


def back_sheet(pcb):
    """Help sheet on the back (bottom silk, mirrored): pin maps, jumpers, boot modes, warnings."""
    import pcbnew
    L = pcbnew.B_SilkS

    tht = pcb.tht_boxes()
    state = {"dy": 0.0}

    def bt(s, x, y, size=0.8, bold=False):
        # mirrored text anchored at its LEFT edge as seen from the back: x grows to the left in board coordinates.
        # The line is pushed down (together with everything below it) until it no longer covers a through-hole pad.
        width = len(s) * size * 0.8
        yy = y + state["dy"]
        for _ in range(60):
            if not any(b[2] > x - width and b[0] < x and b[3] > yy - size * 0.6 and b[1] < yy + size * 0.6 for b in tht):
                break
            yy += 0.5
        state["dy"] = yy - y
        pcb.text(s, x, yy, size, layer=L, mirror=True, bold=bold, justify="right")     # pcbgen swaps left / right on the bottom layer
    x = 118.0
    y = 6.0
    bt("PRESS COUNTER v5 MAIN - HELP SHEET (back side)", x, y, 1.0, True); y += 2.4
    bt("DANGER: left part of the board carries 230VAC. Unplug mains before touching.", x, y, 0.7, True); y += 2.0
    bt("Mains side and low-voltage side are separated by >= 5mm creepage; HLK-5M05 and PC817 isolate them.", x, y, 0.7); y += 2.2
    bt("CONNECTORS", x, y, 0.8, True); y += 1.8
    for s in ("J1  mains in: L, N (F1 = 1A 250V time-lag fuse 5x20 in the L path)",
              "J2  press-sense input: 230VAC present while the press contactor is energised",
              "J5  relay contact NO-COM (10A 250VAC) in series with the press contactor coil circuit",
              "J10-J13  output relays OUT1-OUT4 (K2-K5, contact NO / COM / NC, 2.5A): lamps, horn, contactor coils",
              "J7  12V sense input (+12V, GND)   J8  ZMPT101B 230V sensor module: 3V3 OUT GND",
              "J4  LM35: OUT GND +5V     J3  micro-USB STM32 (console / HID-bootloader update; powers the logic only)",
              "J14  micro-USB ESP8266: CH340G, auto-reset (Arduino IDE / esptool upload, serial monitor)",
              "J6  ribbon cable to PANEL",
              "J9  SWD: GND CLK DIO 3V3 RST (programming / debug)",
              ):
        bt(s, x, y, 0.7); y += 1.5
    y += 0.8
    bt("SWITCHES / BOOT", x, y, 0.8, True); y += 1.8
    for s in ("SW1 = ESP reset   SW2 = ESP flash (only needed if the auto-reset on J14 fails)",
              "SW3 = STM32 reset   SW4 = STM32 BOOT (hold BOOT, tap RESET: USB bootloader mode, upload on J3)",
              "STM32: USB J3 (HID bootloader), SWD (J9) or through the ESP (UART bootloader, ESP drives BOOT0 / NRST)",
              "K1..K5: T73 relay 5V coil (SRD-05VDC-SL-C, needs the NC pin); contacts COM / NO / NC on 3-pin terminals"):
        bt(s, x, y, 0.7); y += 1.5
    y += 0.8
    bt("PANEL CABLE J6 (26-way, pin n <-> pin n)", x, y, 0.8, True); y += 1.8
    names = design.HMI_PINS
    for r in range(13):
        a, b = 2 * r + 1, 2 * r + 2
        bt(f"{a:>2} {names[str(a)]:<10}", x, y, 0.7)
        bt(f"{b:>2} {names[str(b)]:<10}", x - 25.0, y, 0.7)
        y += 1.4
    y += 0.8
    bt("LEDS: green 3V3 ok / red RELAY ON (coil energised) / yellow WiFi-link / red mains on sense input", x, y, 0.7); y += 1.5
    bt("TEST POINTS: TP1 +5V  TP2 +3V3  TP3 GND  TP4 PRESS (opto output, low = press energised)", x, y, 0.7); y += 1.5
    bt("Firmware: counter/ (STM32)  gateway/ (ESP8266)  - see docs/", x, y, 0.7)
