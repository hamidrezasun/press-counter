# v6 design review - block by block

What was checked, how, what was found, and what **cannot** be checked without hardware. Read this before ordering a production run.
Nothing in this document is a bench measurement: the board has never been built. "Verified" means checked against a datasheet or by a tool
(KiCad ERC / DRC / netlist parity, the compiler); "calculated" means a hand calculation from datasheet values.

Source of truth for the circuit: [`hardware/tools/design.py`](../hardware/tools/design.py) (`main_board()`: 124 parts / 85 nets).
Schematic, netlist, PCB and BOM are all generated from it.

## 0. v6.2 review for an industrial production run (2026-10-12)

Changes requested by the owner: ZMPT101B removed; a 5 V screw terminal as an alternative to the mains supply; a full re-check for an
industrial plant, in particular that **no relay clicks while the ESP32 starts**.

### 0.1 Relays at start-up and in every other "uncontrolled" moment

| Moment | What the relay pins do | Result |
|--------|------------------------|--------|
| Power-up, 5 V rising, ESP32 still in reset | IO26 / IO27 / IO16 / IO25 / IO13 are high-impedance inputs; each SS8050 base has a 10 k pull-down (R13, R70-R73) | all relays **off** |
| ROM bootloader / 2nd-stage bootloader (~300 ms) | the ROM only drives IO1 (boot log), IO14 / IO15 (SDIO probing) and reads the straps IO0 / IO2 / IO5 / IO12 / IO15; the relay pins are not touched | **off**. IO14 = buzzer (a short chirp is possible), IO15 = red LED (may flicker) |
| `setup()` | first two calls: `io::begin()` / `outputs::begin()` write the output latch LOW through the IDF driver, **then** switch the pin to output (`io::outputLow`) | **off**, no pulse |
| Brown-out, watchdog, crash, `reboot`, OTA restart | the chip resets: pins return to inputs, the pull-downs take over; after the restart the machine relay stays off until a card is shown | relays drop (fail-safe), **never re-energise by themselves** |
| Firmware upload over USB | the auto-reset holds EN low / IO0 low; the chip is in reset or in the ROM download mode | **off** for the whole upload |
| Even if a relay pin had its weak internal pull-up (~45 k) on | base = 3.3 V x 10 k / (10 k + 1 k + 45 k) = 0.59 V: a few uA of collector current, the coil needs ~40 mA to pull in | **off** (margin) |

Found and fixed: the firmware used `digitalWrite(pin, LOW)` before `pinMode(pin, OUTPUT)`. In the ESP32 Arduino core 3.x that first
`digitalWrite()` is **ignored** (it only logs an error) on a pin not yet configured. No relay clicked because of it (the reset value of the
output latch is 0 and the pull-downs hold the bases), but the intended protection did not exist. Now done with `gpio_set_level()` first.

Behaviour to know: an automatic output (mode *alert*, *call*, *press*) switches on as soon as its condition is true, also right after a start
(e.g. *alert* with E01 when the RC522 is missing). That is a real state, not a glitch.

### 0.2 Changes in v6.2

| Change | Why |
|--------|-----|
| ZMPT101B header J8 (+ R93, C21) removed; IO39 free | owner's request; one off-board 230 V module and one calibration step less |
| **J6: 5 V screw terminal** (2-pin 5.08 mm, top edge) -> F2 PTC 1.1 A / 60 V -> D7 TVS SMBJ6.5A to GND -> D8 SS34 -> +5V | feed the board from a regulated 5 V adapter instead of mains. Reverse polarity or a wrong 9-48 V adapter: D7 conducts, F2 trips (resets when the fault is removed), nothing behind D8 sees more than the TVS clamp. D8 keeps the HLK module (or USB) from feeding the adapter: mains and adapter at the same time does no harm. +5V is then ~4.6 V: relays pull in at 3.75 V, AMS1117 needs ~4.4 V at 0.5 A |
| 12 V input divider 47 k / 10 k -> **100 k / 10 k** (0-30 V, 36 V max) | 24 V is the usual industrial control voltage; with 47 k / 10 k it put 4.2 V on a 3.3 V pin. Now J7 and J9 are identical, and 47 k left the BOM |
| D14 SS14 -> SS34 | same SMA footprint, same part as D8 (one BOM line), SS14 was not in stock at eca.ir |
| **Press input: R12 2.2 k across the PC817 LED, R1 pull-up 10 k -> 22 k** | Found by SPICE (section 0.3): when the press is off, a contact snubber (RC across the switch that drives the contactor) or a triac output leaks a few mA into the contactor coil. With 100 nF the v6.1 input read "press on" permanently, with 47 nF it sat at an undefined 2 V. R12 gives the input a threshold (the opto LED needs ~0.5 mA before it lights); R1 22 k keeps a weak PC817 (CTR 35 %) saturated at 195 VAC. Bonus: the input releases ~5 ms after the voltage goes away (was 20-40 ms) |
| Web page: the *press* output mode warns about relay wear | a T73 is good for ~100 000 switchings at rated load: an output that clicks every stroke wears out in weeks on a busy press |

### 0.3 Simulation (what can be simulated before building, and what was)

| Tool | What it covers | Done here |
|------|----------------|-----------|
| **SPICE** (ngspice, included in KiCad - `hardware/sim/*.py` drive it from Python; LTspice works too) | analog behaviour: the 230 V sense input, relay drivers, dividers, protection parts | **yes**: [`sense_input.py`](../hardware/sim/sense_input.py) (mains -15 % / +10 %, weak optocoupler, snubber leakage), [`relay_driver.py`](../hardware/sim/relay_driver.py) (start-up with a floating / weakly pulled pin, saturation, flyback) |
| **Wokwi** (wokwi.com or its VS Code extension) | runs the real ESP32 firmware with a virtual SSD1306 OLED, DS18B20, buttons, LEDs, relays, buzzer and WiFi (MQTT to a test broker); no RC522 part | not yet - a `diagram.json` + `wokwi.toml` pointing at `firmware/build` would let the firmware run for the first time without hardware |
| **QEMU** (Espressif's ESP32 fork) | boots the firmware binary, finds crashes / watchdog resets; no WiFi, no peripherals | not done |
| KiCad ERC / DRC | wiring, clearances, 5 mm mains creepage | yes (section 9) |
| Paper / 1:1 print | lay the real parts on a 1:1 print of `pcb_layers.pdf` to check footprints (relay, terminals, fuse holder, PTC, USB, buzzer, switches) | cheap, recommended before the pilot order |
| What no simulation replaces | EMC / ESD, surge, WiFi and NFC range in the real enclosure, heat in the enclosure, relay contact wear, footprint fit | the 3-5 board pilot (PRODUCTION.md) |

Sense input, board values (SPICE, generic diode / opto models; IO34 LOW < 0.8 V, HIGH > 2.5 V):

| Case | IO34 while the press is on | IO34 while off |
|------|----------------------------|----------------|
| 230 VAC, CTR 100 % | 0.05 V (low after 38 ms) | 3.30 V (6 ms after the voltage goes) |
| 195 VAC (-15 %), CTR 35 % (worst PC817 at 1 mA) | 0.39 V | 3.30 V |
| press off, 100 nF snubber leaking into the coil | - | 3.29 V (v6.1 values: **0.15 V = false "press on"**) |
| press off, 220 nF snubber | - | 0.06 V: such a large snubber must be removed or the input wired across a contact without one |

### 0.5 The open items of v6.2 - what was closed, what still needs a part in the hand

| Item | Status |
|------|--------|
| OLED pin order | **checked against the shop's product photo** (eca.ir, module 20271): silk `GND VDD SCK SDA`, pin 1 = GND = socket P2. The listing text names VCC first and the driver chip is an **SSD1315** (SSD1306 compatible). Still: check the module you actually receive, and buy the same listing again for the production run |
| Fuse holder F1 | pitch quoted as 22.3 / 22.5 / 22.6 mm by different sellers; the footprint has 2.4 mm slots (about +-0.7 mm for a 1 mm tail) and covers all three |
| F2 PTC RLD60P110XF | 1.1 A hold, 2.2 A trip, 60 V (Polytronics datasheet via LCSC); lead pitch of the family is 5.08 mm, footprint pad pitch 5.0 mm: fits with slightly bent leads |
| PS1 HLK-5M05 | body 38 x 23 x 18 mm (JLCPCB listing); the pin positions could not be read from text sources, the board uses KiCad's `Converter_ACDC_Hi-Link_HLK-5Mxx` footprint: **check with the 1:1 sheet** |
| Relays K1-K5 | KiCad `Relay_SPDT_SANYOU_SRD_Series_Form_C` (made from the Songle datasheet; resellers quote 15.6 x 19 mm bodies and 1.5 kV or 0.5 kV coil-contact insulation depending on the maker - **take the maker's datasheet for the exact relay you buy**): **check with the 1:1 sheet** |
| Terminals, micro-USB, PC817 SMD, tact switch, buzzer | generic parts, dimensions vary by maker: **check with the 1:1 sheet** |
| **1:1 sheet** | [`hardware/fabrication/check/print_1to1_A4_top.pdf`](../hardware/fabrication/check/print_1to1_A4_top.pdf): top copper, pads with real drill holes, silk and courtyards, scale 1:1 on A4 landscape. Print at 100 % (no "fit to page"; the board edge must measure 202 mm), lay the real parts on it, push the pins through the holes. Costs nothing and settles every footprint question above before any board is ordered |
| Enclosure | requirements, cutout drawing and coordinates: [INSTALLATION.md](INSTALLATION.md), `hardware/fabrication/enclosure/` (OLED window estimated) |
| K1 is not a safety function, contactor snubber, external antenna for metal cabinets | documented with the wiring and the label text in [INSTALLATION.md](INSTALLATION.md) |
| Needs real hardware (no desk work possible) | the relay start-up test, the 5 V terminal test, supply-voltage margins, EMC / ESD, WiFi / NFC range, heat, all of PRODUCTION.md section 4 |

### 0.4 Industrial checklist (not circuit changes - installation and production)

| # | Item | Recommendation |
|---|------|----------------|
| I1 | **Not a safety device.** K1 interrupts the contactor's enable circuit for access control; it is not rated as a safety function (ISO 13849 / IEC 62061) | never wire it in place of the emergency stop, safety relay, light curtain or two-hand control - in series with them at most |
| I2 | Contact life / EMI | put an RC snubber or varistor across the **contactor coil** (most contactor makers sell a plug-on one): longer T73 contact life and much less interference reaching the ESP32 |
| I3 | Dust, oil mist, metal particles near presses | closed enclosure (IP54 or better), cable glands with strain relief, ferrules on stranded wire; **conformal coating** of the assembled board (mask the terminals, relays, sockets, buzzer, USB, the ESP32 antenna area) |
| I4 | Cable routing | 230 V wiring (J1 / J2 / J5 / J10-J13) separate from the low-voltage cables (J4 DS18B20, J6, J7, J9); screened cable for the DS18B20 probe near VFDs / welders |
| I5 | Metal cabinet | the PCB antenna does not work inside metal: use ESP32-WROOM-32**U** (same footprint) with an external antenna |
| I6 | USB service port | a PC terminal that toggles DTR / RTS can reset the ESP32 (auto-reset circuit): the machine relay drops. Connect a PC only with the press stopped |
| I7 | Supply choice | mains on J1 **or** a regulated 5 V / >= 1.5 A adapter on J6. Boards that will always use J6 may be assembled without J1, F1, RV1, PS1 (about 0.7 M Toman less); then J2 and the relay contacts are still 230 V |
| I8 | Pilot | 3-5 boards through the full test (PRODUCTION.md section 4) and a 24 h burn-in before the run; ESD gun 4 kV on the front, contactor switching next to the board |

## 1. What changed from v5 and why (simplification for mass production)

The owner's critical functions: **230 V opto input to count strokes, NFC, a display, the press-contactor relay, a temperature sensor**
(plus the two call buttons and MQTT). Everything else was removed.

| v5 | v6 | Why |
|----|----|-----|
| two boards (MAIN 188 x 104 + PANEL 150 x 96) + 26-wire ribbon | **one board 202 x 104 mm** | one PCB, no cable, no IDC parts, one assembly |
| STM32F103C8T6 + 8 MHz crystal + SWD + ESP-12F + UART link + two USB ports / HID bootloader | **one ESP32-WROOM-32** + one USB port (CH340G auto-reset) | one chip, one firmware; the ESP32 has enough pins, two cores and an interrupt-capable input for the stroke timing; same chip cost (voltatech 630 k Toman vs STM32 299 k + ESP-12F 336 k) |
| 2.4in ST7789 TFT (1.31 M Toman), 26-pin panel cable | **0.96in I2C OLED module** (449 k Toman), 4-pin socket | cheaper, 4 wires, readable at ~1 m; the web page has everything else |
| 7 buttons (D-pad + OK + 2 calls) | **2 call buttons** | no on-device menu any more: all settings are on the web page; cards and the display still work without a network |
| 5 relays (K1 + 4 outputs) | 5 relays (K1 + 4 outputs on free ESP32 pins, v6.1) | kept at the owner's request: use every free port |
| 12 V / 230 V ADC inputs, ZMPT101B module | two DC inputs 0-30 V (12/24 V sense, AUX) on ADC1; ZMPT101B removed in v6.2 | ADC1 works while WiFi runs |
| mains supply only | mains (HLK-5M05) **or a 5 V adapter on J6** (v6.2) | |
| LM35 analog sensor | **DS18B20** waterproof probe (digital 1-wire) | immune to cable noise, no ADC calibration, +-0.5 C |
| 136 + 35 = 171 parts on two boards | **124 parts** on one board | |
| about 5.1 M Toman per unit | **about 2.9 M Toman** of parts (about 2.5 M without the 4 output relays), four shops | see the BOM |

Kept unchanged because they were already reviewed: mains input (T1A fuse on board, varistor, HLK-5M05), the press-sense input (one 120 k 1 W
resistor, MB10S, PC817), the relay stage, the 5 mm creepage rules and the routing method.

## 2. Findings / open points

| # | Finding | Severity | Status |
|---|---------|----------|--------|
| 1 | **MB10S pin numbering.** Datasheet (Diodes DS39045): 1 = **+**, 2 = ~, 3 = **-**, 4 = ~; KiCad's `TO-269AA` numbers the pads 1 / 2 down the left, 3 / 4 up the right. Mapping the datasheet figure onto the footprint (rotation only) gives **pad 1 = +, pad 2 = -, pads 3 / 4 = AC** - used in `design.py`. | high if wrong (sense input dead) | Silk "+" / "-" beside BR1. **First article: "+" lead on pad 1.** |
| 2 | **Relay coil <-> contact insulation is only basic** (SRD / T73: 1500 VAC, pads 3.55 mm inside the relay). The coil side is the 3.3 / 5 V logic, which includes the buttons, the OLED and the USB port. | design limit | The board must sit behind a closed insulating front (class II style enclosure: buttons with plastic plungers, OLED and NFC behind a window). Never use the USB port while mains is connected unless the enclosure is closed. For reinforced insulation use a relay rated for it (>= 6 kV / 8 mm). |
| 3 | **Relay contact track current:** 1.2 mm, 35 um copper -> about 2.7 A at +10 K (all five relays). | design limit | Use the contacts only for control circuits (contactor coils, lamps, horns), never for motors. Put this on the enclosure label. |
| 3b | **5 V budget with all five relays on:** 5 x 74 mA = 0.37 A + ESP32 / logic ~0.25 A (WiFi peaks 0.5 A for ms) = 0.6-0.9 A of the HLK-5M05's 1 A. | design limit | C1 100 uF carries the WiFi peaks; check the 5 V rail with all relays on during WiFi TX on the first article. With J6: adapter >= 1.5 A; F2 holds 1.1 A at 20 C (about 0.85 A at 50 C) - enough for the average load, the peaks are ms long. |
| 3c | **Voltage inputs J7 / J9:** 100 k / 10 k, 0-30 V, the ESP32 ADC is nonlinear below ~0.1 V and above ~2.9 V at the pin (= ~1 V and ~32 V at the terminal). | calibration | `v12cal` / `auxcal` against a meter if better than ~3 % is needed. |
| 4 | **Single sense resistor** RS1 (120 k 1 W): one failure point; a short is limited only by the 1 A fuse. | safety | Use a flame-proof / fusible metal-oxide type rated >= 400 V working voltage (datasheet check). |
| 5 | USB data lines have no ESD protection. | low (service port inside the enclosure) | Accepted. |
| 6 | **ESP32 power:** WiFi TX bursts of ~0.5 A on 3.3 V. AMS1117 (1 A) with 47 uF + 10 uF + 100 nF at the module; the HLK-5M05 (1 A) has 0.6 A headroom. Brown-out during a burst would reset the ESP32 (relay off = safe). | check on the first article | Measure the 3.3 V rail during WiFi TX with a scope: dips must stay above 3.0 V. |
| 7 | **AMS1117 heat:** 1.7 V x (0.12 A average, 0.25 A with WiFi traffic) = 0.2-0.43 W; 3V3 copper area under the tab. | thermal | Calculated about +25 K worst case. Check at 50 C ambient in the enclosure. |
| 8 | **Boot straps / boot glitches:** IO0 (pull-up + auto-reset); IO2 / IO12 left open on purpose (IO12 high at boot would select 1.8 V flash); IO15 = alarm LED (either level only decides whether the ROM prints its boot log); IO5 has the RC522 chip-select pull-up (= its required high level). IO14 / IO15 toggle during boot: they carry the buzzer and the LED, never a relay. The relay pins IO26 / IO27 / IO16 / IO25 / IO13 have no boot-time function and a 10 k pull-down on each transistor base: all relays stay off while booting / flashing (section 0.1). | verified (Espressif datasheet strapping table) | firmware init order fixed in v6.2 |
| 9 | **IO34 (press input) is input-only and has no internal pull-up** - R1 10 k on the board provides it. | verified | - |
| 10 | **Antenna:** the ESP32's PCB antenna sits at the board edge with a 48 x 7 mm copper-free area (all layers) and no parts beside it (Espressif layout guide; the footprint courtyard has the same shape). A metal enclosure kills the WiFi. | design rule | Plastic enclosure, or an ESP32-WROOM-32**U** (external antenna, same footprint) with a cable to an outside antenna. |
| 11 | **RC522 antenna:** the module lies on the board; the copper under its antenna coil (40 x 38 mm) is kept free on both layers. Tags are read through the front window. | design rule | Check the read distance through the real front plate (typical 2-3 cm). |
| 12 | **OLED pin order:** most 0.96in I2C modules are `GND VCC SCL SDA` (used on P2); some are `VCC GND SCL SDA` - those would be reverse-powered. | first article | Buy the module with the matching order; the silk names the pins. |
| 13 | Sense input timing: C5 (22 uF) bridges the zero crossings; the input reads "on" ~35-50 ms after the voltage appears and "off" ~5 ms after it goes (SPICE, with R12). Strokes closer than ~50 ms merge. | functional | Documented; *minimum stroke signal* setting. |
| 14 | ESP32 thermal vias: KiCad's footprint has 0.2 mm vias in the GND pad; the project footprint `ESP32-WROOM-32_Via03` uses 0.3 mm (no hole below 0.3 mm on the board). | fab | Verified by DRC. |

## 3. Power supply

| Check | Result |
|-------|--------|
| Mains input | J1 -> F1 (T1A 250 V, 5x20 glass) in L -> RV1 7D471K across L-N -> PS1 HLK-5M05. Fuse before the varistor: a failed varistor opens the fuse. |
| 5 V budget (calculated) | relay coil 71 mA + LED 3 mA + buzzer <= 30 mA + 3.3 V LDO input (typ. 120 mA, WiFi peaks 0.5 A) -> about 0.23 A typical, 0.6 A peak of the module's 1 A. |
| 3.3 V budget (calculated) | ESP32 80-120 mA average (WiFi on), TX peaks ~0.5 A for a few ms; RC522 13-26 mA; OLED ~20 mA; CH340G 12 mA (only with USB plugged); LEDs 3 x 2-3 mA. |
| Capacitors | 5 V: C1 100 uF + C2 100 nF + C3 10 uF; 3.3 V: C4 47 uF electrolytic (ESR suits the AMS1117), C16 10 uF + C17 100 nF at the ESP32, C22 100 nF at the CH340G. |
| USB power | D14 (SS34) VBUS -> +5 V: a USB cable powers the logic and the relay coil for programming / bench tests; it blocks the module's 5 V from flowing back into the PC. |
| 5 V terminal J6 | J6 -> F2 (PTC 1.1 A hold, 60 V) -> D7 (SMBJ6.5A to GND) -> D8 (SS34) -> +5V. Regulated 5 V adapter >= 1.5 A. Protected against reverse polarity and wrong adapters up to 60 V (F2 trips; it resets when cool). D7 failing short (after a long overload) only disables J6, D8 still blocks. |

## 4. Press-sense input (230 VAC -> IO34)

J2 -> RV2 (across the input) -> RS1 120 k 1 W (axial, leg A; leg B straight to the bridge) -> MB10S -> D5 (red LED, "mains present") in series with the
PC817 LED (R12 2.2 k across it: threshold), C5 22 uF 35 V across the bridge output; PC817 output -> R1 22 k pull-up -> IO34.

| Check | Result (calculated) |
|-------|---------------------|
| LED current at 230 V | (230 x 0.9 - ~4.5 V) / 120 k = 1.7 mA, of which ~0.5 mA goes through R12: **~1.2 mA in the opto LED** (SPICE 1.18 mA; 0.92 mA at 195 V) |
| Resistor stress | 325 V peak (358 V at 253 V), 0.44 W (0.53 W) in a 1 W part: working voltage >= 400 V needed. Surges: RV2 clamps to ~700 V. |
| Opto output | the 22 k pull-up needs 0.15 mA: even a PC817 with CTR 35 % at 0.9 mA saturates (SPICE: 0.39 V). Threshold: below ~0.5 mA (~70-80 VAC on J2) the opto stays dark, so snubber / triac leakage up to ~100 nF is ignored. |
| Input range | made for 220-240 V AC contactor coils. 24 V circuits give too little current; 400 V exceeds RV2 / RS1. |
| Isolation | PC817 5 kV; its input and output pads are > 5 mm apart (wide SOP-4 footprint); mains copper keeps 5 mm to every low-voltage copper item (DRC rule `mains_ac_to_lv`). |

## 5. Relay

| Check | Result |
|-------|--------|
| Coil | 5 V / 70 ohm = 71 mA; Q2 SS8050, base R9 1 k from IO26 (2.6 mA, forced beta 27), R13 10 k pull-down. D12 (M7) flyback. D13 (green) + R43 = "machine on" LED driven by the coil current path. |
| Fail-safe | COM-NO in the contactor circuit: the press is disabled when the board is unpowered, booting, flashing, or after a watchdog reset. NC is also on J5. |
| Pinout | KiCad `SANYOU_SRD_Form_C` (1 COM, 2 / 5 coil, 3 NO, 4 NC). **Compare with the relay you buy.** |

## 6. ESP32 and its peripherals

| Block | Check |
|-------|-------|
| Pins | IO34 press, IO26 relay K1, IO27 / IO16 / IO25 / IO13 output relays, IO36 12/24 V input, IO35 AUX input (IO39 free), IO14 buzzer, IO15 alarm LED, IO32 / IO33 call buttons (10 k pull-ups + 100 nF), VSPI IO18 / IO19 / IO23 + IO5 / IO17 for the RC522, IO21 / IO22 I2C for the OLED (2.2 k pull-ups), IO4 DS18B20 (2.2 k pull-up, 100 R series), UART0 IO1 / IO3 to the CH340G. Analog inputs only on ADC1 pins (ADC2 is blocked while WiFi runs); readings use the chip's eFuse calibration (`analogReadMilliVolts`). |
| EN / IO0 | EN: 10 k pull-up + 1 uF (Espressif reference RC delay); IO0: 10 k pull-up. |
| Auto-reset | NodeMCU circuit: Q7 pulls IO0 low when DTR is asserted and RTS is not, Q8 pulls EN low in the opposite case; asserting both (a terminal opening the port) does nothing. esptool / Arduino IDE upload works without buttons. |
| CH340G | 3.3 V supply, V3 tied to VCC through R97 (0 R), 12 MHz crystal + 2 x 22 pF; TXD -> RXD0, RXD <- TXD0. |
| DS18B20 | 3.3 V supply (not parasitic), 2.2 k pull-up: fine for probe cables of a few metres (4.7 k is the datasheet default; 2.2 k drives longer cables better, the sensor can sink 4 mA). |
| OLED | SSD1306 at 0x3C (0x3D tried as well), 400 kHz; own driver (frame buffer + 5x7 font). |

## 7. PCB

| Check | Result |
|-------|--------|
| Size / layers | 202 x 104 mm, 2 layers, 6 x M3, 3 fiducials, test points TP1 +5V, TP2 +3V3, TP3 GND, TP4 PRESS |
| Mains zones | left part (supply, sense input), bottom-left (relay K1 contacts) and the right strip (output relay contacts), dashed outline on the silk; 5 mm to low voltage by router + DRC rule; mains tracks 1.0-1.2 mm without vias |
| Keep-outs | ESP32 antenna (48 x 7 mm at the top edge), RC522 antenna (40 x 38 mm), fiducial halos |
| Routing | 962 tracks (signals 0.25 mm, power 0.3-0.6 mm, mains 1.0-1.2 mm), 125 vias 0.6 / 0.3 mm, no vias on mains nets |

## 8. First-article checklist (things only a physical part can settle)

| Part | What to compare |
|------|-----------------|
| BR1 MB10S | "+" lead on pad 1 (silk "+"), finding 1 |
| U1 ESP32-WROOM-32 | -32, -32D or -32E with 4 MB flash; antenna at the board edge |
| P2 OLED | pin order GND VCC SCL SDA (finding 12); module lies flat on the 4-pin socket |
| P1 RC522 | module pin order SDA SCK MOSI MISO IRQ GND RST 3.3V; read distance through the front |
| J3 micro-USB + U5 CH340G | connector pad pattern (Wuerth 629105150521 outline vs. the generic part); CH340G pin-1 dot; upload without buttons works |
| RS1 120 k 1 W | lead pitch 15.24 mm (bend the leads), working voltage >= 400 V, flame-proof |
| U2 PC817 SMD | narrow 4.4 mm or wide 7 mm SOP-4 body: the universal footprint takes both; pin 1 = anode (dot) |
| K1-K5 relays | 5 V coil, 10 A 250 VAC, SRD pin pattern, pin 1 COM; terminals J10-J13: wire entry towards the board edge |
| F2 PTC, D7 TVS | F2 radial lead pitch 5.1 mm (footprint 5.0 mm, the leads bend); D7 cathode band towards F2 / the "+" side |
| F1 fuse holder | pitch 22.5 +- 0.5 mm (slotted holes) |
| SW1 / SW2 | 6x6x5 SMD tact switch pad pattern vs. the Omron B3S-1000 footprint |
| BZ1 | active 5 V buzzer, 12 mm, 7.6 mm pitch, + on pad 1 |
| PS1 HLK-5M05 | fits only one way; pins 3 = -Vo, 4 = +Vo |
| DS18B20 probe | wire colours (red 3V3, yellow DATA, black GND) - some probes differ |
| electrolytics, diodes, LEDs | polarity against the position file rotation |

## 9. What the tools verified

| Check | Result |
|-------|--------|
| ERC | 0 errors, 0 warnings; netlist identical to design.py |
| DRC (clearances, silk, drill, courtyard, **schematic parity**, all track errors) | **0 violations, 0 unconnected, 0 footprint errors** |
| 5 mm mains <-> low-voltage rule | active: a copy of the board with a low-voltage resistor 2-4 mm beside the mains pads gives 11 `mains_ac_to_lv` / `mains_dc_to_lv` errors; the real board none (re-run on v6.2) |
| Firmware | compiles (esp32 core 3.3.12, `--warnings all` clean): 1,248,699 bytes (63 % of the 1.9 MB app partition), 68.6 KB RAM |
| SPICE | `hardware/sim/sense_input.py`, `relay_driver.py` (section 0.3) |
