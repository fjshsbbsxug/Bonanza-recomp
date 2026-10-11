"""Thin 68000 decoding layer on top of capstone.

Decodes single instructions, rejects anything that is not plain 68000,
and reports control flow so the tracer can follow it.
"""
import re
from capstone import Cs, CS_ARCH_M68K, CS_MODE_BIG_ENDIAN, CS_MODE_M68K_000

_cs = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_000)

# Mnemonics (without size suffix) that exist on a plain 68000.
VALID = set("""
abcd add adda addi addq addx and andi asl asr bcc bcs beq bge bgt bhi ble bls blt bmi bne bpl bvc bvs
bchg bclr bra bset bsr btst chk clr cmp cmpa cmpi cmpm
dbcc dbcs dbeq dbf dbra dbge dbgt dbhi dble dbls dblt dbmi dbne dbpl dbt dbvc dbvs
divs divu eor eori exg ext illegal jmp jsr lea link lsl lsr move movea movem movep moveq
muls mulu nbcd neg negx nop not or ori pea reset rol ror roxl roxr rte rtr rts
sbcd scc scs seq sf sge sgt shi sle sls slt smi sne spl st svc svs stop sub suba subi subq subx swap tas
trap trapv tst unlk
""".split())

BCC = {"bcc", "bcs", "beq", "bge", "bgt", "bhi", "ble", "bls", "blt", "bmi", "bne", "bpl", "bvc", "bvs"}

ROM_SIZE = None  # set by caller


class Insn:
    __slots__ = ("addr", "size", "raw", "mnem", "base", "ops", "flow", "target")

    def __repr__(self):
        return f"{self.addr:06x}: {self.mnem} {', '.join(self.ops)}"


def split_ops(s):
    ops, depth, cur = [], 0, ""
    for ch in s:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            ops.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        ops.append(cur.strip())
    return ops


_hexnum = re.compile(r"^\$([0-9a-f]+)$")


def decode(rom, addr):
    """Return an Insn at addr or None if it is not a valid 68000 instruction."""
    if addr & 1 or addr + 2 > len(rom):
        return None
    try:
        it = next(_cs.disasm(rom[addr:addr + 10], addr, 1))
    except StopIteration:
        return None
    mnem = it.mnemonic
    base = mnem.split(".")[0]
    if base not in VALID:
        return None
    s = it.op_str
    if any(c in s for c in "[]*{}") or "ccr" in s and base not in ("andi", "ori", "eori", "move"):
        return None
    i = Insn()
    i.addr = addr
    i.size = it.size
    i.raw = bytes(it.bytes)
    i.mnem = mnem
    i.base = base
    i.ops = split_ops(s)
    i.flow = "seq"
    i.target = None
    if base in ("rts", "rte", "rtr", "illegal"):
        i.flow = "end"
    elif base == "bra" or base in BCC or base == "bsr":
        m = _hexnum.match(i.ops[-1])
        i.target = int(m.group(1), 16)
        i.flow = {"bra": "jump", "bsr": "call"}.get(base, "cond")
    elif base.startswith("db"):
        m = _hexnum.match(i.ops[-1])
        i.target = int(m.group(1), 16)
        i.flow = "cond"
    elif base in ("jmp", "jsr"):
        op = i.ops[0]
        m = re.match(r"^\$([0-9a-f]+)\.[wl]$", op) or re.match(r"^\$([0-9a-f]+)\(pc\)$", op)
        if m:
            t = int(m.group(1), 16)
            if op.endswith(".w") and t >= 0x8000:
                t |= 0xFFFF0000
            i.target = t
        i.flow = "jump" if base == "jmp" else "call"
        if i.target is None:
            i.flow = "ijump" if base == "jmp" else "icall"
    return i
