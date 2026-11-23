#!/usr/bin/env python3
"""Creates manifest.json for the firmware update server (see docs/FIRMWARE.md).

    python make_manifest.py --bin press_counter-6.1.bin --version 6.1 --out fw/manifest.json

Upload the .bin next to the manifest (or give --base-url / an absolute file name).  The board checks the size and the MD5 of the image.
The version is "major.minor" (PC_FW_VERSION_STR in firmware/press_counter/pc_proto.h); the board installs it only if it is newer.
The .bin is the application image of the Arduino build (press_counter.ino.bin), not the merged image with bootloader + partitions.
"""
import argparse
import hashlib
import json
import os
import re
import sys


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", required=True, help="application image (press_counter.ino.bin)")
    ap.add_argument("--version", required=True)
    ap.add_argument("--base-url", default="", help="optional: prefix for the file name (default: same folder as the manifest)")
    ap.add_argument("--out", default="manifest.json")
    a = ap.parse_args()
    if not re.fullmatch(r"\d{1,3}\.\d{1,3}", a.version):
        sys.exit(f"version must look like 6.1, got {a.version!r}")
    d = open(a.bin, "rb").read()
    if d[:1] != b"\xe9":
        sys.exit("this does not look like an ESP32 application image (first byte must be 0xE9) - use press_counter.ino.bin, not the merged image")
    m = {"esp32": {"version": a.version, "url": a.base_url + os.path.basename(a.bin), "md5": hashlib.md5(d).hexdigest(), "size": len(d)}}
    with open(a.out, "w", encoding="utf-8") as f:
        json.dump(m, f, indent=2)
        f.write("\n")
    print(json.dumps(m, indent=2))
    print("wrote", a.out, "- upload it together with the image file")


if __name__ == "__main__":
    main()
