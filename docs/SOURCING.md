# Where to buy the parts (Iran) - every BOM line checked on a shop page

Checked on **2026-10-19** line by line on the shops' own pages (price, add-to-cart / quantity), after screening with the price-comparison engine
[isee.sisoog.com](https://isee.sisoog.com/). The BOM ([`hardware/BOM_press_counter_v6.xlsx`](../hardware/BOM_press_counter_v6.xlsx), 44 lines) now uses
**four shops**: eca.ir (30 lines), voltatech.ir (10), javanelec.com (3) and robochip.ir (1).
Parts cost of one board: about **2.93 M Toman** (quantity on the board x price); buying for one board costs about 4.1 M because of the shops' minimum orders
(50 pcs of each resistor / capacitor / LED value at eca.ir ...) - for a pilot of 5 boards the surplus is used up.

## What changed since the 2026-10-18 version

* **Sold out at eca.ir on 2026-10-19** (tile quantity 0): HLK-5M05, SS8050 SOT-23, PC817 SMD, micro-USB SMD, CH340G, 1x40 female header, RC522, every 0.96in OLED, and no 0 ohm 0805.
  They moved to javanelec.com (HLK-5M05, CH340G, 0 ohm) and voltatech.ir (the rest).
* **Mistake corrected: voltatech.ir has no in-stock 5 V "milon" relay.** The "milon 10 A" 5 V page there is the HKF listing, out of stock; the in-stock ones are "bacheh milon 20 A"
  (a different size). The 5 V milon relay (SPDT, T73 pin pattern) is now from **robochip.ir** at 60,000 Toman (listing rated 5 A 240 VAC; price dated spring 1405 - confirm by phone / chat).
  Backups: Mohammad Electronic 66,000 (single contact 7 A milon, no NC), Tejarat Electronic JQC-3FF-005-1HS 29,733 (single contact), Modern Ertebat Rasa JQC-3FP (T73) 5 VDC (no price listed).
* Prices were taken from the **current** price on voltatech.ir pages (the struck-out "original" price is not what you pay): DS18B20 probe 190,356 (not 258,339), OLED 355,000, RC522 225,189.
* eca.ir sells most parts only in **minimum quantities** (the number on the product tile): 50 for resistors / capacitors / LEDs / M7, 20 SS34, 25 for the 120 k, 10 for tact switches /
  SMBJ6.5A / fuses / 100 uF, 5 for terminals, fuse holder, varistor, MB10S, AMS1117, crystal, 22 uF / 47 uF. The BOM's "Qty to buy" applies them.

## Shopping list per shop

### eca.ir (eshop.eca.ir, Tabriz) - passives, connectors, power parts

| Part | On board | Buy | Unit (Toman) | Page |
|---|---|---|---|---|
| Screw terminal 2-pin 5.08 mm | 5 | 5 | 19,000 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=26534) |
| Fuse holder 5x20 PCB, covered (BLX-A type, 22.5 mm) | 1 | 5 | 9,880 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=1016) |
| Varistor 07D471K (275 VAC) | 2 | 5 | 5,980 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=11337) |
| SMD electrolytic 100uF 16V 6.3x5.4 | 1 | 10 | 7,120 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=3239) |
| Capacitor 100nF 50V 1206 | 7 | 50 | 2,000 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=1393) |
| TVS diode SMBJ6.5A unidirectional SMB | 1 | 10 | 4,400 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=21504) |
| Schottky SS34 3A 40V SMA | 2 | 20 | 2,390 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=2420) |
| LDO 3.3 V AMS1117 SOT-223 | 1 | 5 | 5,570 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=978) |
| Capacitor 10uF 50V 1206 | 2 | 50 | 2,280 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=1691) |
| SMD electrolytic 47uF 16V 6.3x5.4 | 1 | 5 | 7,440 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=2614) |
| Resistor 1k 0805 | 12 | 50 | 457 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=3405) |
| LED 0805 green | 2 | 50 | 711 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=3940) |
| Resistor 10k 0805 | 15 | 50 | 820 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=3411) |
| Diode 1N4007 SMD (M7) SMA | 6 | 50 | 997 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=1937) |
| Screw terminal 3-pin 5.08 mm | 6 | 6 | 27,200 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=26535) |
| Resistor 120k 1W 5% axial (mains sense, through-hole) | 1 | 25 | 2,160 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=6567) |
| Bridge rectifier MB10S SOIC-4 | 1 | 5 | 2,110 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=14760) |
| SMD electrolytic 22uF 35V 5x5 | 1 | 5 | 6,980 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=13835) |
| LED 0805 red | 2 | 50 | 711 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=3941) |
| Resistor 2.2k 0805 | 4 | 50 | 457 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=3408) |
| Resistor 22k 0805 | 1 | 50 | 457 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=4438) |
| Capacitor 1uF 1206 | 1 | 50 | 2,750 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=1692) |
| Crystal 12 MHz HC-49USM SMD | 1 | 5 | 14,600 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=1423) |
| Capacitor 22pF C0G 0805 | 2 | 50 | 1,022 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=3548) |
| LED 0805 yellow | 4 | 50 | 711 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=3942) |
| Resistor 100k 0805 (1 %) | 2 | 50 | 457 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=3414) |
| Tactile switch 6x6x5 SMD | 2 | 10 | 2,280 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=2753) |
| Resistor 470R 0805 | 1 | 50 | 457 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=3402) |
| Resistor 100R 0805 | 1 | 50 | 457 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=3396) |
| Glass fuse 1A 250V time-lag 5x20 | 2 | 10 | 3,860 | [open](https://eshop.eca.ir/index.php?controller=product&id_product=1031) |

### voltatech.ir (Mashhad)

| Part | On board | Buy | Unit (Toman) | Page |
|---|---|---|---|---|
| Resettable fuse (PTC) 1.1A 60V radial | 1 | 1 | 8,025 | [open](https://voltatech.ir/product/%D9%81%DB%8C%D9%88%D8%B2-%D8%A8%D8%B1%DA%AF%D8%B4%D8%AA-%D9%BE%D8%B0%DB%8C%D8%B1-pptc-rld60p110xf-1-1a-60v/) |
| NPN transistor SS8050 SOT-23 | 8 | 10 | 1,002 | [open](https://voltatech.ir/product/%D8%AA%D8%B1%D8%A7%D9%86%D8%B2%DB%8C%D8%B3%D8%AA%D9%88%D8%B1-ss8050-%D9%BE%DA%A9%DB%8C%D8%AC-smd/) |
| Optocoupler PC817 SMD SOP-4 | 1 | 1 | 8,584 | [open](https://voltatech.ir/product/%D8%A7%D9%BE%D8%AA%D9%88%DA%A9%D9%88%D9%BE%D9%84%D8%B1-pc817-%D9%BE%DA%A9%DB%8C%D8%AC-smd/) |
| ESP32-WROOM-32 module (4 MB flash, PCB antenna) | 1 | 1 | 630,000 | [open](https://voltatech.ir/product/%D9%85%D8%A7%DA%98%D9%88%D9%84-%D9%88%D8%A7%DB%8C%D9%81%D8%A7%DB%8C-%D9%88-%D8%A8%D9%84%D9%88%D8%AA%D9%88%D8%AB-esp32-wroom-32/) |
| Micro-USB B receptacle SMD | 1 | 1 | 8,022 | [open](https://voltatech.ir/product/%DA%A9%D8%A7%D9%86%DA%A9%D8%AA%D9%88%D8%B1-micro-usb-%D9%85%D8%A7%D8%AF%DA%AF%DB%8C-%D9%85%D8%AF%D9%84-smd/) |
| Active buzzer 5V 12 mm | 1 | 1 | 23,121 | [open](https://voltatech.ir/product/buzzer-%D8%A8%D8%A7%D8%B2%D8%B1-5-%D9%88%D9%84%D8%AA-%D8%A7%D9%88%D8%B1%D8%AC%DB%8C%D9%86%D8%A7%D9%84/) |
| Pin socket female 1x40 (cut: 1x8 for the RC522 + 1x4 for the OLED) | 2 | 1 | 17,994 | [open](https://voltatech.ir/product/%D9%BE%DB%8C%D9%86-%D9%87%D8%AF%D8%B1-401-%D9%85%D8%A7%D8%AF%DA%AF%DB%8C/) |
| NFC reader RC522 module + tags | 1 | 1 | 225,189 | [open](https://voltatech.ir/product/%D9%85%D8%A7%DA%98%D9%88%D9%84-rfid-rc522/) |
| Display 0.96in OLED 128x64 I2C (SSD1306), 4-pin | 1 | 1 | 355,000 | [open](https://voltatech.ir/product/%D9%85%D8%A7%DA%98%D9%88%D9%84-%D9%86%D9%85%D8%A7%DB%8C%D8%B4%DA%AF%D8%B1-oled-%D8%B3%D9%81%DB%8C%D8%AF-0-96-%D8%A7%DB%8C%D9%86%DA%86-%D8%AF%D8%A7%D8%B1%D8%A7%DB%8C-%D8%A7%D8%B1%D8%AA%D8%A8%D8%A7-2/) |
| Temperature probe DS18B20, waterproof, 1 m cable | 1 | 1 | 190,356 | [open](https://voltatech.ir/product/%D8%B3%D9%86%D8%B3%D9%88%D8%B1-%D8%AF%D9%85%D8%A7-%D8%AF%DB%8C%D8%AC%DB%8C%D8%AA%D8%A7%D9%84-ds18b20-%D8%B6%D8%AF-%D8%A2%D8%A8-%D8%A8%D8%A7-%DA%A9%D8%A7%D8%A8%D9%84-1-%D9%85%D8%AA%D8%B1%DB%8C/) |

### javanelec.com (Tehran)

| Part | On board | Buy | Unit (Toman) | Page |
|---|---|---|---|---|
| AC-DC module 5V 1A (Hi-Link HLK-5M05) | 1 | 1 | 649,300 | [open](https://www.javanelec.com/shop/product/35885/hi-link-electronic-technology/hlk-5m05/%d9%85%d8%a8%d8%af%d9%84-220-%d9%88%d9%84%d8%aa-ac-%d8%a8%d9%87-5-%d9%88%d9%84%d8%aa-dc-%d8%a8%d8%a7-%d8%ad%d8%af%d8%a7%da%a9%d8%ab%d8%b1-%d8%ac%d8%b1%db%8c%d8%a7%d9%86-1-%d8%a2%d9%85%d9%be%d8%b1) |
| USB-UART bridge CH340G SOP-16 | 1 | 1 | 80,000 | [open](https://www.javanelec.com/shop/product/22921/nanjing-qinheng-microelectronics/ch340g) |
| Resistor 0R 0805 (link) | 1 | 3 | 407 | [open](https://www.javanelec.com/shop/product/3967/non-brand/resistor-0ohms-0805-5%25-smd/%d9%85%d9%82%d8%a7%d9%88%d9%85%d8%aa-0-%d8%a7%d9%87%d9%85-0805-smd) |

### robochip.ir (Tehran)

| Part | On board | Buy | Unit (Toman) | Page |
|---|---|---|---|---|
| Relay T73 SRD-05VDC-SL-C, 5 V coil, 10A 250VAC, SPDT | 5 | 6 | 60,000 | [open](https://robochip.ir/Product/RBC-13807/%d8%b1%d9%84%d9%87-5-%d9%88%d9%84%d8%aa-%d9%85%db%8c%d9%84%d9%88%d9%86/) |

## Still open before an order

* **Stock moves daily** (eca.ir sold out of 9 lines in 8 days). Check the cart / ask the shop on the day of the order; the BOM sheet notes the alternatives per line.
* **Relay**: confirm stock and price at robochip.ir (price dated 1405-02-26); the T73 / SRD pin pattern must match the footprint - compare with the 1:1 sheet.
* **Fuse**: the design wants a time-lag T1A; eca.ir lists a plain 1 A (ask for slow-blow), javanelec.com has a slow-blow ceramic 5x20 with wire leads (21,400).
* **OLED**: the pin order GND VCC SCL SDA was confirmed on the eca.ir module's photo; the voltatech.ir module (white, I2C) must be checked on arrival.
* The 12 MHz crystal, PC817 and micro-USB footprints must be checked against the real parts (DESIGN_REVIEW.md section 0.5).

## Using isee for the next check

* Search: `https://isee.sisoog.com/products?filters={"search_phrase":"SS8050"}` (every shop with its price; its stock flags are cached - always confirm on the shop's page);
* one shop: `https://isee.sisoog.com/shop_info/<id>/<name>?q=<part>` (eca = 72, voltatech = 259, javan = 143, robochip = 55, Mohammad Electronic = 37);
* javanelec.com: `https://www.javanelec.com/shop?searchfilter=<part>` (a price of 0 = not available);
* eca.ir: `https://eshop.eca.ir/search?controller=search&s=<part>`; the number on a tile is the minimum order, 0 = cannot be bought.
