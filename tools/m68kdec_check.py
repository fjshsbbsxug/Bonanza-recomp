"""Checks tools/m68kdec.py against capstone (pip install capstone): every even address of
the ROM must decode to the same instruction (as tools/m68k.py takes it) with both."""
import sys
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from capstone import Cs, CS_ARCH_M68K, CS_MODE_BIG_ENDIAN, CS_MODE_M68K_000
import m68kdec
import m68k

cs = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_000)


def taken(res):
    """what tools/m68k.py makes of a decode: None if it refuses it"""
    if res is None:
        return None
    mnem, s, size = res
    base = mnem.split(".")[0]
    if base not in m68k.VALID:
        return None
    if any(c in s for c in "[]*{}") or "ccr" in s and base not in ("andi", "ori", "eori", "move"):
        return None
    return res


def capstone(code, addr):
    try:
        it = next(cs.disasm(code, addr, 1))
    except StopIteration:
        return None
    return it.mnemonic, it.op_str, it.size


def main():
    rom = open(sys.argv[1] if len(sys.argv) > 1 else "baserom.md", "rb").read()
    bad = 0
    for a in range(0, len(rom), 2):
        code = rom[a:a + 10]
        x, y = taken(capstone(code, a)), taken(m68kdec.decode(code, a))
        if x != y:
            bad += 1
            if bad <= 40:
                print("%06x %s  capstone %s  ours %s" % (a, code[:6].hex(), x, y))
    print("addresses that differ: %d of %d" % (bad, len(rom) // 2))
    return bad


if __name__ == "__main__":
    sys.exit(1 if main() else 0)
