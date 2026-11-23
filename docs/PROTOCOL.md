# Protocols (v6)

Everything that crosses a boundary of the board. Since v6 one ESP32 does everything, so there is no internal link any more
(the v5 STM32 <-> ESP8266 UART protocol is in `firmware/archive_v5/`). Error codes and command numbers are defined in
[`firmware/press_counter/pc_proto.h`](../firmware/press_counter/pc_proto.h).

```
 press contactor coil (230 VAC) ──► opto ─┐
 personnel / die cards ──► RC522 ─────────┤
 DS18B20 probe, 2 call buttons ───────────┼── ESP32 ──WiFi──►  MQTT broker / firmware server / web browser
 press-contactor control circuit ◄── relay┘     │ USB (J3): upload + console
```

## 1. MQTT (the only data backend)

Everything lives under `<prefix>/<device>/` (`<prefix>` = *Topic prefix* in the settings, default `press`; `<device>` = the device name).
QoS 0. An event stays in the board's queue until it was handed to a connected broker, so nothing is lost while the broker is away (RAM queue
of 64 events; lost on power loss). `ts` = Unix seconds (NTP; events from before the first NTP answer are back-dated when published, 0 if
the clock never got set). `seq` = event sequence number since boot (gaps = dropped events, E07).

| Topic | Direction | Content |
|-------|-----------|---------|
| `online` | out, retained | `online` / last will `offline` |
| `status` | out, retained, every 5 s | `{"relay":0/1,"count":N,"lifetime":N,"lastDuration":ms,"pressing":0/1,"temperature":C or null,"error":code,"errors":bitmask,"operator":"..","die":"..","calls":bits,"session":s,"queued":N,"uptime":s,"rssi":dBm,"fw":"6.2"}` |
| `info` | out, retained, every 10 min and on connect | `{"name":"..","ip":"..","rssi":..,"mac":"..","fw":"6.2","hw":"v6.2"}` |
| `event/press` | out | `{"count":N,"duration":ms,"temperature":C,"relay":0/1,"seq":n,"ts":s}` - one per stroke |
| `event/nfc` | out | `{"tag":"personnel:12","kind":"P|D|U|W","relay":0/1,"seq":n,"ts":s}` (`relay` = state after the card; `W` = card written) |
| `event/relay` | out | `{"relay":0/1,"reason":n,"seq":n,"ts":s}`  reason: 0 card, 1 button, 2 web, 3 idle timeout, 4 remote (MQTT / console), 5 boot, 6 error (over-temperature) |
| `event/call` | out | `{"type":1|2,"state":1|0,"seq":n,"ts":s}` (1 supervisor, 2 net / maintenance; state 1 raised, 0 cancelled / acknowledged) |
| `event/error` | out | `{"code":n,"state":1|0,"detail":n,"text":"..","seq":n,"ts":s}` |
| `event/temperature` | out, every 60 s | `{"temperature":C,"seq":n,"ts":s}` |
| `sensor/voltage` | out, retained, every 5 s | `{"v12":24.12,"aux":0.00}` volts (J7 12/24 V sense input, J9 AUX input, both 0-30 V) |
| `event/output` | out | `{"output":n,"relay":n+1,"state":0/1,"reason":n,"seq":n,"ts":s}` (OUT1..OUT4; reason 7 = automatic by its mode) |
| `relay/<n>/state` | out, retained | `ON` / `OFF`; n = 1 the machine relay K1, 2..5 the output relays OUT1..OUT4 (K2..K5) |
| `relay/<n>/set` | **in** | `ON`, `OFF`, `TOGGLE` (or `1` / `0`). ON is refused unless *Allow remote start* is set; OFF always works; an output that is not in manual mode refuses commands |
| `cmd` | **in** | `{"cmd":"relay","relay":1..5,"state":"on|off|toggle"}`, `{"cmd":"out_on|out_off","arg":1..4}`, `{"cmd":"relay_on|relay_off|reset|clear_errors|reboot"}`, `{"cmd":"ack","arg":0|1|2}` (0 = all calls), `{"cmd":"call","arg":1|2}`, `{"cmd":"buzz","arg":0|1|2}` (ok / warning / error), `{"cmd":"write_tag","text":"die:A12"}` |

`calls` bit mask in `status`: bit 1 = supervisor call open, bit 2 = net call open. `errors`: bit *n* = error code *n* active.
The topic names are the same as in v5 (`status` also carries `relays`, `outputs` bit mask, `v12`, `aux`; `v230` was removed in v6.2 together with the ZMPT101B input).

## 2. HTTP (port 80)

Every POST (and `GET /api/settings`, `GET /api/scan`) needs HTTP basic auth, user `admin`, once an admin password is set; without one the
device is open to the whole network.

| Request | Purpose |
|---------|---------|
| `GET /` | the web page (gzip, built into the firmware); the WiFi form while the setup access point is the only network |
| `GET /api/state` | everything shown on the page: `net`, `m` (machine: counter, relay, temperature, operator, die, calls, errors, LED / button mirror), `update` |
| `GET /api/settings`, `POST /api/settings` | JSON `{"net":{...},"machine":{"minpulse":"20",...}}`; secrets are never returned (`...Set` flags) and only replaced by a non-empty value; out-of-range machine values are rejected (HTTP 400 lists them) |
| `POST /api/cmd` | form `cmd=relay|out|reset|buzz|call|ack|clr|reboot`, `arg=...` (`out`: `n:s`, n = 1..4, s = 0 / 1 / 2 toggle; `relay`: 0 / 1 / empty = toggle; `buzz`: success / warning / error; `call` / `ack`: 1 / 2, `ack` 0 = all) |
| `POST /write_tag` | form `tagText=...` (max 38 characters): write it to the next card within 30 s |
| `POST /api/wifi`, `GET /api/scan` | WiFi credentials / scan (also used by the setup page) |
| `POST /api/update` | form `action=check|install` |
| `GET /api/log` | the last 32 log lines |
| `POST /api/restart` | restart (the machine relay is switched off first) |
| `GET /status`, `GET /press_counter`, `POST /relay`, `POST /buzzer_error|warning|success` | kept from v2 for old dashboards |

Setup access point: `PressCounter-XXXXXX`, password `presscounter`, address 192.168.4.1, captive page. It opens when no WiFi is configured,
when the WiFi has been gone for 2 minutes, by holding both call buttons for 6 s, or with the console command `setup`; it closes 3 minutes after
the normal WiFi connection works.

## 3. Firmware update server

A folder served over HTTP(S) containing `manifest.json` and the application image (tools: `firmware/tools/make_manifest.py`, `update_server.py`):

```json
{ "esp32": { "version": "6.2", "url": "press_counter-6.2.bin", "md5": "<32 hex>", "size": 912345 } }
```

The board sends `Authorization: Bearer <token>` if a token is set, installs only a higher version, only while the machine is off. See
[FIRMWARE.md](FIRMWARE.md).

## 4. USB console (115200 baud)

`help status cfg set <key> <val> save defaults err clear relay <0|1> out <1..4> <0|1|2> reset call <1|2> ack buzz tag [write <text>] wifi <ssid> <pass> mqtt <host> [port] setup log reboot`
- for commissioning and service; `relay 1` obeys *Allow remote start* like the network does.

## 5. Error codes

| Code | Text | Meaning |
|------|------|---------|
| E01 | RFID READER | RC522 does not answer on SPI |
| E02 | TEMP SENSOR | DS18B20 missing or CRC errors (3 failed readings in a row) |
| E03 | SENSE STUCK | press-sense input active longer than `stuck` seconds |
| E04 | OVER TEMP | temperature above `overtemp`; the machine is switched off and cannot be enabled |
| E06 | NO SERVER | MQTT configured but not reachable for 5 minutes |
| E07 | QUEUE FULL | event queue overflowed, events were dropped |
| E08 | SETTINGS | stored settings missing / from another firmware layout, defaults loaded |
| E09 | WATCHDOG | the last reset was a watchdog or a crash |
| E10 | STROKE LOCKED | a stroke was sensed while the machine is switched off (contactor bypassed?) |
| E11 | UPDATE FAILED | firmware update failed |
| E12 | TAG WRITE | writing a card failed / timed out |
| E13 | DISPLAY | OLED enabled in the settings but not found on I2C |

Severity: E01 E02 E03 E04 E10 are *errors* (red LED fast); the rest are *warnings* (slow). E05 (v5: gateway link) is no longer used; the other
numbers are unchanged from v5.
