# Hydraulic press counter (v6.2)

Counts the strokes of a hydraulic die press, identifies operator and die by NFC card, switches the press contactor through an
on-board relay, shows the counter on a small OLED and reports to an MQTT broker. The stroke signal is the 230 V AC on the press
**contactor coil**, read through an optocoupler.

v6 is the "simple and cheap for mass production" revision: **one board, one chip.** An ESP32-WROOM-32 module does the counting, the relay,
the NFC reader, the display, the temperature probe and WiFi / MQTT / web page. The STM32, the ESP8266, the second board, the ribbon cable,
the 2.4in display and the ribbon cable of v5 are gone. The free ESP32 pins drive four extra output relays (COM / NO / NC) and read two DC inputs
(12/24 V sense, AUX, both 0-30 V). The board runs from 230 V (on-board HLK-5M05) **or** from a 5 V adapter on its own screw terminal (v6.2).
Parts cost about 2.9 M Toman of parts per unit (v5: about 5.1 M), every line checked on a shop page (eca.ir, voltatech.ir, javanelec.com, robochip.ir); without the four output relays about 2.5 M.

```
            ┌──────────────────────────── one board, 202 x 104 mm ────────────────────────────┐
 230 V ────►│ fuse · HLK-5M05 5 V (or 5 V adapter) · LDO ESP32-WROOM-32 (WiFi)    OLED 0.96in  │
 contactor ►│ 230 VAC opto input (MB10S + PC817)       USB micro-B (CH340G)      RC522 NFC    │
 coil       │ relay 5 V (COM / NO / NC) ──► press contactor control circuit                    │
            │ DS18B20 probe terminal · 2 call buttons · buzzer · LEDs                           │
            │ 4 output relays (COM / NO / NC) · 12/24 V and AUX inputs (0-30 V)                   │
            └───────────────────────────────────────────────────────────────────────────────────┘
```

| Folder | What |
|--------|------|
| `hardware/` | KiCad 10 project (generated from `tools/design.py`), fabrication files, BOM with Iranian-store links - [hardware/README.md](hardware/README.md) |
| `firmware/press_counter/` | the ESP32 firmware (Arduino sketch) - [docs/FIRMWARE.md](docs/FIRMWARE.md) |
| `docs/` | [DESIGN_REVIEW.md](docs/DESIGN_REVIEW.md) block-by-block check with calculations and open points, [PRODUCTION.md](docs/PRODUCTION.md) what to send to the board house / assembler and how to test, [INSTALLATION.md](docs/INSTALLATION.md) enclosure, wiring, labels, safety limits, [LASER_DIY.md](docs/LASER_DIY.md) prototype with a laser + drill, [ORDER_ESYS.md](docs/ORDER_ESYS.md) ordering at esys.ir, [SOURCING.md](docs/SOURCING.md) which shops (eca.ir + voltatech.ir cover everything), [FIRMWARE.md](docs/FIRMWARE.md), [PROTOCOL.md](docs/PROTOCOL.md) (MQTT topics, HTTP API), [REVIEW.md](docs/REVIEW.md) history of the design decisions |
| `hardware/archive_v5/`, `firmware/archive_v5/` | the previous two-board STM32 + ESP8266 design (reference only) |
| `V2/` | the old single-ESP8266 firmware for the v2 board (kept) |
| `analyzer/` | FastAPI + React dashboard that turns InfluxDB events into work sessions (needs an MQTT -> InfluxDB bridge such as Telegraf) |

## What it does

* **Strokes:** one MQTT event per stroke: `count`, `duration` (ms the contactor was energised), `temperature`; the lifetime counter survives power loss.
* **Who / what:** `personnel:<id>` card = start / end of a work session, switches the relay (enables the press); `die:<id>` card = which die is mounted.
* **Machine relay:** COM / NO / NC on a terminal; with COM-NO in the contactor control circuit the press is *disabled* whenever the board is unpowered, reset or hung (watchdog). Idle timeout and over-temperature switch it off. Remote start over the network is refused unless it was enabled in the settings; stop always works.
* **Local:** OLED with the counter, machine state, operator, temperature, errors (`Exx`), calls and the IP address; two call buttons (supervisor, net / maintenance), buzzer, red alarm LED, green "machine on" LED. Works without any network.
* **Extra I/O:** four output relays OUT1-OUT4 (COM / NO / NC, MQTT `relay/2..5/set`, or automatic: follow the machine, an alarm, a call, the press signal); 12/24 V sense input and AUX input, both 0-30 V (MQTT `sensor/voltage`).
* **Network:** WiFi with a setup access point, NTP, **MQTT** (status, events, `relay/1/set`, `cmd`), web page with every function and setting, ArduinoOTA and updates from your own server.
* **Power:** 230 VAC on J1, or a regulated 5 V adapter on J6 (protected: resettable fuse, TVS, reverse-polarity diode). No relay clicks while the board starts, resets or is programmed.
* **Programming:** one micro-USB cable, Arduino IDE (*ESP32 Dev Module*): the board's CH340G resets the ESP32 into its bootloader by itself, like an Arduino.

## Where it stands (honest status)

| Part | State |
|------|-------|
| PCB | 202 x 104 mm, 124 parts. ERC 0, DRC 0 violations / 0 unconnected / schematic parity OK (KiCad 10); the 5 mm mains creepage rule was shown to trigger on a deliberately wrong placement. **Never fabricated or powered.** Several footprints of generic parts must be checked against the real parts first - [docs/DESIGN_REVIEW.md](docs/DESIGN_REVIEW.md) section 8 |
| Firmware | **compiles** for the ESP32 (Arduino core esp32 3.3.12, no warnings with all warnings on): 1,248,699 bytes of the 1.9 MB app partition, 68.6 KB static RAM. **Never run on a chip** |
| Web page | checked in a browser against a mock (`firmware/tools/mock_device.py`) |
| Mains parts | 5 mm creepage enforced between mains and low voltage, 1 A time-lag fuse on the board; the relay coil / contact insulation is *basic* only - **a qualified person must do the mains wiring and first power-up** |

## Quick start

1. Read [docs/PRODUCTION.md](docs/PRODUCTION.md). Build 3-5 pilot boards first; order the PCB from `hardware/fabrication/main/press_counter_gerbers.zip` and the parts from `hardware/BOM_press_counter_v6.xlsx` (check the footprints of the real parts, see DESIGN_REVIEW).
2. Flash the firmware over USB from the Arduino IDE - [docs/FIRMWARE.md](docs/FIRMWARE.md).
3. Join the setup WiFi `PressCounter-XXXXXX` (password `presscounter`), enter your WiFi, then open the device's page (its IP is on the display): set the MQTT broker, an **admin password** and the OTA password.
4. Wire the contactor coil to `PRESS SENSE 230VAC` (J2), the press-contactor control circuit to the relay contact (J5, COM / NO), the DS18B20 probe to J4, mains to J1 (the 1 A fuse F1 is on the board). Never run the board with the enclosure open.
