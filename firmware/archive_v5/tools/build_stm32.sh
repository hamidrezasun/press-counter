#!/bin/sh
# Compile firmware/counter for the bare STM32F103C8T6 with arduino-cli and the STM32duino core (STMicroelectronics:stm32).
#
#   sh build_stm32.sh [out_dir]
#
# Needs: arduino-cli, core STMicroelectronics:stm32 (tested with 3.0.0), library MFRC522 (1.4.12).  The display driver is display.cpp (no Adafruit libraries).
# Environment:  ARDUINO_CLI  path of arduino-cli          (default: arduino-cli)
#               ARDUINO_CFG  arduino-cli config file      (default: none)
#               OPT          optimisation menu: oslto = -Os + LTO (default, 53 KB) / osstd = -Os (60 KB) / o3 ...
#               EXTRA        extra arduino-cli arguments
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
FW="$HERE/.."
CLI="${ARDUINO_CLI:-arduino-cli}"
CFG=""
[ -n "$ARDUINO_CFG" ] && CFG="--config-file $ARDUINO_CFG"
OUT="${1:-$FW/build/stm32}"
# the sketch folder (flat, with build_opt.h) comes from make_sketch.sh - the same folder the Arduino IDE opens
SKETCH="$OUT/sketch/counter"
sh "$HERE/make_sketch.sh" "$SKETCH"
# Generic F1 board, part F103C8Tx, USB CDC (the 'SerialUSB' console) on PA11/PA12, HSE 8 MHz, -Os
#   UPLOAD=hid   (default) application at 0x08000800 behind the 2 KB HID bootloader 2.2 (upload over USB J3, see docs/FIRMWARE.md)
#   UPLOAD=plain application at 0x08000000, no bootloader (ST-Link / ROM bootloader only)
OPT="${OPT:-oslto}"
UPLOAD="${UPLOAD:-hid}"
case "$UPLOAD" in
  hid)   METHOD=hidMethod; LIMIT=61440 ;;     # 0x0800 .. 0xF800
  plain) METHOD=swdMethod; LIMIT=63488 ;;     # 0x0000 .. 0xF800
  *) echo "UPLOAD must be hid or plain"; exit 1 ;;
esac
FQBN="STMicroelectronics:stm32:GenF1:pnum=GENERIC_F103C8TX,upload_method=$METHOD,usb=CDC,xserial=generic,opt=$OPT"
$CLI $CFG compile --clean --fqbn "$FQBN" --warnings "${WARN:-default}"   $EXTRA --output-dir "$OUT" "$SKETCH"
# the settings and lifetime-counter pages start at 0x0800F800: the application must stay below them
SIZE=$(wc -c < "$OUT/counter.ino.bin")
echo "application: $SIZE bytes (limit $LIMIT, free $((LIMIT - SIZE)))"
[ "$SIZE" -le "$LIMIT" ] || { echo "ERROR: image too large"; exit 1; }
