#!/usr/bin/env python3
"""Bonanza Bros. (Mega Drive, JUE) disassembler.

Traces 68000 code from the vector table, resolves the game's function
pointer / jump table dispatch patterns, and writes a GNU as source tree
that reassembles to a byte-identical ROM.

    python3 tools/disasm.py [baserom.md]

Outputs:
    src/*.s              assembly (code + small data)
    assets.txt           ranges of large binary blobs, extracted by
                         tools/extract_assets.py into assets/
"""
import hashlib
import os
import re
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import m68k  # noqa: E402
import config  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHA1 = "f7313ecaf7143873597e3a6ebf58d542d06c3ef3"

# Mnemonics whose presence suggests we are decoding data rather than code.
WEIRD = {"ori", "abcd", "sbcd", "chk", "movep", "trapv", "reset", "stop", "nbcd", "negx",
         "addx", "subx", "rtr", "tas", "illegal", "trap", "divs", "divu"}


def rd32(rom, a):
    return struct.unpack(">I", rom[a:a + 4])[0]


def rd16s(rom, a):
    return struct.unpack(">h", rom[a:a + 2])[0]


# --------------------------------------------------------------------------
# Tracing
# --------------------------------------------------------------------------

class Tracer:
    def __init__(self, rom):
        self.rom = rom
        self.n = len(rom)
        self.ins = {}       # addr -> Insn
        self.owner = {}     # byte addr -> insn addr
        self.tables = {}    # addr -> (kind, [entries])   kind: long | rel16 | dptr
        self.dptrs = set()  # candidate data pointer tables
        self.calls = set()  # addresses that are called (bsr/jsr/pointers)

    def spec(self, start):
        """Speculatively trace from start. Returns new insns or None."""
        new, work = {}, [start]
        ins, rom, n = self.ins, self.rom, self.n
        while work:
            a = work.pop()
            while True:
                if a in ins or a in new:
                    break
                if a in self.owner or a in config.DATA_START or self._in_forced_data(a):
                    return None
                i = m68k.decode(rom, a)
                if not i or i.raw in (b"\0\0\0\0", b"\xff\xff"):
                    return None
                for b in range(a + 1, a + i.size):
                    if b in ins or b in new:
                        return None
                new[a] = i
                t = i.target
                if t is not None:
                    if t >= n:
                        if t < 0xFF0000:
                            return None
                    elif t & 1:
                        return None
                    else:
                        work.append(t)
                if i.flow in ("end", "jump", "ijump"):
                    break
                a += i.size
                if a >= n:
                    return None
        return new

    def _in_forced_data(self, a):
        return any(s <= a < e for s, e in config.FORCED_DATA)

    def commit(self, new):
        for a, i in new.items():
            self.ins[a] = i
            for b in range(a, a + i.size):
                self.owner[b] = a
            if i.flow == "call" and i.target is not None:
                self.calls.add(i.target)

    def table_entries(self, t, kind, limit):
        ents, a, rom, n = [], t, self.rom, self.n
        while a < limit:
            if kind in ("long", "dptr"):
                v = rd32(rom, a)
                if not (0x200 <= v < n) or v & 1:
                    break
                step = 4
            else:  # rel16
                v = t + rd16s(rom, a)
                if not (0x200 <= v < n) or v & 1:
                    break
                step = 2
            if kind != "dptr" and v in self.owner and v not in self.ins:
                break
            ents.append(v)
            a += step
            if kind != "dptr" and a >= min(ents):
                break
        return ents

    def _jumps_through(self, a, dst):
        """Is register dst used as a jmp/jsr target shortly after insn at a?"""
        q = self.ins[a].size + a
        for _ in range(4):
            n = self.ins.get(q)
            if not n:
                return False
            if n.base in ("jmp", "jsr") and n.ops[0] == "(%s)" % dst:
                return True
            if n.ops and n.ops[-1] == dst:
                return False
            q += n.size
        return False

    def find_tables(self):
        found = []
        ins = self.ins
        cs = sorted(ins)
        idx = {a: k for k, a in enumerate(cs)}
        for a, i in ins.items():
            for op in i.ops:
                m = re.match(r"^\$([0-9a-f]+)\(pc, [da]\d\.[wl]\)$", op)
                if not m:
                    continue
                t = int(m.group(1), 16)
                if i.base in ("movea", "move") and i.mnem.endswith(".l") and op == i.ops[0]:
                    found.append((t, "long" if self._jumps_through(a, i.ops[1]) else "dptr"))
                elif i.base == "move" and i.mnem.endswith(".w") and op == i.ops[0]:
                    nx = ins.get(a + i.size)
                    if nx and nx.base in ("jmp", "jsr", "lea") and nx.ops[0].startswith("$%x(pc" % t):
                        found.append((t, "rel16"))
            if i.base in ("movea", "move") and i.mnem.endswith(".l"):
                m = re.match(r"^\((a\d), [da]\d\.[wl]\)$", i.ops[0])
                if not m:
                    continue
                reg = m.group(1)
                kind = "long" if len(i.ops) > 1 and self._jumps_through(a, i.ops[1]) else "dptr"
                k = idx[a]
                for back in range(1, 12):
                    if k - back < 0:
                        break
                    pa = cs[k - back]
                    p = ins[pa]
                    if pa + p.size != cs[k - back + 1]:
                        break
                    if p.ops and p.ops[-1] == reg:
                        mm = None
                        if p.base == "lea":
                            mm = re.match(r"^\$([0-9a-f]+)(\(pc\)|\.l|\.w)$", p.ops[0])
                        elif p.mnem in ("movea.l", "move.l"):
                            mm = re.match(r"^#\$([0-9a-f]+)()$", p.ops[0])
                        if mm:
                            found.append((int(mm.group(1), 16), kind))
                        break
        return [(t, k) for t, k in found if t < self.n]

    def next_known(self, t):
        best = self.n
        for coll in (self.ins, self.tables):
            for x in coll:
                if t < x < best:
                    best = x
        for x in self.dptrs:
            if t < x < best:
                best = x
        return best

    def dead_code_pass(self):
        """Pick up unreferenced code that directly follows a routine's end."""
        added = False
        for a in sorted(self.ins):
            i = self.ins[a]
            e = a + i.size
            if i.flow not in ("end", "jump", "ijump") or e in self.ins or e in self.tables \
                    or e in self.dptrs or e >= self.n:
                continue
            new = self.spec(e)
            if not new or len(new) < 3:
                continue
            if any(x.base in WEIRD or any(re.search(r"\(a\d, a\d", op) or re.search(r"\(pc, a\d", op)
                                           for op in x.ops) for x in new.values()):
                continue
            self.commit(new)
            self.calls.add(e)
            added = True
        return added

    def run(self):
        rom, n = self.rom, self.n
        roots = []
        for v in range(1, 64):
            a = rd32(rom, v * 4)
            if 0x200 <= a < n:
                roots.append(a)
        roots += config.ENTRY_POINTS
        for r in roots:
            new = self.spec(r)
            if new is not None:
                self.commit(new)
                self.calls.add(r)
            else:
                print("warning: root %06x does not trace" % r, file=sys.stderr)
        for t, kind in config.TABLES:
            self.tables[t] = (kind, None)

        tried = set()
        changed = True
        while changed:
            changed = False
            cands = set()
            for i in list(self.ins.values()):
                if not i.mnem.endswith(".l"):
                    continue
                for op in i.ops:
                    m = re.match(r"^#\$([0-9a-f]+)$", op)
                    if m:
                        v = int(m.group(1), 16)
                        if 0x200 <= v < n and not v & 1:
                            cands.add(v)
            for t, kind in self.find_tables():
                if kind == "dptr":
                    self.dptrs.add(t)
                    continue
                if t in self.tables and self.tables[t][1] is not None:
                    continue
                ents = self.table_entries(t, kind, self.next_known(t))
                self.tables[t] = (kind, ents)
                cands |= set(ents)
            for t, (kind, ents) in list(self.tables.items()):
                if ents is None:  # from config
                    ents = self.table_entries(t, kind, self.next_known(t))
                    self.tables[t] = (kind, ents)
                    cands |= set(ents)
            # data pointer tables whose entries all look like code
            for t in sorted(self.dptrs):
                if t in self.tables:
                    continue
                ents = self.table_entries(t, "long", self.next_known(t))
                if len(ents) < 2:
                    continue
                ok, tot, w = True, 0, 0
                for e in ents:
                    if e in self.ins:
                        continue
                    new = self.spec(e)
                    if not new:
                        ok = False
                        break
                    tot += len(new)
                    w += sum(1 for i in new.values() if i.base in WEIRD)
                if ok and w <= 0.05 * max(tot, 1):
                    self.tables[t] = ("long", ents)
                    cands |= set(ents)
            for c in sorted(cands - tried):
                tried.add(c)
                if c in self.ins:
                    continue
                new = self.spec(c)
                if new:
                    self.commit(new)
                    self.calls.add(c)
                    changed = True
            if not changed:
                changed = self.dead_code_pass()
        # finalize data pointer tables (for symbolization only)
        for t in sorted(self.dptrs):
            if t not in self.tables:
                self.tables[t] = ("dptr", self.table_entries(t, "dptr", self.next_known(t)))


# --------------------------------------------------------------------------
# Formatting
# --------------------------------------------------------------------------

HW = {
    0xC00000: "VDP_DATA", 0xC00004: "VDP_CTRL", 0xC00008: "VDP_HVCOUNTER",
    0xC00011: "PSG_PORT",
    0xA10001: "IO_VERSION", 0xA10003: "IO_DATA1", 0xA10005: "IO_DATA2", 0xA10007: "IO_DATA3",
    0xA10009: "IO_CTRL1", 0xA1000B: "IO_CTRL2", 0xA1000D: "IO_CTRL3",
    0xA11100: "Z80_BUSREQ", 0xA11200: "Z80_RESET", 0xA14000: "TMSS_REG",
}


class Formatter:
    def __init__(self, rom, tr):
        self.rom, self.tr, self.n = rom, tr, len(rom)
        self.labels = {}  # addr -> name

    def label_for(self, a, kind):
        if a in self.labels:
            return
        prefix = {"sub": "sub", "loc": "loc", "data": "dat", "tbl": "tbl"}[kind]
        self.labels[a] = "%s_%05X" % (prefix, a)

    def sym(self, a):
        """Symbolic expression for ROM address a (or None)."""
        if a in self.labels:
            return self.labels[a]
        return None

    def ref_target(self, a):
        """Register a reference to ROM address a and return the expression."""
        tr = self.tr
        if a in tr.owner and a not in tr.ins:
            base = tr.owner[a]
            self.label_for(base, "loc")
            return "%s+%d" % (self.labels[base], a - base)
        if a in tr.ins:
            self.label_for(a, "sub" if a in tr.calls else "loc")
        elif a in tr.tables:
            self.label_for(a, "tbl")
        else:
            self.label_for(a, "data")
        return self.labels[a]

    def collect(self):
        tr = self.tr
        for a in sorted(tr.ins):
            if a in tr.calls:
                self.label_for(a, "sub")
        for a, (kind, ents) in tr.tables.items():
            self.label_for(a, "tbl")
            for e in ents or []:
                self.ref_target(e)
        for a in sorted(tr.ins):
            self.fmt_insn(tr.ins[a], collect=True)
        for v in range(64):
            t = rd32(self.rom, v * 4)
            if t in tr.ins:
                self.label_for(t, "sub")

    @staticmethod
    def hexs(v):
        return "0x%X" % v

    def fmt_operand(self, i, op, is_branch_target, collect):
        n = self.n
        m = re.match(r"^\$([0-9a-f]+)$", op)
        if is_branch_target and m:
            t = int(m.group(1), 16)
            return self.ref_target(t)
        # pc relative
        m = re.match(r"^\$([0-9a-f]+)\(pc(.*)\)$", op)
        if m:
            t = int(m.group(1), 16)
            idx = m.group(2).replace(" ", "")
            return "%s(pc%s)" % (self.ref_target(t), idx)
        # absolute
        m = re.match(r"^\$([0-9a-f]+)\.([wl])$", op)
        if m:
            v, sz = int(m.group(1), 16), m.group(2)
            if sz == "l" and v < n and v >= 0x200 and (v in self.tr.ins or v in self.tr.tables
                                                      or v not in self.tr.owner):
                return "(%s).l" % self.ref_target(v)
            if sz == "w" and v < 0x8000 and v >= 0x200 and v < n and i.base in ("jsr", "jmp"):
                return "(%s).w" % self.ref_target(v)
            if sz == "l" and v in HW:
                return "(%s).l" % HW[v]
            return "(%s).%s" % (self.hexs(v), sz)
        # immediate
        m = re.match(r"^#\$([0-9a-f]+)$", op)
        if m:
            v = int(m.group(1), 16)
            if (i.mnem in ("move.l", "movea.l", "cmpi.l") and 0x200 <= v < n
                    and v in self.tr.ins and not v & 1):
                return "#" + self.ref_target(v)
            if i.base == "moveq" and v >= 0x80:
                v -= 0x100
                return "#-0x%X" % -v
            return "#" + self.hexs(v)
        # displacement / index forms: replace $hex with 0x
        op = re.sub(r"-\$([0-9a-f]+)", lambda mm: "-0x%X" % int(mm.group(1), 16), op)
        op = re.sub(r"\$([0-9a-f]+)", lambda mm: "0x%X" % int(mm.group(1), 16), op)
        op = op.replace(", ", ",")
        # gas would drop an explicit zero displacement; keep the d16 form
        op = re.sub(r"^0x0\((a\d)\)$", r"(0.w,\1)", op)
        return op

    def fmt_insn(self, i, collect=False):
        mnem = i.mnem
        base = i.base
        if base in ("btst", "bset", "bclr", "bchg", "lea", "pea", "abcd", "sbcd", "nbcd"):
            mnem = base
        ops = list(i.ops)
        if base == "move" and "usp" in ops:
            mnem = "move.l"
        if mnem.endswith(".b") and (base in m68k.BCC or base in ("bra", "bsr")):
            mnem = mnem[:-2] + ".s"
        out = []
        for k, op in enumerate(ops):
            is_bt = (k == len(ops) - 1) and (base in m68k.BCC or base in ("bra", "bsr") or base.startswith("db"))
            out.append(self.fmt_operand(i, op, is_bt, collect))
        if base in ("jmp", "jsr") and i.target is not None and i.target < self.n:
            pass
        return "%s %s" % (mnem, ",".join(out)) if out else mnem


# --------------------------------------------------------------------------
# Emission
# --------------------------------------------------------------------------

def rawwords(b):
    return ".short " + ",".join("0x%04X" % struct.unpack(">H", b[k:k + 2])[0] for k in range(0, len(b), 2))


class Emitter:
    BLOB_MIN = 0x400  # data chunks at least this large go to assets/

    def __init__(self, rom, tr, fm, raw_insns):
        self.rom, self.tr, self.fm, self.raw = rom, tr, fm, raw_insns
        self.blobs = []

    def items(self):
        """Yield (addr, size, kind) covering the whole ROM in order."""
        tr, n = self.tr, len(self.rom)
        # table extents (truncate at code)
        tab = {}
        for t, (kind, ents) in tr.tables.items():
            if t >= n or t in tr.owner:
                continue
            step = 2 if kind == "rel16" else 4
            cnt = 0
            for k in range(len(ents or [])):
                a = t + k * step
                if any(b in tr.owner for b in range(a, a + step)):
                    break
                if k and a in tr.tables:
                    break
                cnt += 1
            if cnt:
                tab[t] = (kind, ents[:cnt], step)
        self.tab = tab
        a = 0x200
        while a < n:
            if a in tr.ins:
                yield (a, tr.ins[a].size, "insn")
                a += tr.ins[a].size
            elif a in tab:
                kind, ents, step = tab[a]
                for k in range(len(ents)):
                    yield (a + k * step, step, kind)
                a += step * len(ents)
            else:
                e = a + 1
                while e < n and e not in tr.ins and e not in tab:
                    e += 1
                yield (a, e - a, "data")
                a = e

    def header(self):
        rom, fm = self.rom, self.fm
        L = ["| Vector table and cartridge header", ""]
        L.append("\t.section .text")
        L.append("\t.global _start")
        L.append("_start:")
        names = {0: "initial SSP", 1: "reset", 2: "bus error", 3: "address error", 4: "illegal instruction",
                 5: "zero divide", 6: "CHK", 7: "TRAPV", 8: "privilege violation", 9: "trace",
                 10: "line A", 11: "line F", 24: "spurious", 28: "level 4 (HBlank)", 30: "level 6 (VBlank)"}
        for v in range(64):
            t = rd32(rom, v * 4)
            s = fm.labels.get(t) if t in self.tr.ins else None
            c = names.get(v, "")
            L.append("\t.long %s%s" % (s or "0x%08X" % t, ("\t| " + c) if c else ""))
        L.append("")
        fields = [(0x100, 16, "console"), (0x110, 16, "copyright"), (0x120, 48, "domestic name"),
                  (0x150, 48, "overseas name"), (0x180, 14, "serial")]
        for off, ln, c in fields:
            L.append('\t.ascii "%s"\t| %s' % (rom[off:off + ln].decode("ascii"), c))
        L.append("\t.short 0x%04X\t| checksum" % struct.unpack(">H", rom[0x18E:0x190])[0])
        L.append('\t.ascii "%s"\t| I/O support' % rom[0x190:0x1A0].decode("ascii"))
        L.append("\t.long 0x%08X,0x%08X\t| ROM start/end" % (rd32(rom, 0x1A0), rd32(rom, 0x1A4)))
        L.append("\t.long 0x%08X,0x%08X\t| RAM start/end" % (rd32(rom, 0x1A8), rd32(rom, 0x1AC)))
        L.append('\t.ascii "%s"\t| modem / memo' % rom[0x1B0:0x1F0].decode("ascii"))
        L.append('\t.ascii "%s"\t| regions' % rom[0x1F0:0x200].decode("ascii"))
        return L

    def lines(self, check=False):
        """Generate (addr, [lines]) chunks for the body (0x200..end)."""
        rom, tr, fm = self.rom, self.tr, self.fm
        self.blobs = []
        for a, size, kind in self.items():
            L = []
            if check:
                L.append("\t.org 0x%X" % a)
            if kind == "data":
                # split at labels
                cuts = sorted([x for x in fm.labels if a < x < a + size])
                pts = [a] + cuts + [a + size]
                for s, e in zip(pts, pts[1:]):
                    if s in fm.labels:
                        L.append("%s:" % fm.labels[s])
                    L += self.data_lines(s, e)
                yield a, kind, L
                continue
            if a in fm.labels:
                L.append("%s:" % fm.labels[a])
            if kind == "insn":
                i = tr.ins[a]
                if a in self.raw:
                    L.append("\t%s\t| %s" % (rawwords(i.raw), fm.fmt_insn(i)))
                else:
                    L.append("\t" + fm.fmt_insn(i))
            elif kind in ("long", "dptr"):
                v = rd32(rom, a)
                s = fm.sym(v) if v < len(rom) else None
                L.append("\t.long %s" % (s or "0x%08X" % v))
            elif kind == "rel16":
                tbl = max(t for t in self.tab if t <= a)
                v = tbl + rd16s(rom, a)
                L.append("\t.short %s-%s" % (fm.sym(v), fm.labels[tbl]))
            yield a, kind, L

    def data_lines(self, s, e):
        rom = self.rom
        L = []
        if e - s >= self.BLOB_MIN:
            name = "data_%05X.bin" % s
            self.blobs.append((s, e, name))
            L.append('\t.incbin "assets/%s"\t| 0x%X bytes' % (name, e - s))
            return L
        for k in range(s, e, 16):
            chunk = rom[k:min(e, k + 16)]
            L.append("\t.byte " + ",".join("0x%02X" % b for b in chunk))
        return L


PRELUDE = [
    "| Bonanza Bros. (Mega Drive) - generated by tools/disasm.py",
    "",
    '\t.include "include/md.inc"',
    "",
]


def write_tree(em, check=False, outdir=None):
    """Write the source tree. Returns path to main file."""
    outdir = outdir or ROOT
    src = os.path.join(outdir, "src")
    os.makedirs(src, exist_ok=True)
    files = []
    hdr = PRELUDE + em.header()
    cur_lines, cur_start, size = [], 0x200, 0
    chunks = []
    for a, kind, L in em.lines(check=check):
        # split files at sub_ labels after ~0x2000 bytes
        if size >= 0x2000 and em.fm.labels.get(a, "").startswith("sub_"):
            chunks.append((cur_start, cur_lines))
            cur_lines, cur_start, size = [], a, 0
        cur_lines += L
        size += 2 if kind == "insn" else 4
        if kind == "data":
            size += 0x40
    chunks.append((cur_start, cur_lines))
    main = list(hdr) + [""]
    for start, L in chunks:
        fn = "rom_%05X.s" % start
        files.append(fn)
        with open(os.path.join(src, fn), "w") as f:
            f.write("| ROM 0x%05X\n\n" % start)
            f.write("\n".join(L) + "\n")
        main.append('\t.include "src/%s"' % fn)
    mainpath = os.path.join(src, "bonanza.s")
    with open(mainpath, "w") as f:
        f.write("\n".join(main) + "\n")
    # remove stale chunk files
    for fn in os.listdir(src):
        if fn.startswith("rom_") and fn not in files:
            os.remove(os.path.join(src, fn))
    return mainpath


def write_blobs(em, rom, outdir):
    d = os.path.join(outdir, "assets")
    os.makedirs(d, exist_ok=True)
    for s, e, name in em.blobs:
        with open(os.path.join(d, name), "wb") as f:
            f.write(rom[s:e])


def assemble(outdir):
    """Assemble + link outdir/src/bonanza.s, return binary bytes or (None, stderr)."""
    obj = os.path.join(outdir, "out.o")
    elf = os.path.join(outdir, "out.elf")
    binp = os.path.join(outdir, "out.bin")
    r = subprocess.run(["m68k-linux-gnu-as", "-m68000", "--register-prefix-optional", "-I", outdir,
                        "-o", obj, os.path.join(outdir, "src", "bonanza.s")],
                       cwd=outdir, capture_output=True, text=True)
    if r.returncode:
        return None, r.stderr
    r = subprocess.run(["m68k-linux-gnu-ld", "-T", os.path.join(ROOT, "bonanza.ld"), "-o", elf, obj],
                       capture_output=True, text=True)
    if r.returncode:
        return None, r.stderr
    subprocess.run(["m68k-linux-gnu-objcopy", "-O", "binary", elf, binp], check=True)
    return open(binp, "rb").read(), ""


def main():
    rom_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "baserom.md")
    rom = open(rom_path, "rb").read()
    if hashlib.sha1(rom).hexdigest() != SHA1:
        sys.exit("baserom sha1 mismatch (expected %s)" % SHA1)
    tr = Tracer(rom)
    tr.run()
    fm = Formatter(rom, tr)
    fm.collect()
    code = sum(i.size for i in tr.ins.values())
    print("traced %d instructions, %d bytes of code (%.1f%%)" % (len(tr.ins), code, 100.0 * code / len(rom)))

    raw = set()
    with tempfile.TemporaryDirectory() as tmp:
        os.symlink(os.path.join(ROOT, "include"), os.path.join(tmp, "include"))
        # Pass 1: per-instruction check build with .org before every item.
        for it in range(8):
            em = Emitter(rom, tr, fm, raw)
            write_tree(em, check=True, outdir=tmp)
            write_blobs(em, rom, tmp)
            out, err = assemble(tmp)
            if out is None:
                bad = set()
                for line in err.splitlines():
                    m = re.match(r"^src/rom_[0-9A-F]+\.s:(\d+): Error", line) or \
                        re.match(r"^(\S+):(\d+): Error", line)
                    if not m:
                        continue
                    fn, ln = line.split(":")[0], int(line.split(":")[1])
                    bad |= blame(os.path.join(tmp, fn), ln, tr)
                if not bad:
                    print(err[:4000])
                    sys.exit("assembly failed")
                raw |= bad
                print("pass %d: %d instructions rejected by assembler" % (it, len(bad)))
                continue
            out = out.ljust(len(rom), b"\0")
            bad = set()
            for a, i in tr.ins.items():
                if out[a:a + i.size] != rom[a:a + i.size]:
                    bad.add(a)
            if not bad and out[:len(rom)] == rom:
                break
            if not bad:
                for k in range(len(rom)):
                    if out[k] != rom[k]:
                        sys.exit("data mismatch at 0x%X" % k)
            raw |= bad
            print("pass %d: %d instructions encode differently" % (it, len(bad)))
        print("%d instructions emitted as raw words" % len(raw))

    em = Emitter(rom, tr, fm, raw)
    write_tree(em, check=False)
    with open(os.path.join(ROOT, "assets.txt"), "w") as f:
        f.write("# start end file  (generated by tools/disasm.py)\n")
        for s, e, name in em.blobs:
            f.write("%05X %05X %s\n" % (s, e, name))
    print("wrote src/ (%d blobs listed in assets.txt)" % len(em.blobs))


def blame(path, ln, tr):
    """Map an assembler error line back to an instruction address."""
    lines = open(path).read().splitlines()
    # In check mode every item starts with .org; find the item the line belongs to.
    org = None
    k = ln - 1
    errline = lines[k] if k < len(lines) else ""
    while k >= 0:
        m = re.match(r"^\t\.org 0x([0-9A-F]+)$", lines[k])
        if m:
            org = int(m.group(1), 16)
            break
        k -= 1
    if org is None:
        return set()
    if errline.startswith("\t.org"):
        # .org moved backwards: the previous instruction is too long
        k -= 1
        while k >= 0:
            m = re.match(r"^\t\.org 0x([0-9A-F]+)$", lines[k])
            if m:
                prev = int(m.group(1), 16)
                return {prev} if prev in tr.ins else set()
            k -= 1
        return set()
    return {org} if org in tr.ins else set()


if __name__ == "__main__":
    main()
