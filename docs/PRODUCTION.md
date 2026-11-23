# Production guide (v6.2, one board)

Read [DESIGN_REVIEW.md](DESIGN_REVIEW.md) first (open points, first-article checklist).
**Do not order a large run before a pilot of 3-5 boards has been built, powered and tested** - the board has never been fabricated.

## 1. Files

| What | Where |
|------|-------|
| Gerbers + drill (zip, ready for the board house) | `hardware/fabrication/main/press_counter_gerbers.zip` (RS-274X, Excellon PTH + NPTH separate, `.gbrjob`) |
| Component positions (KiCad, mm, top side, rotation as in the KiCad footprint) | `hardware/fabrication/main/positions.csv` |
| Assembly BOM (designator, value, footprint, description, SMD / THT) | `hardware/fabrication/assembly_bom.csv` |
| Purchasing BOM with Iranian-shop links and prices, "Production" sheet scales it to N units | `hardware/BOM_press_counter_v6.xlsx` (prices read 2026-10-12 - they move) |
| **1:1 print sheet** for the footprint check (print at 100 %, lay the real parts on it) - do this before ordering boards | `hardware/fabrication/check/print_1to1_A4_top.pdf` |
| **PCB order at esys.ir** (Gerber ZIP in their naming, requirement check, form values, comment text) | `hardware/fabrication/esys/press_counter_esys_gerber.zip`, [ORDER_ESYS.md](ORDER_ESYS.md) |
| Laser / hand-made prototype (EzCad bitmaps, drill list, test pattern) - **read the limits first** | `hardware/fabrication/laser/`, [LASER_DIY.md](LASER_DIY.md) |
| Enclosure openings (drawing + coordinates), installation / labels | `hardware/fabrication/enclosure/`, [INSTALLATION.md](INSTALLATION.md) |
| Schematic PDF, layer PDF, renders | `hardware/fabrication/main/schematic.pdf`, `pcb_layers.pdf`, `render_top.png`, `render_bottom.png` |
| Firmware image for the programming station | build `firmware/press_counter` (Arduino IDE / arduino-cli, see FIRMWARE.md); `press_counter.ino.merged.bin` = bootloader + partitions + application in one file for `esptool write_flash 0x0` |

`positions.csv` uses KiCad's rotation convention. Most assembly houses need a rotation correction per package (SOIC, SOT-23, SOP-4, electrolytic
caps ...): **check every polarised / directional part on the first article** and keep the corrected file for the next order.

## 2. PCB specification

| Item | Value |
|------|-------|
| Layers / thickness | 2 layers, 1.6 mm FR-4, TG >= 130 |
| Copper | 35 um (1 oz). The relay-contact tracks (1.2 mm) are good for about 2.7 A: use the contact only for the contactor control circuit |
| Size | 202.1 x 104.1 mm, corners R3, 6 x M3 holes (3.2 mm) |
| Smallest features | track 0.25 mm (a few 0.15-0.2 mm escapes may occur, see the DRC report), spacing >= 0.2 mm, via 0.6 / 0.3 mm, **smallest hole 0.3 mm** (the ESP32 GND-pad vias), copper to edge >= 0.4 mm: an ordinary 6 mil process |
| Surface finish | HASL lead-free is fine (no fine-pitch part below 1.27 mm except the ESP32 module's 1.27 mm castellations); ENIG if you prefer flat pads |
| Solder mask / silk | any colour, white silk; the mask opening of the fiducials is 2 mm |
| Mains clearances | 5 mm mains -> low-voltage, 1.5 mm between mains nets and to the board edge: **the fab must not add copper (thieving) in the empty areas** - the dark keep-out areas (mains gaps, ESP32 antenna, RC522 antenna) must stay copper-free |
| Test | flying-probe / E-test of all nets |
| Panelisation | tab-route with mouse bites, 3 mm rails; do **not** V-score through the 5 mm mains zones or across the ESP32 antenna area |

## 3. Assembly

All SMD parts are on the **top** side (single-sided reflow); the back carries only the silk help sheet.

1. Solder paste (stencil 0.12 mm), place, reflow (lead-free profile). SMD: resistors / capacitors (0805 / 1206), electrolytics C1 / C4 / C5, diodes,
   LEDs, MB10S, PC817, SS8050, AMS1117, CH340G, 12 MHz crystal, **ESP32-WROOM-32 module** (castellated, reflow-able; its GND pad has vias:
   paste with a window pattern), micro-USB, tact switches.
2. THT afterwards (hand or selective wave): terminal blocks J1 / J2 / J4 / J5 / J6 / J7 / J9 / J10-J13, PTC F2, varistors RV1 / RV2, **RS1 (120 k 1 W, lie it flat, 15.24 mm
   pitch)**, **fuse holder F1**, **PS1 HLK-5M05**, **relays K1-K5**, buzzer BZ1, sockets P1 (1x8, RC522) and P2 (1x4, OLED).
   Boards that will always be fed by a 5 V adapter on J6 may leave out J1, F1, RV1 and PS1.
3. Insert the tested fuse last. The RC522 and OLED modules are plugged in after the test (or soldered directly, without the sockets, to save the
   sockets - then P1 / P2 are the module's own header pins).
4. Clean flux (the sense input and the relay area carry 230 V).
5. Conformal coating is **recommended for a press shop** (oil mist, metal dust on 230 V pads); mask the terminals, the relays, the buzzer, the sockets,
   the USB connector and the ESP32 antenna area.

Orientation traps: BR1 MB10S ("+" lead on pad 1, silk "+" / "-"), U2 PC817 (dot = pin 1), U5 CH340G (dot), LEDs D2 / D5 / D6 / D13 / D30-D33 (cathode mark),
D3 / D7 / D8 / D12 / D14 / D20-D23 (band = cathode), electrolytics (bar = minus), U3 (tab = output), K1-K5 / PS1 (fit one way only), BZ1 (+).

## 4. Programming and test (per board)

Equipment: PC with the Arduino IDE / esptool and a micro-USB cable; isolating transformer with current limit (or lamp limiter); multimeter; a 230 V lamp;
a personnel card and a die card; a DS18B20 probe.

**Step A - USB only (no mains)**

1. Visual inspection against the render; polarity of C1 / C4 / C5, diodes, BR1, U2, U5.
2. Plug USB into J3: TP1 (+5 V) 4.5-4.8 V, TP2 (+3.3 V) 3.3 V +-3 %, green PWR LED on. USB current < 200 mA.
3. Program: Arduino IDE *Upload* (or `esptool --chip esp32 write_flash 0x0 press_counter.ino.merged.bin` on a programming station). No buttons needed;
   if the upload does not start, the auto-reset circuit (Q7 / Q8, R94 / R95) is wrong.
4. Serial monitor 115200: the start log; `status` shows `nfc ok`, a temperature (probe on J4), the OLED shows the counter.
5. Console `set remote 1` (not saved), `relay 1` / `relay 0`: the green ON LED and the click of K1; `buzz`; both call buttons (the display shows the call,
   holding 1.5 s cancels); a card on the reader (`tag` prints it). `defaults` restores the factory settings afterwards.

**Step B - mains (qualified person only, isolating transformer, current limit)**

6. Mains on J1 (fuse F1 fitted): TP1 5.0-5.2 V, TP2 3.3 V, no heat at PS1 / U3 after a minute (U3 tab < 60 C at 25 C ambient).
7. Sense input: 230 V through a switch and a lamp on J2: red LED D5 on, TP4 (PRESS) low, one stroke counted per switch-on; with J2 open TP4 high.
   Pilot only (variac): TP4 must stay high up to ~60 VAC on J2 (R12 threshold) and be solidly low from 190 VAC.
8. Relay contacts: a 230 V lamp on J5 COM / NO follows `relay 1` / `relay 0`; NC is the opposite. The same for J10-J13 with `out 1 1` ... `out 4 0`.
8b. Voltage inputs: 24 V (or 12 V) on J7 and a known voltage on J9 -> `status` (adjust `v12cal` / `auxcal` if needed).
8c. 5 V terminal (mains off): a 5 V adapter on J6 -> TP1 4.5-4.7 V, the board starts, all five relays switch. Reverse the adapter: nothing happens
    (F2 trips after a few seconds, the board stays dead), restore the polarity: it starts again after F2 has cooled down.
8d. Relay start-up check: with all relays wired to lamps, switch the supply on and off ten times and press the USB reset (`reboot`) ten times:
    **no lamp may flash** (DESIGN_REVIEW section 0.1).
9. WiFi: connect to the setup network, enter a test WiFi, check the web page and an MQTT broker (`event/press` per stroke).
10. **Hipot** (type test on the pilot, optional routine): 1.5 kV AC (or 2.1 kV DC), 1 s, between J1 + J2 + J5 + J10-J13 and GND / J3 / J4 / J6 / J7 / J9, USB unplugged;
    leakage < 1 mA (1.5 kV = the relay's coil-to-contact rating).
11. Burn-in of the pilot: 24 h with strokes simulated every few seconds and the relay toggled by cards; no resets (E09).

## 5. Costs and sourcing (2026-10-18, check again)

About **2.9 million Toman** of parts per board with all modules (quantity on the board x price, BOM sheet of 2026-10-19; about 2.5 M without the four output relays, about 0.7 M less
when the board is fed by a 5 V adapter and J1 / F1 / RV1 / PS1 are left out). Buying for the first board costs about 4.1 M because of the shops' minimum orders (50 pcs per resistor / capacitor / LED value
at eca.ir). Every line was checked on a shop page on 2026-10-19; the parts come from **four shops** (eca.ir 30 lines, voltatech.ir 10, javanelec.com 3, robochip.ir 1) - see [SOURCING.md](SOURCING.md).
Big items: ESP32 module 630 k (voltatech), HLK-5M05 649 k (javan), OLED 355 k, RC522 225 k, DS18B20 probe 190 k, CH340G 80 k, relays 6 x 60 k (robochip). For a run ask the shops for a quotation: several parts were in stock in small numbers only (tact switch, buzzer, relays, MB10S minimum order 5); the 4.7 k resistor was not at eca.ir (the design uses 2.2 k) and the bare ESP32 module only at voltatech.ir. Stock changes within days (nine lines were sold out at eca.ir on 2026-10-19).

## 6. Pilot -> production gates

| Gate | Pass criterion |
|------|----------------|
| Footprints | all items of DESIGN_REVIEW section 8 fit the received parts |
| Electrical | steps A and B of section 4 on 5 boards |
| Thermal | U3 tab, PS1 and K1 body at 40 C ambient inside the real enclosure: < 85 C |
| RF | WiFi RSSI in the real enclosure at the machine better than -75 dBm; NFC read distance through the front >= 1.5 cm |
| EMC / ESD (informal) | the press contactor switched next to the board (same circuit): no resets, no false strokes; ESD gun 4 kV contact on the front: no resets |
| Firmware | everything in FIRMWARE.md "Verification status" checked on 5 boards (none of it has ever run) |
| Documents | enclosure label: 230 V, fuse T1A, relay contact limit 2.5 A, "qualified person" |

If the product is sold (not only used in your own plant), the electrical safety standard of your market (IEC 62368-1 / 61010-1) and EMC
testing are **not** covered by this repository.
