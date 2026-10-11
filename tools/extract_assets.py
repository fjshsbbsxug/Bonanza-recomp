#!/usr/bin/env python3
"""Extract binary blobs listed in assets.txt from baserom.md into assets/."""
import hashlib
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHA1 = "f7313ecaf7143873597e3a6ebf58d542d06c3ef3"


def main():
    rom_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "baserom.md")
    if not os.path.exists(rom_path):
        sys.exit("missing %s - copy your Bonanza Bros. (JUE) ROM there" % rom_path)
    rom = open(rom_path, "rb").read()
    if hashlib.sha1(rom).hexdigest() != SHA1:
        sys.exit("%s: sha1 mismatch, expected %s" % (rom_path, SHA1))
    out = os.path.join(ROOT, "assets")
    os.makedirs(out, exist_ok=True)
    n = 0
    for line in open(os.path.join(ROOT, "assets.txt")):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        s, e, name = line.split()
        data = rom[int(s, 16):int(e, 16)]
        path = os.path.join(out, name)
        if not os.path.exists(path) or open(path, "rb").read() != data:
            with open(path, "wb") as f:
                f.write(data)
        n += 1
    print("extracted %d assets" % n)


if __name__ == "__main__":
    main()
