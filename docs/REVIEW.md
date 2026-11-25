# Press counter - design review

Four parts: the **v6 simplification** (current), the **v5 design decisions** (superseded, kept for the record), the **problems found while building v4 / v5**, and the **history** of what was wrong in the original EasyEDA board and the v2 firmware.
The block-by-block electrical review with calculations is in [DESIGN_REVIEW.md](DESIGN_REVIEW.md); production in [PRODUCTION.md](PRODUCTION.md).

## v6 - one board, one chip (current)

The owner asked for a review aimed at mass production: "correct, cost effective and as simple as possible"; critical are the 230 V opto input
to count strokes, NFC, a display (a 7-segment would do), the press-contactor relay and a temperature sensor (plus the two call buttons, MQTT).

| Decision | Why |
|----------|-----|
| **One ESP32-WROOM-32 instead of STM32F103 + ESP-12F** | The reason for two chips in v5 was that the ESP8266 has too few pins and its WiFi could stall the counter. The ESP32 has enough pins, two cores and edge interrupts with a microsecond timer: strokes are time-stamped in an interrupt and cannot be lost by a slow network call. One chip, one firmware, no UART link protocol, no second USB port, no SWD / bootloader story; the module costs as much as the two chips together. |
| **One board** (202 x 104 mm since v6.1) instead of MAIN + PANEL + ribbon | Saves a PCB, the IDC headers, the cable and an assembly step. The whole board goes into the front enclosure; mains reaches it on three terminals. |
| **0.96in I2C OLED** instead of the 2.4in TFT | 4 wires instead of 8 + back-light, about a third of the price; shows the counter big, plus state and errors. |
| **2 call buttons**, no menu buttons | Settings live on the web page and in the local menu (hold CALL NET 3 s: update, log, WiFi setup, settings - [MENU.md](MENU.md)). Cards, display, relay and counting work without a network. |
| One relay -> **v6.1: the free pins are used** | First removed as not critical; then the owner asked to use every free port: four output relays (COM / NO / NC) on IO27 / IO16 / IO25 / IO13, 12 V and AUX 0-30 V on ADC1 (IO36 / IO35), the ZMPT101B 230 V module on IO39. Buzzer and LED moved to IO14 / IO15 (these pins toggle at boot, harmless for them). IO2 / IO12 stay free on purpose (boot straps). The board grew to 202 x 104 mm (relay strip). |
| **DS18B20 probe** instead of LM35 | Digital: the cable to the probe cannot add errors; no ADC (the ESP32 ADC is nonlinear and ADC2 is blocked by WiFi). |
| **CH340G + auto-reset on the one USB port** | Programmed like an Arduino board; the same port is the console. CH340C (no crystal) is not sold by the local shops. |
| **v6.2: ZMPT101B removed, 5 V terminal added, industrial re-check** | Owner's request. J6 (5 V screw terminal, PTC + TVS + Schottky) lets a 5 V adapter replace the mains supply; the 12 V input became 0-30 V like AUX (24 V control circuits); the relay start-up behaviour was traced through power-up, ROM boot, `setup()`, resets and programming, and a firmware init-order bug was fixed (DESIGN_REVIEW.md section 0). |
| Kept from v5 | mains input with on-board T1A fuse, MB10S + PC817 sense input with one 120 k 1 W resistor, T73 5 V relay with COM / NO / NC, 5 mm creepage rules, routing method. |

## v5 - design decisions (superseded by v6)

| Decision | Why |
|----------|-----|
| Two boards (MAIN + PANEL) joined by a 26-wire ribbon | The panel (buttons, display, RFID, buzzer) sits on the machine front, the mains / relay board in the cabinet. Pin *n* is wire *n* on both boards (`HMI_PINS` in `hardware/tools/design.py`). Buttons are laid out as a cross (UP top, LEFT / OK / RIGHT middle, DOWN bottom) and the two call buttons beside it. |
| Bare STM32F103C8T6 does counting, UI, relay, RFID | The ESP8266 has too few safe pins and its WiFi stack must not be able to stall the stroke counter. The machine keeps working (and showing errors) with no WiFi at all. ESP-12F = gateway only. |
| UART link `$payload*HH` with acknowledged, queued events | Nothing is lost while WiFi / the server is away (the STM32 queues, the gateway batches). |
| Relay K1 (T73 / SRD, 5 V coil) in the press-contactor circuit, contact COM-NO | Fail-safe: the press is disabled when the board is unpowered, in reset or when the firmware stops (watchdog). The relay is OFF after every reset. A 5 V coil removes the 12 V boost converter of v4. |
| One MB10S bridge in the sense input | 4 diodes -> 1 part. |
| No CAN bus | Removed in v5: less hardware, less firmware, no USB / CAN conflict - USB is always on as a service console. |
| On-board fuse holder | The fuse is part of the product, not a loose inline holder. |
| 5 mm creepage mains <-> low voltage, 1.5 mm between mains nets | Enforced by the router and by a DRC rule (see `hardware/README.md`). Pad pairs inside the relay, PC817 and HLK module are exempt (certified pitch). |
| SMD everywhere except connectors / relay / varistors / fuse holder / HLK module | Smaller board; all parts were looked up on eca.ir / voltatech.ir / digikala. |
| Own ST7789 display driver | The Adafruit libraries made the image 77 KB; the whole-archive link of the Arduino build pulled the I2C and timer HAL in. |
| Remote start disabled by default | A network command must never start a press unattended; stop is always allowed. |

## Problems found while building and checking v4 / v5 (all fixed unless noted)

* **MB10S** numbering differs between the datasheet and KiCad's footprint; the mapping was derived from the datasheet figure (DESIGN_REVIEW finding 1). AMS1117 (SOT-223: the tab is the output) and the relay pinout were checked against datasheets / KiCad footprints. Generic footprints (micro-USB, PC817 SMD, buzzer, tact switch, fuse holder, display sockets) were **not** checked against physical parts.
* The v4 relay (4-pin JQC-3F, 12 V) needed a boost converter and its footprint was not the standard SRD/T73 pattern: replaced by the 5 V SRD/T73 footprint.
* A KiCad rule file (`.kicad_dru`) with one invalid rule is silently ignored - the creepage rule was verified with a negative test (a low-voltage part next to a mains resistor produced violations, the real board none).
* `pcbnew` rewrites the project file on save and loses net classes: `tools/project.py` rewrites it after every save.
* The Arduino build links library objects with `--whole-archive`: Adafruit GFX pulled Wire / the I2C HAL, `analogWrite()` pulled the whole timer HAL -> the first real build was 77 KB for a 64 KB chip. Fixed with an own display driver and register-level back-light PWM (53 KB now).
* STM32duino 3.x has no `Serial1` unless `-DENABLE_HWSERIAL1` is set (and the flag has to reach the core build: `build.extra_flags`), no `wiring_private.h`; STM32duino defines `HEX` as a macro (collided with a constant in the shared header, renamed); PB3 / PB4 / PA15 need JTAG disabled (SWD kept).
* The 0.5 mm pitch supply pins of the STM32 cannot be fanned out by an autorouter: escape stubs and vias for the supply pins are pre-placed (`layout_main.py: stub_geometry`).
* Freerouting is used for the low-voltage nets after the own router has done the mains nets: v1.9.0 needed its GUI mode and Java 21; v2.5.0 runs headless on a Java 25 JRE (the native `freerouting-cli.exe` of 2.5.0 crashes on this board in a GraalVM serialisation error and then hangs). There is no GPU routing in Freerouting.
* Not fixed / accepted: relay coil-to-contact insulation is basic only; no ESD protection on the USB lines; sense release delay 20-40 ms.

## History - the original EasyEDA board and the v2 firmware

### What the product does

An ESP8266 box on a hydraulic die press.

* **Main job - count presses.** The press' contactor is energised during each stroke (it closes when the press
  goes down and releases at the end of the job). That 220 V AC is dropped through resistors, rectified by a
  bridge and fed to an optocoupler. The ESP measures every pulse: `count`, `duration` (= stroke time) and the
  temperature at that moment, and writes them to InfluxDB (`press_counter`).
* **Who is working.** RFID cards (RC522). `personnel:<id>` card = start/stop of a work session (toggles the relay
  that enables the press); `die:<id>` card = which die is mounted. Events go to InfluxDB (`nfc_event`,
  `device_status`), the `analyzer/` FastAPI + React dashboard builds sessions from them.
* Extras: LM35 temperature, buzzer feedback, user button, web UI (`V2/index.html`) + OTA.

### Hardware bugs in the old EasyEDA board (`press counter pcb.eprj`, v424)

| # | Problem | Consequence | Fix in v2 |
|---|---------|-------------|-----------|
| 1 | Transistors were changed to **BC847 (SOT-23)** but the symbol still had the TO-92 pin order C-B-E. The datasheet pinout of the SOT-23 part is **B-E-C**. | Relay driver and buzzer driver would not work (collector/base/emitter swapped). | BC337 TO-92 with a KiCad symbol/footprint pair whose pin numbers I checked against the datasheet. |
| 2 | Relay **SRA-05VDC-CL** (small "T78" relay) is rated **125 VAC** (10 A). It switched 220 V. | Contact/insulation rating exceeded on mains. | Mains switching moved to a standard **5 V relay module with SRD/JQC-3F T73 relay, 250 VAC 10 A**; no mains-switching copper on the PCB. |
| 3 | Press signal on **GPIO2** (a boot-strap pin) and the opto pulls it LOW. | If the press is energised while the box powers up, the ESP8266 can fail to boot. | Moved to **GPIO16** (no strap function). |
| 4 | Button on GPIO16 with a **pull-down**, firmware expected active-LOW with `INPUT_PULLUP` (GPIO16 has no internal pull-up) and the UI board had **no switch** at all. | Button never worked / permanently "pressed". | Button on GPIO2 with 10k pull-up, tactile switch on the board. |
| 5 | LM35 powered from **3.3 V** (needs >= 4 V) and read with a TMP36 formula at a 3.3 V scale; a bare ESP-12 ADC is 0-1 V. | Wrong / unusable temperature. | LM35 on +5 V, 1k + 100nF filter, firmware: `volts = raw/1023 * 1.0`, `degC = volts * 100`. |
| 6 | **5 V module only 400 mA** (R02-T2S05-H) for ESP (TX bursts ~300 mA) + relay + MAX7219 + RC522 + buzzer. | Brown-outs / resets. | **HLK-5M05, 5 V 1 A**. |
| 7 | MAX7219 `DIN` wired to **MISO** (GPIO12) instead of MOSI (GPIO13), 3.3 V logic into a 5 V MAX7219 (VIH 3.5 V), and **no firmware ever drove the display** (`shift74HC595()` was dead code). | Display could never have worked; two extra chips, two PCBs, a ribbon cable. | Display, MAX7219, level shifter and the second board **removed**. Single board. |
| 8 | Sense resistor `R13` was **50 k 1 W** in the schematic but **150 k 2 W** in the BOM; 50 k at 230 V is dissipating exactly its 1 W rating. | Hot / failing resistor. | 4 x 33 k 1/4 W (2 per leg, ~0.1 W each): ~1.5 mA into the opto LED at 230 V. |
| 9 | No fuse, no varistor, no isolation distance between mains and low voltage, no protection against the **contactor's inductive kick**. | Safety / reliability. | Fuse in the L wire (inline holder), varistor across the supply **and** across the sense input, 5 mm creepage mains <-> low voltage enforced by DRC rules and router. |
| 10 | RC522 `SDA/SS` floating at boot, RST not connected, buzzer driver without flyback diode, GPIO0/GPIO15 boot levels not guaranteed. | Random boot / SPI glitches. | 10k pull-ups/-downs on all strap/SS pins, flyback diode on the buzzer, `RST` tied to 3V3 at the RC522 socket. |
| 11 | UART header silk "VCC RX TX GND" was ambiguous (ESP RX or adapter RX?). | Flashing wires crossed. | Header labelled from the ESP side: **GND TXD RXD 3V3**; TXD goes to adapter RX. |

### Firmware bugs fixed (`V2/`)

| # | Bug | Effect | Fix |
|---|-----|--------|-----|
| 1 | `ControlManager` was constructed **without** the InfluxDB pointer. | Temperature report and every relay change (button/web) were silently **never logged**; the analyzer ends sessions from `device_status`. | `ControlManager` now owns a reference; every real relay change is logged with a reason. |
| 2 | `Ticker` callbacks did `analogRead()` and started HTTP requests from the timer context. | Crashes / lost data. | Everything runs from `loop()`. |
| 3 | **Any** tag (also `die:` tags and also after *writing* a tag) toggled the relay, and a tag kept on the reader toggled it again every 2 s. | Machine switching on/off by itself. | Only `personnel:` tags toggle the relay; `die:` tags are just logged; 3 s cooldown per tag; writing a card never switches the machine. |
| 4 | `PCD_PerformSelfTest()` right after `PCD_Init()` without re-init. | RC522 left in test state, reads fail. | Self-test removed; version register check only. |
| 5 | A queued tag-write never expired. | A later operator card could be overwritten. | 30 s timeout. |
| 6 | InfluxDB writer **dropped** a point whenever a request was in flight, and everything while WiFi was down; no timestamps. | Missing press counts (the analyzer even looks for "missing counts"). | 10-point RAM queue with retry + NTP millisecond timestamps. Field types are unchanged (still floats) so the existing bucket keeps working. |
| 7 | `/press_counter` returned `millis()` as `lastPulseTime`; the dashboards treat it as a Unix time. | "Last pulse: 1970". | Real Unix time (NTP). |
| 8 | No CORS headers. | Dashboard (port 8000) could not call the device API. | `Access-Control-Allow-Origin` + OPTIONS handler. |
| 9 | WiFi credentials hard-coded empty, no way to supply them. | Device never connects. | `secrets.h` (git-ignored, example provided), auto-reconnect, device keeps working offline. |
| 10 | Idle auto-off only after the first press. | Relay stayed on forever if no press followed the card. | Counted from relay-on or last press. |
| 11 | Relay pin driven before level set; `INPUT_PULLUP` on GPIO16; `pulseStart==0` sentinel; `%u` for `unsigned long`. | Boot glitch / edge cases. | Level set before `OUTPUT`; debounced state machine with explicit flags. |
| 12 | Dead / unused code (`shift74HC595`, buzzer frequency args, `CallbackData`, duplicated status code). | Noise. | Removed. |

### Known limitations of the v2 firmware (v4 changes: MQTT exists now, optional admin password on the gateway)

* The web UI has MQTT settings but the firmware has no MQTT backend.
* The HTTP API has no authentication (LAN only!). OTA password default is `hydropress` - **change it**.
* Mains wiring must be done by a qualified person; see `hardware/README.md`.
