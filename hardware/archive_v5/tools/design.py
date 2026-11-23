"""Single source of truth for the press-counter hardware (v5): parts and pin->net assignments of BOTH boards.

The schematics, the PCBs and the BOM are all generated from this file, so they cannot drift apart.

Architecture (v5):
  MAIN board  = mains input + fuse + 5 V module, opto-isolated press-sense input (MB10S bridge), on-board 5 V relay for the press
                contactor, bare STM32F103C8T6 machine controller (counter, UI, USB console), ESP-12F WiFi gateway.  Mostly SMD.
  PANEL board = front panel connected with a 26-wire ribbon cable: D-pad (UP/DOWN/LEFT/RIGHT/OK) + two call buttons,
                RUN / ALERT LEDs, buzzer, RC522 RFID module socket and the socket of the 2.4in ST7789 display module.
  Mains is only present in a few small areas of the MAIN board (supply input, press-sense input, relay contacts);
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
G_MCU = "MACHINE CONTROLLER (STM32F103C8T6)"
G_USB = "USB / DEBUG / PROGRAMMING"
G_OUT = "AUXILIARY OUTPUT RELAYS OUT1-OUT4"
G_ADC = "VOLTAGE SENSE: 12 V AND 230 V (ADC)"
G_ESP = "WIFI GATEWAY (ESP-12F/E)"
G_PROG = "USB PROGRAMMING: ESP8266 (CH340G, AUTO-RESET) / STM32 (BOOT + RESET)"
G_TEMP = "TEMPERATURE SENSOR INPUT (LM35)"
G_HMI = "PANEL CONNECTOR"
G_UI = "BUTTONS"
G_LED = "INDICATORS"
G_BUZ = "BUZZER"
G_RFID = "RFID READER (RC522 MODULE)"
G_LCD = "DISPLAY SOCKETS"
G_MECH = "MECHANICAL / TEST"

# ── the 26-wire panel cable (identical pin numbering on both boards, ribbon cable pin n <-> pin n) ──────────
HMI_PINS = {
    "1": "+5V", "2": "GND", "3": "+3V3", "4": "GND", "5": "SCK", "6": "GND", "7": "MOSI", "8": "MISO", "9": "TFT_CS",
    "10": "TFT_DC", "11": "TFT_RST", "12": "TFT_BL", "13": "RC_SS", "14": "RC_RST", "15": "BTN_UP", "16": "BTN_DOWN",
    "17": "BTN_LEFT", "18": "BTN_RIGHT", "19": "BTN_OK", "20": "BTN_SUP", "21": "BTN_NET", "22": "LED_RUN",
    "23": "LED_ALERT", "24": "BUZZ", "25": "+3V3", "26": "GND",
}

MAIN_MAINS = ["L", "LF", "N", "SENSE_A", "SENSE_B", "BR_A", "RLY_COM", "RLY_NO", "RLY_NC"] + [f"OUT{n}_{c}" for n in range(1, 5) for c in ("COM", "NO", "NC")]
MAIN_MAINS_DC = ["BR_P", "BR_N", "LED_K"]
MAIN_POWER = ["+5V", "+3V3", "VBUS"]


def main_board():
    b = Board("press_counter_main", "Hydraulic press counter v5 - MAIN board (STM32F103 + ESP-12F + relay)",
              mains_nets=MAIN_MAINS, mains_dc_nets=MAIN_MAINS_DC, power_nets=MAIN_POWER,
              wide={"RLY_COM": 1.2, "RLY_NO": 1.2, "RLY_NC": 1.2, "L": 1.0, "LF": 1.0, "N": 1.0, "+3V3": 0.3, "VBUS": 0.4,
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

    # ── 3.3 V ───────────────────────────────────────────────────────────────────
    A("U3", "Regulator_Linear:AMS1117-3.3", "AMS1117-3.3", SOT223, {"1": "GND", "2": "+3V3", "3": "+5V"}, G_LDO,
      mpn="AMS1117-3.3 SOT-223")
    A("C3", "Device:C", "10uF", C1206, {"1": "+5V", "2": "GND"}, G_LDO, mpn="10uF 50V X7R 1206")
    A("C4", "Device:C_Polarized", "47uF 16V", CE54_63, {"1": "+3V3", "2": "GND"}, G_LDO, mpn="SMD electrolytic 47uF 16V 6.3x5.4")
    A("C19", "Device:C", "100nF", C1206, {"1": "+3V3", "2": "GND"}, G_LDO, mpn="100nF 50V X7R 1206")
    A("R2", "Device:R", "470", R0805, {"1": "+3V3", "2": "PWR_LED_A"}, G_LDO, mpn="470R 0805")
    A("D6", "Device:LED", "GREEN 3V3 ok", LED0805, {"2": "PWR_LED_A", "1": "GND"}, G_LDO, mpn="LED 0805 green")

    # ── press-contactor relay: 5 V coil T73 (SRD-05VDC-SL-C, SPDT; the -SL-A 4-pin SPST fits the same pads).  The contact used is
    #    COM -> NO (normally open: the machine is disabled when the board is unpowered).  NC is tied to COM on the board so that the
    #    pad of a 5-pin part is never a floating mains-potential pad. ──────────────────────────────────────────────────────────────
    A("K1", "Relay:SANYOU_SRD_Form_C", "SRD-05VDC-SL-C", RELAY, {"1": "RLY_COM", "2": "+5V", "3": "RLY_NO", "4": "RLY_NC", "5": "RLY_N"},
      G_RELAY, mpn="Relay T73 SRD-05VDC-SL-C (SPDT) or SRD-05VDC-SL-A (SPST-NO): 5V coil 70R, 10A 250VAC")
    A("Q2", "Transistor_BJT:SS8050", "SS8050", SOT23, {"1": "RLY_B", "2": "GND", "3": "RLY_N"}, G_RELAY, mpn="SS8050 SOT-23")
    A("R9", "Device:R", "1k", R0805, {"1": "RELAY_CTRL", "2": "RLY_B"}, G_RELAY, mpn="1k 0805")
    A("R13", "Device:R", "10k", R0805, {"1": "RLY_B", "2": "GND"}, G_RELAY, mpn="10k 0805")
    A("D12", "Device:D", "M7", DSMA, {"1": "+5V", "2": "RLY_N"}, G_RELAY, mpn="1N4007 SMD (M7)")
    A("R43", "Device:R", "1k", R0805, {"1": "+5V", "2": "RLY_LED_A"}, G_RELAY, mpn="1k 0805")
    A("D13", "Device:LED", "RED relay on", LED0805, {"2": "RLY_LED_A", "1": "RLY_N"}, G_RELAY, mpn="LED 0805 red")
    A("J5", "Connector_Generic:Conn_01x03", "RELAY CONTACT: NO, COM, NC", TB3, {"1": "RLY_NO", "2": "RLY_COM", "3": "RLY_NC"}, G_RELAY, mains=True,
      mpn="Terminal block 3P 5.08mm (KF128-5.08)")
    A("RV3", "Device:Varistor", "7D471K (optional)", VARISTOR, {"1": "RLY_COM", "2": "RLY_NO"}, G_RELAY, mains=True, dnp=True,
      mpn="Varistor 07D471K (optional, across the contact)")

    # ── four auxiliary output relays K2-K5 (OUT1..OUT4) on the otherwise unused STM32 pins PB11, PC13, PC14, PC15 (PA2 / PA3, the two free
    #    ADC pins, measure the 12 V and the 230 V input below).  Same T73 / SRD 5 V relay as K1; every relay brings COM, NO **and NC** to a
    #    3-pin terminal.  PC13-PC15 may only source / sink 3 mA: base resistor 2.2 k (1.2 mA x hFE >= 85 = 100 mA > 74 mA coil + LED).
    #    Contact tracks are about 2.5 A: control circuits (lamps, horns, contactor coils), not motors.
    for n in range(1, 5):
        k, q, rb, rp, d, led, rl, j = f"K{n + 1}", f"Q{n + 2}", f"R{59 + n}", f"R{69 + n}", f"D{19 + n}", f"D{29 + n}", f"R{79 + n}", f"J{9 + n}"
        o = f"OUT{n}"
        A(k, "Relay:SANYOU_SRD_Form_C", "SRD-05VDC-SL-C", RELAY, {"1": f"{o}_COM", "2": "+5V", "3": f"{o}_NO", "4": f"{o}_NC", "5": f"{o}_N"},
          G_OUT, mpn="Relay T73 SRD-05VDC-SL-C (SPDT, needs the NC pin): 5V coil 70R, 10A 250VAC", mains=True)
        A(q, "Transistor_BJT:SS8050", "SS8050", SOT23, {"1": f"{o}_B", "2": "GND", "3": f"{o}_N"}, G_OUT, mpn="SS8050 SOT-23")
        A(rb, "Device:R", "2.2k", R0805, {"1": f"{o}_CTRL", "2": f"{o}_B"}, G_OUT, mpn="2.2k 0805")
        A(rp, "Device:R", "10k", R0805, {"1": f"{o}_B", "2": "GND"}, G_OUT, mpn="10k 0805")
        A(d, "Device:D", "M7", DSMA, {"1": "+5V", "2": f"{o}_N"}, G_OUT, mpn="1N4007 SMD (M7)")
        A(rl, "Device:R", "1k", R0805, {"1": "+5V", "2": f"{o}_LED_A"}, G_OUT, mpn="1k 0805")
        A(led, "Device:LED", f"YELLOW OUT{n} on", LED0805, {"2": f"{o}_LED_A", "1": f"{o}_N"}, G_OUT, mpn="LED 0805 yellow")
        A(j, "Connector_Generic:Conn_01x03", f"OUT{n} CONTACT: NO, COM, NC", TB3, {"1": f"{o}_NO", "2": f"{o}_COM", "3": f"{o}_NC"}, G_OUT, mains=True,
          mpn="Terminal block 3P 5.08mm (KF128-5.08)")

    # ── voltage measurement on the two free ADC pins: 12 V DC input (PA2) and 230 V AC through a ZMPT101B module (PA3) ──────────────
    A("J7", "Connector_Generic:Conn_01x02", "12V SENSE IN: +12V, GND", TB2, {"1": "V12_IN", "2": "GND"}, G_ADC,
      mpn="Terminal block 2P 5.08mm (KF128-5.08)")
    A("R90", "Device:R", "33k", R1206, {"1": "V12_IN", "2": "V12_DIV"}, G_ADC, mpn="33k 1206")
    A("R91", "Device:R", "10k", R0805, {"1": "V12_DIV", "2": "GND"}, G_ADC, mpn="10k 0805")
    A("R92", "Device:R", "1k", R0805, {"1": "V12_DIV", "2": "V12_ADC"}, G_ADC, mpn="1k 0805")
    A("C20", "Device:C", "100nF", C1206, {"1": "V12_ADC", "2": "GND"}, G_ADC, mpn="100nF 50V X7R 1206")
    A("J8", "Connector_Generic:Conn_01x03", "ZMPT101B module: 3V3 OUT GND", HDR3, {"1": "+3V3", "2": "VAC_IN", "3": "GND"}, G_ADC,
      mpn="Pin header 1x3 for the ZMPT101B AC voltage module (its own screw terminals take the 230 V)")
    A("R93", "Device:R", "1k", R0805, {"1": "VAC_IN", "2": "VAC_ADC"}, G_ADC, mpn="1k 0805")
    A("C21", "Device:C", "100nF", C1206, {"1": "VAC_ADC", "2": "GND"}, G_ADC, mpn="100nF 50V X7R 1206")

    # ── press-sense input: 230 VAC -> 120k 1 W (one axial resistor in leg A) -> bridge -> LED + PC817 ────────
    A("J2", "Connector_Generic:Conn_01x02", "PRESS SENSE 230VAC", TB2, {"1": "SENSE_A", "2": "SENSE_B"}, G_SENSE, mains=True,
      mpn="Terminal block 2P 5.08mm (KF128-5.08)")
    A("RV2", "Device:Varistor", "7D471K", VARISTOR, {"1": "SENSE_A", "2": "SENSE_B"}, G_SENSE, mpn="Varistor 07D471K", mains=True)
    A("RS1", "Device:R", "120k 1W", RAX1W, {"1": "SENSE_A", "2": "BR_A"}, G_SENSE, mpn="120k 1W 5% axial (metal oxide), through-hole", mains=True)
    A("BR1", "Device:D_Bridge_+-AA", "MB10S", BRIDGE, {"1": "BR_P", "2": "BR_N", "3": "BR_A", "4": "SENSE_B"}, G_SENSE,
      mpn="MB10S bridge rectifier 1000V 0.5A SOIC-4", mains=True)
    A("C5", "Device:C_Polarized", "22uF 35V", CE54_5, {"1": "BR_P", "2": "BR_N"}, G_SENSE, mpn="SMD electrolytic 22uF 35V 5x5", mains=True)
    A("D5", "Device:LED", "RED mains present", LED0805, {"2": "BR_P", "1": "LED_K"}, G_SENSE, mpn="LED 0805 red", mains=True)
    A("U2", "Isolator:PC817", "PC817 SMD", OPTO, {"1": "LED_K", "2": "BR_N", "3": "GND", "4": "PRESS"}, G_SENSE,
      mpn="PC817 optocoupler SMD (SOP-4)", mains=True)
    A("R1", "Device:R", "10k", R0805, {"1": "PRESS", "2": "+3V3"}, G_SENSE, mpn="10k 0805")

    # ── STM32F103C8T6 (bare chip) ───────────────────────────────────────────────
    mcu = by_name("MCU_ST_STM32F1:STM32F103C8Tx", {
        "VBAT": "+3V3", "VDD": "+3V3", "VDDA": "+3V3", "VSS": "GND", "VSSA": "GND", "NRST": "MCU_NRST", "BOOT0": "BOOT0",
        "PD0": "XIN", "PD1": "XOUT",
        "PA0": "BTN_UP_R", "PA1": "BTN_DOWN_R", "PA4": "TFT_CS", "PA5": "SCK_R",
        "PA6": "MISO", "PA7": "MOSI_R", "PA8": "BUZZ", "PA9": "LINK_S2E_RAW", "PA10": "LINK_E2S", "PA11": "USB_DM",
        "PA12": "USB_DP", "PA13": "SWDIO", "PA14": "SWCLK", "PA15": "BTN_NET_R",
        "PB0": "TFT_BL", "PB1": "TEMP_ADC", "PB2": "BOOT1", "PB3": "BTN_SUP_R", "PB4": "BTN_LEFT_R", "PB5": "BTN_RIGHT_R",
        "PB6": "PRESS", "PB7": "RELAY_CTRL", "PB8": "LED_RUN", "PB9": "LED_ALERT", "PB10": "BTN_OK_R",
        "PB12": "RC_SS_R", "PB13": "RC_RST", "PB14": "TFT_DC", "PB15": "TFT_RST",
        "PB11": "OUT1_CTRL", "PC13": "OUT2_CTRL", "PC14": "OUT3_CTRL", "PC15": "OUT4_CTRL",
        "PA2": "V12_ADC", "PA3": "VAC_ADC"})
    A("U4", "MCU_ST_STM32F1:STM32F103C8Tx", "STM32F103C8T6", LQFP48, mcu, G_MCU, mpn="STM32F103C8T6 LQFP-48")
    A("Y1", "Device:Crystal", "8MHz", XTAL, {"1": "XIN", "2": "XOUT"}, G_MCU, mpn="Crystal 8MHz HC-49SMD CL=20pF")
    A("C6", "Device:C", "22pF", C0805, {"1": "XIN", "2": "GND"}, G_MCU, mpn="22pF C0G 0805")
    A("C7", "Device:C", "22pF", C0805, {"1": "XOUT", "2": "GND"}, G_MCU, mpn="22pF C0G 0805")
    for ref in ["C8", "C9", "C10", "C11"]:
        A(ref, "Device:C", "100nF", C1206, {"1": "+3V3", "2": "GND"}, G_MCU, mpn="100nF 50V X7R 1206")
    A("C12", "Device:C", "1uF", C1206, {"1": "+3V3", "2": "GND"}, G_MCU, mpn="1uF 25V X7R 1206")
    A("C13", "Device:C", "10uF", C1206, {"1": "+3V3", "2": "GND"}, G_MCU, mpn="10uF 50V X7R 1206")
    A("C14", "Device:C", "100nF", C1206, {"1": "MCU_NRST", "2": "GND"}, G_MCU, mpn="100nF 50V X7R 1206")
    A("R3", "Device:R", "10k", R0805, {"1": "BOOT0", "2": "GND"}, G_MCU, mpn="10k 0805")
    A("R4", "Device:R", "10k", R0805, {"1": "BOOT1", "2": "GND"}, G_MCU, mpn="10k 0805")
    A("R5", "Device:R", "1k", R0805, {"1": "BOOT0_CTL_RAW", "2": "BOOT0"}, G_MCU, mpn="1k 0805")
    A("R16", "Device:R", "470", R0805, {"1": "MCU_RST_CTL", "2": "MCU_NRST"}, G_MCU, mpn="470R 0805")
    A("R14", "Device:R", "330", R0805, {"1": "LINK_TXE", "2": "LINK_E2S"}, G_MCU, mpn="330R 0805")
    A("R15", "Device:R", "330", R0805, {"1": "LINK_S2E_RAW", "2": "LINK_S2E"}, G_MCU, mpn="330R 0805")
    # panel lines: series resistors protect the pins and tame the cable
    for ref, raw, net, val in [("R20", "BTN_UP_R", "BTN_UP", "1k"), ("R21", "BTN_DOWN_R", "BTN_DOWN", "1k"),
                               ("R22", "BTN_LEFT_R", "BTN_LEFT", "1k"), ("R23", "BTN_RIGHT_R", "BTN_RIGHT", "1k"),
                               ("R24", "BTN_OK_R", "BTN_OK", "1k"), ("R25", "BTN_SUP_R", "BTN_SUP", "1k"),
                               ("R26", "BTN_NET_R", "BTN_NET", "1k"), ("R27", "SCK_R", "SCK", "100"),
                               ("R28", "MOSI_R", "MOSI", "100"), ("R29", "RC_SS_R", "RC_SS", "100")]:
        A(ref, "Device:R", val, R0805, {"1": raw, "2": net}, G_MCU, mpn=f"{val} 0805")
    A("R32", "Device:R", "10k", R0805, {"1": "TFT_CS", "2": "+3V3"}, G_MCU, mpn="10k 0805")
    A("R33", "Device:R", "10k", R0805, {"1": "RC_SS", "2": "+3V3"}, G_MCU, mpn="10k 0805")

    # ── USB (always on: console + service) / SWD ────────────────────────────────
    A("J3", "Connector:USB_B_Micro", "USB micro-B", USBM, {"1": "VBUS", "2": "USB_DM", "3": "USB_DP", "4": None, "5": "GND", "SH": "GND"},
      G_USB, mpn="Micro-USB B receptacle SMD 5-pin")
    A("R30", "Device:R", "1k", R0805, {"1": "+3V3", "2": "USB_PU"}, G_USB, mpn="1k 0805 (+470R = 1.47k USB pull-up)")
    A("R31", "Device:R", "470", R0805, {"1": "USB_PU", "2": "USB_DP"}, G_USB, mpn="470R 0805")
    A("D14", "Device:D_Schottky", "SS14", DSMA, {"2": "VBUS", "1": "+5V"}, G_USB, mpn="SS14 Schottky 1A 40V SMA")
    A("J9", "Connector_Generic:Conn_01x05", "SWD: GND SWCLK SWDIO 3V3 NRST", HDR5,
      {"1": "GND", "2": "SWCLK", "3": "SWDIO", "4": "+3V3", "5": "MCU_NRST"}, G_USB,
      mpn="Pin header 1x5 (development / programming; pogo-pin pads in production)")

    # ── LM35 temperature input ──────────────────────────────────────────────────
    A("J4", "Connector_Generic:Conn_01x03", "LM35 OUT GND +5V", TB3, {"1": "TEMP_IN", "2": "GND", "3": "+5V"}, G_TEMP,
      mpn="Terminal block 3P 5.08mm (KF128-5.08)")
    A("R10", "Device:R", "1k", R0805, {"1": "TEMP_IN", "2": "TEMP_ADC"}, G_TEMP, mpn="1k 0805")
    A("C15", "Device:C", "100nF", C1206, {"1": "TEMP_ADC", "2": "GND"}, G_TEMP, mpn="100nF 50V X7R 1206")

    # ── ESP-12F/E gateway ───────────────────────────────────────────────────────
    A("U1", "RF_Module:ESP-12F", "ESP-12F (or ESP-12E)", "RF_Module:ESP-12E",
      {"1": "RST", "2": None, "3": "EN", "4": None, "5": None, "6": None, "7": "LINK_S2E", "8": "+3V3",
       "9": None, "10": None, "11": None, "12": None, "13": None, "14": None,
       "15": "GND", "16": "LINK_TXE", "17": "WIFI_LED", "18": "FLASH", "19": "MCU_RST_CTL", "20": "BOOT0_CTL_RAW",
       "21": "RXD", "22": "TXD"}, G_ESP, mpn="ESP-12F or ESP-12E module")
    A("R50", "Device:R", "10k", R0805, {"1": "RST", "2": "+3V3"}, G_ESP, mpn="10k 0805")
    A("R51", "Device:R", "10k", R0805, {"1": "EN", "2": "+3V3"}, G_ESP, mpn="10k 0805")
    A("R52", "Device:R", "10k", R0805, {"1": "LINK_TXE", "2": "GND"}, G_ESP, mpn="10k 0805")          # GPIO15 low at boot
    A("R53", "Device:R", "10k", R0805, {"1": "FLASH", "2": "+3V3"}, G_ESP, mpn="10k 0805")           # GPIO0 high at boot
    A("R54", "Device:R", "10k", R0805, {"1": "WIFI_LED", "2": "+3V3"}, G_ESP, mpn="10k 0805")        # GPIO2 high at boot
    A("R55", "Device:R", "470", R0805, {"1": "+3V3", "2": "WIFI_LED_A"}, G_ESP, mpn="470R 0805")
    A("D16", "Device:LED", "YELLOW WiFi / link", LED0805, {"2": "WIFI_LED_A", "1": "WIFI_LED"}, G_ESP, mpn="LED 0805 yellow")
    A("C16", "Device:C", "10uF", C1206, {"1": "+3V3", "2": "GND"}, G_ESP, mpn="10uF 50V X7R 1206")
    A("C17", "Device:C", "100nF", C1206, {"1": "+3V3", "2": "GND"}, G_ESP, mpn="100nF 50V X7R 1206")
    A("SW1", "Switch:SW_Push", "ESP RESET", SWSMD, {"1": "RST", "2": "GND"}, G_ESP, mpn="Tactile switch 6x6x5 SMD")
    A("SW2", "Switch:SW_Push", "ESP FLASH", SWSMD, {"1": "FLASH", "2": "GND"}, G_ESP, mpn="Tactile switch 6x6x5 SMD")

    # ── USB programming of the ESP8266 like an Arduino board: micro-USB -> CH340G (3.3 V supply, 12 MHz crystal) -> ESP UART0, DTR / RTS drive GPIO0 and EN
    #    through two NPN transistors (the NodeMCU / esptool auto-reset circuit).  VBUS also powers the board through D15 (like J3 / D14).
    #    The STM32 is programmed over its own USB (J3, HID bootloader) with the BOOT / RESET buttons SW4 / SW3 - see docs/FIRMWARE.md. ──────────────
    A("J14", "Connector:USB_B_Micro", "USB micro-B ESP8266", USBM, {"1": "VBUS2", "2": "UDM2", "3": "UDP2", "4": None, "5": "GND", "SH": "GND"},
      G_PROG, mpn="Micro-USB B receptacle SMD 5-pin")
    A("D15", "Device:D_Schottky", "SS14", DSMA, {"2": "VBUS2", "1": "+5V"}, G_PROG, mpn="SS14 Schottky 1A 40V SMA")
    ch = by_name("Interface_USB:CH340G", {"GND": "GND", "TXD": "RXD", "RXD": "TXD", "V3": "CH_V3", "UD+": "UDP2", "UD-": "UDM2", "XI": "CH_XI", "XO": "CH_XO",
                                          "~{DTR}": "USB_DTR", "~{RTS}": "USB_RTS", "VCC": "+3V3"})
    A("U5", "Interface_USB:CH340G", "CH340G", SOIC16, ch, G_PROG, mpn="CH340G USB-UART SOP-16")
    A("Y2", "Device:Crystal", "12MHz", XTAL, {"1": "CH_XI", "2": "CH_XO"}, G_PROG, mpn="Crystal 12MHz HC-49USM SMD")
    A("C25", "Device:C", "22pF", C0805, {"1": "CH_XI", "2": "GND"}, G_PROG, mpn="22pF C0G 0805")
    A("C26", "Device:C", "22pF", C0805, {"1": "CH_XO", "2": "GND"}, G_PROG, mpn="22pF C0G 0805")
    A("C22", "Device:C", "100nF", C1206, {"1": "+3V3", "2": "GND"}, G_PROG, mpn="100nF 50V X7R 1206")
    A("R97", "Device:R", "0", R0805, {"1": "CH_V3", "2": "+3V3"}, G_PROG, mpn="0R 0805 (ties the CH340 V3 pin to VCC: 3.3 V operation)")
    A("R94", "Device:R", "10k", R0805, {"1": "USB_RTS", "2": "PROG_QB0"}, G_PROG, mpn="10k 0805")
    A("R95", "Device:R", "10k", R0805, {"1": "USB_DTR", "2": "PROG_QB1"}, G_PROG, mpn="10k 0805")
    A("Q7", "Transistor_BJT:SS8050", "SS8050", SOT23, {"1": "PROG_QB0", "2": "USB_DTR", "3": "FLASH"}, G_PROG, mpn="SS8050 SOT-23")   # GPIO0 low while RTS = 0, DTR = 1 (pins)
    A("Q8", "Transistor_BJT:SS8050", "SS8050", SOT23, {"1": "PROG_QB1", "2": "USB_RTS", "3": "EN"}, G_PROG, mpn="SS8050 SOT-23")       # EN low while DTR = 0, RTS = 1 (pins)
    # STM32 buttons: RESET (NRST to GND) and BOOT (BOOT1 / PB2 to 3.3 V through 1 k: with the HID bootloader flashed, BOOT held at reset = USB update mode)
    A("SW3", "Switch:SW_Push", "STM32 RESET", SWSMD, {"1": "MCU_NRST", "2": "GND"}, G_PROG, mpn="Tactile switch 6x6x5 SMD")
    A("SW4", "Switch:SW_Push", "STM32 BOOT", SWSMD, {"1": "BOOT1_SW", "2": "+3V3"}, G_PROG, mpn="Tactile switch 6x6x5 SMD")
    A("R96", "Device:R", "1k", R0805, {"1": "BOOT1", "2": "BOOT1_SW"}, G_PROG, mpn="1k 0805")

    # ── panel connector ─────────────────────────────────────────────────────────
    A("J6", "Connector_Generic:Conn_02x13_Odd_Even", "PANEL CABLE (26-pin IDC)", IDC26, dict(HMI_PINS), G_HMI,
      mpn="IDC box header 2x13 + 26-wire ribbon cable")

    # ── test points / mounting / fiducials ──────────────────────────────────────
    for ref, net in [("TP1", "+5V"), ("TP2", "+3V3"), ("TP3", "GND"), ("TP4", "PRESS")]:
        A(ref, "Connector:TestPoint", net, TP, {"1": net}, G_MECH, mpn="test pad")
    for i in range(1, 7):
        A(f"MH{i}", "Mechanical:MountingHole", "M3", MH, {}, G_MECH)
    for i in range(1, 4):
        A(f"FID{i}", "Mechanical:Fiducial", "Fiducial", FID, {}, G_MECH)
    return b


# ───────────────────────────────────────────────────────────────────────────────────────────────────────────
PANEL_POWER = ["+5V", "+3V3"]


def panel_board():
    b = Board("press_counter_panel", "Hydraulic press counter v5 - PANEL board (buttons, RFID, display, buzzer, LEDs)",
              mains_nets=[], mains_dc_nets=[], power_nets=PANEL_POWER)
    A = b.add
    A("J1", "Connector_Generic:Conn_02x13_Odd_Even", "TO MAIN BOARD (26-pin IDC)", IDC26, dict(HMI_PINS), G_HMI,
      mpn="IDC box header 2x13 + 26-wire ribbon cable")

    # D-pad + call buttons
    for sw, label, net in [("SW1", "UP", "BTN_UP"), ("SW2", "DOWN", "BTN_DOWN"), ("SW3", "LEFT", "BTN_LEFT"),
                           ("SW4", "RIGHT", "BTN_RIGHT"), ("SW5", "OK", "BTN_OK"), ("SW6", "CALL SUPERVISOR", "BTN_SUP"),
                           ("SW7", "CALL NET/MAINT", "BTN_NET")]:
        A(sw, "Switch:SW_Push", label, SWSMD, {"1": net, "2": "GND"}, G_UI, mpn="Tactile switch 6x6x5 SMD")
    for i, net in enumerate(["BTN_UP", "BTN_DOWN", "BTN_LEFT", "BTN_RIGHT", "BTN_OK", "BTN_SUP", "BTN_NET"], start=1):
        A(f"C{i}", "Device:C", "100nF", C1206, {"1": net, "2": "GND"}, G_UI, mpn="100nF 50V X7R 1206")

    # indicators
    A("R1", "Device:R", "470", R0805, {"1": "LED_RUN", "2": "LED_RUN_A"}, G_LED, mpn="470R 0805")
    A("D1", "Device:LED", "GREEN machine enabled", LED0805, {"2": "LED_RUN_A", "1": "GND"}, G_LED, mpn="LED 0805 green")
    A("R2", "Device:R", "470", R0805, {"1": "LED_ALERT", "2": "LED_ALERT_A"}, G_LED, mpn="470R 0805")
    A("D2", "Device:LED", "RED error / call", LED0805, {"2": "LED_ALERT_A", "1": "GND"}, G_LED, mpn="LED 0805 red")

    # buzzer (active 5 V type; a passive one also works with a tone() from the STM32)
    A("BZ1", "Device:Buzzer", "Buzzer 5V", BUZZ12, {"1": "+5V", "2": "BZ_N"}, G_BUZ, mpn="Buzzer 12mm 5V (active)")
    A("Q1", "Transistor_BJT:SS8050", "SS8050", SOT23, {"1": "BZ_B", "2": "GND", "3": "BZ_N"}, G_BUZ, mpn="SS8050 SOT-23")
    A("R3", "Device:R", "1k", R0805, {"1": "BUZZ", "2": "BZ_B"}, G_BUZ, mpn="1k 0805")
    A("R4", "Device:R", "10k", R0805, {"1": "BZ_B", "2": "GND"}, G_BUZ, mpn="10k 0805")
    A("D3", "Device:D", "M7", DSMA, {"1": "+5V", "2": "BZ_N"}, G_BUZ, mpn="1N4007 SMD (M7)")

    # RFID socket (RC522 module plugs in; keep the copper under the antenna free)
    A("P1", "Connector_Generic:Conn_01x08", "RC522 SDA SCK MOSI MISO IRQ GND RST 3V3", SOCK8,
      {"1": "RC_SS", "2": "SCK", "3": "MOSI", "4": "MISO", "5": None, "6": "GND", "7": "RC_RST", "8": "+3V3"},
      G_RFID, mpn="Female header 1x8 (for the RC522 module)")

    # display socket: 2.4in ST7789 240x320 SPI module, 8 pins in the order printed on most modules (GND VCC SCL SDA RES DC CS BL).
    # CHECK THE ORDER OF THE MODULE YOU BUY before ordering boards.
    A("J2", "Connector_Generic:Conn_01x08", "LCD: GND VCC SCL SDA RES DC CS BLK", SOCK8,
      {"1": "GND", "2": "+3V3", "3": "SCK", "4": "MOSI", "5": "TFT_RST", "6": "TFT_DC", "7": "TFT_CS", "8": "BL"},
      G_LCD, mpn="Female header 1x8 (2.4in ST7789 display module)")
    A("R5", "Device:R", "100", R0805, {"1": "TFT_BL", "2": "BL_PWM"}, G_LCD, mpn="100R 0805")
    A("SJ1", "Jumper:SolderJumper_3_Bridged12", "BL: PWM (cut for always-on)", SJ3, {"1": "BL_PWM", "2": "BL", "3": "+3V3"}, G_LCD,
      mpn="solder jumper (default: back-light PWM from the MCU)")

    for i in range(1, 5):
        A(f"MH{i}", "Mechanical:MountingHole", "M3", MH, {}, G_MECH)
    for i in range(1, 4):                      # on the bottom face, where all SMD parts are
        A(f"FID{i}", "Mechanical:Fiducial", "Fiducial", FID, {}, G_MECH)
    return b


BOARDS = {"main": main_board, "panel": panel_board}

if __name__ == "__main__":
    for k, f in BOARDS.items():
        b = f()
        print(k, len(b.parts), "parts,", len(b.nets()), "nets")
