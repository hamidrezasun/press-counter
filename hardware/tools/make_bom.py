"""Generate hardware/BOM_press_counter_v6.xlsx (the board + off-board parts, Iranian-store links + prices) from design.py.

Run with the system python (needs openpyxl):   python make_bom.py
Prices/stock were read from the shops' websites on 2026-10-06 ... 11 and WILL change - re-check before ordering.
Items without an exact product page (the shop search did not give a full URL) link to the shop's search for that item.

Sheets:  "BOM"        - one set of boards (quantities to buy incl. spares)
         "Production" - the same list scaled to N units (edit cells B2 / B3)
For an assembly house the script also writes hardware/fabrication/assembly_bom.csv
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
DATE = "2026-10-19"


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
    "SS34": dict(store="eca.ir", name="دیود شاتکی 3A-40V مدل SS34 پکیج SMA", rial=23900, stock="in stock (20)",
                 url=eca("دیود-شاتکی/2420-دیود-شاتکی-3a-40v-مدل-ss34-پکیج-sma.html"),
                 alt="D14 (USB) and D8 (5 V terminal). SS14 / SS24 (same SMA package) work too."),
    "SMBJ6.5A": dict(store="eca.ir", name="دیود TVS Unidirect پکیج SMB کد SMBJ6.5A", rial=44000, stock="in stock (10)",
                     url=eca("دیود-tvs/21504-دیود-tvs-unidirect-پکیج-smb-کد-smbj65a.html"),
                     alt="D7 on the 5 V terminal (cathode band to +). SMBJ6.0A / SMBJ7.0A also fit; NOT SMBJ5.0A (leaks at 5.2 V)."),
    "PTC": dict(store="voltatech.ir", name="فیوز برگشت پذیر PPTC RLD60P110XF 1.1A 60V", rial=80250, stock="in stock",
                url=volta("فیوز-برگشت-پذیر-pptc-rld60p110xf-1-1a-60v"),
                alt="F2 on the 5 V terminal: radial resettable fuse, hold 1.1 A, 60 V, lead pitch 5.1 mm. Any radial PTC with hold 1.1-1.6 A and >= 30 V fits "
                    "(e.g. MF-R110, RXEF110). eca.ir: no PTC found on 2026-10-12."),
    "M7": dict(store="eca.ir", name="دیود 1N4007 SMD (M7)", rial=9970, stock="in stock (50)",
               url=eca("سایر-دیودها/1937-دیود-1n4007-smd-m7.html"), alt="voltatech.ir: 1,089 Toman."),
    "SS8050": dict(store="eca.ir", name="ترانزیستور SS8050 نوع NPN پکیج SOT-23", rial=13400, stock="in stock (10)",
                   url=eca("ترانزیستور-bjt/6548-ترانزیستور-ss8050-نوع-npn-پکیج-sot-23.html")),
    "Relay": dict(store="voltatech.ir", name="رله 5 ولت میلون 10 آمپر برند LIMING (T73 type, 5 V coil, SPDT, 5 pins)", rial=640000, stock="in stock (2026-10-18)",
                  url=volta("رله-5-ولت-میلون-10-آمپر-برند-liming"),
                  alt="'Milon' = T73 / SRD-05VDC-SL-C pin pattern. CHECK the marking (5VDC, 10A 250VAC) and the pins on the 1:1 sheet. NOT the 'baccheh milon 20 A' relays (smaller / different). "
                      "Same relay at javanelec.com: 'RELAY 5V 7A 1C 5PIN' 112,500 Toman; Digikala dkp-13772470 (HKE / Liming, pack of 3) was the earlier source. voltatech HKF 5 V version out of stock."),
    "FuseHolder": dict(store="eca.ir", name="BLX-A FUSE جافیوز درب دار لولایی (PCB, with cover, for 5x20 glass fuse)", rial=91600, stock="in stock",
                       url=eca_search("جا فیوزی"),
                       alt="Product id 1016 in the category 'جا فیوزی'. Pin pitch must be 22.5 mm +-0.5 (slotted holes in the footprint): CHECK. "
                           "Alternative: open clips for 5x20 (id 1124, 73,600 Rial) - pitch about 20.5-22.5 mm, check too."),
    # ── MCU / WiFi / USB ──────────────────────────────────────────────────────────────────────────────────
    "ESP32": dict(store="voltatech.ir", name="ماژول وایفای و بلوتوث ESP32-WROOM-32", rial=6300000, stock="in stock",
                  url=volta("ماژول-وایفای-و-بلوتوث-esp32-wroom-32"),
                  alt="ESP32-WROOM-32D / -32E fit the same footprint (4 MB flash needed); for a metal cabinet use the -32U (U.FL connector + external antenna). eca.ir had only the -32U (external antenna, out of stock) on 2026-10-11."),
    "OLED": dict(store="eca.ir", name="ماژول OLED 0.96 inch آبی رزولیشن 128x64 دارای رابط I2C", rial=4490000, stock="in stock",
                 url=eca("نمایشگر-oled/20271-ماژول-oled-096-inch-آبی-رزولیشن-128x64-دارای-رابط-i2c.html"),
                 alt="SSD1306 / SSD1315 I2C 4-pin module. The shop photo (2026-10-13) prints GND VDD SCK SDA = socket order. CHECK the pin order: the board socket P2 is GND VCC SCL SDA (some modules are VCC GND SCL SDA - those must not be used, or swap with a 4-wire jumper)."),
    "DS18B20": dict(store="voltatech.ir", name="سنسور دما دیجیتال DS18B20 ضد آب با کابل 1 متری", rial=2583390, stock="in stock (2026-10-18)",
                    url=volta("سنسور-دما-دیجیتال-ds18b20-ضد-آب-با-کابل-1-متری"),
                    alt="Waterproof probe (stainless tube, 3 wires: red 3V3, yellow DATA, black GND) on terminal J4. eca.ir: the probe was out of stock on 2026-10-11."),
    "CH340G": dict(store="eca.ir", name="تراشه مبدل USB به سریال CH340G پکیج SOP-16", rial=936000, stock="listed (stock not shown)",
                   url=eca("تراشه-واسط/6515-تراشه-مبدل-usb-به-سریال-ch340g-پکیج-sop-16.html"),
                   alt="USB-UART bridge with auto-reset for programming over USB (needs the 12 MHz crystal Y2). CH340C (internal oscillator) was not found on eca.ir / voltatech.ir."),
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
    "Buzzer": dict(store="voltatech.ir", name="Buzzer بازر 5 ولت اورجینال (active, 12 mm)", rial=239990, stock="in stock (2026-10-18)",
                   url=volta("buzzer-بازر-5-ولت-اورجینال"),
                   alt="Alt (daneshjookit.com): بیزر قطب دار 5 ولتی. Must be ACTIVE (built-in oscillator), 7.6 mm pin pitch."),
    # ── resistors 0805 / 1206 ───────────────────────────────────────────────────────────────────────────────────
    "R100": dict(store="eca.ir", name="مقاومت 100 اهم SMD 0805", rial=4570, stock="in stock (50)", url=eca("مقاومت-smd-0805/3396-مقاومت-100-اهم-smd-0805.html")),
    "R470": dict(store="eca.ir", name="مقاومت 470 اهم SMD 0805", rial=4570, stock="in stock (50)", url=eca("مقاومت-smd-0805/3402-مقاومت-470-اهم-smd-0805.html")),
    "R1k": dict(store="eca.ir", name="مقاومت 1K اهم SMD 0805", rial=4570, stock="in stock (50+)", url=eca_search("مقاومت 1K اهم SMD 0805"), alt="Product id 3405."),
    "R2k2": dict(store="eca.ir", name="مقاومت 2.2K اهم SMD 0805", rial=4570, stock="in stock (50)", url=eca("مقاومت-smd-0805/3408-مقاومت-22k-اهم-smd-0805.html"),
                 alt="Product id 3408 (check that it is 2.2k, not 22k). I2C and 1-wire pull-ups."),
    "R100k": dict(store="eca.ir", name="مقاومت 100K اهم SMD 0805", rial=4570, stock="not checked", url=eca_search("مقاومت 100K اهم SMD 0805"),
                  alt="Voltage input dividers R90 / R98 (100k / 10k = 0-30 V). 1 % preferred (any 0805 100k works; calibrate with v12cal / auxcal)."),
    "R22k": dict(store="eca.ir", name="مقاومت 22K اهم SMD 0805", rial=4570, stock="not checked", url=eca_search("مقاومت 22K اهم SMD 0805"),
                 alt="R1, pull-up of the press input IO34 (value chosen by SPICE, see hardware/sim). Do not confuse with the 2.2k listing."),
    "R10k": dict(store="eca.ir", name="مقاومت 10K اهم SMD 0805", rial=8200, stock="in stock", url=eca_search("مقاومت 10K اهم SMD 0805"), alt="Product id 3411."),
    "R0": dict(store="eca.ir", name="مقاومت 0 اهم SMD 0805 (jumper)", rial=4570, stock="not checked", url=eca_search("مقاومت 0 اهم SMD 0805"),
               alt="0R link (R97) between the CH340G V3 pin and VCC (3.3 V operation). Any 0805 0R jumper."),
    "R120k-1W": dict(store="eca.ir", name="مقاومت 120K اهم 1W (axial, through-hole)", rial=21600, stock="listed",
                     url=eca("مقاومت-1-وات/6567-مقاومت-120k-اهم-1w.html"),
                     alt="Press-sense input RS1: one axial 120k 1 W resistor in the line leg (about 0.45 W at 230 V). Metal-oxide / flame-proof, working voltage >= 400 V (check the "
                         "datasheet), 15.24 mm lead spacing. Pack of 50 (ABCO MORS): eca id 13037, 1,940,000 Rial."),
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
    "Header-F": dict(store="eca.ir", name="پین هدر 1x40 مادگی (cut to the needed 1x8 pieces)", rial=243000, stock="in stock",
                     url=eca("پین-هدر/359-پین-هدر-1x40-مادگی.html")),
    # ── off-board items ─────────────────────────────────────────────────────────────────────────────────────
    "RC522": dict(store="eca.ir", name="ماژول RFID RC522 Mifare 13.56MHz به همراه تگ", rial=2770000, stock="in stock",
                  url=eca("ماژول-های-rfid/2160-ماژول-rfid-با-قابلیت-خواندن-و-نوشتن-rfid-readerwriter-rc522-mifare-1356mhz-به-همراه-تگ.html")),
    "Fuse": dict(store="eca.ir", name="فیوز شیشه ای 1A - 250V سایز 5x20", rial=38600, stock="in stock (10)",
                 url=eca("فیوز-شیشه-ای/1031-فیوز-شیشه-ای-1a-250v-سایز-5x20.html"),
                 alt="Use the TIME-LAG (slow-blow, T1A 250V) type: the HLK-5M05 and the varistor give a switch-on surge that a fast fuse may not tolerate."),
}


# ── 2026-10-19: every line re-checked on the shop's own page (price, orderable).  Overrides of the entries above. ─────────────────────────
def eca_id(i):
    return f"https://eshop.eca.ir/index.php?controller=product&id_product={i}"          # redirects to the product page


def volta2(slug):
    return "https://voltatech.ir/product/" + quote(slug) + "/"


ECA_IDS = {"KF128-2P": 26534, "KF128-3P": 26535, "FuseHolder": 1016, "Varistor": 11337, "E100u16": 3239, "E47u16": 2614, "E22u35": 13835,
           "C100n": 1393, "C10u": 1691, "C1u": 1692, "C22p": 3548, "SMBJ6.5A": 21504, "SS34": 2420, "AMS1117": 978, "LED-green": 3940,
           "LED-red": 3941, "LED-yellow": 3942, "R100": 3396, "R470": 3402, "R1k": 3405, "R2k2": 3408, "R10k": 3411, "R22k": 4438, "R100k": 3414,
           "M7": 1937, "R120k-1W": 6567, "MB10S": 14760, "Crystal12": 1423, "Tactile": 2753, "Fuse": 1031}
# minimum order quantity of the ECA listing (the number on the product tile); 0 on the tile = cannot be bought
ECA_MIN = {"KF128-2P": 5, "KF128-3P": 5, "FuseHolder": 5, "Varistor": 5, "E100u16": 10, "E47u16": 5, "E22u35": 5, "C100n": 50, "C10u": 50, "C1u": 50,
           "C22p": 50, "SMBJ6.5A": 10, "SS34": 20, "AMS1117": 5, "LED-green": 50, "LED-red": 50, "LED-yellow": 50, "R100": 50, "R470": 50, "R1k": 50,
           "R2k2": 50, "R10k": 50, "R22k": 50, "R100k": 50, "M7": 50, "R120k-1W": 25, "MB10S": 5, "Crystal12": 5, "Tactile": 10, "Fuse": 10}
for _k, _i in ECA_IDS.items():
    SRC[_k]["url"] = eca_id(_i)
    SRC[_k]["stock"] = f"in stock (checked 2026-10-19), min. order {ECA_MIN[_k]}"
SRC["Varistor"]["rial"] = 59800
SRC["FuseHolder"]["rial"] = 98800
SRC["FuseHolder"]["name"] = "BLX-A FUSE جافیوزی درب دار لولایی (PCB, with cover, for 5x20 glass fuse)"
SRC["Fuse"]["alt"] = ("Listed as plain '1A 250V 5x20': the design wants a TIME-LAG (T1A) fuse - ask the shop for the slow-blow type, or buy javanelec.com 'FUSE 5*20 1A SLOW' "
                      "(Walter 5TPT-1A, ceramic, 21,400 Toman, has wire leads: cut them off).")

# parts that ECA had sold out on 2026-10-19 (tile quantity 0) -> other shops, each checked on its own page
SRC["HLK-5M05"] = dict(store="javanelec.com", name="HLK-5M05 Hi-Link (مبدل 220 ولت AC به 5 ولت DC 1 آمپر)", rial=6493000, stock="in stock (add-to-cart, 2026-10-19)",
                       url="https://www.javanelec.com/shop/product/35885/hi-link-electronic-technology/hlk-5m05/%d9%85%d8%a8%d8%af%d9%84-220-%d9%88%d9%84%d8%aa-ac-%d8%a8%d9%87-5-%d9%88%d9%84%d8%aa-dc-%d8%a8%d8%a7-%d8%ad%d8%af%d8%a7%da%a9%d8%ab%d8%b1-%d8%ac%d8%b1%db%8c%d8%a7%d9%86-1-%d8%a2%d9%85%d9%be%d8%b1",
                       alt="eca.ir (684,000) was out of stock on 2026-10-19. Other listings: leon (697,136), RoboEQ (695,000), skytech (782,660). Not the 3 W HLK-PM01 / PM03.")
SRC["CH340G"] = dict(store="javanelec.com", name="CH340G (SOP-16) Nanjing Qinheng", rial=800000, stock="in stock (add-to-cart, 2026-10-19)",
                     url="https://www.javanelec.com/shop/product/22921/nanjing-qinheng-microelectronics/ch340g",
                     alt="eca.ir (93,600) out of stock on 2026-10-19. Other listings: Atrin 87,300, Mohammad Electronic 105,105 ('CH340G smd').")
SRC["R0"] = dict(store="javanelec.com", name="RESISTOR 0OHMS 0805 5% SMD (SER0805J0000AR)", rial=4070, stock="in stock (price listed, 2026-10-19)",
                 url="https://www.javanelec.com/shop/product/3967/non-brand/resistor-0ohms-0805-5%25-smd/%d9%85%d9%82%d8%a7%d9%88%d9%85%d8%aa-0-%d8%a7%d9%87%d9%85-0805-smd",
                 alt="R97 only joins the CH340G V3 pin to 3.3 V: any 0805 resistor <= 10 ohm works too (eca.ir has 10 ohm 0805, id 4709).")
SRC["Relay"] = dict(store="robochip.ir", name="رله 5 ولت میلون (SPDT, T73 pin pattern; part code Relay-5V-Milon-SPDT-5A-240VAC)", rial=600000, stock="in stock (add-to-cart, price dated 1405-02)",
                    url="https://robochip.ir/Product/RBC-13807/%d8%b1%d9%84%d9%87-5-%d9%88%d9%84%d8%aa-%d9%85%db%8c%d9%84%d9%88%d9%86/",
                    alt="voltatech.ir has NO 5 V milon in stock (its 'milon 10 A' 5 V page is the HKF listing, out of stock; its in-stock 'bacheh milon 20 A' are another size). "
                        "Backups: Mohammad Electronic 'رله 5 ولت تک کنتاکت 7 امپر میلون' 66,000 (single contact, 4 pins: fits the footprint, no NC); Tejarat Electronic JQC-3FF-005-1HS 29,733 (single contact); "
                        "Modern Ertebat Rasa JQC-3FP (T73) 5 VDC (price not listed). The rating on this listing is 5 A 240 VAC; the board tracks limit the contacts to about 2.5 A anyway. "
                        "CHECK the pins against the 1:1 sheet.")
SRC["SS8050"] = dict(store="voltatech.ir", name="ترانزیستور SS8050 پکیج SMD", rial=10020, stock="in stock (2026-10-19)", url=volta2("ترانزیستور-ss8050-پکیج-smd"),
                     alt="eca.ir (SOT-23) out of stock on 2026-10-19. javanelec.com 'SS8050' 1,050-1,870 Toman (package not stated).")
SRC["USB-micro"] = dict(store="voltatech.ir", name="کانکتور Micro USB مادگی مدل SMD", rial=80220, stock="in stock (2026-10-19)", url=volta2("کانکتور-micro-usb-مادگی-مدل-smd"),
                        alt="eca.ir (85,200 Rial) out of stock on 2026-10-19. Footprint is the Wuerth 629105150521 outline: compare the pads with the connector you receive.")
SRC["PC817-SMD"] = dict(store="voltatech.ir", name="اپتوکوپلر PC817 پکیج SMD", rial=85840, stock="in stock (2026-10-19)", url=volta2("اپتوکوپلر-pc817-پکیج-smd"),
                        alt="eca.ir out of stock on 2026-10-19. Footprint 'SOP4_Wide_Universal' covers wide (7.2 mm) and narrow SOP-4 bodies - check the part you get. CTR class C (200-400 %) wanted.")
SRC["RC522"] = dict(store="voltatech.ir", name="ماژول RFID-RC522", rial=2251890, stock="in stock (2026-10-19)", url=volta2("ماژول-rfid-rc522"),
                    alt="eca.ir out of stock on 2026-10-19. javanelec.com 'RFID-RC522' 260,500.")
SRC["OLED"] = dict(store="voltatech.ir", name="ماژول نمایشگر OLED سفید 0.96 اینچ دارای ارتباط I2C", rial=3550000, stock="in stock (2026-10-19)",
                   url=volta2("ماژول-نمایشگر-oled-سفید-0-96-اینچ-دارای-ارتبا-2"),
                   alt="eca.ir: all 0.96in OLEDs out of stock on 2026-10-19 (the pin-order photo I checked earlier was an eca.ir module: GND VDD SCK SDA). CHECK the pin order of the module you receive: socket P2 is GND VCC SCL SDA.")
SRC["Header-F"] = dict(store="voltatech.ir", name="پین هدر 40*1 مادگی", rial=179940, stock="in stock (2026-10-19)", url=volta2("پین-هدر-401-مادگی"),
                       alt="eca.ir (1x40 female, 24,300) out of stock on 2026-10-19.")
SRC["Buzzer"]["rial"] = 231210
SRC["Buzzer"]["stock"] = "in stock (2026-10-19)"
SRC["DS18B20"]["rial"] = 1903560
SRC["DS18B20"]["stock"] = "in stock (2026-10-19)"
SRC["DS18B20"]["alt"] = "Waterproof probe (stainless tube, 3 wires: red 3V3, yellow DATA, black GND) on terminal J4; current price 190,356 Toman (258,339 is the struck-out old price)."
SRC["ESP32"]["stock"] = "in stock (2026-10-19)"
SRC["PTC"]["stock"] = "in stock (2026-10-19)"


def key_for(p, tag):
    r, v = p.ref, p.value
    if r.startswith(("MH", "TP", "FID", "SJ")):
        return None
    table = {"PS1": "HLK-5M05", "BR1": "MB10S", "U2": "PC817-SMD", "U3": "AMS1117", "F1": "FuseHolder", "U1": "ESP32", "U5": "CH340G",
             "Y2": "Crystal12", "J3": "USB-micro", "C1": "E100u16", "C4": "E47u16", "C5": "E22u35", "C18": "C1u", "RS1": "R120k-1W", "K1": "Relay", "K2": "Relay", "K3": "Relay", "K4": "Relay", "K5": "Relay", "F2": "PTC", "D7": "SMBJ6.5A"}
    if r in table:
        return table[r]
    if r == "BZ1":
        return "Buzzer"
    if r.startswith("RV"):
        return "Varistor"
    if v == "SS34":
        return "SS34"
    if v == "M7":
        return "M7"
    if v == "SS8050":
        return "SS8050"
    if r.startswith("D"):
        return {"GREEN": "LED-green", "RED": "LED-red", "YELLOW": "LED-yellow"}[v.split()[0].upper()]
    if r.startswith("SW"):
        return "Tactile"
    if "Terminal block 2P" in p.mpn:
        return "KF128-2P"
    if "Terminal block 3P" in p.mpn:
        return "KF128-3P"
    if p.fp.startswith("Connector_PinSocket"):
        return "Header-F"
    if v == "22pF":
        return "C22p"
    if v == "100nF":
        return "C100n"
    if v == "10uF":
        return "C10u"
    if p.fp.endswith("R_0805_2012Metric"):
        return {"0": "R0", "100": "R100", "470": "R470", "1k": "R1k", "2.2k": "R2k2", "10k": "R10k", "22k": "R22k", "100k": "R100k"}[v]
    raise KeyError(f"{r} {v} {p.fp}")


SHORT = {
    "HLK-5M05": "AC-DC module 5V 1A (Hi-Link HLK-5M05)", "Varistor": "Varistor 07D471K (275 VAC)", "MB10S": "Bridge rectifier MB10S SOIC-4",
    "PC817-SMD": "Optocoupler PC817 SMD SOP-4", "AMS1117": "LDO 3.3 V AMS1117 SOT-223", "SS34": "Schottky SS34 3A 40V SMA",
    "SMBJ6.5A": "TVS diode SMBJ6.5A unidirectional SMB", "PTC": "Resettable fuse (PTC) 1.1A 60V radial",
    "M7": "Diode 1N4007 SMD (M7) SMA", "SS8050": "NPN transistor SS8050 SOT-23", "Relay": "Relay T73 SRD-05VDC-SL-C, 5 V coil, 10A 250VAC, SPDT",
    "FuseHolder": "Fuse holder 5x20 PCB, covered (BLX-A type, 22.5 mm)", "ESP32": "ESP32-WROOM-32 module (4 MB flash, PCB antenna)",
    "CH340G": "USB-UART bridge CH340G SOP-16", "Crystal12": "Crystal 12 MHz HC-49USM SMD", "USB-micro": "Micro-USB B receptacle SMD",
    "LED-green": "LED 0805 green", "LED-red": "LED 0805 red", "LED-yellow": "LED 0805 yellow", "Tactile": "Tactile switch 6x6x5 SMD",
    "Buzzer": "Active buzzer 5V 12 mm", "R0": "Resistor 0R 0805 (link)", "R100": "Resistor 100R 0805", "R470": "Resistor 470R 0805",
    "R1k": "Resistor 1k 0805", "R2k2": "Resistor 2.2k 0805", "R10k": "Resistor 10k 0805", "R22k": "Resistor 22k 0805", "R100k": "Resistor 100k 0805 (1 %)",
    "R120k-1W": "Resistor 120k 1W 5% axial (mains sense, through-hole)",
    "C100n": "Capacitor 100nF 50V 1206", "C10u": "Capacitor 10uF 50V 1206", "C1u": "Capacitor 1uF 1206", "C22p": "Capacitor 22pF C0G 0805",
    "E100u16": "SMD electrolytic 100uF 16V 6.3x5.4", "E47u16": "SMD electrolytic 47uF 16V 6.3x5.4", "E22u35": "SMD electrolytic 22uF 35V 5x5",
    "KF128-2P": "Screw terminal 2-pin 5.08 mm", "KF128-3P": "Screw terminal 3-pin 5.08 mm",
    "Header-F": "Pin socket female 1x40 (cut: 1x8 for the RC522 + 1x4 for the OLED)", "RC522": "NFC reader RC522 module + tags",
    "OLED": "Display 0.96in OLED 128x64 I2C (SSD1306), 4-pin", "DS18B20": "Temperature probe DS18B20, waterproof, 1 m cable",
    "Fuse": "Glass fuse 1A 250V time-lag 5x20",
}
SPARES = {"R100": 2, "R470": 2, "R1k": 4, "R10k": 4, "R100k": 2, "LED-yellow": 2, "R2k2": 2, "R0": 2, "R120k-1W": 1, "C100n": 4, "C10u": 2, "C1u": 1, "C22p": 2,
          "LED-green": 2, "LED-red": 2, "SS8050": 2, "M7": 2, "SS34": 2, "SMBJ6.5A": 1, "MB10S": 3, "Relay": 1}
MIN_BUY = dict(ECA_MIN)
HEADERS_ONE = ("Header-F",)
OFFBOARD = (("RC522", 1), ("OLED", 1), ("DS18B20", 1), ("Fuse", 2))


def collect():
    groups = OrderedDict()
    parts = []
    for p in design.main_board().parts:
        k = key_for(p, "MAIN")
        if k is None:
            continue
        g = groups.setdefault(k, dict(refs=[], fps=set()))
        g["refs"].append(p.ref)
        g["fps"].add(p.fp.split(":")[-1])
        parts.append(p)
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
        rows.append(dict(key=k, refs="(plugs in / on a cable)", value=SHORT[k], fp="-", qty=q, buy=max(q, MIN_BUY.get(k, 0)), **SRC[k]))
    return rows


def write_assembly_csv(outdir):
    """Designator / value / footprint / description for an assembly house (DNP parts flagged)."""
    _, parts = collect()
    os.makedirs(outdir, exist_ok=True)
    for f in ("assembly_bom_main.csv", "assembly_bom_panel.csv"):
        if os.path.exists(os.path.join(outdir, f)):
            os.remove(os.path.join(outdir, f))
    with open(os.path.join(outdir, "assembly_bom.csv"), "w", newline="", encoding="utf-8-sig") as f:
        w = csv.writer(f)
        w.writerow(["Designator", "Value", "Footprint", "Description", "Mounting + side", "DNP", "Group"])
        for p in parts:
            tht = any(s in p.fp for s in ("TerminalBlock", "PinSocket", "Relay", "Varistor", "Converter_ACDC", "Buzzer", "FuseHolder", "Resistor_THT", "Capacitor_THT"))
            w.writerow([p.ref, p.value, p.fp.split(":")[-1], p.mpn, ("THT" if tht else "SMD") + " top", "yes" if p.dnp else "", p.group])


def write(rows, path):
    wb = Workbook()
    ws = wb.active
    ws.title = "BOM"
    hdr = ["#", "Designators", "Qty on boards", "Qty to buy (incl. spares)", "Part / value", "Footprint", "Store", "Shop product name",
           "Unit price (Rial)", "Unit price (Toman)", "Line total (Toman)", "Stock when checked", "Buy link", "Notes / alternative"]
    ws.append(["Hydraulic press counter v6.2 - BOM (one board + plug-in modules)   |   prices read on " + DATE + " - they change, re-check before ordering"])
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
    ws.cell(tot, 8, "TOTAL to buy now (Toman, incl. spares and shop minimum orders)").font = Font(bold=True)
    ws.cell(tot + 1, 8, "Parts cost of ONE board (qty on board x price, Toman)").font = Font(bold=True)
    ws.cell(tot + 1, 11, f"=SUMPRODUCT(C{first}:C{last},J{first}:J{last})").font = Font(bold=True)
    ws.cell(tot + 1, 11).number_format = "#,##0"
    tot += 1
    stores = OrderedDict((r["store"], None) for r in rows)
    for j, st in enumerate(stores):
        ws.cell(tot + 1 + j, 8, f"  subtotal {st}")
        ws.cell(tot + 1 + j, 11, f'=SUMIF(G{first}:G{last},"{st}",K{first}:K{last})')
    note_row = tot + len(stores) + 2
    notes = [
        "Stores (every line was re-checked on the shop's own page on 2026-10-19, price + orderable): eca.ir for the passives, connectors, power parts and fuses; voltatech.ir for the ESP32 module, SMD transistor / optocoupler / micro-USB, RC522, OLED, header, buzzer, DS18B20 probe, PTC; javanelec.com for HLK-5M05, CH340G and the 0 ohm resistor; robochip.ir for the 5 V relays. Shipping is not included.",
        "ECA sells many parts only in minimum quantities (50 pcs of one resistor / capacitor / LED value, 5-25 of the others): 'Qty to buy' already applies the minimum, so a pilot of 5 boards buys more than it uses. Parts that were sold out at eca.ir that day were moved to another shop (see the notes column).",
        "Power: either 230 VAC on J1 (HLK-5M05 fitted) or a regulated 5 V >= 1.5 A adapter on J6. Boards that are always fed from J6 can leave out J1, PS1, F1, RV1 and the fuse "
        "(about 0.7 M Toman less).",
        "Prices are in Rial as listed (Toman = Rial / 10). 'Qty to buy' adds spares for the cheap SMD parts; expensive parts have no spare.",
        "NOT on the list: enclosure, standoffs, cable glands. Programming needs only a micro-USB cable (CH340G with auto-reset on the board: Arduino IDE 'ESP32 Dev Module').",
        "Check before ordering the boards: (1) OLED pin order GND VCC SCL SDA, (2) micro-USB / PC817 SMD / buzzer / tact switch / fuse holder footprints against the parts you receive, "
        "(3) relay marking (5VDC, 10A 250VAC), (4) RS1 120k 1W working voltage >= 400 V.",
        "eca.ir links marked 'open shop search' open the shop's search for that value because the exact product slug was not captured - the product id is given in the notes.",
        "Sheet 'Production' scales this list to N units; an assembly house needs fabrication/assembly_bom.csv and the position file.",
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
        ps.cell(rr, 8, "module / probe, one per machine" if r["refs"].startswith("(") else "")
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
    out = os.path.normpath(os.path.join(HERE, "..", "BOM_press_counter_v6.xlsx"))
    write(rows, out)
    write_assembly_csv(os.path.normpath(os.path.join(HERE, "..", "fabrication")))
    tot = sum(r["buy"] * r["rial"] / 10 for r in rows)
    by = {}
    for r in rows:
        by[r["store"]] = by.get(r["store"], 0) + r["buy"] * r["rial"] / 10
    print("wrote", out, len(rows), "lines")
    print("total Toman:", f"{tot:,.0f}", {k: f"{v:,.0f}" for k, v in by.items()})
