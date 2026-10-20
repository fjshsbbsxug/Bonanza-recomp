"""What the game's code needs from each routine when it returns: the registers and
condition flags its callers read before writing them again (live after the return).

A routine rewritten in C (port/src/logic.c) has to leave only those as the 68000
routine left them; the rest is free. This is a standard backward liveness analysis
over the whole traced program (tools/disasm.py):

  - every instruction's registers / flags read and written (from its decoded text);
  - a call: the callee's own needs at its entry, then the caller goes on (nothing is
    assumed written by the callee);
  - a return: what the callers of every routine that reaches this rts need;
  - an indirect call or jump, an exception return, a trap: everything is needed
    (unknown targets);
  - routines whose address is in the ROM as data (tables, handlers kept in memory) may
    be called from any indirect call: they get the needs at all of those.

Usage: python3 tools/liveness.py [ADDR ...]   (hex; none: every routine)
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import disasm  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# data registers in three parts: d0..d7 their low bytes, m0..m7 the bytes above, h0..h7
# their high words (a byte or word written keeps the rest: the rest is not read)
REGS = (["d%d" % i for i in range(8)] + ["a%d" % i for i in range(8)] + ["h%d" % i for i in range(8)]
        + ["m%d" % i for i in range(8)])
FLAGS = ["X", "N", "Z", "V", "C"]
BIT = {n: 1 << i for i, n in enumerate(REGS + FLAGS)}
ALL_REGS = sum(BIT[r] for r in REGS)
NZVC = BIT["N"] | BIT["Z"] | BIT["V"] | BIT["C"]
XNZVC = NZVC | BIT["X"]
ALL = ALL_REGS | XNZVC
SP = BIT["a7"]

_reg = re.compile(r"(?<![$0-9a-fx])([da][0-7]|sp)(\.[wl])?\b")


def dreg(n, size):
    """a data register's bits for an operation of this size"""
    m = BIT["d%d" % n]
    if size == "b":
        return m
    m |= BIT["m%d" % n]
    return m | BIT["h%d" % n] if size == "l" else m


def full(r):
    """a register named in text: all of it"""
    r = "a7" if r == "sp" else r
    return dreg(int(r[1]), "l") if r[0] == "d" else BIT[r]


def regs_in(s):
    """registers forming an address (base registers, index registers by their size)"""
    m = 0
    for r, sz in _reg.findall(s):
        r = "a7" if r == "sp" else r
        if r[0] == "d":
            m |= dreg(int(r[1]), "l" if sz == ".l" else "w")
        else:
            m |= BIT[r]
    return m


def reglist(s):
    """a movem list: d0-d3/a5-a6 (whole registers)"""
    names = ["d%d" % i for i in range(8)] + ["a%d" % i for i in range(8)]
    m = 0
    for part in s.split("/"):
        part = part.replace("sp", "a7")
        if "-" in part:
            a, b = part.split("-")
            for k in range(names.index(a), names.index(b) + 1):
                m |= full(names[k])
        else:
            m |= full(part)
    return m


def is_reg(op):
    return re.fullmatch(r"[da][0-7]|sp", op) is not None


def opbits(op, size):
    """the bits of a register operand (by the operation's size), or of the registers
    forming a memory operand's address"""
    if re.fullmatch(r"d[0-7]", op):
        return dreg(int(op[1]), size)
    if re.fullmatch(r"a[0-7]|sp", op):
        return full(op)
    return regs_in(op)


def ea_side(op):
    """registers an operand's (an)+ / -(an) changes"""
    m = re.fullmatch(r"-?\((a[0-7]|sp)\)\+?", op)
    if m and ("+" in op or op.startswith("-")):
        return full(m.group(1))
    return 0


def size_of(mnem):
    return mnem.split(".")[1] if "." in mnem else ""


# flags each kind of instruction writes (the rest kept)
SETS_NZVC = {"move", "moveq", "tst", "and", "andi", "or", "ori", "eor", "eori", "not", "clr",
             "mulu", "muls", "divu", "divs", "ext", "swap", "cmp", "cmpi", "cmpa", "cmpm", "tas"}
SETS_ALL = {"add", "addi", "addq", "addx", "sub", "subi", "subq", "subx", "neg", "negx",
            "asl", "asr", "lsl", "lsr", "rol", "ror", "roxl", "roxr", "abcd", "sbcd", "nbcd"}
SHIFTS = {"asl", "asr", "lsl", "lsr", "rol", "ror", "roxl", "roxr"}


def dest(op, size, reads):
    """(read, written) for a destination operand"""
    if re.fullmatch(r"d[0-7]", op):
        r = dreg(int(op[1]), size or "l")
        return (r if reads else 0), r
    if re.fullmatch(r"a[0-7]|sp", op):
        return (full(op) if reads else 0), full(op)
    return regs_in(op) | ea_side(op), ea_side(op)


def effects(i):
    """(read, written) masks of an instruction, conservatively"""
    b, ops, sz = i.base, i.ops, size_of(i.mnem)
    if b == "rte":
        # the end of an interrupt routine: the frame routine (the vertical blank) runs
        # the whole game and interrupts only the idle loop, which holds nothing; the
        # band routines (line interrupts) keep the registers; SR comes back
        return SP, XNZVC | SP
    if b == "stop":
        return 0, 0                                  # (the error handlers: the end)
    if b in ("rtr", "trap", "trapv", "illegal", "reset", "chk"):
        return ALL, 0
    if b == "rts":
        return SP, SP
    if b in ("nop", "bra"):
        return 0, 0
    if b in disasm.m68k.BCC:
        return NZVC, 0
    if b.startswith("db"):
        r = opbits(ops[0], "w")
        return r | (NZVC if b not in ("dbf", "dbra") else 0), r
    if b.startswith("s") and b not in ("sub", "subi", "subq", "subx", "suba", "swap", "sbcd", "stop"):
        u, d = dest(ops[0], "b", True)              # scc
        return NZVC | u, d
    if b in ("jsr", "jmp", "pea"):
        return regs_in(ops[0]) | (SP if b != "jmp" else 0), (SP if b != "jmp" else 0)
    if b == "lea":
        return regs_in(ops[0]), full(ops[1])
    if b == "link":
        return full(ops[0]) | SP, full(ops[0]) | SP
    if b == "unlk":
        return full(ops[0]), full(ops[0]) | SP
    if b == "movem":
        if is_reg(ops[0].split("/")[0].split("-")[0]):   # registers to memory
            lst, mem = reglist(ops[0]), ops[1]
            if mem in ("-(a7)", "-(sp)"):
                return SP, SP          # (saved to be taken back: a caller's value passes through)
            if sz == "w":
                lst &= ~sum(BIT["h%d" % k] for k in range(8))
            return lst | regs_in(mem), ea_side(mem)
        lst, mem = reglist(ops[1]), ops[0]
        return regs_in(mem), lst | ea_side(mem)
    if b == "exg":
        r = full(ops[0]) | full(ops[1])
        return r, r
    if b == "swap":
        r = full(ops[0])
        return r, r | NZVC
    if b == "ext":
        n = int(ops[0][1])
        return dreg(n, "b" if sz == "w" else "w"), dreg(n, sz) | NZVC
    if b == "moveq":
        return 0, full(ops[1]) | NZVC
    if b == "movea":
        return opbits(ops[0], sz), full(ops[1])
    flags = XNZVC if b in SETS_ALL else NZVC if b in SETS_NZVC else 0
    if b in ("btst", "bchg", "bclr", "bset"):
        flags = BIT["Z"]
    use = 0
    if b in ("addx", "subx", "negx", "roxl", "roxr", "abcd", "sbcd", "nbcd"):
        use |= BIT["X"] | (BIT["Z"] if b in ("addx", "subx", "negx", "abcd", "sbcd", "nbcd") else 0)
    if b in ("mulu", "muls", "divu", "divs"):
        use |= opbits(ops[0], "w") | ea_side(ops[0])
        n = int(ops[1][1])
        rd = dreg(n, "w") if b in ("mulu", "muls") else dreg(n, "l")
        return use | rd, ea_side(ops[0]) | dreg(n, "l") | NZVC
    if b == "move" and ops[0] == "sr":
        u, d = dest(ops[1], "w", False)
        return u, d                                  # (saved to be put back: not a use)
    if b in ("move", "andi", "ori", "eori") and len(ops) == 2 and ops[1] in ("ccr", "sr"):
        if b != "move":
            use |= XNZVC
        return use | opbits(ops[0], "w"), XNZVC
    if len(ops) == 1:                               # clr, tst, not, neg, nbcd, tas, shifts of memory
        if b == "clr":
            u, d = dest(ops[0], sz, False)
        elif b == "tst":
            return use | opbits(ops[0], sz) | ea_side(ops[0]), ea_side(ops[0]) | flags
        else:
            u, d = dest(ops[0], sz, True)
        return use | u, d | flags
    src, dst = ops[0], ops[-1]
    if b in SHIFTS:
        cnt = opbits(src, "w") if re.fullmatch(r"d[0-7]", src) else 0
        u, d = dest(dst, sz, True)
        return use | cnt | u, d | flags
    use |= opbits(src, "w" if b in ("mulu", "muls", "divu", "divs") else (sz or "l")) | ea_side(src)
    dfn = ea_side(src)
    if b in ("btst", "bchg", "bclr", "bset"):
        if re.fullmatch(r"d[0-7]", src):
            use = (use & ~dreg(int(src[1]), "l")) | dreg(int(src[1]), "w")
        if re.fullmatch(r"d[0-7]", dst):
            r = dreg(int(dst[1]), "l")
            return use | r, dfn | flags | (r if b != "btst" else 0)
        return use | regs_in(dst) | ea_side(dst), dfn | ea_side(dst) | flags
    if b in ("cmp", "cmpi", "cmpa", "cmpm"):
        return use | opbits(dst, "l" if b == "cmpa" else sz) | ea_side(dst), dfn | ea_side(dst) | flags
    if b in ("mulu", "muls"):
        n = int(dst[1])
        return use | BIT["d%d" % n], dfn | dreg(n, "l") | flags
    if b in ("divu", "divs"):
        n = int(dst[1])
        return use | dreg(n, "l"), dfn | dreg(n, "l") | flags
    if b in ("adda", "suba") or (re.fullmatch(r"a[0-7]|sp", dst) and b in ("addq", "subq")):
        r = full(dst)
        return use | r, dfn | r
    if b == "move":
        u, d = dest(dst, sz, False)
        return use | u, dfn | d | flags
    u, d = dest(dst, sz, True)
    return use | u, dfn | d | flags


def min_and(ms):
    v = None
    for m in ms:
        v = m if v is None else v & m
    return v or 0


class Program:
    def __init__(self, rom):
        self.rom = rom
        tr = disasm.Tracer(rom)
        tr.run()
        self.ins = tr.ins
        self.tr_tables = dict(tr.tables)
        self.entries = set()
        self.direct = set()
        for a, i in self.ins.items():
            if i.flow == "call" and i.target is not None and i.target in self.ins:
                self.entries.add(i.target)
                self.direct.add(i.target)
        # routine addresses kept in the ROM as data (handler tables, pointers stored)
        self.indirect = set()
        code = set(self.ins)
        for a in range(0, len(rom) - 3, 2):
            v = int.from_bytes(rom[a:a + 4], "big")
            if v in code and (a not in tr.owner):
                self.indirect.add(v)
        # ... and in the code: handler addresses stored or loaded (move.l #sub,..; lea sub)
        num = re.compile(r"\$([0-9a-f]+)")
        for a, i in self.ins.items():
            if i.flow in ("call", "jump", "cond"):
                continue
            for op in i.ops:
                for m in num.findall(op):
                    v = int(m, 16)
                    if v in code and v != a:
                        self.indirect.add(v)
        for t, (kind, ents) in tr.tables.items():
            for v in ents:
                if v in code:
                    self.indirect.add(v)
        for t in getattr(tr, "calls", ()):
            if t in code:
                self.entries.add(t)
        self.entries |= self.indirect
        self.eff = {a: effects(i) for a, i in self.ins.items()}
        self.resolve_sites()
        self.callers = {}
        for a, i in self.ins.items():
            if i.flow == "call" and i.target is not None:
                self.callers.setdefault(i.target, []).append(a)
            elif i.flow == "icall":
                for t in self.site.get(a, ()):
                    self.callers.setdefault(t, []).append(a)

    def resolve_sites(self):
        """the targets of each indirect call / jump, where the code shows them: a handler
        address kept in RAM (stored there as an immediate somewhere) or a table of them"""
        ins, rom = self.ins, self.rom
        addrs = sorted(ins)
        idx = {a: k for k, a in enumerate(addrs)}
        num = re.compile(r"^#?\$([0-9a-f]+)(\.l)?$")
        starts = set(self.tr_tables)

        def table(t):
            out, a = set(), t
            while a + 4 <= len(rom) and (a == t or a not in starts):
                v = int.from_bytes(rom[a:a + 4], "big")
                if v == 0 and not out and a - t < 16:
                    a += 4
                    continue
                if v not in ins:
                    break
                out.add(v)
                a += 4
            return out

        def base_of(k, reg):
            """the table a register was pointed at (lea / movea of an address) just before"""
            for jj in range(k - 1, max(0, k - 8), -1):
                y = ins[addrs[jj]]
                if y.flow in ("end", "jump", "ijump", "call", "icall"):
                    return None
                if len(y.ops) == 2 and y.ops[1] == reg:
                    mm = re.fullmatch(r"#?\$([0-9a-f]+)(\.l|\(pc\))?", y.ops[0])
                    if mm and y.base in ("lea", "movea"):
                        return int(mm.group(1), 16)
                    return None
            return None

        # pointer variables in RAM: what is stored in them (handlers by immediate, from a
        # table, or copied from another such variable); anything else: unknown
        ptrvals, loose, alias = {}, set(), []
        for a, i in ins.items():
            if i.base == "move" and i.mnem.endswith(".l") and len(i.ops) == 2:
                d = re.match(r"^\$([0-9a-f]+)\.l$", i.ops[1])
                if not d:
                    continue
                dst, src = int(d.group(1), 16), i.ops[0]
                m = num.match(src)
                if m and src.startswith("#"):
                    v = int(m.group(1), 16)
                    if v in ins:
                        ptrvals.setdefault(dst, set()).add(v)
                    continue
                m = re.fullmatch(r"\$([0-9a-f]+)\.l", src)
                if m:
                    alias.append((dst, int(m.group(1), 16)))
                    continue
                m = re.fullmatch(r"\((a[0-7]), d[0-7]\.[wl]\)", src)
                if m:
                    t = base_of(idx[a], m.group(1))
                    if t is not None:
                        ptrvals.setdefault(dst, set()).update(table(t))
                        continue
                m = re.fullmatch(r"\$([0-9a-f]+)\(pc, d[0-7]\.[wl]\)", src)
                if m:
                    ptrvals.setdefault(dst, set()).update(table(int(m.group(1), 16)))
                    continue
                if re.fullmatch(r"a[0-7]", src):        # a register: how it was loaded
                    k, got = idx[a], False
                    for jj in range(k - 1, max(0, k - 6), -1):
                        y = ins[addrs[jj]]
                        if y.flow in ("end", "jump", "ijump", "call", "icall") or len(y.ops) != 2 or y.ops[1] != src:
                            if y.flow in ("end", "jump", "ijump", "call", "icall"):
                                break
                            continue
                        mm = re.fullmatch(r"\((a[0-7])(, d[0-7]\.[wl])?\)", y.ops[0])
                        if y.base == "movea" and mm:
                            t = base_of(jj, mm.group(1))
                            if t is not None:
                                ptrvals.setdefault(dst, set()).update(table(t))
                                got = True
                        mm = re.fullmatch(r"\$([0-9a-f]+)\.l", y.ops[0])
                        if y.base == "movea" and mm:
                            alias.append((dst, int(mm.group(1), 16)))
                            got = True
                        mm = re.fullmatch(r"\$([0-9a-f]+)(\.l|\(pc\))", y.ops[0])
                        if y.base == "lea" and mm and int(mm.group(1), 16) in ins:
                            ptrvals.setdefault(dst, set()).add(int(mm.group(1), 16))
                            got = True
                        break
                    if got:
                        continue
                loose.add(dst)
        grew = True
        while grew:
            grew = False
            for dst, src in alias:
                if src in loose and dst not in loose:
                    loose.add(dst)
                    grew = True
                add = ptrvals.get(src, set()) - ptrvals.get(dst, set())
                if add:
                    ptrvals.setdefault(dst, set()).update(add)
                    grew = True
        for v in loose:
            ptrvals.pop(v, None)

        self.site = {}
        for a in addrs:
            i = ins[a]
            if i.flow not in ("icall", "ijump"):
                continue
            r = re.fullmatch(r"\(([ad][0-7])\)", i.ops[0])
            if not r:
                continue
            reg = r.group(1)
            k = idx[a]
            targets = None
            j = k - 1
            while j >= max(0, k - 8):
                x = ins[addrs[j]]
                if x.flow in ("end", "jump", "ijump", "call", "icall"):
                    break
                if len(x.ops) == 2 and x.ops[1] == reg and x.base in ("movea", "move", "lea"):
                    src = x.ops[0]
                    m = re.fullmatch(r"\$([0-9a-f]+)\.l", src)
                    if m and x.base != "lea":
                        targets = ptrvals.get(int(m.group(1), 16))
                        break
                    m = re.fullmatch(r"(d[0-7])", src)
                    if m:
                        reg = m.group(1)
                        j -= 1
                        continue
                    m = re.fullmatch(r"\$([0-9a-f]+)\(pc, d[0-7]\.[wl]\)", src)
                    if m:
                        targets = table(int(m.group(1), 16))
                        break
                    m = re.fullmatch(r"\((a[0-7]), d[0-7]\.[wl]\)", src)
                    if m:
                        base = m.group(1)
                        for jj in range(j - 1, max(0, j - 6), -1):
                            y = ins[addrs[jj]]
                            if len(y.ops) == 2 and y.ops[1] == base:
                                mm = re.fullmatch(r"#?\$([0-9a-f]+)(\.l|\(pc\))?", y.ops[0])
                                if mm and y.base in ("lea", "movea"):
                                    targets = table(int(mm.group(1), 16))
                                break
                        break
                    break
                j -= 1
            if targets:
                self.site[a] = targets
        self.attributed = set()
        for t in self.site.values():
            self.attributed |= t

    def succ(self, i):
        """successors within the routine (calls go on to the next instruction)"""
        a, n = i.addr, i.addr + i.size
        if i.flow == "end":
            return []
        if i.flow in ("ijump",):
            return sorted(self.site.get(a, ()))
        if i.flow == "jump":
            return [i.target] if i.target in self.ins else []
        if i.flow == "cond":
            out = [n] if n in self.ins else []
            if i.target in self.ins:
                out.append(i.target)
            return out
        return [n] if n in self.ins else []

    def members(self):
        """routine -> the instructions reached from its entry (calls not followed)"""
        self.belongs = {}
        mem = {}
        for e in self.entries:
            seen, work = set(), [e]
            while work:
                a = work.pop()
                if a in seen or a not in self.ins:
                    continue
                seen.add(a)
                work += self.succ(self.ins[a])
            mem[e] = seen
            for a in seen:
                self.belongs.setdefault(a, set()).add(e)
        self.mem = mem

    def must_defs(self):
        """what each routine writes on every way through it (registers it saves and takes
        back from the stack left out: they come back as they were)"""
        ins = self.ins
        restored = {}
        for e, mem in self.mem.items():
            r = 0
            for a in mem:
                i = ins[a]
                if i.base == "movem" and i.ops[0] in ("(a7)+", "(sp)+"):
                    r |= reglist(i.ops[1])
            restored[e] = r
        md = {e: ALL for e in self.entries}
        order = sorted(ins)
        while True:
            changed = False
            for e in self.entries:
                mem = self.mem[e]
                inn = {e: 0}
                work = [e]
                res = None
                # forward over the routine (a simple pass in address order, a few times)
                for _ in range(3):
                    for a in sorted(mem):
                        if a not in inn:
                            continue
                        i = ins[a]
                        cur = inn[a] | self.eff[a][1]
                        if i.flow == "call":
                            cur = inn[a] | md.get(i.target, 0)
                        elif i.flow == "icall":
                            t = self.site.get(a)
                            cur = inn[a] | (min_and([md.get(x, 0) for x in t]) if t else 0)
                        if i.base == "rts":
                            res = inn[a] if res is None else res & inn[a]
                            continue
                        for s_ in self.succ(i):
                            if s_ in mem:
                                v = cur if s_ not in inn else inn[s_] & cur
                                if s_ != e:
                                    inn[s_] = v
                v = ((res if res is not None else 0) & ~restored[e] & ~SP) & ALL_REGS
                if v != md[e]:
                    md[e] = v
                    changed = True
            if not changed:
                break
        self.md = md

    def flow(self, live, rts_out, own, ind_in):
        """one backward pass over every instruction; True if anything changed"""
        ins, changed = self.ins, False
        for a in self.order:
            i = ins[a]
            use, dfn = self.eff[a]
            if i.flow == "end":
                out = (rts_out(a) | SP) if i.base == "rts" else 0
            elif i.flow == "ijump":
                if a in self.site:
                    out = 0
                    for t in self.site[a]:
                        out |= live.get(t, 0)
                else:
                    out = ind_in
            else:
                out = 0
                for s_ in self.succ(i):
                    out |= live[s_]
            if i.flow == "call":
                cal = own.get(i.target, ALL) if i.target is not None else ALL
                kill = self.md.get(i.target, 0)
                new = ((out & ~kill) | cal | use) & ALL
            elif i.flow == "icall":
                if a in self.site:
                    cal = 0
                    for t in self.site[a]:
                        cal |= own.get(t, 0)
                    kill = min_and([self.md.get(t, 0) for t in self.site[a]])
                else:
                    cal, kill = ind_in, 0
                new = ((out & ~kill) | cal | use) & ALL
            else:
                new = (out & ~dfn) | use
            if new != live[a]:
                live[a] = new
                changed = True
        return changed

    def solve(self):
        """phase 1: what each routine itself reads at its entry (values passing through it
        untouched are its callers' business: a call keeps the caller's needs after it);
        phase 2: what is needed at each instruction, returns needing what the callers of
        the routine need after their calls"""
        self.members()
        self.must_defs()
        ins = self.ins
        self.order = sorted(ins, reverse=True)
        loose_entries = [e for e in self.indirect - self.attributed]
        own = {e: 0 for e in self.entries}
        live = {a: 0 for a in ins}
        rounds = 0
        while True:
            rounds += 1
            ind_in = 0
            for e in loose_entries:
                ind_in |= own.get(e, 0)
            ch = self.flow(live, lambda a: 0, own, ind_in)
            for e in self.entries:
                v = live.get(e, ALL)
                if v != own[e]:
                    own[e] = v
                    ch = True
            if not ch:
                break
        liveout = {e: 0 for e in self.entries}
        live2 = {a: 0 for a in ins}

        def rts_out(a):
            out = 0
            for e in self.belongs.get(a, ()):
                out |= liveout[e]
            return out
        while True:
            rounds += 1
            ch = self.flow(live2, rts_out, own, ind_in)
            ind = 0
            need = {e: 0 for e in self.entries}
            for a, i in ins.items():
                if i.flow == "call" and i.target in need:
                    need[i.target] |= live2.get(a + i.size, ALL)
                elif i.flow == "icall":
                    after = live2.get(a + i.size, ALL)
                    if a in self.site:
                        for t in self.site[a]:
                            if t in need:
                                need[t] |= after
                    else:
                        ind |= after
            ij = 0
            for a, i in ins.items():
                if i.flow == "ijump" and a not in self.site:
                    for e in self.belongs.get(a, ()):
                        ij |= liveout[e]
            for e in self.entries:
                loose = (e in self.indirect or e not in self.direct) and e not in self.attributed
                v = need[e] | (ind | ij if loose else 0)
                if v != liveout[e]:
                    liveout[e] = v
                    ch = True
            if not ch:
                break
        self.liveout, self.livein, self.live = liveout, own, live2
        return rounds


def written(p, e, memo=None, stack=()):
    """registers and flags routine e (or what it calls) may change"""
    memo = {} if memo is None else memo
    if e in memo:
        return memo[e]
    if e in stack:
        return 0
    m = 0
    for a in p.mem.get(e, ()):
        i = p.ins[a]
        m |= p.eff[a][1]
        if i.flow == "call" and i.target is not None:
            m |= written(p, i.target, memo, stack + (e,))
        elif i.flow == "icall":
            m |= ALL
    memo[e] = m
    return m


def names(m):
    out = []
    for k in range(8):
        b_, m_, h_ = m & BIT["d%d" % k], m & BIT["m%d" % k], m & BIT["h%d" % k]
        if b_ and m_ and h_:
            out.append("d%d" % k)
        elif b_ and m_:
            out.append("d%d.w" % k)
        elif b_ and not m_ and not h_:
            out.append("d%d.b" % k)
        elif b_ or m_ or h_:
            out.append("d%d(%s)" % (k, "".join(c for c, f in (("b", b_), ("m", m_), ("h", h_)) if f)))
    out += [n for n in ["a%d" % k for k in range(8)] + FLAGS if m & BIT[n]]
    return " ".join(out) or "-"


def main():
    rom = open(os.path.join(ROOT, "baserom.md"), "rb").read()
    p = Program(rom)
    p.solve()
    want = [int(x, 16) for x in sys.argv[1:]] or sorted(p.entries)
    for e in want:
        if e not in p.liveout:
            print("%06X: not a routine entry" % e)
            continue
        w = written(p, e)
        out = p.liveout[e] & w & ~SP
        print("sub_%05X  leaves for its callers: %-34s (changes: %s)"
              % (e, names(out), names(w & ~SP)))


if __name__ == "__main__":
    main()


def why(p, a, reg, limit=40):
    """a path from a to an instruction that reads reg before writing it (debugging)"""
    bit = BIT[reg]
    seen, work = set(), [(a, [])]
    while work and limit:
        x, path = work.pop(0)
        if x in seen or x not in p.ins:
            continue
        seen.add(x)
        i = p.ins[x]
        use, dfn = p.eff[x]
        if use & bit:
            return path + [x]
        if i.flow == "call" and i.target in p.livein and p.livein[i.target] & bit:
            work.append((i.target, path + [x]))
            continue
        if i.flow == "icall":
            for t in p.site.get(x, ()):
                if p.livein.get(t, 0) & bit:
                    work.append((t, path + [x]))
            continue
        if dfn & bit:
            continue
        if i.base == "rts":
            limit -= 1
            for e in p.belongs.get(x, ()):
                for c in p.callers.get(e, ()):
                    work.append((c + p.ins[c].size, path + [x]))
            continue
        for s in p.succ(i):
            work.append((s, path + [x]))
    return None
