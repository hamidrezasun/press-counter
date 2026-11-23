"""Join the HID bootloader and the counter application into one image for 0x08000000.

    python make_full_image.py hid_bootloader.bin counter.ino.bin -o counter_full.bin

The application of a `UPLOAD=hid` build (build_stm32.sh) is linked at 0x08000800 behind the 2 KB bootloader.  The joined file is what you
  * write once with an ST-Link:  st-flash write counter_full.bin 0x8000000     (or STM32CubeProgrammer, address 0x08000000), or
  * give to firmware/tools/make_manifest.py (--stm) so that the gateway can also program a blank chip through the ROM bootloader.
Later updates over the USB cable (Arduino IDE, upload method HID Bootloader 2.2) only replace the application part.
The bootloader binary is not part of this repository (see docs/FIRMWARE.md).
"""
import argparse
import struct
import sys

APP_OFFSET = 0x800
APP_MAX = 0xF800 - APP_OFFSET          # the two settings pages start at 0x0800F800


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("bootloader")
    ap.add_argument("application")
    ap.add_argument("-o", "--out", default="counter_full.bin")
    a = ap.parse_args()
    bl = open(a.bootloader, "rb").read()
    app = open(a.application, "rb").read()
    if len(bl) > APP_OFFSET:
        sys.exit(f"bootloader is {len(bl)} bytes, it must fit in {APP_OFFSET} (2 KB) - a bootloader for another flash layout?")
    if len(app) > APP_MAX:
        sys.exit(f"application is {len(app)} bytes, the limit is {APP_MAX}")
    sp, reset = struct.unpack("<2I", app[:8])
    if not (0x20000000 <= sp <= 0x20005000) or not (0x08000800 <= reset < 0x0800F800):
        sys.exit(f"the application does not look like an image linked at 0x08000800 (SP {sp:#x}, reset vector {reset:#x}): build with UPLOAD=hid")
    out = bl + b"\xff" * (APP_OFFSET - len(bl)) + app
    open(a.out, "wb").write(out)
    print(f"{a.out}: {len(out)} bytes = bootloader {len(bl)} + padding {APP_OFFSET - len(bl)} + application {len(app)}")


if __name__ == "__main__":
    main()
