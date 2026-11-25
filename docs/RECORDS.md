# Records: what the machine stores and where to read it

## On the machine (survives a power cut)

| Value | Stored in | Cleared |
|-------|-----------|---------|
| **Die in the press** (`die`) | ESP32 NVS, written on every die-card scan | only by scanning another die card (or a factory reset) |
| Operator (`operator`) | RAM — the card scanned for the session in progress | next session start / reset |
| Session stroke count | RAM | relay off / new session / reset |
| Lifetime stroke count | NVS, flushed a few seconds after the last stroke | never |

The die is deliberately **persistent**: it identifies the tool mounted in the press, so after a power
cut the display, the web page and every event still report which die is in the machine.

## What goes to the server (MQTT)

Everything is published under `<prefix>/<device>/…` — with the defaults `press/press-1/…`:

| Topic | Payload (added fields in this version) |
|-------|----------------------------------------|
| `…/event/press` | `count`, `duration`, `temperature`, `relay`, **`operator`**, **`die`**, `seq`, `ts` |
| `…/event/nfc` | `tag`, `kind`, **`operator`**, **`die`**, `relay`, `seq`, `ts` |
| `…/event/relay` `…/output` `…/call` `…/error` `…/temperature` | as before |
| `…/status` (retained) | full state incl. `operator`, `die`, `count`, `lifetime`, `temperature`, `error`, `rssi` |
| `…/info` (retained) | `name`, `ip`, `mac`, `fw` |

So every counted stroke says **who pressed and which die was in the press**, and a personnel-card scan
records the die that was mounted at that moment.

## On the server (192.168.1.57)

`/srv/pcreport/collector.py` (systemd `pcreport.service`) subscribes to the broker with
`mosquitto_sub`, stores every message in SQLite (`/srv/pcreport/press.db`) and serves a report:

* **http://192.168.1.57:8081/** — devices (state, operator, die, session/lifetime, errors), strokes per
  operator and per die for the last 7 days, the last 40 strokes (time, count, duration, temperature,
  operator, die), card scans and recent errors. Refreshes itself every 15 s.
* **http://192.168.1.57:8081/api/summary** — the same data as JSON, for a dashboard or script.
* **http://192.168.1.57:8081/healthz** — `ok`.

Tables: `messages` (every raw message), `strokes`, `cards`, `devices` (one row per machine, updated by
its retained status). Standard library only; nothing to install.

Useful queries:

```bash
sqlite3 /srv/pcreport/press.db \
  "SELECT date(rx,'unixepoch','localtime') d, operator, die, COUNT(*) FROM strokes GROUP BY d, operator, die ORDER BY d DESC;"
```
(`sqlite3` is not installed on the host; use `python3 -c "import sqlite3;…"` instead.)

Service control:

```bash
systemctl status pcreport      # is it running
journalctl -u pcreport -f      # live
systemctl restart pcreport
```
