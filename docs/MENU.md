# Local menu (two buttons, no network needed)

Everything the menu does works on the machine itself: firmware update, log, WiFi setup and every
machine setting. Only two buttons are used and **no gesture ever needs both buttons at once**.

## The two buttons

| Button | Normal operation (menu closed) |
|--------|-------------------------------|
| **CALL SUPERVISOR** (SUP) | short press = call the supervisor, hold 1.5 s = cancel that call |
| **CALL NET / MAINT.** (NET) | short press = call net/maintenance, hold 1.5 s = cancel that call |

## Opening the menu

**Hold CALL NET for 3 seconds.** (The 1.5 s cancel happens first if a call was active; that is
harmless.) A short beep confirms it and the OLED switches to the menu.

The counter keeps counting and the relay keeps its state while the menu is open. The menu closes
itself after leaving it — the display returns to the normal screen.

## Inside the menu

| Gesture | Effect |
|---------|--------|
| short **SUP** | next entry / move up the list / −1 (in a value) |
| short **NET** | open the selected entry / +1 (in a value) |
| hold **SUP** 1 s | back one level; at the top level it leaves the menu |
| hold **NET** 1 s | (only while editing a value) save it and go back |

Every screen shows its own hint on the bottom line, e.g. `NET=ok SUP=next  hold SUP=out`.

## The entries

1. **Check update** — asks the update server (`updateUrl`) and reports the version found. Nothing is installed.
2. **Install update** — downloads, verifies (MD5), flashes and restarts. If the machine relay is on, it is
   switched **off first** (safe state). Also runs while a stroke signal is active.
3. **Show log** — the last log lines; SUP/NET scroll.
4. **WiFi setup** — opens the `PressCounter-XXXXXX` access point (password `presscounter`) so the WiFi,
   MQTT broker and update server can be set from a phone or laptop at `192.168.4.1`.
5. **Settings** — every numeric or yes/no machine setting, edited one step at a time. Text values
   (device name, MQTT host/user/password, tokens, admin/OTA password) are only on the web page.
   Shown: `name`, `minpulse`, `idle`, `stuck`, `tempoff`, `overtemp`, `remote`, `sound`, `bright`,
   `display`, `out1..out4`, `v12cal`, `auxcal`.
   Editing: `NET` = +1 step, `SUP` = −1 step, **hold NET = save** (writes NVS), hold SUP = cancel.
   Step size is 1 for small ranges (booleans toggle), 5 for wide ranges; values clamp to their limits.
6. **Exit** — same as holding SUP.

## Cards

| Card | What happens |
|------|--------------|
| Personnel card | switches the machine **on**; the same card again switches it **off** (counter resets on every change) |
| **Die card** | switches the machine **on for a die try-out** and records the die id. **Strokes are not counted while this is on.** Scan the *same* die card again to switch the machine off; the die stays recorded |
| Blank / unknown card | logged, nothing changes |

Die test details:

* Entering: the machine relay closes (refused only on E04 over-temperature), the session counter is zeroed,
  the OLED bottom line shows `DIE TEST <die>`, and the web state reports `"dieTest": true`.
* While it runs: strokes are timed but **not counted** — no session count, no lifetime count, no stroke event on
  MQTT. Other counters are untouched.
* Exiting: scan the **same** die card again (or a personnel card). Scanning a *different* die card just switches
  the test to that die.
* The die id stays recorded after the test, so the last mounted die is always visible on the display and web page.

## Automatic updates

Automatic installs happen **only** while the machine relay is off **and** no stroke is in progress.
A running press is never interrupted by an automatic update. A manual update (menu, web page or
serial `update install`) always works and switches the machine off first.

## Notes

* While an update is downloading/flashing, the web page is stopped on purpose (only one user of the
  network stack at a time): `http://<board>/` answers again a few seconds after the restart, and the
  counter, relay and display keep working meanwhile.
* The counter, the relay, NFC cards and the display keep working with no network at all.
* An interrupted update (power loss, WiFi drop) leaves the previous firmware running; the boot slot is
  switched only after a complete, MD5-verified write.
* The same menu is reachable without buttons through the serial console: `update`, `update install`,
  `updateurl <url>`, `status`, `log`, `cfg`, `set <key> <val>`, `save`.
