# Installation guide (v6.2): enclosure, wiring, labels, what the device is and is not

For the person who builds the board into a machine. Read [DESIGN_REVIEW.md](DESIGN_REVIEW.md) section 0.4 first. Nothing here has been
tested on a built unit; the points marked **check** must be confirmed on the pilot boards.

## 1. What the device is - and is not

* It is a **production monitor with access control**: it counts strokes, identifies people and dies, and switches the *enable* circuit of the press
  contactor with relay K1.
* It is **not a safety device**. K1 and its firmware are not rated as a safety function (ISO 13849-1 / IEC 62061), the board has no safety
  certification, and the contacts are a single channel without monitoring. **Never** use K1 instead of, or in parallel with, the emergency stop,
  a safety relay, a light curtain, a two-hand control or a guard interlock. K1 may only be wired **in series** with those, in the normal start /
  enable circuit.
* Fail-safe behaviour (by design, not measured): with COM-NO in the enable circuit the press is disabled when the board is unpowered, starting,
  reset, programmed over USB, or after a watchdog restart. After a restart the machine relay stays off until a personnel card is shown.
* Remote start (web / MQTT) is **off** in the factory settings; switching OFF over the network always works.

## 2. Enclosure

| Requirement | Why | Value |
|-------------|-----|-------|
| Closed, insulating (class II style) | the relay coil-to-contact insulation is only basic (1.5 kV), and the front parts (buttons, OLED, NFC, USB) are on the logic side | plastic (ABS / PC), no accessible metal, no exposed 230 V terminal |
| Protection | oil mist, coolant, metal dust, splashes near presses | **IP54 or better**; cable glands with strain relief; ferrules on stranded wire; the lid gasket intact |
| Plastic front, no metal behind the NFC window | RC522 range (13.56 MHz) and the ESP32 antenna | front <= 3 mm; no metal, foil, magnet or carbon plastic within 15 mm of the RC522 antenna area and the ESP32 antenna (top edge of the board) |
| Metal cabinet | a PCB antenna does not work inside metal | fit the **ESP32-WROOM-32U** (same footprint, U.FL connector) and a cable to an antenna **outside** the cabinet (BOM line "ESP32" alternative); the RC522 reader must still be behind a plastic window |
| Temperature | AMS1117, PS1, relay coils | ambient <= 50 C inside; keep 10 mm free above PS1 and the relays; do not mount the board above a heat source |
| Mounting | 6 x M3 holes | standoffs >= 6 mm; the board is 202 x 104 mm, height about 28 mm (relays, PS1); the OLED and RC522 modules add about 12 mm |

Openings and light pipes: [`hardware/fabrication/enclosure/enclosure_interface.svg`](../hardware/fabrication/enclosure/enclosure_interface.svg) and
`.csv` (generated from the board layout by `hardware/tools/make_enclosure_drawing.py`): the 2 call buttons (5 mm plunger holes, plastic caps),
the three LEDs (2 mm light pipes), the OLED window, the USB slot, the terminal entries. **The OLED window position is an estimate**: print the
drawing 1:1, plug in the module you will use and mark the window from the real module.

## 3. Wiring

| Terminal | Connect | Notes |
|----------|---------|-------|
| J1 | 230 VAC L, N | fused on the board (T1A 5x20 time-lag, fuse in L). Protective earth goes to the enclosure fixings if any are metal, not to the board |
| J6 | regulated **5 V**, >= 1.5 A (adapter) | instead of J1; observe polarity (printed +5V / GND); reversed or 9-48 V: F2 trips and the board stays dead until the fault is removed |
| J2 | the 230 V **contactor coil**, in parallel with it | the press input; no current is switched. Use 220-240 V AC contactor coils only. With a 24 V coil this input does not work |
| J5 | relay K1: COM and NO in the **enable circuit** of the contactor (series with the other interlocks); NC is available | 2.5 A maximum, control circuits only |
| J10-J13 | output relays OUT1-OUT4: COM / NO / NC | 2.5 A, lamps, horns, PLC inputs, contactor coils; never motors; same insulation notes as J5 |
| J4 | DS18B20 probe: red 3V3, yellow DATA, black GND | probe cable up to a few metres; use a screened cable near drives and welders, screen to GND at the board end only |
| J7 / J9 | 12/24 V sense and AUX: DC 0-30 V against GND | not galvanically isolated: GND is the board's logic ground. Do not connect to a circuit that is at mains potential |

Cable routing: keep 230 V wiring (J1, J2, J5, J10-J13) away from the low-voltage wiring (J4, J6, J7, J9, USB); cross at 90 degrees.

### Contactor coil snubber (recommended for every installation)

Switching an inductive contactor coil produces voltage spikes that wear the relay contacts and can reset the ESP32.

* Put an **RC snubber or a varistor across the contactor coil** (plug-on suppressors are sold for most contactor makes; for 230 V AC about
  100 ohm + 100 nF / 275 V X2, or a 275 VAC varistor).
* **Do not use a snubber across the contact that switches the coil** if that coil is also what J2 senses: a snubber across the contact leaks a
  current into the coil while the press is off. The board tolerates up to about 100 nF (SPICE, `hardware/sim/sense_input.py`), 220 nF is too much.
* Never put a diode across an AC coil.

## 4. Labels on the enclosure (text to print)

Minimum, in the language of the plant (and in English):

1. **WARNING 230 V** - "Hazardous voltage inside. Disconnect power before opening. Qualified persons only." (lightning-in-triangle symbol)
2. "Supply: 230 V AC 50 Hz, fuse T1A 250 V **or** 5 V DC 1.5 A" (whichever is wired)
3. "Relay contacts K1, OUT1-OUT4: 250 V AC, **2.5 A max**, control circuits only"
4. "**Not a safety device** - does not replace the emergency stop or the safety circuit"
5. Next to the terminals: the silk-screen names are on the board; repeat L N / SNS A B / NO COM NC on the lid sticker if the terminals are hidden.

If the product is **sold** (not only used in your own plant), the electrical safety standard of your market (IEC 62368-1 / 61010-1 / the
machinery rules) and EMC testing apply and are not covered by this repository.

## 5. Commissioning checklist (after wiring, power off first)

1. Check the wiring against section 3 with a meter, power off; the contactor coil is the only thing on J2.
2. Cover on, power on: green PWR LED, the OLED shows the counter. Show a personnel card: green ON LED, relay K1 clicks, the press is enabled.
3. Start the press once: one stroke counted, red mains LED D5 follows the contactor.
4. Remove the supply: the press must be disabled. Restore it: the press stays disabled until a card is shown.
5. Change the default WiFi / admin password, set the MQTT broker, test the remote command with the machine stopped.
6. Do not connect a laptop to the USB port with the press running: a terminal program toggling DTR / RTS resets the ESP32 (relay drops).
