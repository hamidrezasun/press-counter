"""PANEL board: outline, placement and silk-screen help (board-local mm, origin = top-left, y down).

Everything the operator touches or looks at is on the BOTTOM face: the 7 buttons (D-pad cross + two call buttons), the display socket J2
(2.4in ST7789 module), the RC522 socket P1, the two LEDs, the buzzer and all SMD parts (single-sided SMD assembly).  The only part on the TOP
face is the 26-pin ribbon header J1.  Parts flagged True in PLACE are flipped (their THT pins are soldered from the top).
Coordinates are board coordinates seen from the top; the operator sees the board from below, i.e. mirrored left-right (LEFT / RIGHT swapped in x).
"""
import design

W, H = 150.0, 96.0

PLACE = {
    "MH1": (4.0, 4.0, 0), "MH2": (W - 4.0, 4.0, 0), "MH3": (4.0, H - 4.0, 0), "MH4": (W - 4.0, H - 4.0, 0),
    "FID1": (20.0, 4.0, 0, True), "FID2": (W - 11.0, H - 4.0, 0, True), "FID3": (11.0, H - 4.0, 0, True),
    # ribbon header (pin 1 at the top) - the only part on the top face
    "J1": (128.0, 52.0, 0),
    # modules
    "P1": (78.0, 12.0, 0, True),
    "J2": (9.0, 16.0, 0, True),
    "R5": (8.0, 55.0, 0, True), "SJ1": (8.0, 59.0, 0, True),
    # D-pad (cross) with OK in the middle.  The board is seen from the BOTTOM: LEFT / RIGHT are mirrored in board x
    "SW1": (32.0, 64.0, 0, True), "SW2": (32.0, 88.0, 0, True), "SW3": (44.5, 76.0, 0, True), "SW4": (19.5, 76.0, 0, True), "SW5": (32.0, 76.0, 0, True),
    "C1": (41.0, 64.0, 0, True), "C2": (41.0, 88.0, 0, True), "C3": (44.5, 82.0, 0, True), "C4": (19.5, 82.0, 0, True), "C5": (32.0, 82.0, 0, True),
    # call buttons
    "SW6": (72.0, 66.0, 0, True), "SW7": (72.0, 84.0, 0, True), "C6": (80.0, 66.0, 0, True), "C7": (80.0, 84.0, 0, True),
    # LEDs
    "D1": (96.0, 62.0, 0, True), "R1": (102.0, 62.0, 0, True), "D2": (96.0, 70.0, 0, True), "R2": (102.0, 70.0, 0, True),
    # buzzer + driver
    "BZ1": (104.6, 79.0, 0, True), "Q1": (99.0, 90.0, 0, True), "R3": (88.0, 90.0, 0, True), "R4": (93.0, 90.0, 0, True), "D3": (107.0, 90.0, 0, True),
}

# reference designator text: offset (dx, dy) from the footprint centre (default = above the part)
REF_OFFSET = {"SW1": (-6.5, 0.0), "SW2": (-6.5, 0.0), "SW3": (0.0, -4.8), "SW4": (0.0, -4.8), "SW5": (0.0, -4.8),
              "SW6": (-6.5, 0.0), "SW7": (-6.5, 0.0), "C1": (0.0, 2.2), "C2": (0.0, 2.2), "C6": (0.0, 2.2), "C7": (0.0, 2.2),
              "C3": (0.0, 2.2), "C4": (0.0, 2.2), "C5": (0.0, 2.2)}

# RC522 antenna area: no copper (ground planes detune the coil)
ANTENNA = (84.0, 3.5, 138.0, 44.0)


def build_pcb(PCBCls):
    b = design.panel_board()
    pcb = PCBCls(b)
    pcb.add_parts(PLACE)
    pcb.ref_offsets(REF_OFFSET)
    pcb.outline(0, 0, W, H, r=3.0)
    return pcb


def extras(pcb):
    """Copper-free area under the RC522 antenna (rule area: no tracks / vias / pours)."""
    pcb.rect_keepout(*ANTENNA, name="RC522 antenna keepout")
    for ref in ("FID1", "FID2", "FID3"):
        x, y = pcb.pos(ref)
        pcb.rect_keepout(x - 2.0, y - 2.0, x + 2.0, y + 2.0, name="fiducial halo " + ref, tracks=False, vias=False)


def arrow(pcb, x, y, d, s=1.6):
    import pcbnew
    pts = {"up": [(x, y - s), (x - s, y + s), (x + s, y + s)], "down": [(x, y + s), (x - s, y - s), (x + s, y - s)],
           "left": [(x - s, y), (x + s, y - s), (x + s, y + s)], "right": [(x + s, y), (x - s, y - s), (x - s, y + s)]}[d]
    for a, b in zip(pts, pts[1:] + pts[:1]):
        pcb.line(a, b, layer=pcbnew.B_SilkS, w=0.25)


def silk(pcb):
    import pcbnew
    B = pcbnew.B_SilkS
    t = pcb.text
    pcb.hide_refs(("P1", "J2", "C5"))               # P1 / J2 are printed as module labels; C5 has no room
    # ── TOP face: only the ribbon header ───────────────────────────────────────────────────────────────────
    t("RIBBON CABLE TO MAIN (J6)", 76.0, 55.0, 0.8, bold=True, justify="left")
    t("pin 1 = +5V (arrow mark)", 76.0, 57.2, 0.7, justify="left")
    t("PANEL: all parts are on the BOTTOM face", 76.0, 46.5, 0.8, bold=True, justify="left")
    names = design.HMI_PINS
    odd = {n: f"{v} {n}" for n, v in names.items() if int(n) % 2 == 1}
    even = {n: f"{n} {v}" for n, v in names.items() if int(n) % 2 == 0}
    pcb.label_pads("J1", odd, dx=-5.6, dy=0, size=0.7, justify="right")
    pcb.label_pads("J1", even, dx=8.1, dy=0, size=0.7, justify="left")
    # ── BOTTOM face (mirrored silk; justification is given as seen from the board front, pcbgen mirrors it) ────────────────
    t("PRESS COUNTER v5  -  FRONT PANEL", 76.0, 46.5, 1.1, bold=True, justify="left", layer=B)
    pcb.rect_outline(5.0, 4.0, 68.0, 52.0, layer=B, dash=1.2)
    t("DISPLAY MODULE (optional): 2.4in ST7789 240x320, plug into J2", 25.0, 3.0, 0.8, justify="left", layer=B)
    t("pin order GND VCC SCL SDA RES DC CS BLK - check your module", 7.0, 51.0, 0.7, justify="left", layer=B)
    pcb.rect_outline(76.0, 3.0, 138.0, 43.5, layer=B, dash=1.2)
    t("RC522 RFID MODULE", 80.0, 1.4, 0.8, justify="left", layer=B)
    t("no copper under the antenna", 100.0, 40.0, 0.7, justify="left", layer=B)
    a = {"1": "GND", "2": "3V3", "3": "SCL", "4": "SDA", "5": "RES", "6": "DC", "7": "CS", "8": "BLK"}
    pcb.label_pads("J2", a, dx=-1.7, dy=0, size=0.7, justify="right", layer=B)
    t("J2", 9.0, 13.5, 0.8, bold=True, layer=B)
    pcb.label_pads("P1", {"1": "SDA", "2": "SCK", "3": "MOSI", "4": "MISO", "5": "IRQ", "6": "GND", "7": "RST", "8": "3V3"},
                   dx=1.7, dy=0, size=0.7, justify="left", layer=B)
    t("P1", 78.0, 9.5, 0.8, bold=True, layer=B)
    t("R5/SJ1: display backlight", 12.5, 55.0, 0.7, justify="left", layer=B)
    t("SJ1: cut 1-2, bridge 2-3", 12.5, 58.0, 0.7, justify="left", layer=B)
    t("= back-light always on", 12.5, 59.6, 0.7, justify="left", layer=B)
    # D-pad (the operator looks at the bottom face: x is mirrored, LEFT sits at the larger board x)
    arrow(pcb, 32.0, 58.0, "up")
    arrow(pcb, 32.0, 93.5, "down")
    arrow(pcb, 52.0, 76.0, "right")
    arrow(pcb, 12.0, 76.0, "left")
    t("UP", 36.5, 58.0, 0.8, bold=True, justify="left", layer=B)
    t("DOWN", 36.5, 93.5, 0.8, bold=True, justify="left", layer=B)
    t("LEFT", 48.0, 71.5, 0.8, bold=True, justify="left", layer=B)
    t("RIGHT", 8.0, 71.0, 0.8, bold=True, justify="left", layer=B)
    t("OK", 32.0, 69.4, 0.8, bold=True, layer=B)
    t("CALL SUPERVISOR", 72.0, 59.0, 0.8, bold=True, layer=B)
    t("CALL NET / MAINT.", 72.0, 77.0, 0.8, bold=True, layer=B)
    t("hold 1.5 s = cancel", 72.0, 60.8, 0.7, layer=B)
    t("hold 1.5 s = cancel", 72.0, 78.8, 0.7, layer=B)
    t("RUN (machine enabled)", 90.0, 58.5, 0.7, justify="left", layer=B)
    t("ALERT (error / call)", 90.0, 66.5, 0.7, justify="left", layer=B)
    t("BUZZER", 112.0, 79.0, 0.8, bold=True, justify="left", layer=B)
    t("Q1 drives the buzzer from +5V", 86.0, 94.0, 0.7, justify="left", layer=B)
