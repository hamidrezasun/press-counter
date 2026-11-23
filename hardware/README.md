# Press counter v6.2 - hardware (KiCad 10)

One 2-layer board, generated from one file, [`tools/design.py`](tools/design.py) (parts + nets), so schematic, PCB and BOM cannot disagree.
Review with calculations and open points: [../docs/DESIGN_REVIEW.md](../docs/DESIGN_REVIEW.md). Production / test: [../docs/PRODUCTION.md](../docs/PRODUCTION.md).

| Board | Size | What |
|-------|------|------|
| `main/press_counter.*` | 202 x 104 mm, 124 parts | mains input + T1A fuse + HLK-5M05, 230 VAC press-sense optocoupler, 5 V relay (COM / NO / NC), **ESP32-WROOM-32**, USB micro-B with CH340G auto-reset, sockets for the **RC522** NFC module and the **0.96in I2C OLED**, DS18B20 terminal, 2 call buttons, buzzer, LEDs; **4 output relays** (COM / NO / NC) in a strip on the right edge; **12/24 V and AUX inputs (0-30 V)**; **5 V screw terminal** as an alternative to the mains supply |

```
hardware/
  main/                    KiCad project (open press_counter.kicad_pro), drc.rpt, erc.rpt
  fabrication/main/        press_counter_gerbers.zip, gerbers/ (RS-274X + Excellon PTH / NPTH), positions.csv, pcb_layers.pdf, schematic.pdf, render_top|bottom.png
  fabrication/assembly_bom.csv   designators / values / footprints / SMD-THT
  lib/press_counter.pretty custom footprints (wide SOP-4 for the PC817, 5x20 PCB fuse holder with slotted holes, ESP32 with 0.3 mm GND vias)
  BOM_press_counter_v6.xlsx   Iranian-store links + prices (2026-10-12), sheet "Production" scales to N units
  fabrication/check/       print_1to1_A4_top.pdf: print at 100 % and lay the real parts on it (footprint check before ordering boards)
  fabrication/esys/        press_counter_esys_gerber.zip: Gerber + drill files named for esys.ir (make_esys_package.py, check_esys.py) - see docs/ORDER_ESYS.md
  fabrication/laser/       laser-etch bitmaps (top / mirrored bottom), drill list, test pattern (make_laser_files.py) - see docs/LASER_DIY.md
  fabrication/enclosure/   enclosure_interface.svg / .csv: openings, light pipes, cable entries (make_enclosure_drawing.py)
  tools/                   generators - see "Regenerating"
  sim/                     SPICE checks run with KiCad's ngspice: sense_input.py (230 V input), relay_driver.py (relay at start-up)
  archive_v5/              the previous two-board design (MAIN + PANEL, STM32 + ESP8266) with its fabrication files
```

**Help on the board:** the front silk names the pins of every connector and socket; the **back carries a help sheet** (terminals, wire colours,
LEDs, test points). Fiducials FID1-3, test points TP1 (+5 V), TP2 (+3.3 V), TP3 (GND), TP4 (PRESS).

## Blocks

| Block | Parts | Notes |
|-------|-------|-------|
| Mains in | J1 (L, N), **F1 fuse holder 5x20 + T1A 250 V**, RV1 varistor, PS1 Hi-Link HLK-5M05, C1 / C2 | 230 V -> 5 V / 1 A isolated module. Fuse in L, before the varistor. |
| 5 V terminal (v6.2) | **J6 (+5V, GND)** at the top edge, F2 PTC 1.1 A / 60 V, D7 SMBJ6.5A, D8 SS34 | instead of mains: a regulated 5 V / >= 1.5 A adapter. Reverse polarity / wrong adapter (up to 60 V): D7 conducts, F2 trips. D8 blocks back-feeding (mains and adapter together do no harm). |
| Press-sense input | J2 (SNS A/B), RV2, **RS1 120 k 1 W axial**, **BR1 MB10S**, C5 22 uF, D5 (red), U2 PC817, R12 2.2 k (threshold), R1 22 k | 230 V AC across the contactor coil -> 120 k -> bridge -> LED + optocoupler -> IO34 LOW while the contactor is energised. |
| Relay | K1 T73 / SRD-05VDC-SL-C **5 V coil** (SPDT, 10 A 250 V), Q2 SS8050, R9 / R13, D12 M7, D13 (green "ON") + R43, **J5 (NO, COM, NC)** | COM-NO in series with the contactor control circuit: **the press is disabled whenever the board is unpowered, booting or reset.** Contact tracks ~2.5 A. |
| 3.3 V | U3 AMS1117-3.3, C3, C4, D6 (green PWR) + R2 | 3V3 copper around the tab as a heat sink |
| Controller | U1 **ESP32-WROOM-32** (antenna at the top edge, copper-free 48 x 7 mm), C16 / C17, EN RC R51 / C18, IO0 pull-up R53 | one chip for counting, relay, NFC, display, temperature, WiFi / MQTT / web page |
| USB | J3 micro-B, D14 SS34 (VBUS -> +5 V), **U5 CH340G** + Y2 12 MHz + C25 / C26, R97, C22, Q7 / Q8 + R94 / R95 (DTR / RTS auto-reset) | upload from the Arduino IDE without buttons + 115200 console; USB powers the logic for bench work |
| NFC | P1 1x8 socket for the **RC522 module** (`SDA SCK MOSI MISO IRQ GND RST 3V3`), R33 | the module lies over the board; no copper under its antenna coil |
| Display | P2 1x4 socket for the **0.96in OLED** (`GND VCC SCL SDA`), R40 / R41 2.2 k | check the module's pin order |
| Temperature | J4 (3V3, DATA, GND), R10 2.2 k pull-up, R11 100 R | **DS18B20 waterproof probe** |
| Output relays | K2-K5 (OUT1-OUT4, T73 5 V), Q3-Q6 SS8050, R60-R63 1 k, R70-R73 10 k, D20-D23 M7, D30-D33 yellow + R80-R83, **J10-J13 (NO, COM, NC)** | IO27 / IO16 / IO25 / IO13; strip on the right edge (x > 150 mm), wire entry at the edge. Contacts 230 V, about 2.5 A. |
| Voltage inputs | J7 12/24 V sense (R90 100 k / R91 10 k, C20 -> IO36), J9 AUX (R98 100 k / R99 10 k, C23 -> IO35), both 0-30 V (36 V max.) | ADC1 pins (usable with WiFi). IO39 is free (the ZMPT101B header of v6.1 was removed). |
| Front | SW1 CALL SUPERVISOR, SW2 CALL NET (+ R20 / R21, C7 / C8), D2 red ALARM + R44 (IO15), BZ1 active buzzer (IO14) + Q1 / R3 / R4 / D3 | |

ESP32 pin map: [../docs/FIRMWARE.md](../docs/FIRMWARE.md) "Pins".

## Mains safety rules (enforced in the design files)

* Mains copper exists only in the left part of the board: supply input (J1, F1, RV1, PS1 AC pins), the press-sense input (J2 ... PC817 pins 1-2)
  the relay contacts (K1, J5) in the bottom-left corner and the output relay contacts (K2-K5, J10-J13) in the right strip. Dashed outlines on the silk.
* **5 mm clearance between mains copper and any low-voltage copper** (reinforced insulation, 250 V, pollution degree 2), enforced by the router and
  by DRC rules in `press_counter.kicad_dru`. Pad pairs inside a certified part (HLK module, PC817, relay) are exempt. **The relay's coil-to-contact
  insulation is basic only**: the front (buttons, OLED, NFC, USB) must be behind a closed insulating enclosure.
* 1.5 mm between different mains nets; 1.5 mm from mains copper to the board edge.
* Fuse: **T1A, 5x20 mm, slow-blow, in the on-board holder F1.** Mains wiring and the first power-up are for a qualified person.
* J2 goes across the contactor **coil** (220-240 V AC). LED current = (V x 0.9 - 4.5 V) / 120 k. Sense timing: C5 bridges the zero crossings, so the
  release is seen ~20-40 ms late; set *Minimum stroke signal* to 20-30 ms.

## Verification status (KiCad 10.0.6, 2026-10-11)

| Check | Result |
|-------|--------|
| ERC | 0 errors, 0 warnings; netlist check against design.py: 0 differences |
| DRC incl. schematic parity, all track errors, silk | **0 violations, 0 unconnected, 0 footprint errors** |
| Creepage rule 5 mm mains <-> low voltage | active - negative test: a low-voltage resistor 3 mm beside a mains track gave four `mains_ac_to_lv` errors, the real board none |
| Routing | 1007 tracks, 116 vias (0.6 / 0.3 mm), smallest hole 0.3 mm, no vias on the mains nets |

How it was routed: the mains nets by an own grid router (class clearances, 5 mm creepage, hand-placed relay-coil stubs), the rest by Freerouting 2.5.0
(headless, Java 25), then a small island-repair step for the nets the autorouter left incomplete.

**Not verified** (needs a person / the physical parts / a bench): electrical function of anything, the footprints of the micro-USB connector, PC817 SMD,
buzzer, tact switch, fuse holder, relay and terminal blocks against the exact parts you receive, WiFi range in the enclosure, NFC read distance,
thermal behaviour, EMC / ESD. The board has never been fabricated.

## Regenerating (KiCad's own Python, Java for Freerouting)

`tools\run_all.bat` runs everything (schematic + ERC, place + route, island repair, DRC, silk, fabrication files). Individual steps (run with
`D:\KiCad\10.0\bin\python.exe -P`):

| Command | What |
|---------|------|
| `gen_all.py main` | schematic + ERC + netlist check |
| `route_fr.py main [--passes N] [--reuse-ses]` | placement, mains routing, Freerouting (`--reuse-ses` re-imports the last result) |
| `repair.py main <net> --apply --inplace` | join the islands of a net the autorouter left incomplete |
| `finish.py main [--no-export]` | zone fill, DRC, gerbers / drill / zip / position / PDF / renders |
| `cleanup.py main`, `resilk.py main` | remove dangling copper reported by the DRC / rebuild the silkscreen without re-routing |
| `make_custom_lib.py` (any Python) | the custom footprints in `lib/press_counter.pretty` |
| `make_bom.py` (system Python + openpyxl) | `BOM_press_counter_v6.xlsx` + `fabrication/assembly_bom.csv` |
| `placecheck.py main` | courtyard / silk overlap check of the placement only |

Freerouting 2.5.0 needs `tools/freerouting/freerouting-2.5.0.jar` and a Java 25 runtime in `tools/freerouting/jre25/` (downloaded, not committed);
without them `route_fr.py` falls back to the 1.9.0 jar. Edit `design.py` / `layout_main.py`, not the generated KiCad files.

Gotchas learned: KiCad **silently ignores the whole `.kicad_dru`** if one rule has an unknown property; `pcbnew`'s `Save()` rewrites the `.kicad_pro`
(losing net classes) - `project.py` re-writes both after every save; after `BOARD.Remove()` the SWIG getters return untyped objects; pin names with
`/` become `{slash}` in KiCad's unconnected-net names.
