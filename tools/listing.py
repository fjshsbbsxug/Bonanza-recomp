"""Print the game's instructions with their addresses: python3 tools/listing.py START END"""
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import disasm  # noqa: E402

rom = open(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "baserom.md"), "rb").read()
tr = disasm.Tracer(rom)
tr.run()
a, b = int(sys.argv[1], 16), int(sys.argv[2], 16)
for x in sorted(tr.ins):
    if a <= x < b:
        i = tr.ins[x]
        print("%05X  %-9s %s" % (x, i.mnem, ", ".join(i.ops)))
