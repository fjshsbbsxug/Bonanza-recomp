"""A 68000 decoder giving the same text as capstone's M68K disassembler (68000 mode).

tools/m68k.py used capstone; this is the same in plain Python (no compiled module),
so the tools also run in a browser (the builder pages run them with Pyodide). It
covers the plain 68000 instruction set and spells instructions and operands the way
capstone does, its quirks included: for the supported ROM every address decodes to
the same text as with capstone (tools/m68kdec_check.py compares them).

decode(code, addr) -> (mnemonic, op_str, size) or None
"""

import struct

CC = ["t", "f", "hi", "ls", "cc", "cs", "ne", "eq", "vc", "vs", "pl", "mi", "ge", "lt", "gt", "le"]
SZ = {0: "b", 1: "w", 2: "l"}


class Bad(Exception):
    pass


def hexs(v):
    """$-hex, as capstone prints a value"""
    return "$%x" % v


def shex(v):
    """signed $-hex (displacements)"""
    return "-$%x" % -v if v < 0 else "$%x" % v


class Reader:
    def __init__(self, code, addr):
        self.code, self.addr, self.pos = code, addr, 2

    def w(self):
        if self.pos + 2 > len(self.code):
            self.pos += 2                 # capstone reads 0xAAAAAAAA past the bytes it is given
            return 0xAAAAAAAA
        v = struct.unpack(">H", self.code[self.pos:self.pos + 2])[0]
        self.pos += 2
        return v

    def l(self):
        if self.pos + 4 > len(self.code):
            self.pos += 4
            return 0xAAAAAAAA
        hi = self.w()
        return (hi << 16) | self.w()


def s8(v):
    return v - 0x100 if v & 0x80 else v


def s16(v):
    return v - 0x10000 if v & 0x8000 else v


def ea(r, mode, reg, size):
    """an effective address operand as capstone writes it (size: 'b', 'w', 'l' for #imm)"""
    if mode == 0:
        return "d%d" % reg
    if mode == 1:
        return "a%d" % reg
    if mode == 2:
        return "(a%d)" % reg
    if mode == 3:
        return "(a%d)+" % reg
    if mode == 4:
        return "-(a%d)" % reg
    if mode == 5:
        d = s16(r.w())
        return "%s(a%d)" % (shex(d), reg)
    if mode == 6:
        return index(r, "a%d" % reg, None)
    if reg == 0:
        v = r.w()
        return "%s.w" % hexs(v)
    if reg == 1:
        return "%s.l" % hexs(r.l())
    if reg == 2:
        at = r.addr + 2                   # capstone: from the opcode word, whatever comes first
        d = s16(r.w())
        return "%s(pc)" % hexs((at + d) & 0xFFFFFFFF)
    if reg == 3:
        return index(r, "pc", r.addr + 2)
    if reg == 4:
        if size == "b":
            return "#%s" % hexs(r.w() & 0xFF)
        if size == "w":
            return "#%s" % hexs(r.w())
        if size == "l":
            return "#%s" % hexs(r.l())
        raise Bad()
    raise Bad()


def index(r, base, pc_at):
    """d8(An, Xn) / d8(PC, Xn), and the 68020's full format, which capstone also takes
    in 68000 mode (what it writes with [ ] or * the tools refuse: Bad)"""
    ext = r.w()
    xn = ("a%d" if ext & 0x8000 else "d%d") % ((ext >> 12) & 7)
    xs = ".l" if ext & 0x0800 else ".w"
    scale = (ext >> 9) & 3
    if not ext & 0x100:                   # brief format
        d = s8(ext & 0xFF)
        if pc_at is not None:
            if d == 0:                    # capstone: no space here, and the scale
                if scale:
                    raise Bad()
                return "%s(pc,%s%s)" % (hexs(pc_at & 0xFFFFFFFF), xn, xs)
            return "%s(pc, %s%s)" % (hexs((pc_at + d) & 0xFFFFFFFF), xn, xs)
        if d == 0:
            if scale:
                raise Bad()               # "* n"
            return "(%s, %s%s)" % (base, xn, xs)
        return "%s(%s, %s%s)" % (shex(d), base, xn, xs)
    if (ext & 7) not in (0, 4) or (scale and not ext & 0x40):   # memory indirect ([ ]), a scale (*)
        raise Bad()
    bds = (ext >> 4) & 3
    bd = r.w() if bds == 2 else r.l() if bds == 3 else 0
    parts = []
    if not ext & 0x80:
        parts.append("a16" if pc_at is not None else base)
    parts.append("invalid.w" if ext & 0x40 else xn + xs)
    return (hexs(bd) if bd else "") + "(%s)" % ", ".join(parts)


def ea_ok(mode, reg, allow):
    """is this addressing mode allowed? allow: a set of 'dn an ind post pre d16 idx absw absl pcd pcx imm'"""
    k = ["dn", "an", "ind", "post", "pre", "d16", "idx"][mode] if mode < 7 else \
        {0: "absw", 1: "absl", 2: "pcd", 3: "pcx", 4: "imm"}.get(reg)
    return k in allow


ALL = {"dn", "an", "ind", "post", "pre", "d16", "idx", "absw", "absl", "pcd", "pcx", "imm"}
DATA = ALL - {"an"}
MEM = ALL - {"dn", "an"}
ALTER = {"dn", "an", "ind", "post", "pre", "d16", "idx", "absw", "absl"}
DATA_ALT = ALTER - {"an"}
MEM_ALT = ALTER - {"dn", "an"}
CTRL = {"ind", "d16", "idx", "absw", "absl", "pcd", "pcx"}
CTRL_ALT = CTRL - {"pcd", "pcx"}


def reglist(mask, predec):
    if not mask:
        return "#$0"                      # capstone, for no registers
    if predec:
        mask = int("{:016b}".format(mask)[::-1], 2)
    names = ["d%d" % i for i in range(8)] + ["a%d" % i for i in range(8)]
    out, i = [], 0
    while i < 16:
        if mask & (1 << i):
            j = i
            while j + 1 < 16 and mask & (1 << (j + 1)) and (j + 1) // 8 == i // 8:
                j += 1
            out.append(names[i] if j == i else "%s-%s" % (names[i], names[j]))
            i = j + 1
        else:
            i += 1
    return "/".join(out)


def decode(code, addr):
    if len(code) < 2:
        return None
    op = struct.unpack(">H", code[:2])[0]
    r = Reader(code, addr)
    try:
        res = _decode(op, r)
    except Bad:
        return None
    if res is None:
        return None
    mnem, ops = res
    return mnem, ", ".join(ops), min(r.pos, len(code))


def _decode(op, r):
    top = op >> 12
    mode, reg = (op >> 3) & 7, op & 7
    if top == 0:
        return group0(op, r, mode, reg)
    if top in (1, 2, 3):
        size = {1: "b", 3: "w", 2: "l"}[top]
        dmode, dreg = (op >> 6) & 7, (op >> 9) & 7
        if not ea_ok(mode, reg, ALL) or (size == "b" and mode == 1):
            return None
        if dmode == 1:
            if size == "b":
                return None
            src = ea(r, mode, reg, size)
            return "movea." + size, [src, "a%d" % dreg]
        if not ea_ok(dmode, dreg, DATA_ALT):
            return None
        src = ea(r, mode, reg, size)
        dst = ea(r, dmode, dreg, size)
        return "move." + size, [src, dst]
    if top == 4:
        return group4(op, r, mode, reg)
    if top == 5:
        sz = (op >> 6) & 3
        if sz == 3:
            cc = (op >> 8) & 15
            if mode == 1:
                d = s16(r.w())
                name = "dbra" if cc == 1 else "db" + CC[cc]
                return name, ["d%d" % reg, hexs((r.addr + 2 + d) & 0xFFFFFFFF)]
            if not ea_ok(mode, reg, DATA_ALT):
                return None
            return "s%s.b" % CC[cc], [ea(r, mode, reg, "b")]
        if not ea_ok(mode, reg, ALTER) or (sz == 0 and mode == 1):
            return None
        q = (op >> 9) & 7 or 8
        name = ("subq." if op & 0x100 else "addq.") + SZ[sz]
        return name, ["#" + hexs(q), ea(r, mode, reg, SZ[sz])]
    if top == 6:
        cc = (op >> 8) & 15
        d = op & 0xFF
        if d == 0:
            disp = s16(r.w())
            sfx = ".w"
        elif d == 0xFF:
            return None                   # 68020 (long)
        else:
            disp = s8(d)
            sfx = ".b"
        name = {0: "bra", 1: "bsr"}.get(cc, "b" + CC[cc])
        return name + sfx, [hexs((r.addr + 2 + disp) & 0xFFFFFFFF)]
    if top == 7:
        if op & 0x100:
            return None
        return "moveq", ["#" + hexs(op & 0xFF), "d%d" % ((op >> 9) & 7)]
    if top in (8, 0xC):
        return group8c(op, r, mode, reg, top)
    if top in (9, 0xD):
        return group9d(op, r, mode, reg, top)
    if top == 0xB:
        return groupb(op, r, mode, reg)
    if top == 0xE:
        return groupe(op, r, mode, reg)
    return None


def group0(op, r, mode, reg):
    if op & 0x100:                        # dynamic bit ops / movep
        dn = "d%d" % ((op >> 9) & 7)
        kind = (op >> 6) & 3
        if mode == 1:
            sz = "l" if op & 0x40 else "w"
            if op & 0x80:
                d = s16(r.w())
                return "movep." + sz, [dn, "%s(a%d)" % (shex(d), reg)]
            d = s16(r.w())
            return "movep." + sz, ["%s(a%d)" % (shex(d), reg), dn]
        name = ["btst", "bchg", "bclr", "bset"][kind]
        allow = DATA if kind == 0 else DATA_ALT
        if not ea_ok(mode, reg, allow):
            return None
        if kind == 0:                     # capstone: btst.l (and a long immediate)
            return "btst.l", [dn, ea(r, mode, reg, "l")]
        return name + ".b", [dn, ea(r, mode, reg, "b")]
    sub = (op >> 9) & 7
    sz = (op >> 6) & 3
    if sub == 4:                          # static bit ops
        kind = sz
        name = ["btst", "bchg", "bclr", "bset"][kind]
        allow = DATA - {"imm"} if kind == 0 else DATA_ALT
        if not ea_ok(mode, reg, allow):
            return None
        b = r.w()
        if (b >> 8) not in ((0, 1) if kind == 3 else (0,)):   # capstone takes bset with 1 there
            return None
        return name + ".b", ["#" + hexs(b & 0xFF), ea(r, mode, reg, "b")]
    if sub in (7,):
        return None
    name = ["ori", "andi", "subi", "addi", None, "eori", "cmpi"][sub]
    if name is None or sz == 3:
        return None
    if mode == 7 and reg == 4:            # to ccr / sr
        if name not in ("ori", "andi", "eori"):
            return None
        if sz == 0:
            v = r.w()
            if v & 0xFF00:
                return None
            return name + ".b", ["#" + hexs(v), "ccr"]
        if sz == 1:
            v = r.w()
            return name + ".w", ["#" + hexs(v), "sr"]
        return None
    allow = DATA_ALT if name != "cmpi" else DATA_ALT
    if not ea_ok(mode, reg, allow):
        return None
    s = SZ[sz]
    imm = ea(r, 7, 4, s)
    return name + "." + s, [imm, ea(r, mode, reg, s)]


def group4(op, r, mode, reg):
    if op == 0x4AFC:
        return "illegal", ["#$4afc"]
    if op == 0x4E70:
        return "reset", []
    if op == 0x4E71:
        return "nop", []
    if op == 0x4E72:
        return "stop", ["#" + hexs(r.w())]
    if op == 0x4E73:
        return "rte", []
    if op == 0x4E75:
        return "rts", []
    if op == 0x4E76:
        return "trapv", []
    if op == 0x4E77:
        return "rtr", []
    if (op & 0xFFF0) == 0x4E40:
        return "trap", ["#" + hexs(op & 15)]
    if (op & 0xFFF8) == 0x4E50:
        return "link.w", ["a%d" % reg, "#" + hexs(r.w())]
    if (op & 0xFFF8) == 0x4E58:
        return "unlk", ["a%d" % reg]
    if (op & 0xFFF8) == 0x4E60:
        return "move", ["a%d" % reg, "usp"]
    if (op & 0xFFF8) == 0x4E68:
        return "move", ["usp", "a%d" % reg]
    if (op & 0xFFC0) == 0x4E80:
        if not ea_ok(mode, reg, CTRL):
            return None
        return "jsr", [ea(r, mode, reg, None)]
    if (op & 0xFFC0) == 0x4EC0:
        if not ea_ok(mode, reg, CTRL):
            return None
        return "jmp", [ea(r, mode, reg, None)]
    if (op & 0xF1C0) == 0x41C0:
        if not ea_ok(mode, reg, CTRL):
            return None
        return "lea.l", [ea(r, mode, reg, None), "a%d" % ((op >> 9) & 7)]
    if (op & 0xF1C0) == 0x4180:
        if not ea_ok(mode, reg, DATA):
            return None
        return "chk.w", [ea(r, mode, reg, "w"), "d%d" % ((op >> 9) & 7)]
    if (op & 0xF140) == 0x4100:
        return None                       # 68020 chk.l and others
    if (op & 0xFFC0) == 0x40C0:
        if not ea_ok(mode, reg, DATA_ALT):
            return None
        return "move.w", ["sr", ea(r, mode, reg, "w")]
    if (op & 0xFFC0) == 0x44C0:
        if not ea_ok(mode, reg, DATA):
            return None
        return "move.w", [ea(r, mode, reg, "w"), "ccr"]
    if (op & 0xFFC0) == 0x46C0:
        if not ea_ok(mode, reg, DATA):
            return None
        return "move.w", [ea(r, mode, reg, "w"), "sr"]
    if (op & 0xFFF8) == 0x4840:
        return "swap", ["d%d" % reg]
    if (op & 0xFFC0) == 0x4840:
        if not ea_ok(mode, reg, CTRL):
            return None
        return "pea.l", [ea(r, mode, reg, None)]
    if (op & 0xFFF8) == 0x4880:
        return "ext.w", ["d%d" % reg]
    if (op & 0xFFF8) == 0x48C0:
        return "ext.l", ["d%d" % reg]
    if (op & 0xFB80) == 0x4880:           # movem
        sz = "l" if op & 0x40 else "w"
        mask = r.w()
        if op & 0x400:
            if not ea_ok(mode, reg, CTRL | {"post"}):
                return None
            return "movem." + sz, [ea(r, mode, reg, sz), reglist(mask, False)]
        if not ea_ok(mode, reg, CTRL_ALT | {"pre"}):
            return None
        return "movem." + sz, [reglist(mask, mode == 4), ea(r, mode, reg, sz)]
    if (op & 0xFFC0) == 0x4800:
        if not ea_ok(mode, reg, DATA_ALT):
            return None
        return "nbcd.b", [ea(r, mode, reg, "b")]
    if (op & 0xFFC0) == 0x4AC0:
        if not ea_ok(mode, reg, DATA_ALT):
            return None
        return "tas.b", [ea(r, mode, reg, "b")]
    sz = (op >> 6) & 3
    if sz == 3:
        return None
    s = SZ[sz]
    kind = (op >> 8) & 0xF
    names = {0x0: "negx", 0x2: "clr", 0x4: "neg", 0x6: "not", 0xA: "tst"}
    if kind not in names:
        return None
    if not ea_ok(mode, reg, DATA_ALT):
        return None
    return names[kind] + "." + s, [ea(r, mode, reg, s)]


def group8c(op, r, mode, reg, top):
    dn = (op >> 9) & 7
    opm = (op >> 6) & 7
    if opm in (3, 7):                     # divu/divs, mulu/muls
        if not ea_ok(mode, reg, DATA):
            return None
        name = ("divu" if opm == 3 else "divs") if top == 8 else ("mulu" if opm == 3 else "muls")
        return name + ".w", [ea(r, mode, reg, "w"), "d%d" % dn]
    if opm == 4 and mode in (0, 1):       # sbcd / abcd
        name = "sbcd" if top == 8 else "abcd"
        if mode == 0:
            return name + ".b", ["d%d" % reg, "d%d" % dn]
        if top == 8:                      # capstone takes 68020's pack form here
            v = r.w()
            return name, ["-(a%d)" % reg, "-(a%d)" % dn] + (["#" + hexs(v)] if v else [])
        return name + ".b", ["-(a%d)" % reg, "-(a%d)" % dn]
    if top == 0xC and opm in (5, 6) and mode in (0, 1):   # exg
        if opm == 5 and mode == 0:
            return "exg.l", ["d%d" % dn, "d%d" % reg]
        if opm == 5 and mode == 1:
            return "exg.l", ["a%d" % dn, "a%d" % reg]
        if opm == 6 and mode == 1:
            return "exg.l", ["d%d" % dn, "a%d" % reg]
        return None
    if top == 8 and opm in (5, 6) and mode in (0, 1):
        return None                       # 68020 pack/unpk
    name = "or" if top == 8 else "and"
    s = SZ[opm & 3]
    if opm < 3:
        if not ea_ok(mode, reg, DATA):
            return None
        return name + "." + s, [ea(r, mode, reg, s), "d%d" % dn]
    if not ea_ok(mode, reg, MEM_ALT):
        return None
    return name + "." + s, ["d%d" % dn, ea(r, mode, reg, s)]


def group9d(op, r, mode, reg, top):
    dn = (op >> 9) & 7
    opm = (op >> 6) & 7
    base = "sub" if top == 9 else "add"
    if opm in (3, 7):
        s = "w" if opm == 3 else "l"
        return base + "a." + s, [ea(r, mode, reg, s), "a%d" % dn]
    s = SZ[opm & 3]
    if opm < 3:
        if s == "b" and mode == 1:
            return None
        return base + "." + s, [ea(r, mode, reg, s), "d%d" % dn]
    if mode in (0, 1):                    # addx / subx
        if mode == 0:
            return base + "x." + s, ["d%d" % reg, "d%d" % dn]
        return base + "x." + s, ["-(a%d)" % reg, "-(a%d)" % dn]
    if not ea_ok(mode, reg, MEM_ALT):
        return None
    return base + "." + s, ["d%d" % dn, ea(r, mode, reg, s)]


def groupb(op, r, mode, reg):
    dn = (op >> 9) & 7
    opm = (op >> 6) & 7
    if opm in (3, 7):
        s = "w" if opm == 3 else "l"
        return "cmpa." + s, [ea(r, mode, reg, s), "a%d" % dn]
    s = SZ[opm & 3]
    if opm < 3:
        if s == "b" and mode == 1:
            return None
        return "cmp." + s, [ea(r, mode, reg, s), "d%d" % dn]
    if mode == 1:
        return "cmpm." + s, ["(a%d)+" % reg, "(a%d)+" % dn]
    if not ea_ok(mode, reg, DATA_ALT):
        return None
    return "eor." + s, ["d%d" % dn, ea(r, mode, reg, s)]


def groupe(op, r, mode, reg):
    sz = (op >> 6) & 3
    names = ["as", "ls", "rox", "ro"]
    if sz == 3:                           # memory, by one
        if op & 0x0800:
            return None
        kind = (op >> 9) & 3
        if not ea_ok(mode, reg, MEM_ALT):
            return None
        d = "l" if op & 0x100 else "r"
        return names[kind] + d + ".w", [ea(r, mode, reg, "w")]
    kind = (op >> 3) & 3
    d = "l" if op & 0x100 else "r"
    s = SZ[sz]
    cnt = (op >> 9) & 7
    if op & 0x20 and kind == 2 and d == "r" and s == "b":
        return "roxr.l", ["#" + hexs(cnt or 8), "d%d" % reg]   # capstone
    if op & 0x20:
        src = "d%d" % cnt
    else:
        src = "#" + hexs(cnt or 8)
    return names[kind] + d + "." + s, [src, "d%d" % reg]
