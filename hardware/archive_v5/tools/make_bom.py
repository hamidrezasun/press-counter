"""Generate hardware/BOM_press_counter_v5.xlsx (both boards, Iranian-store links + prices) from design.py.

Run with the system python (needs openpyxl):   python make_bom.py
Prices/stock were read from the shops' websites on 2026-10-06 / 07 and WILL change - re-check before ordering.
Items without an exact product page (the shop search did not give a full URL) link to the shop's search for that item.

Sheets:  "BOM"        - one set of boards (quantities to buy incl. spares)
         "Production" - the same list scaled to N units (edit cells B2 / B3)
For an assembly house the script also writes hardware/fabrication/assembly_bom_main.csv and assembly_bom_panel.csv
(designators, value, footprint, description, SMD/THT, DNP).
"""
import csv
import os
import sys
from collections import OrderedDict
from urllib.parse import quote

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import design  # noqa: E402
from openpyxl import Workbook
from openpyxl.styles import Alignment, Border, Font, PatternFill, Side
from openpyxl.utils import get_column_letter

ECA = "https://eshop.eca.ir/"
VOLTA = "https://voltatech.ir/product/"
DATE = "2026-10-07"


def eca(path):
    return ECA + quote(path, safe="/:%-.")


def eca_search(q):
    return ECA + "search?controller=search&s=" + quote(q)


def volta(slug):
    return VOLTA + quote(slug) + "/"


# key -> sourcing.  rial = unit price in Rial as listed by the shop (Toman = Rial / 10)
SRC = {
    # ── mains / power ───────────────────────────────────────────────────────────────────────────────
    "HLK-5M05": dict(store="eca.ir", name="مبدل کاهنده روبردی ایزوله 220AC to 5VDC 1A مدل HLK-5M05 مارک Hi-Link", rial=6840000, stock="in stock",
                     url=eca("کانورتر-اینورتر-و-ups/21573-مبدل-کاهنده-روبردی-ایزوله-220ac-to-5vdc-1a-مدل-hlk-5m05-مارک-hi-link.html")),
    "Varistor": dict(store="eca.ir", name="وریستور VDR 07D471K", rial=61600, stock="in stock",
                     url=eca("وریستور/11337-وریستور-vdr-07d471k.html")),
    "MB10S": dict(store="eca.ir", name="پل دیود MB10S (SOIC-4, 1000V 0.5A)", rial=21100, stock="in stock (min. order 5)",
                  url=eca("پل-دیودها/14760-پل-دیود-mb10s.html")),
    "PC817-SMD": dict(store="eca.ir", name="تراشه اپتوکوپلر PC817 پکیج SMD (SOIC-4)", rial=76600, stock="in stock",
                      url=eca("تراشه-اپتوکوپلر-و-اپتوکانتر/1703-تراشه-اپتوکوپلر-pc817-پکیج-smd.html"),
                      alt="Footprint 'SOP4_Wide_Universal' covers wide (7.2 mm) and narrow SOP-4 bodies - check the part you get. CTR class C (200-400 %) wanted."),
    "AMS1117": dict(store="eca.ir", name="رگولاتور 3.3 ولت AMS1117 پکیج SOT-223", rial=55700, stock="in stock",
                    url=eca("رگولاتور/978-رگولاتور-33-ولت-ams1117-پکیج-sot-223.html")),
    "SS14": dict(store="voltatech.ir", name="دیود 1N5819 SMD کد SS14", rial=19330, stock="in stock",
                 url=volta("دیود-1n5819-smd-کد-ss14"), alt="eca.ir: SS14/1N5819 SMA currently out of stock."),
    "M7": dict(store="eca.ir", name="دیود 1N4007 SMD (M7)", rial=9970, stock="in stock (50)",
               url=eca("سایر-دیودها/1937-دیود-1n4007-smd-m7.html"), alt="voltatech.ir: 1,089 Toman."),
    "SS8050": dict(store="eca.ir", name="ترانزیستور SS8050 نوع NPN پکیج SOT-23", rial=13400, stock="in stock (10)",
                   url=eca("ترانزیستور-bjt/6548-ترانزیستور-ss8050-نوع-npn-پکیج-sot-23.html")),
    "Relay": dict(store="digikala.com", name="رله 5 ولت لیمینگ مدل پایه میلون T73 بسته 3 عددی (5 V coil 70 ohm, SRD-05VDC-SL-C type)", rial=1133000,
                  stock="marketable (pack of 3 = 3,400,000 Rial)", url="https://www.digikala.com/product/dkp-13772470/",
                  alt="Any T73 / SRD / JQC-3F 5 V coil relay, 10 A 250 VAC, 5-pin SPDT (4-pin SPST-NO versions fit too). Alternative: Digikala dkp-13941792 "
                      "(HKE 5V 10A, pack of 4, 4,444,000 Rial). Global: Songle SRD-05VDC-SL-C = LCSC C35449. eca.ir / voltatech.ir had no 5 V T73 in stock "
                      "on 2026-10-07 (12 V only). CHECK the marking: 5VDC, 10A 250VAC."),
    "FuseHolder": dict(store="eca.ir", name="BLX-A FUSE جافیوز درب دار لولایی (PCB, with cover, for 5x20 glass fuse)", rial=91600, stock="in stock",
                       url=eca_search("جا فیوزی"),
                       alt="Product id 1016 in the category 'جا فیوزی'. Pin pitch must be 22.5 mm +-0.5 (slotted holes in the footprint): CHECK. "
                           "Alternative: open clips for 5x20 (id 1124, 73,600 Rial) - pitch about 20.5-22.5 mm, check too."),
    # ── MCU / WiFi / USB ──────────────────────────────────────────────────────────────────────────────────
    "STM32": dict(store="eca.ir", name="میکروکنترلر STM32F103C8T6 (LQFP48)", rial=2990000, stock="in stock (sold as original)",
                  url=eca("stm/3604-میکروکنترلر-stm32f103c8t6.html")),
    "Crystal": dict(store="eca.ir", name="کریستال 8MHZ SMD پکیج HC-49USM", rial=162000, stock="in stock",
                    url=eca_search("کریستال 8MHz"),
                    alt="Product id 2897. Load capacitance 12-20 pF (the 22 pF caps give about 16 pF with the board's stray capacitance)."),
    "ESP-12F": dict(store="voltatech.ir", name="ماژول وای فای ESP8266-12F", rial=3360000, stock="in stock",
                    url=volta("ماژول-وای-فای-esp8266-12f"),
                    alt="Alt (eca.ir): ESP-12E 3,710,000 Rial, pin-compatible: " + eca("ماژول-های-esp-و-اینترنت-اشیا/5694-ماژول-وای-فای-esp8266-esp-12e.html")),
    "CH340G": dict(store="eca.ir", name="تراشه مبدل USB به سریال CH340G پکیج SOP-16", rial=936000, stock="listed (stock not shown)",
                   url=eca("تراشه-واسط/6515-تراشه-مبدل-usb-به-سریال-ch340g-پکیج-sop-16.html"),
                   alt="USB-UART bridge of the ESP8266 USB port J14 (needs the 12 MHz crystal Y2). CH340C (internal oscillator, no crystal) is the same pinout but was not found on eca.ir / voltatech.ir."),
    "Crystal12": dict(store="eca.ir", name="کریستال 12MHZ SMD پکیج HC-49USM", rial=146000, stock="listed (5)",
                      url=eca("کریستال-و-اسیلاتور/1423-کریستال-12mhz-smd-پکیج-hc-49usm.html"), alt="12 MHz for the CH340G, with the two 22 pF C0G capacitors."),
    "USB-micro": dict(store="eca.ir", name="کانکتور Micro USB مادگی 5pin با دو هولدر سطحی SMD", rial=85200, stock="in stock",
                      url=eca("تبدیل-و-کانکتور-usb/3681-کانکتور-micro-usb-مادگی-5pin-با-دو-هولدر-سطحی-smd.html"),
                      alt="Footprint is the Wuerth 629105150521 outline: compare the pad drawing with the connector you receive."),
    # ── LEDs / switches / buzzer ─────────────────────────────────────────────────────────────────────────────
    "LED-green": dict(store="eca.ir", name="SMD LED سبز پکیج 0805", rial=7110, stock="in stock (50)", url=eca("led-smd/3940-smd-led-سبز-پکیج-0805.html")),
    "LED-red": dict(store="eca.ir", name="SMD LED قرمز پکیج 0805", rial=7110, stock="in stock (50)", url=eca("led-smd/3941-smd-led-قرمز-پکیج-0805.html")),
    "LED-yellow": dict(store="eca.ir", name="SMD LED زرد پکیج 0805", rial=7110, stock="in stock (50)", url=eca("led-smd/3942-smd-led-زرد-پکیج-0805.html")),
    "Tactile": dict(store="eca.ir", name="تک سوئیچ 6x6x5mm مشکی پکیج SMD", rial=22800, stock="in stock (10)", url=eca_search("تک سوئیچ SMD"),
                    alt="Product id 2753 (category تک-سوئیچ). Only 10 in stock when checked - ask for more. CHECK the pad layout of the part you get "
                        "(footprint = Omron B3S-1000 style)."),
    "Buzzer": dict(store="voltatech.ir", name="Buzzer بازر 5 ولت اورجینال (active, 12 mm)", rial=231210, stock="in stock",
                   url=volta("buzzer-بازر-5-ولت-اورجینال"),
                   alt="Alt (daneshjookit.com): بیزر قطب دار 5 ولتی. Must be ACTIVE (built-in oscillator), 7.6 mm pin pitch."),
    # ── resistors 0805 / 1206 ───────────────────────────────────────────────────────────────────────────────────
    "R100": dict(store="eca.ir", name="مقاومت 100 اهم SMD 0805", rial=4570, stock="in stock (50)", url=eca("مقاومت-smd-0805/3396-مقاومت-100-اهم-smd-0805.html")),
    "R330": dict(store="eca.ir", name="مقاومت 330 اهم SMD 0805", rial=4570, stock="in stock (50)", url=eca("مقاومت-smd-0805/4444-مقاومت-330-اهم-smd-0805.html")),
    "R470": dict(store="eca.ir", name="مقاومت 470 اهم SMD 0805", rial=4570, stock="in stock (50)", url=eca("مقاومت-smd-0805/3402-مقاومت-470-اهم-smd-0805.html")),
    "R1k": dict(store="eca.ir", name="مقاومت 1K اهم SMD 0805", rial=4570, stock="in stock (50+)", url=eca_search("مقاومت 1K اهم SMD 0805"), alt="Product id 3405."),
    "R2k2": dict(store="eca.ir", name="مقاومت 2.2K اهم SMD 0805", rial=4570, stock="in stock (50)", url=eca("مقاومت-smd-0805/3408-مقاومت-22k-اهم-smd-0805.html"), alt="Product id 3408 (check that it is 2.2k, not 22k). Output relay base resistors (PC13-15 may only drive 3 mA)."),
    "R10k": dict(store="eca.ir", name="مقاومت 10K اهم SMD 0805", rial=8200, stock="in stock", url=eca_search("مقاومت 10K اهم SMD 0805"), alt="Product id 3411."),
    "R33k-1206": dict(store="eca.ir", name="مقاومت 33K اهم پکیج SMD 1206", rial=8200, stock="in stock (50)", url=eca_search("مقاومت 33K اهم SMD 1206"),
                      alt="Product id 1856 (not the 3.3K listing). 12 V sense divider R90 (33k / 10k)."),
    "R0": dict(store="eca.ir", name="مقاومت 0 اهم SMD 0805 (jumper)", rial=4570, stock="not checked", url=eca_search("مقاومت 0 اهم SMD 0805"),
               alt="0R link (R97) between the CH340G V3 pin and VCC (3.3 V operation). Any 0805 0R jumper."),
    "R120k-1W": dict(store="eca.ir", name="مقاومت 120K اهم 1W (axial, through-hole)", rial=21600, stock="listed",
                     url=eca("مقاومت-1-وات/6567-مقاومت-120k-اهم-1w.html"),
                     alt="Press-sense input (RS1): one axial 120k 1 W 5 % resistor in the line leg replaces the four 33k 1206 resistors (about 0.45 W dissipated). Metal-oxide / "
                         "flame-proof type, 15.24 mm lead spacing. Pack of 50 ABCO MORS: eca id 13037 (1,940,000 Rial)."),
    # ── capacitors ─────────────────────────────────────────────────────────────────────────────────────────────
    "C100n": dict(store="eca.ir", name="خازن 100nF پکیج SMD 1206 (50V)", rial=20000, stock="in stock", url=eca("خازن-smd-پکیج-1206/1393-خازن-100nf-پکیج-smd-1206.html")),
    "C10u": dict(store="eca.ir", name="خازن 10uF پکیج SMD 1206 (50V)", rial=22800, stock="in stock (50)", url=eca_search("خازن 10uF SMD 1206"), alt="Product id 1691."),
    "C1u": dict(store="eca.ir", name="خازن 1uF پکیج SMD 1206 (50V)", rial=27500, stock="listed", url=eca("خازن-smd-پکیج-1206/1692-خازن-1uf-پکیج-smd-1206.html")),
    "C22p": dict(store="eca.ir", name="خازن 22pF پکیج 0805 SMD (C0G 5%)", rial=10220, stock="in stock (50)", url=eca("خازن-smd-پکیج-0805/3548-خازن-22pf-پکیج-0805-smd.html")),
    "E100u16": dict(store="eca.ir", name="خازن SMD الکترولیت 100uF / 16V (6.3x5.4)", rial=71200, stock="in stock (10)", url=eca_search("خازن SMD الکترولیت 100uF 16V"), alt="Product id 3239."),
    "E47u16": dict(store="eca.ir", name="خازن SMD الکترولیت 47uF / 16V (6.3x5.4)", rial=74400, stock="in stock", url=eca_search("خازن SMD الکترولیت 47uF 16V"), alt="Product id 2614."),
    "E22u35": dict(store="eca.ir", name="خازن SMD الکترولیت 22uF / 35V (5x5)", rial=69800, stock="in stock (5)", url=eca_search("خازن SMD الکترولیت 22uF 35V"), alt="Product id 13835."),
    # ── connectors ──────────────────────────────────────────────────────────────────────────────────────────────
    "KF128-2P": dict(store="eca.ir", name="ترمینال پیچی KF128-2Pin (5.08mm)", rial=190000, stock="in stock",
                     url=eca("ترمینال-پیچی-و-کشویی/26534-ترمینال-پیچی-مدل-kf128-2pin-رنگ-مشکی.html")),
    "KF128-3P": dict(store="eca.ir", name="ترمینال پیچی KF128-3Pin (5.08mm)", rial=272000, stock="in stock",
                     url=eca("ترمینال-پیچی-و-کشویی/26535-ترمینال-پیچی-مدل-kf128-3pin-رنگ-مشکی.html")),
    "IDC26": dict(store="eca.ir", name="باکس هدر Box 26Pin 2X13", rial=168000, stock="listed",
                  url=eca("باکس-هدر-و-idc/2007-باکس-هدر-box-26pin-2x13.html")),
    "Header-M": dict(store="eca.ir", name="پین هدر 1x40 نری استاندارد (cut to the needed pieces)", rial=125000, stock="in stock",
                     url=eca("پین-هدر/360-پین-هدر-1x40-نری-استاندارد.html")),
    "ZMPT101B": dict(store="eca.ir", name="ماژول اندازه گیری ولتاژ 250AC مدل ZMPT101B", rial=3810000, stock="listed (stock only visible when logged in)",
                     url=eca("ماژول-تغذیه-ولتاژ-و-شارژ/17646-ماژول-اندازه-گیری-ولتاژ-250ac-مدل-zmpt101b.html"),
                     alt="Module with the ZMPT101B transformer, op-amp and trimmer, 3-pin output (VCC OUT GND), its own screw terminals for the 230 V: wired to J8 on the main board. "
                         "Power it from 3.3 V (J8 pin 1) so that its mid-supply bias is 1.65 V. A bare ZMPT101B transformer is not sold on eca.ir."),
    "Header-F": dict(store="eca.ir", name="پین هدر 1x40 مادگی (cut to the needed 1x8 pieces)", rial=243000, stock="in stock",
                     url=eca("پین-هدر/359-پین-هدر-1x40-مادگی.html")),
    # ── off-board items ─────────────────────────────────────────────────────────────────────────────────────
    "RC522": dict(store="eca.ir", name="ماژول RFID RC522 Mifare 13.56MHz به همراه تگ", rial=2770000, stock="in stock",
                  url=eca("ماژول-های-rfid/2160-ماژول-rfid-با-قابلیت-خواندن-و-نوشتن-rfid-readerwriter-rc522-mifare-1356mhz-به-همراه-تگ.html")),
    "LM35DZ": dict(store="eca.ir", name="سنسور دما LM35DZ", rial=883000, stock="in stock", url=eca("ماژول-دما-و-رطوبت/368-سنسور-دما-lm35dz.html")),
    "Display": dict(store="eca.ir", name="ماژول نمایشگر LCD 2.4 درایور ST7789 (SPI, no touch)", rial=13100000, stock="in stock",
                    url=eca("ماژول-های-نمایشگر-HMI/28871-ماژول-نمایشگر-lcd-24-درایور-st7889.html"),
                    alt="VERIFY THE PIN ORDER before ordering boards: panel socket J2 = GND VCC SCL SDA RES DC CS BLK (the order printed on most 2.4in ST7789 SPI "
                        "modules); the eca.ir page lists no pin names. The eca.ir 2.4in ST7789V IPS (14,365,000 Rial) has a 24-pin parallel flex - NOT usable."),
    "Fuse": dict(store="eca.ir", name="فیوز شیشه ای 1A - 250V سایز 5x20", rial=38600, stock="in stock (10)",
                 url=eca("فیوز-شیشه-ای/1031-فیوز-شیشه-ای-1a-250v-سایز-5x20.html"),
                 alt="Use the TIME-LAG (slow-blow, T1A 250V) type: the HLK-5M05 and the varistor give a switch-on surge that a fast fuse may not tolerate."),
    "Ribbon": dict(store="eca.ir", name="کابل فلت 26 رشته + 2 عدد سوکت IDC مادگی 26 پین (2x13) - NOT FOUND ONLINE", rial=0, stock="not verified",
                   url=eca_search("کابل فلت"),
                   alt="The eca.ir search for '26-wire flat cable' returned nothing; ask in a local shop (cable ~20 cm + 2 crimp sockets, or a ready-made 26-pin IDC ribbon). Price unknown."),
}


def key_for(p, tag):
    r, v = p.ref, p.value
    if r.startswith(("MH", "TP", "FID", "SJ")):
        return None
    table = {"PS1": "HLK-5M05", "BR1": "MB10S", "U2": "PC817-SMD", "U3": "AMS1117", "F1": "FuseHolder",
             "U4": "STM32", "Y1": "Crystal", "U1": "ESP-12F", "J3": "USB-micro", "C1": "E100u16", "C4": "E47u16",
             "C5": "E22u35", "C12": "C1u", "J14": "USB-micro", "U5": "CH340G", "Y2": "Crystal12", "RS1": "R120k-1W"}
    if tag == "MAIN" and r in table:
        return table[r]
    if tag == "MAIN" and r == "J8":
        return "Header-M"
    if tag == "MAIN" and r.startswith("K"):
        return "Relay"
    if r == "BZ1":
        return "Buzzer"
    if r.startswith("RV"):
        return "Varistor"
    if "SS14" in v:
        return "SS14"
    if v == "M7":
        return "M7"
    if v == "SS8050":
        return "SS8050"
    if r.startswith("D") or r.startswith("LED"):
        return {"GREEN": "LED-green", "RED": "LED-red", "YELLOW": "LED-yellow"}[v.split()[0].upper()]
    if r.startswith("SW"):
        return "Tactile"
    if "Terminal block 2P" in p.mpn:
        return "KF128-2P"
    if "Terminal block 3P" in p.mpn:
        return "KF128-3P"
    if p.fp.startswith("Connector_PinHeader"):
        return "Header-M"
    if p.fp.startswith("Connector_PinSocket"):
        return "Header-F"
    if p.fp.startswith("Connector_IDC"):
        return "IDC26"
    if v == "22pF":
        return "C22p"
    if v == "100nF":
        return "C100n"
    if v == "10uF":
        return "C10u"
    if p.fp.endswith("R_1206_3216Metric"):
        return {"33k": "R33k-1206"}[v]
    if p.fp.endswith("R_0805_2012Metric"):
        return {"0": "R0", "100": "R100", "330": "R330", "470": "R470", "1k": "R1k", "2.2k": "R2k2", "10k": "R10k"}[v]
    raise KeyError(f"{r} {v} {p.fp}")


SHORT = {
    "HLK-5M05": "AC-DC module 5V 1A (Hi-Link HLK-5M05)", "Varistor": "Varistor 07D471K (275 VAC)", "MB10S": "Bridge rectifier MB10S SOIC-4",
    "PC817-SMD": "Optocoupler PC817 SMD SOP-4", "AMS1117": "LDO 3.3 V AMS1117 SOT-223", "SS14": "Schottky SS14 1A 40V SMA",
    "M7": "Diode 1N4007 SMD (M7) SMA", "SS8050": "NPN transistor SS8050 SOT-23", "Relay": "Relay T73 SRD-05VDC-SL-C, 5 V coil, 10A 250VAC, SPDT (K1 + K2-K7 outputs)",
    "FuseHolder": "Fuse holder 5x20 PCB, covered (BLX-A type, 22.5 mm)", "STM32": "STM32F103C8T6 LQFP-48", "Crystal": "Crystal 8 MHz HC-49 SMD",
    "ESP-12F": "ESP-12F WiFi module", "USB-micro": "Micro-USB B receptacle SMD", "LED-green": "LED 0805 green", "LED-red": "LED 0805 red",
    "LED-yellow": "LED 0805 yellow", "Tactile": "Tactile switch 6x6x5 SMD", "Buzzer": "Active buzzer 5V 12 mm", "R100": "Resistor 100R 0805",
    "R0": "Resistor 0R 0805 (link)", "R33k-1206": "Resistor 33k 1206 (12 V sense divider)", "R120k-1W": "Resistor 120k 1W 5% axial (mains sense, through-hole)", "CH340G": "USB-UART bridge CH340G SOP-16",
    "Crystal12": "Crystal 12 MHz HC-49USM SMD", "R330": "Resistor 330R 0805", "R470": "Resistor 470R 0805", "R1k": "Resistor 1k 0805", "R2k2": "Resistor 2.2k 0805", "R10k": "Resistor 10k 0805",
"C100n": "Capacitor 100nF 50V 1206", "C10u": "Capacitor 10uF 50V 1206", "C1u": "Capacitor 1uF 1206",
    "C22p": "Capacitor 22pF C0G 0805", "E100u16": "SMD electrolytic 100uF 16V 6.3x5.4", "E47u16": "SMD electrolytic 47uF 16V 6.3x5.4",
    "E22u35": "SMD electrolytic 22uF 35V 5x5", "KF128-2P": "Screw terminal 2-pin 5.08 mm", "KF128-3P": "Screw terminal 3-pin 5.08 mm",
    "IDC26": "IDC box header 2x13 (26 pin)", "Header-M": "Pin header male 1x40 (cut: SWD 1x5 J9 for the ST-Link - not fitted in production; 1x3 J8 for the ZMPT101B module - fitted)",
    "Header-F": "Pin socket female 1x40 (cut: two 1x8 for display + RC522)", "RC522": "RFID reader RC522 module + tags",
    "LM35DZ": "Temperature sensor LM35DZ (TO-92, on a cable)", "ZMPT101B": "AC voltage module ZMPT101B (230 V sense, wired to J8)", "Display": "Display module 2.4in ST7789 SPI 8-pin (OPTIONAL)",
    "Fuse": "Glass fuse 1A 250V time-lag 5x20", "Ribbon": "26-wire ribbon cable + 2 IDC sockets (panel cable)",
}
SPARES = {"R100": 3, "R330": 2, "R470": 2, "R1k": 4, "R10k": 4, "R2k2": 4, "R0": 2, "R120k-1W": 2, "R33k-1206": 1, "C100n": 4, "C10u": 2, "C1u": 2, "C22p": 2,
          "LED-green": 2, "LED-red": 2, "LED-yellow": 1, "SS8050": 2, "M7": 2, "SS14": 2, "MB10S": 3}
MIN_BUY = {"MB10S": 5, "Relay": 9}     # relays: packs of 3 (Digikala)
HEADERS_ONE = ("Header-M", "Header-F")
OFFBOARD = (("Display", 1), ("RC522", 1), ("LM35DZ", 1), ("ZMPT101B", 1), ("Fuse", 2), ("Ribbon", 1))
DEV_ONLY = ("J9",)      # programming / debug headers: pads are test points in production
DISPLAY_OPTIONAL = ("J2", "R5", "SJ1")   # PANEL: fitted only when a display module is used (the firmware runs without one)
TOP_SIDE_PANEL = ("J1",)                 # PANEL: only the ribbon header is on the top face, everything else is on the bottom


def collect():
    groups = OrderedDict()
    parts = []
    for tag, fn in (("MAIN", design.main_board), ("PANEL", design.panel_board)):
        for p in fn().parts:
            k = key_for(p, tag)
            if k is None:
                continue
            g = groups.setdefault(k, dict(refs=[], fps=set()))
            g["refs"].append(f"{tag[0]}:{p.ref}")
            g["fps"].add(p.fp.split(":")[-1])
            parts.append((tag, p, k))
    return groups, parts


def build():
    groups, _ = collect()
    rows = []
    for k, g in groups.items():
        qty = len(g["refs"])
        buy = max(qty + SPARES.get(k, 0), MIN_BUY.get(k, 0))
        if k in HEADERS_ONE:
            buy = 1
        rows.append(dict(key=k, refs=", ".join(g["refs"]), value=SHORT[k], fp=", ".join(sorted(g["fps"])), qty=qty, buy=buy, **SRC[k]))
    for k, q in OFFBOARD:
        rows.append(dict(key=k, refs="(off-board)", value=SHORT[k], fp="-", qty=q, buy=q, **SRC[k]))
    return rows


def write_assembly_csv(outdir):
    """Designator / value / footprint / description per board for an assembly house (DNP parts flagged)."""
    _, parts = collect()
    os.makedirs(outdir, exist_ok=True)
    for tag in ("MAIN", "PANEL"):
        path = os.path.join(outdir, f"assembly_bom_{tag.lower()}.csv")
        with open(path, "w", newline="", encoding="utf-8-sig") as f:
            w = csv.writer(f)
            w.writerow(["Designator", "Value", "Footprint", "Description", "Mounting + side", "DNP", "Group"])
            for t, p, k in parts:
                if t != tag:
                    continue
                tht = any(s in p.fp for s in ("TerminalBlock", "PinHeader", "PinSocket", "IDC", "Relay", "Varistor", "Converter_ACDC", "Buzzer", "FuseHolder", "Resistor_THT"))
                dev = t == "MAIN" and p.ref in DEV_ONLY
                opt = t == "PANEL" and p.ref in DISPLAY_OPTIONAL
                side = "bottom" if (t == "PANEL" and p.ref not in TOP_SIDE_PANEL and not p.ref.startswith("MH")) else "top"
                w.writerow([p.ref, p.value, p.fp.split(":")[-1], p.mpn, ("THT" if tht else "SMD") + " " + side,
                            "yes" if (p.dnp or dev) else ("optional (display)" if opt else ""), p.group])


def write(rows, path):
    wb = Workbook()
    ws = wb.active
    ws.title = "BOM"
    hdr = ["#", "Designators (M = main board, P = panel)", "Qty on boards", "Qty to buy (incl. spares)", "Part / value", "Footprint", "Store", "Shop product name",
           "Unit price (Rial)", "Unit price (Toman)", "Line total (Toman)", "Stock when checked", "Buy link", "Notes / alternative"]
    ws.append(["Hydraulic press counter v5 - BOM (MAIN + PANEL boards)   |   prices read on " + DATE + " - they change, re-check before ordering"])
    ws.append([])
    ws.append(hdr)
    thin = Side(style="thin", color="999999")
    for c in ws[3]:
        c.font = Font(bold=True, color="FFFFFF")
        c.fill = PatternFill("solid", fgColor="2F5597")
        c.alignment = Alignment(wrap_text=True, vertical="center")
    first = 4
    for i, r in enumerate(rows, 1):
        row = first + i - 1
        ws.append([i, r["refs"], r["qty"], r["buy"], r["value"], r["fp"], r["store"], r["name"], r["rial"] or None, None, None, r["stock"], r["url"], r.get("alt", "")])
        ws.cell(row, 10, f"=I{row}/10")
        ws.cell(row, 11, f"=D{row}*J{row}")
        ws.cell(row, 13).hyperlink = r["url"]
        ws.cell(row, 13).value = "open page" if "search?" not in r["url"] and "?s=" not in r["url"] else "open shop search"
        ws.cell(row, 13).font = Font(color="0563C1", underline="single")
    last = first + len(rows) - 1
    tot = last + 2
    ws.cell(tot, 8, "TOTAL (Toman, items with a price)").font = Font(bold=True)
    ws.cell(tot, 11, f"=SUM(K{first}:K{last})").font = Font(bold=True)
    stores = OrderedDict((r["store"], None) for r in rows)
    for j, st in enumerate(stores):
        ws.cell(tot + 1 + j, 8, f"  subtotal {st}")
        ws.cell(tot + 1 + j, 11, f'=SUMIF(G{first}:G{last},"{st}",K{first}:K{last})')
    note_row = tot + len(stores) + 2
    notes = [
        "Stores: eca.ir for nearly everything; voltatech.ir for ESP-12F, SS14 diodes and the active buzzer; digikala.com (marketplace seller) for the 5 V T73 relay. Shipping is not included.",
        "Prices are in Rial as listed (Toman = Rial / 10). 'Qty to buy' adds spares for the cheap SMD parts; expensive parts have no spare. Pin header J9 (SWD) is for the ST-Link only.",
        "NOT on the list (not found online / do locally): ribbon cable + IDC sockets, standoffs, enclosure. Tools: ST-Link V2 (once per board: the HID bootloader + first image into the STM32 over SWD) and two micro-USB cables (J14 = ESP8266, J3 = STM32; no other adapter is needed). The HID bootloader binary is a free download (see docs/FIRMWARE.md).",
        "The display is OPTIONAL: without it leave out the display module, socket J2, R5 and SJ1 and set 'Display module = No' on the web page (Settings); every function stays available on the web page. Subtract about 1.31 M Toman and the 1x8 socket.",
        "PANEL faces: TOP = only the ribbon header J1; BOTTOM = everything else (buttons, display socket, RC522 socket, buzzer, LEDs and all SMD parts: single-sided SMD assembly). Solder the THT parts from the top.",
        "Check before ordering the boards: (1) display pin order, (2) micro-USB / PC817 SMD / buzzer / tact switch / fuse holder footprints against the parts you actually receive, (3) relay marking (5VDC, 10A 250VAC).",
        "eca.ir links marked 'open shop search' open the shop's search for that value because the exact product slug was not captured - the product id is given in the notes.",
        "Sheet 'Production' scales this list to N units; an assembly house needs fabrication/assembly_bom_main.csv, assembly_bom_panel.csv and the position files.",
    ]
    for n, t in enumerate(notes):
        ws.cell(note_row + n, 1, t)
    widths = [4, 30, 9, 11, 34, 34, 14, 54, 14, 14, 16, 20, 18, 70]
    for i, w in enumerate(widths, 1):
        ws.column_dimensions[get_column_letter(i)].width = w
    for row in ws.iter_rows(min_row=3, max_row=last):
        for c in row:
            c.border = Border(top=thin, bottom=thin, left=thin, right=thin)
            c.alignment = Alignment(wrap_text=True, vertical="top")
    for col in "IJK":
        for r in range(first, tot + len(stores) + 2):
            ws[f"{col}{r}"].number_format = "#,##0"
    ws.freeze_panes = "A4"

    # ── production sheet: N units ────────────────────────────────────────────────────────────────────────
    ps = wb.create_sheet("Production")
    ps["A1"] = "Production quantities (edit the two yellow cells)"
    ps["A1"].font = Font(bold=True, size=12)
    ps["A2"] = "Units to build"
    ps["B2"] = 100
    ps["A3"] = "Overage for placement losses on cheap parts (fraction)"
    ps["B3"] = 0.02
    for c in ("B2", "B3"):
        ps[c].fill = PatternFill("solid", fgColor="FFF2CC")
    for j, h in enumerate(["#", "Part / value", "Per unit", "To buy for N units", "Unit price (Toman)", "Line total (Toman)", "Store", "Note"], 1):
        c = ps.cell(5, j, h)
        c.font = Font(bold=True, color="FFFFFF")
        c.fill = PatternFill("solid", fgColor="2F5597")
    for i, r in enumerate(rows, 1):
        rr = 5 + i
        cheap = bool(r["rial"]) and r["rial"] <= 100000
        ps.cell(rr, 1, i)
        ps.cell(rr, 2, r["value"])
        ps.cell(rr, 3, r["qty"])
        ps.cell(rr, 4, f"=ROUNDUP(C{rr}*$B$2*(1+{'$B$3' if cheap else '0'}),0)")
        ps.cell(rr, 5, f"=BOM!J{3 + i}")
        ps.cell(rr, 6, f"=D{rr}*E{rr}")
        ps.cell(rr, 7, r["store"])
        ps.cell(rr, 8, "development header - leave off in production" if r["key"] == "Header-M" else ("per machine, not per board" if r["refs"] == "(off-board)" else ""))
        for col in (5, 6):
            ps.cell(rr, col).number_format = "#,##0"
    end = 5 + len(rows)
    ps.cell(end + 2, 5, "TOTAL (Toman)").font = Font(bold=True)
    ps.cell(end + 2, 6, f"=SUM(F6:F{end})").font = Font(bold=True)
    ps.cell(end + 2, 6).number_format = "#,##0"
    ps.cell(end + 3, 5, "per unit")
    ps.cell(end + 3, 6, f"=F{end + 2}/B2")
    ps.cell(end + 3, 6).number_format = "#,##0"
    for i, w in enumerate([5, 52, 10, 18, 18, 18, 14, 40], 1):
        ps.column_dimensions[get_column_letter(i)].width = w
    wb.save(path)


if __name__ == "__main__":
    rows = build()
    out = os.path.normpath(os.path.join(HERE, "..", "BOM_press_counter_v5.xlsx"))
    write(rows, out)
    write_assembly_csv(os.path.normpath(os.path.join(HERE, "..", "fabrication")))
    tot = sum(r["buy"] * r["rial"] / 10 for r in rows)
    by = {}
    for r in rows:
        by[r["store"]] = by.get(r["store"], 0) + r["buy"] * r["rial"] / 10
    print("wrote", out, len(rows), "lines")
    print("total Toman:", f"{tot:,.0f}", {k: f"{v:,.0f}" for k, v in by.items()})
