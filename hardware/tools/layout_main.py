"""Board (v6, one board): outline, placement and silk-screen help (board-local mm, origin = top-left, y down).

Left part (x < ~68): mains - supply input + fuse + HLK module, press-sense input, press-contactor relay contacts.
Middle: ESP32-WROOM-32 (antenna at the top edge), 3.3 V regulator, call buttons, LEDs, buzzer, USB + CH340G, DS18B20 terminal.
Right: the 0.96in OLED module (top) and the RC522 NFC module (below it), both plugged into sockets and lying over the board.
5 mm creepage between every mains copper item and every low-voltage copper item is enforced by the router and a DRC rule.
"""
import design

W, H = 202.0, 104.0              # 150 x 104 mm for the controller part + a 52 mm strip on the right with the four output relays (v6.1)
OLD_W = 150.0

# output relay cells (K2..K5 = OUT1..OUT4): relay COM pad at (XR, CY[n]), contacts east, terminal at the right edge, driver parts west
XR = 170.0
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

# ESP32 module: origin = centre of the pad field; the module top (antenna end) is 16.5 mm above it -> at the board edge
ESP_X, ESP_Y = 96.0, 16.6
# antenna: no copper on any layer under the antenna and 15 mm beside it (Espressif layout guide; the footprint courtyard has the same T shape)
ANTENNA = (ESP_X - 24.0, 0.0, ESP_X + 24.0, 7.2)
# RC522 module (40 x 60 mm) lies over the board, header P1 at its top edge; its antenna coil is the lower ~35 mm: no copper under it
RC_X0, RC_Y0 = 109.0, 35.5
RFID_KEEPOUT = (RC_X0, RC_Y0 + 23.0, RC_X0 + 40.0, RC_Y0 + 61.0)
FIDUCIALS = ("FID1", "FID2", "FID3")

PLACE = {
    "MH1": (4.0, 4.0, 0), "MH2": (W - 4.0, 4.0, 0), "MH3": (4.0, H - 4.0, 0), "MH4": (W - 4.0, H - 4.0, 0),
    "MH5": (OLD_W + 0.5, 4.0, 0), "MH6": (OLD_W + 0.5, H - 4.0, 0),
    "FID1": (9.0, 3.5, 0), "FID2": (105.0, H - 3.0, 0), "FID3": (9.0, H - 3.5, 0),

    # ── mains in + fuse + HLK-5M05 (AC pads left, DC pads right) - unchanged from v5
    "J1": (6.0, 20.0, 270), "F1": (31.0, 8.0, 0), "RV1": (15.0, 28.0, 90), "PS1": (24.0, 24.0, 0),
    "C1": (66.0, 28.0, 90), "C2": (66.0, 38.0, 0), "TP1": (70.3, 4.0, 0), "TP3": (70.3, 8.5, 0),

    # ── alternative 5 V input (top edge, wire entry up), its PTC / TVS / Schottky; low voltage: 5 mm away from the fuse F1
    "J6": (57.5, 6.0, 180), "F2": (62.3, 3.6, 0), "D7": (64.5, 9.0, 0), "D8": (65.5, 13.5, 0),

    # ── press-sense input (mains)
    "J2": (6.0, 52.0, 270), "RV2": (15.0, 50.0, 90), "RS1": (30.0, 62.5, 0),
    "BR1": (38.0, 51.0, 180), "C5": (47.0, 47.0, 0), "D5": (47.0, 56.0, 0), "U2": (58.0, 52.0, 0), "R12": (50.6, 52.0, 270), "R1": (67.0, 55.0, 90),
    "TP4": (69.0, 61.0, 0),

    # ── relay: contacts = mains (COM at the east end of the body, NO / NC at the west end next to J5); driver parts east of it (LV)
    "K1": (40.0, 85.0, 180), "J5": (6.0, 79.0, 270),
    "Q2": (54.0, 92.0, 0), "R9": (59.0, 89.0, 0), "R13": (59.0, 93.0, 0), "D12": (55.0, 84.0, 0),

    # ── 3.3 V LDO (below the ESP32, next to the 5 V module output)
    "U3": (77.0, 34.0, 0), "C3": (72.0, 41.0, 0), "C4": (82.0, 41.5, 0), "R2": (72.0, 28.0, 0), "D6": (76.5, 28.0, 0), "TP2": (88.0, 30.0, 0),

    # ── ESP32 + its small parts (decoupling at the VDD pin, EN delay, IO0 pull-up)
    "U1": (ESP_X, ESP_Y, 0),
    "C16": (82.5, 10.0, 90), "C17": (79.0, 10.0, 90), "R51": (82.5, 17.0, 90), "C18": (79.0, 17.0, 90), "R53": (110.0, 24.0, 90),

    # ── call buttons, LEDs, buzzer (front side, between the ESP32 and the RC522 module)
    "SW1": (77.0, 50.0, 0), "SW2": (98.0, 50.0, 0), "R20": (72.0, 56.0, 0), "C7": (77.0, 56.0, 0), "R21": (98.0, 56.0, 0), "C8": (103.0, 56.0, 0),
    "D2": (85.5, 47.0, 0), "R44": (85.5, 50.0, 0), "D13": (89.5, 47.0, 0), "R43": (89.5, 50.0, 0),
    "BZ1": (76.0, 66.0, 0), "Q1": (96.0, 63.0, 0), "R3": (96.0, 67.0, 0), "R4": (100.5, 67.0, 0), "D3": (98.0, 71.0, 0),

    # ── USB + CH340G (bottom edge), DS18B20 terminal
    "J3": (88.0, 98.0, 0), "D14": (100.0, 96.0, 90), "U5": (88.0, 83.0, 0), "C22": (94.5, 77.0, 90), "R97": (83.0, 75.5, 0),
    "Y2": (79.0, 84.5, 90), "C25": (73.5, 80.5, 0), "C26": (73.5, 88.5, 0),
    "Q7": (98.5, 81.0, 0), "Q8": (98.5, 87.0, 0), "R94": (103.5, 81.0, 90), "R95": (103.5, 87.0, 90),
    "J4": (64.0, 98.0, 0), "R10": (72.0, 75.5, 0), "R11": (77.0, 75.5, 0),

    # ── OLED (P2) and RC522 (P1) sockets + their resistors
    "P2": (126.0, 4.0, 90), "R40": (113.0, 30.0, 0), "R41": (113.0, 33.0, 0),
    "P1": (RC_X0 + 20.0 - 8.89, RC_Y0 + 2.5, 90), "R33": (106.0, 40.0, 90),

    # ── voltage inputs: 12/24 V and AUX terminals + dividers below the relay strip
    "J7": (157.0, 98.0, 0), "J9": (170.0, 98.0, 0),
    "R90": (180.0, 96.0, 0), "R91": (184.5, 96.0, 0), "C20": (189.5, 96.0, 0),
    "R98": (180.0, 99.5, 0), "R99": (184.5, 99.5, 0), "C23": (189.5, 99.5, 0),
}
PLACE.update(cell_places())


def coil_stubs(pcb):
    """Hand-placed first segments of the relay-coil nets (+5V and the driver collector).  The coil pads of a T73 relay are only about 6 mm
    from the COM pad (the relay maker certifies that distance, so it is exempt inside the relay's courtyard); the stubs leave the 5 mm
    creepage ring of COM straight away from it before the autorouter takes over."""
    ax, ay = pcb.pad_pos("K1", "2")[0]
    bx, by = pcb.pad_pos("K1", "5")[0]
    north = -1.0 if ay < by else 1.0
    out = [(0, [(ax, ay + north * 1.0), (ax, ay + north * 7.0)], 0.5, "+5V"),
           (0, [(bx, by - north * 1.0), (bx, by - north * 7.0)], 0.5, "RLY_N")]
    # K2-K5: a short stub and a diagonal that ends 8.5 mm from the relay centre line on the driver side (west)
    for n in range(1, 5):
        ref = f"K{n + 1}"
        cx, cy = pcb.pos(ref)                                     # COM pad = footprint origin
        px, py = pcb.pad_pos(ref, "2")[0]                         # +5V coil pad
        qx, qy = pcb.pad_pos(ref, "5")[0]                         # driver-side coil pad
        sgn = 1.0 if py > cy else -1.0
        out.append((0, [(px, py + sgn * 0.8), (px, py + sgn * 1.9), (px - 1.4, cy + sgn * 8.5), (px - 4.5, cy + sgn * 8.5)], 0.5, "+5V"))
        out.append((0, [(qx, qy - sgn * 0.8), (qx, qy - sgn * 1.9), (qx - 1.4, cy - sgn * 8.5), (qx - 4.5, cy - sgn * 8.5)], 0.5, f"OUT{n}_N"))
    return out


def extras(pcb):
    """Copper-free ESP antenna area, copper-free area under the RC522 antenna coil, pour-free halo around each fiducial."""
    pcb.rect_keepout(*ANTENNA, name="ESP antenna keepout")
    pcb.rect_keepout(*RFID_KEEPOUT, name="RC522 antenna keepout")
    for ref in FIDUCIALS:
        x, y = pcb.pos(ref)
        pcb.rect_keepout(x - 2.0, y - 2.0, x + 2.0, y + 2.0, name="fiducial halo " + ref, tracks=False, vias=False)


def extra_zones(pcb):
    """+3V3 copper on the top layer around the tab of the AMS1117 (the tab IS the output pin and the heat path of the chip)."""
    import pcbnew
    x, y = pcb.pos("U3")
    z = pcb.add_zone("+3V3", "F.Cu", x + 1.0, y - 4.0, x + 9.0, y + 4.0, clearance=0.3, min_w=0.25, priority=2)
    z.SetZoneName("3V3 heat sink")
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)


def build_pcb(PCBCls):
    b = design.main_board()
    pcb = PCBCls(b)
    pcb.add_parts(PLACE)
    pcb.outline(0, 0, W, H, r=3.0)
    return pcb


REF_OFFSET = {"TP1": (0.0, 2.0), "TP2": (0.0, 2.0), "TP3": (0.0, 2.0), "TP4": (0.0, 2.0), "J3": (0.0, 3.2)}
HIDE_REF = ("J6", "F2", "D7", "D8", "R12", "TP1", "TP3", "RV1", "RV2", "PS1", "J4", "P1", "P2", "D14", "C20", "C23", "R90", "R91", "R98", "R99", "J7", "J9", "J10", "J11", "J12", "J13") + tuple(
    ref for n in range(1, 5) for ref in (f"D{19 + n}", f"Q{n + 2}", f"R{59 + n}", f"R{69 + n}", f"D{29 + n}", f"R{79 + n}")) + ("U3", "R1", "C1", "C17", "C18", "R51", "R20", "R21", "C7", "C8", "R44", "R43", "R94", "R95", "C25", "C26", "R97", "SW1", "SW2", "D2", "D13", "D6", "R40", "R41")


def silk(pcb):
    import pcbnew
    pcb.ref_offsets(REF_OFFSET)
    pcb.hide_refs(HIDE_REF)
    pcb.edit_fp_rect("U2", 2.4, 2.4, dot_center=(-1.7, -1.7))   # narrower body outline: the old one ran over the pads of this wide SOP-4
    t = pcb.text
    lp = pcb.label_pads
    # ── zones and warnings
    pcb.poly_outline([(1.5, 1.5), (47.0, 1.5), (47.0, 14.5), (66.5, 14.5), (66.5, 66.5), (1.5, 66.5)], dash=1.5)   # the 5 V input corner is LV
    pcb.rect_outline(1.5, 69.0, 46.5, 102.5, dash=1.5)
    t("MAINS ZONE - 230V", 3.0, 67.8, 0.8, bold=True, justify="left")
    t("RELAY CONTACT (MAINS)", 13.0, 101.2, 0.7, bold=True, justify="left")
    # ── mains terminals
    lp("J1", {"1": "L", "2": "N"}, dx=6.2, size=1.0, bold=True, justify="left")
    t("MAINS IN 230VAC", 2.5, 12.3, 0.7, bold=True, justify="left")
    t("F1  T1A 250V", 31.0, 8.0, 0.8, bold=True)
    lp("J2", {"1": "SNS A", "2": "SNS B"}, dx=6.2, size=0.8, bold=True, justify="left")
    t("PRESS SENSE 230V", 2.5, 43.0, 0.7, bold=True, justify="left")
    t("-", 43.4, 50.6, 0.9, bold=True)                          # BR1 MB10S: the + lead of the part (bevelled corner) goes on pad 1
    t("+", 43.4, 52.7, 0.9, bold=True)
    lp("J5", {"1": "NO", "2": "COM", "3": "NC"}, dx=6.2, size=0.8, bold=True, justify="left")
    t("RELAY OUT 10A", 2.5, 72.8, 0.7, bold=True, justify="left")
    # ── low voltage
    lp("J4", {"1": "3V3", "2": "DATA", "3": "GND"}, dy=-3.6, size=0.8, bold=True)
    t("DS18B20", 62.0, 91.5, 0.8, bold=True, justify="left")
    t("USB / console", 82.0, 92.0, 0.7, bold=True, justify="left")
    t("CALL SUPERVISOR", 73.5, 45.9, 0.7, bold=True)
    t("CALL NET", 98.0, 45.9, 0.7, bold=True)
    t("ALARM", 85.5, 52.0, 0.7)
    t("ON", 89.5, 52.0, 0.7)
    t("PWR", 76.5, 26.4, 0.7)
    for ref, s in (("TP1", "5V"), ("TP3", "GND"), ("TP2", "3V3"), ("TP4", "PRESS")):
        lp(ref, {"1": s}, dy=-2.0, size=0.7, bold=True)
    lp("P2", {"1": "GND", "2": "VCC", "3": "SCL", "4": "SDA"}, dy=1.8, size=0.7, rot=90, justify="right", bold=True)
    t("OLED 0.96in I2C", 129.8, 12.5, 0.8, bold=True)
    lp("P1", {"1": "SDA", "2": "SCK", "3": "MOSI", "4": "MISO", "5": "IRQ", "6": "GND", "7": "RST", "8": "3V3"}, dy=-2.4, size=0.7, rot=90, justify="left")
    t("RC522 NFC MODULE", RC_X0 + 20.0, RC_Y0 + 12.0, 0.9, bold=True)
    t("(antenna area: no copper)", RC_X0 + 20.0, RC_Y0 + 14.0, 0.7)
    t("PRESS COUNTER v6.2", RC_X0 + 20.0, RC_Y0 + 18.0, 1.0, bold=True)
    t("ESP32 - 230VAC - relay - NFC - OLED", RC_X0 + 20.0, RC_Y0 + 20.0, 0.7)
    # ── output relays + voltage inputs
    for n in range(1, 5):
        cy = CY[n - 1]
        lp(f"J{9 + n}", {"1": "NO", "2": "COM", "3": "NC"}, dx=-4.4, size=0.8, bold=True, justify="right", layer=pcbnew.B_SilkS)
        t(f"OUT{n} (K{n + 1})", XR + 24.9, cy - 8.8, 0.8, bold=True)
    pcb.rect_outline(XR - 18.0, 1.5, W - 1.5, 91.5, dash=1.5)
    t("OUTPUT RELAYS 230V / 2.5A", XR + 6.0, 3.0, 0.7, bold=True)
    t("J7 12/24V  J9 AUX >", 148.4, 101.3, 0.7, bold=True, justify="right")      # beside the terminals (a label under a terminal body is hidden); + is the left screw of each
    # ── alternative 5 V input
    lp("J6", {"1": "+5V", "2": "GND"}, dy=3.4, size=0.7, bold=True)
    t("5V DC IN", 54.0, 12.8, 0.7, bold=True)
    back_sheet(pcb)


def back_sheet(pcb):
    """Help sheet on the back (bottom silk, mirrored)."""
    import pcbnew
    L = pcbnew.B_SilkS
    tht = pcb.tht_boxes()
    state = {"dy": 0.0}

    def bt(s, x, y, size=0.8, bold=False):
        width = len(s) * size * 0.8
        yy = y + state["dy"]
        for _ in range(60):
            if not any(b[2] > x - width and b[0] < x and b[3] > yy - size * 0.6 and b[1] < yy + size * 0.6 for b in tht):
                break
            yy += 0.5
        state["dy"] = yy - y
        pcb.text(s, x, yy, size, layer=L, mirror=True, bold=bold, justify="right")     # pcbgen swaps left / right on the bottom layer
    x = 146.0
    y = 40.0
    bt("PRESS COUNTER v6.2 - HELP (back side)", x, y, 1.0, True); y += 2.4
    bt("DANGER: the left part carries 230VAC. Unplug mains before touching.", x, y, 0.7, True); y += 2.0
    for s in ("J1  mains in L, N (F1 = T1A 250V 5x20)      J2  press sense: 230VAC across the contactor coil",
              "J5  relay NO / COM / NC, 10A 250VAC, in the press-contactor circuit",
              "J4  DS18B20 probe: 3V3 (red)  DATA (yellow)  GND (black)",
              "J10-J13  output relays OUT1-OUT4 (K2-K5): NO / COM / NC, 2.5A - MQTT relay/2..5",
              "J7  12/24V sense (0-30V)   J9  AUX input (0-30V)",
              "J6  5V DC in (regulated 5V >= 1.5A adapter) instead of mains on J1; protected by F2 (PTC), D7, D8",
              "J3  micro-USB: program like an Arduino (ESP32 Dev Module) + console; powers the logic only",
              "P1  RC522 module   P2  0.96in OLED (check GND/VCC order!)",
              "LEDs: green PWR / green ON (relay) / red ALARM / red = mains on the sense input",
              "TP1 +5V  TP2 +3V3  TP3 GND  TP4 PRESS (low = press energised)",
              "Firmware: firmware/press_counter (see docs/FIRMWARE.md)"):
        bt(s, x, y, 0.7); y += 1.6
