"""Single source of truth for the press-counter hardware (v6): parts and pin->net assignments of the board.

The schematic, the PCB and the BOM are all generated from this file, so they cannot drift apart.

Architecture (v6, one board, one chip):
  ONE ESP32-WROOM-32 module does everything: press counting (230 VAC opto input), press-contactor relay, RC522 NFC reader,
  0.96in I2C OLED, DS18B20 temperature probe, two call buttons, buzzer, alert LED, WiFi / MQTT / web page.
  v6.1: the free ESP32 pins drive four output relays (COM / NO / NC) and read two DC inputs (0-30 V: 12 V / 24 V sense, AUX).
  v6.2: ZMPT101B header removed; a 5 V screw terminal (PTC + TVS + Schottky) lets a 5 V adapter replace the mains supply.
  Mains input + T1A fuse + HLK-5M05 5 V module, AMS1117 3.3 V, one micro-USB with CH340G and auto-reset (programmed like an Arduino board).
  Mains is only present in the left part of the board (supply input, press-sense input, relay contacts);
  5 mm creepage to every low-voltage net is enforced by the router and by a DRC rule.
"""
from dataclasses import dataclass, field

import kilib

# ── footprints ──────────────────────────────────────────────────────────────────
R0805 = "Resistor_SMD:R_0805_2012Metric"
R1206 = "Resistor_SMD:R_1206_3216Metric"
RAX1W = "Resistor_THT:R_Axial_DIN0411_L9.9mm_D3.6mm_P15.24mm_Horizontal"     # 1 W through-hole resistor (mains sense)
C0805 = "Capacitor_SMD:C_0805_2012Metric"
C1206 = "Capacitor_SMD:C_1206_3216Metric"
CE54_5 = "Capacitor_SMD:CP_Elec_5x5.4"
CE54_63 = "Capacitor_SMD:CP_Elec_6.3x5.4"
LED0805 = "LED_SMD:LED_0805_2012Metric"
DSMA = "Diode_SMD:D_SMA"
DSMB = "Diode_SMD:D_SMB"
PTC_RAD = "Capacitor_THT:C_Disc_D7.5mm_W5.0mm_P5.00mm"     # radial PTC (RLD60P110XF: 7.4 x 3.1 mm body, 5.1 mm lead pitch)
SOT23 = "Package_TO_SOT_SMD:SOT-23"
SOT223 = "Package_TO_SOT_SMD:SOT-223-3_TabPin2"
SOIC8 = "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm"
SOIC16 = "Package_SO:SOIC-16_3.9x9.9mm_P1.27mm"
LQFP48 = "Package_QFP:LQFP-48_7x7mm_P0.5mm"
XTAL = "Crystal:Crystal_SMD_HC49-SD"
BRIDGE = "Package_TO_SOT_SMD:TO-269AA"
OPTO = "press_counter:SOP4_Wide_Universal"
USBM = "Connector_USB:USB_Micro-B_Wuerth_629105150521"
SWSMD = "Button_Switch_SMD:SW_SPST_B3S-1000"
RELAY = "Relay_THT:Relay_SPDT_SANYOU_SRD_Series_Form_C"     # T73 / SRD-05VDC-SL-C: 5 pads, a 4-pin SPST (-SL-A) part fits too
FUSE = "press_counter:FuseHolder_5x20_PCB_P22.5"
FID = "Fiducial:Fiducial_1mm_Mask2mm"
VARISTOR = "Varistor:RV_Disc_D7mm_W3.4mm_P5mm"
TB2 = "TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal"
TB3 = "TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-3-5.08_1x03_P5.08mm_Horizontal"
HDR1 = "Connector_PinHeader_2.54mm:PinHeader_1x01_P2.54mm_Vertical"
HDR2 = "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical"
HDR3 = "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical"
HDR4 = "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical"
HDR5 = "Connector_PinHeader_2.54mm:PinHeader_1x05_P2.54mm_Vertical"
SOCK8 = "Connector_PinSocket_2.54mm:PinSocket_1x08_P2.54mm_Vertical"
SOCK4 = "Connector_PinSocket_2.54mm:PinSocket_1x04_P2.54mm_Vertical"
IDC26 = "Connector_IDC:IDC-Header_2x13_P2.54mm_Vertical"
SJ3 = "Jumper:SolderJumper-3_P1.3mm_Bridged12_Pad1.0x1.5mm"
TP = "TestPoint:TestPoint_Pad_D1.5mm"
BUZZ12 = "Buzzer_Beeper:Buzzer_12x9.5RM7.6"
MH = "MountingHole:MountingHole_3.2mm_M3"


@dataclass
class Part:
    ref: str
    sym: str                 # "Lib:Name"
    value: str
    fp: str                  # "Lib:Name"
    pins: dict = field(default_factory=dict)   # pin number (str) -> net name or None (= intentionally unconnected)
    group: str = ""
    mpn: str = ""
    mains: bool = False
    dnp: bool = False


@dataclass
class Board:
    name: str
    title: str
    parts: list = field(default_factory=list)
    mains_nets: list = field(default_factory=list)       # AC, 230 V between them
    mains_dc_nets: list = field(default_factory=list)    # rectified, referenced to mains
    power_nets: list = field(default_factory=list)
    wide: dict = field(default_factory=dict)             # net -> track width override (mm)
    creepage_exempt: list = field(default_factory=list)  # footprints whose own pin pitch is certified (relay): 5 mm rule not applied inside their courtyard

    def add(self, *a, **k):
        p = Part(*a, **k)
        assert all(q.ref != p.ref for q in self.parts), f"duplicate ref {p.ref}"
        self.parts.append(p)
        return p

    def nets(self):
        return sorted({n for p in self.parts for n in p.pins.values() if n})

    def part(self, ref):
        return next(p for p in self.parts if p.ref == ref)


def by_name(sym, names, default=None):
    """Pin-number dict for a symbol from {pin name: net}.  Several pins may share a name (VSS, VDD): all get the net."""
    lib, name = sym.split(":")
    s = kilib.flat_symbol(lib, name)
    out = {}
    seen = set()
    for u in range(1, kilib.n_units(s) + 1):
        for p in kilib.unit_pins(s, u):
            if p["num"] in out:
                continue
            out[p["num"]] = names.get(p["name"], default)
            seen.add(p["name"])
    missing = set(names) - seen
    assert not missing, f"{sym}: unknown pin names {missing}"
    return out


# ── groups (schematic sections) ─────────────────────────────────────────────────
G_PSU = "MAINS INPUT & 5V SUPPLY"
G_LDO = "3.3V REGULATOR"
G_RELAY = "PRESS CONTACTOR RELAY (CONNECT / DISCONNECT)"
G_SENSE = "PRESS-SENSE INPUT (230VAC -> OPTOCOUPLER)"
G_ESP = "CONTROLLER (ESP32-WROOM-32: COUNTER + WIFI)"
G_USB = "USB PROGRAMMING (CH340G, AUTO-RESET)"
G_UI = "CALL BUTTONS, ALERT LED, BUZZER"
G_RFID = "NFC READER (RC522 MODULE)"
G_LCD = "DISPLAY (0.96in I2C OLED MODULE)"
G_TEMP = "TEMPERATURE PROBE (DS18B20)"
G_MECH = "MECHANICAL / TEST"
G_OUT = "OUTPUT RELAYS OUT1-OUT4 (COM / NO / NC)"
G_ADC = "VOLTAGE INPUTS: 12/24 V SENSE, AUX (0-30 V)"
G_EXT = "ALTERNATIVE 5 V INPUT (5 V ADAPTER INSTEAD OF MAINS)"

MAINS = ["L", "LF", "N", "SENSE_A", "SENSE_B", "BR_A", "RLY_COM", "RLY_NO", "RLY_NC"] + [f"OUT{n}_{c}" for n in range(1, 5) for c in ("COM", "NO", "NC")]
MAINS_DC = ["BR_P", "BR_N", "LED_K"]
POWER = ["+5V", "+3V3", "VBUS", "EXT5V", "EXT5V_F"]

# ESP32 pins (module pin names).  Boot straps: IO0 (pull-up + auto-reset), IO2 / IO12 left open (IO12 high at boot would select 1.8 V
# flash), IO15 = alarm LED (either level is fine at boot: it only selects whether the ROM prints its boot log), IO5 = RC522 chip select (pull-up).
# IO14 / IO15 toggle during boot: they carry the buzzer and the LED (a short chirp / flicker is harmless), never a relay.
# The relays sit on IO26 / IO27 / IO16 / IO25 / IO13: none of them is a strap or is driven by the ROM while booting (they stay inputs
# until the firmware sets them LOW), and each relay transistor has a 10k base pull-down, so no relay can click at power-up,
# reset, brown-out, watchdog restart or while the board is programmed.  Do not fit a WROVER module (it uses IO16 / IO17 for its PSRAM).
# Analog inputs only on ADC1 pins (ADC2 is blocked while WiFi runs): IO36 12/24 V sense, IO35 AUX.  IO34 = press input.  IO39 is free.
# UART0 (IO1 / IO3) goes to the CH340G.
ESP_PINS = {
    "GND": "GND", "VDD": "+3V3", "EN": "EN",
    "IO34": "PRESS", "IO26": "RELAY_CTRL", "IO14": "BUZZ", "IO15": "LED_ALERT",
    "IO27": "OUT1_CTRL", "IO16": "OUT2_CTRL", "IO25": "OUT3_CTRL", "IO13": "OUT4_CTRL",
    "SENSOR_VP": "V12_ADC", "IO35": "AUX_ADC",
    "IO32": "BTN_SUP", "IO33": "BTN_NET",
    "IO18": "SCK", "IO19": "MISO", "IO23": "MOSI", "IO5": "RC_SS", "IO17": "RC_RST",
    "IO21": "SDA", "IO22": "SCL", "IO4": "TEMP_IO",
    "IO0": "BOOT", "RXD0/IO3": "RXD", "TXD0/IO1": "TXD",
}


def main_board():
    b = Board("press_counter", "Hydraulic press counter v6 - one board (ESP32, 230VAC opto input, relay, NFC, OLED, DS18B20)",
              mains_nets=MAINS, mains_dc_nets=MAINS_DC, power_nets=POWER,
              wide={"RLY_COM": 1.2, "RLY_NO": 1.2, "RLY_NC": 1.2, "L": 1.0, "LF": 1.0, "N": 1.0, "+3V3": 0.4, "+5V": 0.5, "VBUS": 0.4,
                    **{f"OUT{n}_{c}": 1.2 for n in range(1, 5) for c in ("COM", "NO", "NC")}},
              creepage_exempt=["K1"] + [f"K{n}" for n in range(2, 6)])
    A = b.add

    # ── mains in, fuse, MOV, 5 V module ─────────────────────────────────────────
    A("J1", "Connector_Generic:Conn_01x02", "MAINS 230VAC", TB2, {"1": "L", "2": "N"}, G_PSU, mains=True,
      mpn="Terminal block 2P 5.08mm (KF128-5.08)")
    A("F1", "Device:Fuse", "1A 250V T", FUSE, {"1": "L", "2": "LF"}, G_PSU, mains=True,
      mpn="Fuse holder 5x20 PCB (BLX-A type, 22.5mm) + glass fuse 5x20 1A 250V time-lag")
    A("RV1", "Device:Varistor", "7D471K", VARISTOR, {"1": "LF", "2": "N"}, G_PSU, mpn="Varistor 07D471K", mains=True)
    A("PS1", "Converter_ACDC:HLK-5M05", "HLK-5M05", "Converter_ACDC:Converter_ACDC_Hi-Link_HLK-5Mxx",
      {"1": "LF", "2": "N", "3": "GND", "4": "+5V"}, G_PSU, mpn="Hi-Link HLK-5M05 (5V 1A)", mains=True)
    A("C1", "Device:C_Polarized", "100uF 16V", CE54_63, {"1": "+5V", "2": "GND"}, G_PSU, mpn="SMD electrolytic 100uF 16V 6.3x5.4")
    A("C2", "Device:C", "100nF", C1206, {"1": "+5V", "2": "GND"}, G_PSU, mpn="100nF 50V X7R 1206")

    # ── alternative 5 V input: a regulated 5 V / >= 1.5 A adapter on J6 instead of mains on J1 (both at once does no harm: the diode keeps
    #    the HLK module from feeding the adapter).  F2 + D7 = protection against a wrong adapter: reverse polarity or 9-48 V makes D7
    #    conduct and F2 trip (it resets after the fault is removed); D8 blocks back-feeding.  +5V is then about 4.6 V: enough for the relays
    #    (pull-in 3.75 V) and the AMS1117. ──
    A("J6", "Connector_Generic:Conn_01x02", "5V DC IN: +5V, GND", TB2, {"1": "EXT5V", "2": "GND"}, G_EXT, mpn="Terminal block 2P 5.08mm (KF128-5.08)")
    A("F2", "Device:Polyfuse", "1.1A 60V", PTC_RAD, {"1": "EXT5V", "2": "EXT5V_F"}, G_EXT,
      mpn="Resettable fuse PPTC RLD60P110XF radial (hold 1.1 A, trip 2.2 A, 60 V)")
    A("D7", "Device:D_Zener", "SMBJ6.5A", DSMB, {"1": "EXT5V_F", "2": "GND"}, G_EXT, mpn="TVS SMBJ6.5A unidirectional SMB (600 W)")
    A("D8", "Device:D_Schottky", "SS34", DSMA, {"2": "EXT5V_F", "1": "+5V"}, G_EXT, mpn="SS34 Schottky 3A 40V SMA")

    # ── 3.3 V (ESP32 WiFi peaks about 0.5 A: AMS1117 1 A + 47 uF + 10 uF at the module) ─────────────────────────────
    A("U3", "Regulator_Linear:AMS1117-3.3", "AMS1117-3.3", SOT223, {"1": "GND", "2": "+3V3", "3": "+5V"}, G_LDO, mpn="AMS1117-3.3 SOT-223")
    A("C3", "Device:C", "10uF", C1206, {"1": "+5V", "2": "GND"}, G_LDO, mpn="10uF 50V X7R 1206")
    A("C4", "Device:C_Polarized", "47uF 16V", CE54_63, {"1": "+3V3", "2": "GND"}, G_LDO, mpn="SMD electrolytic 47uF 16V 6.3x5.4")
    A("R2", "Device:R", "1k", R0805, {"1": "+3V3", "2": "PWR_LED_A"}, G_LDO, mpn="1k 0805")
    A("D6", "Device:LED", "GREEN power", LED0805, {"2": "PWR_LED_A", "1": "GND"}, G_LDO, mpn="LED 0805 green")

    # ── press-contactor relay: 5 V coil T73 (SRD-05VDC-SL-C, SPDT); COM / NO / NC on a 3-pin terminal ─────────────
    A("K1", "Relay:SANYOU_SRD_Form_C", "SRD-05VDC-SL-C", RELAY, {"1": "RLY_COM", "2": "+5V", "3": "RLY_NO", "4": "RLY_NC", "5": "RLY_N"},
      G_RELAY, mpn="Relay T73 SRD-05VDC-SL-C (SPDT): 5V coil 70R, 10A 250VAC")
    A("Q2", "Transistor_BJT:SS8050", "SS8050", SOT23, {"1": "RLY_B", "2": "GND", "3": "RLY_N"}, G_RELAY, mpn="SS8050 SOT-23")
    A("R9", "Device:R", "1k", R0805, {"1": "RELAY_CTRL", "2": "RLY_B"}, G_RELAY, mpn="1k 0805")
    A("R13", "Device:R", "10k", R0805, {"1": "RLY_B", "2": "GND"}, G_RELAY, mpn="10k 0805")       # relay OFF while the ESP32 boots
    A("D12", "Device:D", "M7", DSMA, {"1": "+5V", "2": "RLY_N"}, G_RELAY, mpn="1N4007 SMD (M7)")
    A("R43", "Device:R", "1k", R0805, {"1": "+5V", "2": "RLY_LED_A"}, G_RELAY, mpn="1k 0805")
    A("D13", "Device:LED", "GREEN machine on", LED0805, {"2": "RLY_LED_A", "1": "RLY_N"}, G_RELAY, mpn="LED 0805 green")
    A("J5", "Connector_Generic:Conn_01x03", "RELAY CONTACT: NO, COM, NC", TB3, {"1": "RLY_NO", "2": "RLY_COM", "3": "RLY_NC"}, G_RELAY, mains=True,
      mpn="Terminal block 3P 5.08mm (KF128-5.08)")

    # ── press-sense input: 230 VAC -> 120k 1 W (one axial resistor in leg A) -> bridge -> LED + PC817 ─────────────
    A("J2", "Connector_Generic:Conn_01x02", "PRESS SENSE 230VAC", TB2, {"1": "SENSE_A", "2": "SENSE_B"}, G_SENSE, mains=True,
      mpn="Terminal block 2P 5.08mm (KF128-5.08)")
    A("RV2", "Device:Varistor", "7D471K", VARISTOR, {"1": "SENSE_A", "2": "SENSE_B"}, G_SENSE, mpn="Varistor 07D471K", mains=True)
    A("RS1", "Device:R", "120k 1W", RAX1W, {"1": "SENSE_A", "2": "BR_A"}, G_SENSE,
      mpn="120k 1W 5% axial (metal oxide, flame-proof), working voltage >= 400 V", mains=True)
    A("BR1", "Device:D_Bridge_+-AA", "MB10S", BRIDGE, {"1": "BR_P", "2": "BR_N", "3": "BR_A", "4": "SENSE_B"}, G_SENSE,
      mpn="MB10S bridge rectifier 1000V 0.5A SOIC-4", mains=True)
    A("C5", "Device:C_Polarized", "22uF 35V", CE54_5, {"1": "BR_P", "2": "BR_N"}, G_SENSE, mpn="SMD electrolytic 22uF 35V 5x5", mains=True)
    A("D5", "Device:LED", "RED mains present", LED0805, {"2": "BR_P", "1": "LED_K"}, G_SENSE, mpn="LED 0805 red", mains=True)
    A("U2", "Isolator:PC817", "PC817 SMD", OPTO, {"1": "LED_K", "2": "BR_N", "3": "GND", "4": "PRESS"}, G_SENSE,
      mpn="PC817 optocoupler SMD (SOP-4)", mains=True)
    # R12 across the opto LED: the LED only starts to conduct above ~0.5 mA (about 70-80 VAC on J2), so leakage of a triac / snubber
    # (a few mA into the contactor coil, tens of volts across J2 while the press is off) cannot half-switch the opto: no false strokes.
    # Values checked with SPICE (hardware/sim/sense_input.py): at 195 VAC with a weak PC817 (CTR 35 % at 1 mA) IO34 still reads 0.4 V,
    # and a 100 nF snubber leaking into the coil of a switched-off contactor leaves IO34 at 3.3 V.  R1 22k (not 10k) is what gives the
    # weak-opto margin: the opto only has to sink 0.15 mA.
    A("R12", "Device:R", "2.2k", R0805, {"1": "LED_K", "2": "BR_N"}, G_SENSE, mpn="2.2k 0805", mains=True)
    A("R1", "Device:R", "22k", R0805, {"1": "PRESS", "2": "+3V3"}, G_SENSE, mpn="22k 0805")         # IO34 has no internal pull-up

    # ── ESP32-WROOM-32 ──────────────────────────────────────────────────────────
    A("U1", "RF_Module:ESP32-WROOM-32", "ESP32-WROOM-32", "press_counter:ESP32-WROOM-32_Via03", by_name("RF_Module:ESP32-WROOM-32", ESP_PINS),
      G_ESP, mpn="ESP32-WROOM-32 / -32D / -32E module (4 MB flash)")
    A("C16", "Device:C", "10uF", C1206, {"1": "+3V3", "2": "GND"}, G_ESP, mpn="10uF 50V X7R 1206")
    A("C17", "Device:C", "100nF", C1206, {"1": "+3V3", "2": "GND"}, G_ESP, mpn="100nF 50V X7R 1206")
    A("R51", "Device:R", "10k", R0805, {"1": "EN", "2": "+3V3"}, G_ESP, mpn="10k 0805")
    A("C18", "Device:C", "1uF", C1206, {"1": "EN", "2": "GND"}, G_ESP, mpn="1uF 25V X7R 1206")              # EN delay (Espressif reference)
    A("R53", "Device:R", "10k", R0805, {"1": "BOOT", "2": "+3V3"}, G_ESP, mpn="10k 0805")                  # IO0 high = run

    # ── USB: micro-B -> CH340G (3.3 V, 12 MHz) -> UART0, DTR / RTS auto-reset of EN / IO0 (programmed like an Arduino board) ──
    A("J3", "Connector:USB_B_Micro", "USB micro-B", USBM, {"1": "VBUS", "2": "UDM", "3": "UDP", "4": None, "5": "GND", "SH": "GND"},
      G_USB, mpn="Micro-USB B receptacle SMD 5-pin")
    A("D14", "Device:D_Schottky", "SS34", DSMA, {"2": "VBUS", "1": "+5V"}, G_USB, mpn="SS34 Schottky 3A 40V SMA")
    ch = by_name("Interface_USB:CH340G", {"GND": "GND", "TXD": "RXD", "RXD": "TXD", "V3": "CH_V3", "UD+": "UDP", "UD-": "UDM", "XI": "CH_XI",
                                          "XO": "CH_XO", "~{DTR}": "USB_DTR", "~{RTS}": "USB_RTS", "VCC": "+3V3"})
    A("U5", "Interface_USB:CH340G", "CH340G", SOIC16, ch, G_USB, mpn="CH340G USB-UART SOP-16")
    A("R97", "Device:R", "0", R0805, {"1": "CH_V3", "2": "+3V3"}, G_USB, mpn="0R 0805 (CH340 V3 tied to VCC: 3.3 V operation)")
    A("C22", "Device:C", "100nF", C1206, {"1": "+3V3", "2": "GND"}, G_USB, mpn="100nF 50V X7R 1206")
    A("Y2", "Device:Crystal", "12MHz", XTAL, {"1": "CH_XI", "2": "CH_XO"}, G_USB, mpn="Crystal 12MHz HC-49USM SMD")
    A("C25", "Device:C", "22pF", C0805, {"1": "CH_XI", "2": "GND"}, G_USB, mpn="22pF C0G 0805")
    A("C26", "Device:C", "22pF", C0805, {"1": "CH_XO", "2": "GND"}, G_USB, mpn="22pF C0G 0805")
    A("R94", "Device:R", "10k", R0805, {"1": "USB_RTS", "2": "PROG_QB0"}, G_USB, mpn="10k 0805")
    A("R95", "Device:R", "10k", R0805, {"1": "USB_DTR", "2": "PROG_QB1"}, G_USB, mpn="10k 0805")
    A("Q7", "Transistor_BJT:SS8050", "SS8050", SOT23, {"1": "PROG_QB0", "2": "USB_DTR", "3": "BOOT"}, G_USB, mpn="SS8050 SOT-23")   # IO0 low: DTR asserted, RTS not
    A("Q8", "Transistor_BJT:SS8050", "SS8050", SOT23, {"1": "PROG_QB1", "2": "USB_RTS", "3": "EN"}, G_USB, mpn="SS8050 SOT-23")     # EN low: RTS asserted, DTR not

    # ── four output relays K2-K5 (OUT1..OUT4), same T73 5 V relay as K1, COM / NO / NC on 3-pin terminals; switched over MQTT / web page,
    #    or automatically (follow the machine, an alert, a call, the press signal).  Contact tracks about 2.5 A: control circuits, not motors.
    for n in range(1, 5):
        k, q, rb, rp, d, led, rl, j = f"K{n + 1}", f"Q{n + 2}", f"R{59 + n}", f"R{69 + n}", f"D{19 + n}", f"D{29 + n}", f"R{79 + n}", f"J{9 + n}"
        o = f"OUT{n}"
        A(k, "Relay:SANYOU_SRD_Form_C", "SRD-05VDC-SL-C", RELAY, {"1": f"{o}_COM", "2": "+5V", "3": f"{o}_NO", "4": f"{o}_NC", "5": f"{o}_N"},
          G_OUT, mpn="Relay T73 SRD-05VDC-SL-C (SPDT): 5V coil 70R, 10A 250VAC", mains=True)
        A(q, "Transistor_BJT:SS8050", "SS8050", SOT23, {"1": f"{o}_B", "2": "GND", "3": f"{o}_N"}, G_OUT, mpn="SS8050 SOT-23")
        A(rb, "Device:R", "1k", R0805, {"1": f"{o}_CTRL", "2": f"{o}_B"}, G_OUT, mpn="1k 0805")
        A(rp, "Device:R", "10k", R0805, {"1": f"{o}_B", "2": "GND"}, G_OUT, mpn="10k 0805")                 # off while the ESP32 boots
        A(d, "Device:D", "M7", DSMA, {"1": "+5V", "2": f"{o}_N"}, G_OUT, mpn="1N4007 SMD (M7)")
        A(rl, "Device:R", "1k", R0805, {"1": "+5V", "2": f"{o}_LED_A"}, G_OUT, mpn="1k 0805")
        A(led, "Device:LED", f"YELLOW OUT{n} on", LED0805, {"2": f"{o}_LED_A", "1": f"{o}_N"}, G_OUT, mpn="LED 0805 yellow")
        A(j, "Connector_Generic:Conn_01x03", f"OUT{n} CONTACT: NO, COM, NC", TB3, {"1": f"{o}_NO", "2": f"{o}_COM", "3": f"{o}_NC"}, G_OUT, mains=True,
          mpn="Terminal block 3P 5.08mm (KF128-5.08)")

    # ── voltage inputs on ADC1, both 100k / 10k = 0-30 V (36 V max.: 12 V and 24 V control circuits): 12/24 V sense (IO36), AUX (IO35).
    #    The 100k limits the current into the ESP32 pin to < 0.5 mA even at 48 V; the 100nF filters noise and ESD from the cable. ──
    A("J7", "Connector_Generic:Conn_01x02", "12/24V SENSE IN: +, GND", TB2, {"1": "V12_IN", "2": "GND"}, G_ADC, mpn="Terminal block 2P 5.08mm (KF128-5.08)")
    A("R90", "Device:R", "100k", R0805, {"1": "V12_IN", "2": "V12_ADC"}, G_ADC, mpn="100k 0805 1%")
    A("R91", "Device:R", "10k", R0805, {"1": "V12_ADC", "2": "GND"}, G_ADC, mpn="10k 0805 1%")
    A("C20", "Device:C", "100nF", C1206, {"1": "V12_ADC", "2": "GND"}, G_ADC, mpn="100nF 50V X7R 1206")
    A("J9", "Connector_Generic:Conn_01x02", "AUX IN 0-30V: IN, GND", TB2, {"1": "AUX_IN", "2": "GND"}, G_ADC, mpn="Terminal block 2P 5.08mm (KF128-5.08)")
    A("R98", "Device:R", "100k", R0805, {"1": "AUX_IN", "2": "AUX_ADC"}, G_ADC, mpn="100k 0805 1%")
    A("R99", "Device:R", "10k", R0805, {"1": "AUX_ADC", "2": "GND"}, G_ADC, mpn="10k 0805 1%")
    A("C23", "Device:C", "100nF", C1206, {"1": "AUX_ADC", "2": "GND"}, G_ADC, mpn="100nF 50V X7R 1206")

    # ── call buttons, alert LED, buzzer ─────────────────────────────────────────
    for sw, label, net, r, c in [("SW1", "CALL SUPERVISOR", "BTN_SUP", "R20", "C7"), ("SW2", "CALL NET/MAINT", "BTN_NET", "R21", "C8")]:
        A(sw, "Switch:SW_Push", label, SWSMD, {"1": net, "2": "GND"}, G_UI, mpn="Tactile switch 6x6x5 SMD")
        A(r, "Device:R", "10k", R0805, {"1": net, "2": "+3V3"}, G_UI, mpn="10k 0805")
        A(c, "Device:C", "100nF", C1206, {"1": net, "2": "GND"}, G_UI, mpn="100nF 50V X7R 1206")
    A("R44", "Device:R", "470", R0805, {"1": "LED_ALERT", "2": "ALERT_A"}, G_UI, mpn="470R 0805")
    A("D2", "Device:LED", "RED error / call", LED0805, {"2": "ALERT_A", "1": "GND"}, G_UI, mpn="LED 0805 red")
    A("BZ1", "Device:Buzzer", "Buzzer 5V", BUZZ12, {"1": "+5V", "2": "BZ_N"}, G_UI, mpn="Buzzer 12mm 5V (active)")
    A("Q1", "Transistor_BJT:SS8050", "SS8050", SOT23, {"1": "BZ_B", "2": "GND", "3": "BZ_N"}, G_UI, mpn="SS8050 SOT-23")
    A("R3", "Device:R", "1k", R0805, {"1": "BUZZ", "2": "BZ_B"}, G_UI, mpn="1k 0805")
    A("R4", "Device:R", "10k", R0805, {"1": "BZ_B", "2": "GND"}, G_UI, mpn="10k 0805")
    A("D3", "Device:D", "M7", DSMA, {"1": "+5V", "2": "BZ_N"}, G_UI, mpn="1N4007 SMD (M7)")

    # ── NFC: RC522 module on an 8-pin socket (module order SDA SCK MOSI MISO IRQ GND RST 3V3); no copper under its antenna ──
    A("P1", "Connector_Generic:Conn_01x08", "RC522 SDA SCK MOSI MISO IRQ GND RST 3V3", SOCK8,
      {"1": "RC_SS", "2": "SCK", "3": "MOSI", "4": "MISO", "5": None, "6": "GND", "7": "RC_RST", "8": "+3V3"},
      G_RFID, mpn="Female header 1x8 (for the RC522 module)")
    A("R33", "Device:R", "10k", R0805, {"1": "RC_SS", "2": "+3V3"}, G_RFID, mpn="10k 0805")            # IO5 strap high + reader deselected at boot

    # ── display: 0.96in 128x64 SSD1306 I2C OLED module on a 4-pin socket.  CHECK the pin order of the module you buy (GND VCC SCL SDA is
    #    the common one; some modules have VCC GND SCL SDA). ──
    A("P2", "Connector_Generic:Conn_01x04", "OLED GND VCC SCL SDA", SOCK4, {"1": "GND", "2": "+3V3", "3": "SCL", "4": "SDA"}, G_LCD,
      mpn="Female header 1x4 (0.96in I2C OLED module, SSD1306)")
    A("R40", "Device:R", "2.2k", R0805, {"1": "SCL", "2": "+3V3"}, G_LCD, mpn="2.2k 0805")
    A("R41", "Device:R", "2.2k", R0805, {"1": "SDA", "2": "+3V3"}, G_LCD, mpn="2.2k 0805")

    # ── DS18B20 waterproof probe (1-wire) ───────────────────────────────────────
    A("J4", "Connector_Generic:Conn_01x03", "DS18B20: 3V3 DATA GND", TB3, {"1": "+3V3", "2": "TEMP_DQ", "3": "GND"}, G_TEMP,
      mpn="Terminal block 3P 5.08mm (KF128-5.08)")
    A("R10", "Device:R", "2.2k", R0805, {"1": "TEMP_DQ", "2": "+3V3"}, G_TEMP, mpn="2.2k 0805 (1-wire pull-up, good for probe cables up to a few metres)")
    A("R11", "Device:R", "100", R0805, {"1": "TEMP_DQ", "2": "TEMP_IO"}, G_TEMP, mpn="100R 0805")       # protects the pin (cable)

    # ── test points / mounting / fiducials ──────────────────────────────────────
    for ref, net in [("TP1", "+5V"), ("TP2", "+3V3"), ("TP3", "GND"), ("TP4", "PRESS")]:
        A(ref, "Connector:TestPoint", net, TP, {"1": net}, G_MECH, mpn="test pad")
    for i in range(1, 7):
        A(f"MH{i}", "Mechanical:MountingHole", "M3", MH, {}, G_MECH)
    for i in range(1, 4):
        A(f"FID{i}", "Mechanical:Fiducial", "Fiducial", FID, {}, G_MECH)
    return b


BOARDS = {"main": main_board}

if __name__ == "__main__":
    for k, f in BOARDS.items():
        b = f()
        print(k, len(b.parts), "parts,", len(b.nets()), "nets")
