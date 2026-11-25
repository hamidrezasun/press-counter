# Firmware (v6: one ESP32 does everything)

```
firmware/
  press_counter/     Arduino sketch for the ESP32-WROOM-32 on the v6 board (open press_counter.ino)
    pc_*.cpp / .h    counter, relay, NFC, DS18B20, OLED, buttons, events, MQTT, WiFi, web page, updates, console
    data/            web page sources (index.html, setup.html) -> pc_page.h (tools/embed_page.py)
  tools/             embed_page.py, make_manifest.py, update_server.py, mock_device.py (web page without hardware)
  archive_v5/        the v5 firmware (STM32 counter + ESP8266 gateway) - not used any more
```

## Verification status (honest)

| Part | State |
|------|-------|
| Firmware | **compiles** with arduino-cli + Arduino core esp32 3.3.12 (board ESP32 Dev Module, partition *Minimal SPIFFS*): 1,248,699 bytes = 63 % of the 1.9 MB app partition, 68,552 bytes static RAM; no warnings with `--warnings all` (v6.2, 2026-10-12). **Never run on a chip.** |
| Web page | checked in a browser against `tools/mock_device.py` (renders, no script errors, the settings form posts). Not checked against the real board. |
| Not run on hardware | everything: stroke timing on GPIO34, the output relays, the two ADC inputs (12/24 V, AUX), the RC522 reader, the own SSD1306 driver (`pc_display.cpp`), the own DS18B20 1-wire driver (`pc_temp.cpp`), MQTT, OTA, the auto-reset programming circuit. |
| Hardware | designed and DRC-checked, **not built** - see [DESIGN_REVIEW.md](DESIGN_REVIEW.md). |

## Build and upload (like an Arduino board)

One-time setup in the Arduino IDE (or `arduino-cli`):

1. *Preferences -> Additional boards manager URLs:* `https://espressif.github.io/arduino-esp32/package_esp32_index.json`, then *Boards manager -> esp32 by Espressif* (3.x).
2. *Library manager:* **MFRC522** (miguelbalboa, 1.4.x), **PubSubClient** (2.8), **ArduinoJson** (version **6**.x, not 7).
3. *Tools:* Board **ESP32 Dev Module**, Partition scheme **Minimal SPIFFS (1.9MB APP with OTA)**, Upload speed 921600 (460800 if it fails), the COM port of the board (CH340 driver: Windows `CH341SER` from wch.cn; Linux / macOS have one built in).

Open `firmware/press_counter/press_counter.ino`, plug a micro-USB cable into the board (**J3**) and press **Upload**. The CH340G's DTR / RTS lines put the ESP32 into its bootloader and reset it afterwards - no buttons. The board can stay powered from mains or run from the USB cable alone (the relay coil is powered too, the mains parts are not).

```
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs firmware/press_counter --output-dir build
arduino-cli upload  --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs -p COM5 firmware/press_counter
```

After editing `data/index.html` or `data/setup.html` run `python firmware/tools/embed_page.py` (writes `pc_page.h`) and build again.

The USB port is also the **console** (115200 baud, the Arduino serial monitor works): log lines and the commands
`help status cfg set <key> <val> save defaults err clear relay <0|1> out <1..4> <0|1|2> reset call <1|2> ack buzz tag [write <text>] wifi <ssid> <pass> mqtt <host> [port] setup log reboot`.
Opening a terminal does not reset the board when the terminal asserts DTR and RTS together (the auto-reset circuit only reacts to the uploader's alternating sequence). If the board restarts when you open a terminal, switch DTR / RTS off in the terminal program.

## Pins (board v6)

| GPIO | Function | GPIO | Function |
|------|----------|------|----------|
| 34 | PRESS (PC817 output, LOW = contactor energised; input-only pin, 10 k pull-up) | 26 | RELAY K1 (HIGH = machine enabled; 10 k pull-down while booting) |
| 27 / 16 / 25 / 13 | output relays OUT1..OUT4 (K2..K5, HIGH = on, 10 k pull-down) | 36 / 35 | ADC1: 12/24 V input J7, AUX input J9 (both 100k/10k = 0-30 V); IO39 free |
| 18 / 19 / 23 | SPI SCK / MISO / MOSI (RC522) | 5 / 17 | RC522 SS (10 k pull-up, boot strap) / RST |
| 21 / 22 | I2C SDA / SCL (OLED, 2.2 k pull-ups) | 4 | DS18B20 data (2.2 k pull-up, 100 R series) |
| 32 / 33 | CALL SUPERVISOR / CALL NET buttons (LOW = pressed) | 14 | buzzer (HIGH = on; IO14 toggles at boot: a short chirp) |
| 15 | red ALARM LED (boot strap: either level is fine) | 1 / 3 | UART0 = USB (CH340G) |
| 0 / EN | boot / reset, driven by the CH340G's DTR / RTS | 2, 12 | not used on purpose (boot straps: IO12 high at boot would select 1.8 V flash) |

## Using it

**Display** (0.96in OLED): device name and network state on top (`MQTT` = all good, `NO MQTT`, `NO WIFI`, `SETUP`), the stroke counter in big digits, `ON <operator>` / `OFF`, the temperature, and a bottom line with the most important message (banner, error `Exx` text, call, card write prompt, die, else the IP address). It dims after 2 minutes without a stroke, card or button.

**Machine relay** (K1, press contactor): always OFF after a reset or power loss. A `personnel:<id>` card toggles it (and starts / ends a work session; the session counter starts at 0). The web page / MQTT / console can switch it OFF at any time; switching it ON from the network needs the setting *Allow remote start* (off by default: a press must not be started remotely unless the owner decides so). It switches off by itself after *idle* minutes without a stroke and on over-temperature (E04).

**Cards** (RC522, MIFARE Classic 1K / NTAG / Ultralight with one NDEF text record): `personnel:<id>` toggles the machine, `die:<id>` records the mounted die, anything else is logged as unknown. Cards are written from the web page (*Write card*), MQTT (`{"cmd":"write_tag","text":"die:A12"}`) or the console (`tag write personnel:1234`): the next card held to the reader within 30 s is written. Cards written by the v2 / v5 firmware stay readable (same NDEF layout).

**Call buttons:** a short press raises the call (buzzer, red LED blinks, MQTT event), holding the button 1.5 s cancels it; a supervisor acknowledges it on the web page or over MQTT. **Holding CALL NET for 3 s opens the local menu** (update, log, WiFi setup, machine settings) - see [MENU.md](MENU.md).

**Output relays OUT1-OUT4** (K2-K5, COM / NO / NC): manual ones are switched on the web page, over MQTT (`relay/2..5/set`) or on the console (`out 2 1`); ON needs *Allow remote start*, OFF always works. Automatic ones follow their mode (see settings). All are OFF after a reset.

**Voltage inputs:** 12/24 V sense (J7) and AUX (J9), both 0-30 V (100 k / 10 k), read every 10 ms and averaged over 200 ms. Shown on the web page, published on MQTT `sensor/voltage` and `status` every 5 s. The ESP32's factory ADC calibration is used; `v12cal` / `auxcal` trim the scale.

**Relays at start-up:** `io::begin()` and `outputs::begin()` run first in `setup()`; `io::outputLow()` writes the output latch LOW with `gpio_set_level()` before `pinMode(OUTPUT)` (in core 3.x a `digitalWrite()` on an unconfigured pin is ignored). Together with the 10 k base pull-downs no relay can pulse while the board starts, resets or is programmed (DESIGN_REVIEW.md section 0.1). After any restart the machine relay stays off until a card is shown; manual outputs start off.

**Red LED:** fast blinking = error (E01-E04, E10), slow = warning or open call. The green *ON* LED is wired to the relay coil.

**Errors** (`Exx` on the display, MQTT `event/error`, web page): E01 RFID reader, E02 temperature sensor, E03 sense input stuck, E04 over-temperature (machine stops), E06 MQTT not reachable for 5 min, E07 event queue overflow, E08 settings reset, E09 watchdog / crash restart, E10 stroke while the machine was off (contactor bypassed?), E11 update failed, E12 tag write failed, E13 display missing.

## First setup

1. Power the board. Without WiFi settings it opens the access point **`PressCounter-XXXXXX`**, password **`presscounter`** (the display shows it). Connect a phone; the setup page opens (or browse to `http://192.168.4.1`), choose the WiFi and enter its password.
2. The board joins the WiFi; the display shows its IP address (also reachable as `http://<device-name>.local`). Open it and set on *Settings*: device name, MQTT broker (host, port, topic prefix, user / password), **an admin password** and the OTA password, *Allow remote start* if wanted, the over-temperature limit.
3. Write the personnel and die cards from the *Status* page.

If the WiFi is gone for 2 minutes the setup access point opens again by itself (the counter keeps counting and switching all the time).

## Settings (stored in the ESP32 NVS)

| Key | Default | Range | Meaning |
|-----|---------|-------|---------|
| `name` | press-1 | 1-23 chars | device name: MQTT topic part, host name |
| `minpulse` | 10 ms | 1-5000 | a sense signal shorter than this is noise (use 20-30 ms: C5 lengthens the signal by ~30 ms) |
| `idle` | 10 min | 0-600 | machine off after this many minutes without a stroke (0 = never) |
| `stuck` | 60 s | 5-3600 | E03 when the sense input stays active this long |
| `tempoff` | 0 | -100..100 | DS18B20 offset, 0.1 degC |
| `overtemp` | 700 | 300-1200 | E04 limit (0.1 degC): the machine is switched off |
| `remote` | 0 | 0 / 1 | web / MQTT may switch the machine ON (stop is always allowed) |
| `sound` | 2 | 0-2 | buzzer: off / errors only / all |
| `bright` | 80 | 5-100 | OLED contrast |
| `display` | 1 | 0 / 1 | an OLED is fitted (E13 if it is missing) |
| `out1`..`out4` | 0 | 0-5 | mode of output relay OUTn: 0 manual (web / MQTT `relay/2..5/set` / console `out`), 1 follows the machine relay, 2 on while any error or call is active, 3 supervisor call, 4 net call, 5 press sense input active |
| `v12cal`, `auxcal` | 1000 | 500-2000 | calibration of the 12 V / AUX input, permille |

Network settings (WiFi, MQTT, NTP, update server, passwords) are on the web page *Settings* / *Update*; the console has `wifi` and `mqtt`.

## Updates

Three ways: the **USB cable** (Arduino IDE, above), **ArduinoOTA** (the board appears as a network port in the Arduino IDE; password = *OTA password*, default `hydropress` - change it), and **your own update server**:

1. Build, then `python firmware/tools/make_manifest.py --bin build/press_counter.ino.bin --version 6.2 --out fw/manifest.json` (raise `PC_FW_VERSION_STR` in `pc_proto.h` first) and copy the `.bin` into `fw/`.
2. Serve the folder: `python firmware/tools/update_server.py fw --port 8080 --token SECRET` (or any web server, optionally HTTPS).
3. On each board: *Update -> server* `http://<server>:8080`, the token, *Install automatically* if wanted; then *Check now / Install update*.

The image is streamed into the second OTA slot, MD5-checked, and the board restarts into it; a broken download leaves the running firmware untouched. Updates are refused while the machine is ON or a stroke is running; automatic updates only happen while the machine is off. The images are **not signed**: use a server and network you trust.

## Robustness

* Strokes are time-stamped in an interrupt (GPIO34 edge -> ring buffer of 128 edges) and evaluated in the loop, so a blocking network call (MQTT connect up to 4 s, WiFi scan ~2 s) does not lose or lengthen strokes.
* Task watchdog 15 s on the loop: a hang restarts the board (relay drops = machine off = safe), E09 after the restart.
* Events (strokes, cards, relay, calls, errors, temperature) are queued in RAM (64 entries) while the broker is away and published in order when it is back; when the queue is full the oldest temperature reports go first, strokes last (E07). A power loss loses the queue - the lifetime counter is saved in NVS every 25 strokes and 10 s after the last stroke.
* Events that happen before NTP answered are back-dated when they are published.
* Over-temperature switches the machine off; a stroke while the machine is off raises E10 (contactor bypassed).

## Console and over-the-air updates on a bench programmer

Build options (no source change needed):

| Flag | Effect |
|------|--------|
| `PC_NO_CONSOLE` | never touches the Arduino `Serial` (UART driver). Set it when `Serial.begin()` crashes on the module/toolchain in use |
| `PC_ROM_CONSOLE` | logs and the command shell over UART0 (115200) through the ROM (`ets_printf` + direct FIFO reads) — works even with `PC_NO_CONSOLE` |

```
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs \
  --build-property "build.extra_flags=-DPC_NO_CONSOLE -DPC_ROM_CONSOLE" <sketch>
```

Serial commands for updates:

```
update            check the update server and report what it found
update install    download, flash and restart
updateurl <url>   set the update server (folder holding manifest.json) and save it
```

Notes:

* The manifest check/download/flash runs in a FreeRTOS task pinned to **core 0**; the control loop keeps
  counting strokes on core 1 and the watchdog window is widened for the duration of the update.
* An update is installed in every situation: if the machine relay is on it is switched off first (safe
  state), and an active stroke signal is tolerated instead of refusing the update.
* After flashing the app over USB on a board that has previously updated over the air, erase `otadata`
  (`esptool erase-region 0xe000 0x2000`) — otherwise the bootloader keeps booting the OTA slot.
