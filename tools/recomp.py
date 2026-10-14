#!/usr/bin/env python3
"""Static recompiler: 68000 code of Bonanza Bros. -> C (port/gen/).

    python3 tools/recomp.py [baserom.md]

Code is found with the same tracer as tools/disasm.py. Every traced
instruction becomes a block of C inside a per-region function:

  * branches, calls and returns are translated directly into C control
    flow (goto inside a region, return to the dispatcher across regions);
  * every other instruction is produced by partially evaluating the
    reference interpreter's (Musashi) handler for that opcode: the opcode,
    register numbers, extension words and cycle counts become constants;
  * status register writes, rte, stop and move usp are written out with the
    interpreter handler's steps (there is no interpreter in the port).

Each instruction yields to the scheduler at the same points the interpreter
does, so a recompiled run is bit-identical to an interpreted one; the
headless runner's --hash output is used to check that.
"""
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import disasm  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MUSASHI = os.path.join(ROOT, "port", "ext", "musashi")
OUT = os.path.join(ROOT, "port", "gen")

CHUNK_INSNS = 400
CHUNKS_PER_FILE = 12

# Tokens that make a specialised handler unsafe (they depend on or change
# PC/SR/exception state); such instructions go through the interpreter.
FORBIDDEN = re.compile(r"\b(REG_PC|REG_PPC|m68ki_jump\w*|m68ki_branch_\w+|m68ki_exception_\w+|m68ki_set_sr\w*|"
                       r"m68ki_set_s_flag|m68ki_set_sm_flag\w*|CPU_STOPPED|USE_ALL_CYCLES|m68ki_rte\w*|"
                       r"m68ki_fake_\w+|m68ki_stack_frame_\w+|m68ki_read_imm_\w+|m68ki_get_ea_\w+|OPER_\w+|EA_\w+|"
                       r"REG_VBR|m68ki_cpu\.instr_mode|m68ki_cpu\.run_mode|RESET_CYCLES|m68ki_output_reset)\b")


# --------------------------------------------------------------------------
# Musashi tables
# --------------------------------------------------------------------------

# places where port/src/gamemods.c may step in: routine entries it can take over, and
# 0x3B08 (the Options menu drawing EXIT, where the stage select code adds STAGE)
HOOKS = {0x58B4, 0x59C6, 0x11106, 0x3B08}    # gamemods.c, extrastages.c, cheats.c

# operands that the port changes in the ROM buffer while running: read from it at run
# time instead of being built in, so changing them needs no interpreter.
#   instruction -> (offset of the operand, size)
LIVE = {
    0x59E4: (2, 4), 0x5B32: (2, 4), 0x5DB2: (2, 4),   # lea (frame table).l,a6: level mods' own frame table
    0x3C22: (2, 2), 0x3C42: (2, 2),                   # Options cursor wrap (stage select code)
}
MARK = {2: 0x7A5B, 4: 0x5A5A1234}


class Musashi:
    def __init__(self):
        src = open(os.path.join(MUSASHI, "m68kops.c")).read()
        self.bodies = {}
        for m in re.finditer(r"^static void (m68k_op_\w+)\(void\)\n\{\n(.*?)^\}", src, re.S | re.M):
            self.bodies[m.group(1)] = m.group(2)
        tab = src[src.index("m68k_opcode_handler_table[] ="):]
        self.table = []
        for m in re.finditer(r"\{(m68k_op_\w+)\s*,\s*0x([0-9a-f]+),\s*0x([0-9a-f]+),\s*\{\s*(\d+)", tab):
            self.table.append((m.group(1), int(m.group(2), 16), int(m.group(3), 16), int(m.group(4))))
        self._cache = {}

    def lookup(self, op):
        """(handler name, 68000 cycles) for an opcode, as m68ki_build_opcode_table does."""
        if op in self._cache:
            return self._cache[op]
        res = ("m68k_op_illegal", 0)
        for name, mask, match, cyc in self.table:
            if mask == 0xFF00:
                if (op & 0xFF00) == match:
                    res = (name, cyc)
            elif (op & mask) == match:
                res = (name, cyc)
        self._cache[op] = res
        return res


# --------------------------------------------------------------------------
# Specialisation
# --------------------------------------------------------------------------

def s16(v):
    return v - 0x10000 if v & 0x8000 else v


def s8(v):
    return v - 0x100 if v & 0x80 else v


def u32(v):
    return v & 0xFFFFFFFF


class ExtStream:
    def __init__(self, addr, raw):
        self.addr = addr
        self.raw = raw
        self.pos = 2

    def word(self):
        if self.pos + 2 > len(self.raw):
            raise ValueError("extension words exhausted")
        a = self.addr + self.pos
        v = struct.unpack(">H", self.raw[self.pos:self.pos + 2])[0]
        self.pos += 2
        return v, a

    def done(self):
        return self.pos == len(self.raw)


def index_expr(base, ext):
    """68000 (d8,base,Xn) effective address expression."""
    k = ext >> 12
    xn = "REG_DA[%d]" % k
    if not ext & 0x800:
        xn = "(uint)MAKE_INT_16(%s)" % xn
    return "(%s + %s + 0x%Xu)" % (base, xn, u32(s8(ext & 0xFF)))


def ea_expr(kind, mode, size, op, ext):
    """C expression for an EA macro (EA_<kind>_<mode>_<size>)."""
    if kind == "AY":
        reg = "REG_A[%d]" % (op & 7)
    elif kind == "AX":
        reg = "REG_A[%d]" % ((op >> 9) & 7)
    elif kind == "A7":
        reg = "REG_A[7]"
    else:
        reg = None
    n = {"8": 1, "16": 2, "32": 4}[size]
    if mode == "AI":
        return reg
    if mode == "PI":
        if kind == "A7":
            return "((REG_A[7]+=2)-2)"
        return "(%s++)" % reg if n == 1 else "((%s+=%d)-%d)" % (reg, n, n)
    if mode == "PD":
        if kind == "A7":
            return "(REG_A[7]-=2)"
        return "(--%s)" % reg if n == 1 else "(%s-=%d)" % (reg, n)
    if mode == "DI":
        v, _ = ext.word()
        return "(%s+0x%Xu)" % (reg, u32(s16(v)))
    if mode == "IX":
        v, _ = ext.word()
        return index_expr(reg, v)
    if mode == "AW":
        v, _ = ext.word()
        return "0x%Xu" % u32(s16(v))
    if mode == "AL":
        hi, _ = ext.word()
        lo, _ = ext.word()
        return "0x%Xu" % ((hi << 16) | lo)
    if mode == "PCDI":
        v, a = ext.word()
        return "0x%Xu" % u32(a + s16(v))
    if mode == "PCIX":
        v, a = ext.word()
        return index_expr("0x%Xu" % a, v)
    raise ValueError(mode)


MACRO = re.compile(r"\b(OPER|EA)_(?:(AY|AX|A7)_(AI|PI|PD|DI|IX)|(AW|AL|PCDI|PCIX)|(I))_(8|16|32)\(\)")


def specialise(body, op, addr, raw):
    """Return C code for the handler body with everything known made constant, or None."""
    ext = ExtStream(addr, raw)

    def repl(m):
        what, kind, mode, absmode, imm, size = m.groups()
        if imm:
            if size == "32":
                hi, _ = ext.word()
                lo, _ = ext.word()
                return "0x%Xu" % ((hi << 16) | lo)
            v, _ = ext.word()
            return "0x%Xu" % (v & 0xFF if size == "8" else v)
        if absmode:
            e = ea_expr(None, absmode, size, op, ext)
        else:
            e = ea_expr(kind, mode, size, op, ext)
        if what == "EA":
            return e
        if absmode in ("PCDI", "PCIX"):
            return "m68ki_read_pcrel_%s(%s)" % (size, e)
        return "m68ki_read_%s(%s)" % (size, e)

    try:
        out = MACRO.sub(repl, body)
    except ValueError:
        return None
    if not ext.done():
        return None
    out = re.sub(r"\bREG_IR\b", "0x%04Xu" % op, out)
    out = re.sub(r"\bDX\b", "REG_D[%d]" % ((op >> 9) & 7), out)
    out = re.sub(r"\bDY\b", "REG_D[%d]" % (op & 7), out)
    out = re.sub(r"\bAX\b", "REG_A[%d]" % ((op >> 9) & 7), out)
    out = re.sub(r"\bAY\b", "REG_A[%d]" % (op & 7), out)
    out = re.sub(r"CPU_TYPE_IS_010_LESS\(CPU_TYPE\)", "1", out)
    out = re.sub(r"\bm68ki_trace_t[01]\(\);[^\n]*", "", out)
    if FORBIDDEN.search(out) or "CPU_TYPE" in out:
        return None
    return out


# The game's drawing routines written in C (port/src/draw.c): called where they start;
# the rts is done here. When one cannot run in one go, the routine runs as translated.
DRAW = {
    0x01146: "draw_tiles_packed",
    0x0119C: "draw_tiles_raw",
    0x011B8: "draw_text",
    0x011F2: "draw_text_d7",
    0x13318: "draw_text_queue",
    0x1376A: "draw_text_queue2",
    0x0070A: "draw_registers",
}
# the split screen's line effects: interrupt routines (rte)
DRAW_RTE = {a: "draw_band_%05X" % a for a in (
    0x00C1E,
    0x00C32,
    0x00CD8,
    0x00D2C,
    0x00D74,
    0x00DAC,
    0x00DEC,
    0x00E34,
    0x00E7C,
    0x00EC4,
    0x00F5A,
    0x00FA0,
    0x00FE8,
    0x01020,
    0x01058,
    0x010A0,
    0x010E8,
)}

# --------------------------------------------------------------------------
# Video: the game's commands to the video chip, decoded here
# --------------------------------------------------------------------------

VDP_DATA, VDP_CTRL = 0xC00000, 0xC00004


def video_words(words):
    """C calls for control words written one after the other (none waiting before)."""
    out, i = [], 0
    while i < len(words):
        w = words[i]
        if (w & 0xC000) == 0x8000 and ((w >> 8) & 0x1F) < 24:
            out.append("vdp_set_reg(%d, 0x%02X);" % ((w >> 8) & 0x1F, w & 0xFF))
            i += 1
        elif (w & 0xC000) != 0x8000 and i + 1 < len(words):
            hi, lo = w, words[i + 1]
            code = ((hi >> 14) & 3) | ((lo >> 2) & 0x3C)
            addr = (hi & 0x3FFF) | ((lo & 3) << 14)
            out.append("vdp_command(0x%02X, 0x%04X);" % (code, addr))
            i += 2
        else:
            out.append("vdp_write_ctrl(0x%04X);" % w)
            i += 1
    return " ".join(out)


def video_calls(spec, stats):
    """Writes and reads of the video chip's ports as direct calls; fixed control words
    (registers, where data goes) decoded now."""
    m = re.search(r"uint ea = 0x([0-9A-F]+)u;", spec)
    w = re.search(r"m68ki_write_(16|32)\(ea, res\);", spec)
    if m and w and int(m.group(1), 16) in (VDP_DATA, VDP_CTRL):
        port, size = int(m.group(1), 16), int(w.group(1))
        k = re.search(r"uint res = 0x([0-9A-F]+)u;", spec)
        if port == VDP_CTRL and k:
            v = int(k.group(1), 16)
            words = [v >> 16, v & 0xFFFF] if size == 32 else [v & 0xFFFF]
            call = video_words(words)
            stats["video_fixed"] += 1
        else:
            fn = "vdp_write_data" if port == VDP_DATA else "vdp_write_ctrl"
            call = ("%s((uint16_t)(res >> 16)); %s((uint16_t)res);" % (fn, fn) if size == 32
                    else "%s((uint16_t)res);" % fn)
            stats["video_port"] += 1
        spec = spec[:w.start()] + call + spec[w.end():]
    for port, fn in ((VDP_DATA, "vdp_read_data"), (VDP_CTRL, "vdp_read_ctrl")):
        pat = "m68ki_read_16(0x%Xu)" % port
        if pat in spec:
            spec = spec.replace(pat, "%s()" % fn)
            stats["video_port"] += 1
    return spec


# --------------------------------------------------------------------------
# Code generation
# --------------------------------------------------------------------------

COND = {0: "1", 1: "0", 2: "COND_HI()", 3: "COND_LS()", 4: "COND_CC()", 5: "COND_CS()", 6: "COND_NE()",
        7: "COND_EQ()", 8: "COND_VC()", 9: "COND_VS()", 10: "COND_PL()", 11: "COND_MI()", 12: "COND_GE()",
        13: "COND_LT()", 14: "COND_GT()", 15: "COND_LE()"}


class Gen:
    def __init__(self, rom, tr):
        self.rom = rom
        self.tr = tr
        self.mu = Musashi()
        self.stats = {"native": 0, "flow": 0, "special": 0, "call": 0, "video_fixed": 0, "video_port": 0, "draw": 0}
        self.callop_names = {}

    # -- chunking --
    def chunks(self):
        addrs = sorted(self.tr.ins)
        chunks, cur = [], []
        prev_end = None
        for a in addrs:
            i = self.tr.ins[a]
            gap = prev_end is not None and a != prev_end
            if cur and ((gap and a - prev_end > 0x200) or
                        (len(cur) >= CHUNK_INSNS and (a in self.tr.calls or gap))):
                chunks.append(cur)
                cur = []
            cur.append(a)
            prev_end = a + i.size
        if cur:
            chunks.append(cur)
        return chunks

    # -- control flow helpers --
    def goto(self, target, inchunk):
        if target in inchunk:
            return "goto L_%06X;" % target
        return "{ REG_PC = 0x%Xu; return; }" % target

    def flow_ea(self, i, op):
        """Effective address of a jmp/jsr as (C expression) or None."""
        mode, reg = (op >> 3) & 7, op & 7
        ext = ExtStream(i.addr, i.raw)
        if mode == 2:
            e = "REG_A[%d]" % reg
        elif mode == 5:
            e = ea_expr("AY", "DI", "32", op, ext)
        elif mode == 6:
            e = ea_expr("AY", "IX", "32", op, ext)
        elif mode == 7 and reg == 0:
            e = ea_expr(None, "AW", "32", op, ext)
        elif mode == 7 and reg == 1:
            e = ea_expr(None, "AL", "32", op, ext)
        elif mode == 7 and reg == 2:
            e = ea_expr(None, "PCDI", "32", op, ext)
        elif mode == 7 and reg == 3:
            e = ea_expr(None, "PCIX", "32", op, ext)
        else:
            return None
        return e if ext.done() else None

    def emit_flow(self, i, op, cyc, inchunk):
        """Branches, calls, returns. Returns list of lines or None."""
        a, nxt = i.addr, i.addr + i.size
        L = []
        if (op & 0xF000) == 0x6000:
            cond = (op >> 8) & 0xF
            d8 = op & 0xFF
            if d8 == 0:
                target = u32(a + 2 + s16(struct.unpack(">H", i.raw[2:4])[0]))
                notake = "CYC_BCC_NOTAKE_W"
            else:
                target = u32(a + 2 + s8(d8))
                notake = "CYC_BCC_NOTAKE_B"
            if target == a and cond in (0,):
                return None  # bra to itself: interpreter burns the slice
            if cond == 0:      # bra
                L.append("USE_CYCLES(%d); %s" % (cyc, self.goto(target, inchunk)))
            elif cond == 1:    # bsr
                L.append("m68ki_push_32(0x%Xu); USE_CYCLES(%d); %s" % (nxt, cyc, self.goto(target, inchunk)))
            else:
                L.append("if (%s) { USE_CYCLES(%d); %s }" % (COND[cond], cyc, self.goto(target, inchunk)))
                L.append("USE_CYCLES(%d + %s);" % (cyc, notake))
            return L
        if (op & 0xF0F8) == 0x50C8:  # DBcc
            cond = (op >> 8) & 0xF
            r = op & 7
            target = u32(a + 2 + s16(struct.unpack(">H", i.raw[2:4])[0]))
            body = ("{ uint res = MASK_OUT_ABOVE_16(REG_D[%d] - 1); REG_D[%d] = MASK_OUT_BELOW_16(REG_D[%d]) | res; "
                    "if (res != 0xffff) { USE_CYCLES(%d + CYC_DBCC_F_NOEXP); %s } USE_CYCLES(%d + CYC_DBCC_F_EXP); }"
                    % (r, r, r, cyc, self.goto(target, inchunk), cyc))
            if cond == 1:
                L.append(body)
            else:
                L.append("if (%s) { USE_CYCLES(%d); } else %s" % (COND[cond], cyc, body))
            return L
        if op == 0x4E75:  # rts
            L.append("REG_PC = m68ki_pull_32(); USE_CYCLES(%d); goto dispatch;" % cyc)
            return L
        if (op & 0xFFC0) in (0x4E80, 0x4EC0):  # jsr / jmp
            e = self.flow_ea(i, op)
            if e is None:
                return None
            const = re.fullmatch(r"0x([0-9A-F]+)u", e)
            if (op & 0xFFC0) == 0x4EC0 and const and int(const.group(1), 16) == a:
                return None  # jmp to itself
            if (op & 0xFFC0) == 0x4E80:
                L.append("{ uint ea = %s; m68ki_push_32(0x%Xu); USE_CYCLES(%d);" % (e, nxt, cyc))
            else:
                L.append("{ uint ea = %s; USE_CYCLES(%d);" % (e, cyc))
            if const:
                t = int(const.group(1), 16)
                L.append("  %s }" % self.goto(t, inchunk))
            else:
                L.append("  REG_PC = ea; goto dispatch; }")
            return L
        return None

    def emit_special(self, i, op):
        """Status register, return from exception, stop, USP: the interpreter handler's
        steps written out (same order, same cycles), so nothing goes through it."""
        a, nxt = i.addr, i.addr + i.size
        imm = struct.unpack(">H", i.raw[2:4])[0] if i.size >= 4 else None
        pre = "REG_PPC = 0x%Xu; REG_PC = 0x%Xu; REG_IR = 0x%04X;" % (a, nxt, op)
        n = self.mu.lookup(op)[1]
        cyc = "USE_CYCLES(%d);" % n
        priv = "else m68ki_exception_privilege_violation();"
        if op == 0x4E73:                                   # rte (68000 frame)
            return [pre, "if (FLAG_S) { uint sr = m68ki_pull_16(); uint pc = m68ki_pull_32(); m68ki_jump(pc); m68ki_set_sr(sr); "
                    "CPU_INSTR_MODE = INSTRUCTION_YES; CPU_RUN_MODE = RUN_MODE_NORMAL; } " + priv, cyc, "goto dispatch;"]
        if op == 0x4E72:                                   # stop #imm
            return [pre, "if (FLAG_S) { CPU_STOPPED |= STOP_LEVEL_STOP; m68ki_set_sr(0x%Xu); "
                    "if (m68ki_remaining_cycles >= %d) m68ki_remaining_cycles = %d; "
                    "else m68ki_remaining_cycles %%= %d; } " % (imm, n, n, n) + priv, cyc, "goto dispatch;"]
        if op == 0x46FC:                                   # move #imm,sr
            body = "m68ki_set_sr(0x%Xu);" % imm
        elif (op & 0xFFF8) == 0x46D8:                      # move (an)+,sr
            body = "{ uint ea = (REG_A[%d] += 2) - 2; m68ki_set_sr(m68ki_read_16(ea)); }" % (op & 7)
        elif op == 0x007C:                                 # ori #imm,sr
            body = "m68ki_set_sr(m68ki_get_sr() | 0x%Xu);" % imm
        elif op == 0x027C:                                 # andi #imm,sr
            body = "m68ki_set_sr(m68ki_get_sr() & 0x%Xu);" % imm
        elif (op & 0xFFF8) == 0x40E0:                      # move sr,-(an): allowed in user mode on a 68000
            return [pre, "{ uint ea = (REG_A[%d] -= 2); m68ki_write_16(ea, m68ki_get_sr()); }" % (op & 7), cyc]
        elif (op & 0xFFF8) == 0x4E60:                      # move an,usp
            return [pre, "if (FLAG_S) REG_USP = REG_A[%d]; " % (op & 7) + priv, cyc, "if (REG_PC != 0x%Xu) goto dispatch;" % nxt]
        else:
            return None
        return [pre, "if (FLAG_S) " + body + " " + priv, cyc, "if (REG_PC != 0x%Xu) goto dispatch;" % nxt]

    def emit_insn(self, i, inchunk):
        a, nxt = i.addr, i.addr + i.size
        op = struct.unpack(">H", i.raw[:2])[0]
        name, cyc = self.mu.lookup(op)
        L = ["L_%06X: CHK(0x%Xu)  /* %s */" % (a, a, self.fm_text(i))]
        if a in HOOKS:
            # optional game changes (port/src/gamemods.c): 1 = the routine was done (return
            # from it), 2 = the hook set where to go on
            L.append("\tif (game_hooks) { int h = game_hook(0x%Xu); if (h == 1) { REG_PC = m68ki_pull_32(); USE_CYCLES(16); goto dispatch; } "
                     "if (h == 2) goto dispatch; }" % a)
        if a in DRAW:
            # 0: an interrupt would come within it: the routine below, as translated
            self.stats["draw"] += 1
            L.append("\tif (%s()) { REG_PC = m68ki_pull_32(); USE_CYCLES(16); goto dispatch; }" % DRAW[a])
        elif a in DRAW_RTE:
            self.stats["draw"] += 1
            L.append("\tif (%s()) { uint sr = m68ki_pull_16(); uint pc = m68ki_pull_32(); m68ki_jump(pc); m68ki_set_sr(sr); "
                     "CPU_INSTR_MODE = INSTRUCTION_YES; CPU_RUN_MODE = RUN_MODE_NORMAL; USE_CYCLES(%d); goto dispatch; }"
                     % (DRAW_RTE[a], self.mu.lookup(0x4E73)[1]))
        flow = self.emit_flow(i, op, cyc, inchunk)
        if flow is not None:
            self.stats["flow"] += 1
            return L + ["\t" + x for x in flow]
        body = self.mu.bodies.get(name)
        raw = i.raw
        if a in LIVE:
            off, size = LIVE[a]
            raw = raw[:off] + MARK[size].to_bytes(size, "big") + raw[off + size:]
        spec = specialise(body, op, a, raw) if body is not None else None
        if spec is not None and a in LIVE:
            off, size = LIVE[a]
            mark = "0x%Xu" % MARK[size]
            assert mark in spec, "live operand at %X not found" % a
            spec = spec.replace(mark, "ROM%d(0x%Xu)" % (8 * size, a + off))
        if spec is not None:
            spec = video_calls(spec, self.stats)
            self.stats["native"] += 1
            lines = spec.strip("\n").split("\n")
            has_ret = any(re.search(r"\breturn;", x) for x in lines)
            if has_ret:
                lines = [re.sub(r"\breturn;", "goto D_%06X;" % a, x) for x in lines]
            L.append("\t{")
            L += ["\t" + x for x in lines]
            L.append("\t}")
            if has_ret:
                L.append("D_%06X:" % a)
            L.append("\tUSE_CYCLES(%d);" % cyc)
            return L
        sp = self.emit_special(i, op)
        if sp is not None:
            self.stats["special"] += 1
            return L + ["\t" + x for x in sp]
        raise SystemExit("no native translation for %s at %06X" % (name, a))

    def fm_text(self, i):
        return ("%s %s" % (i.mnem, ", ".join(i.ops))).replace("*/", "* /").strip()

    def emit_chunk(self, addrs):
        start = addrs[0]
        inchunk = set(addrs)
        name = "rc_%06X" % start
        L = ["void %s(void)" % name, "{", "dispatch:", "\tswitch (REG_PC) {"]
        for a in addrs:
            L.append("\tcase 0x%X: goto L_%06X;" % (a, a))
        L.append("\tdefault: return;")
        L.append("\t}")
        for k, a in enumerate(addrs):
            i = self.tr.ins[a]
            L += self.emit_insn(i, inchunk)
            nxt = a + i.size
            last = k + 1 == len(addrs) or addrs[k + 1] != nxt
            if last and i.flow not in ("end", "jump", "ijump"):
                L.append("\tREG_PC = 0x%Xu; return;" % nxt)
        L.append("}")
        return name, L

    def run(self):
        os.makedirs(OUT, exist_ok=True)
        for fn in os.listdir(OUT):
            if fn.startswith("recomp_") and fn.endswith(".c"):
                os.remove(os.path.join(OUT, fn))
        chunks = self.chunks()
        index = []
        hdr = ["/* Generated by tools/recomp.py - do not edit. */", '#include "recomp_rt.h"', ""]
        for f in range(0, len(chunks), CHUNKS_PER_FILE):
            out = list(hdr)
            for ch in chunks[f:f + CHUNKS_PER_FILE]:
                name, L = self.emit_chunk(ch)
                end = ch[-1] + self.tr.ins[ch[-1]].size
                index.append((ch[0], end, name))
                out += L + [""]
            path = os.path.join(OUT, "recomp_%03d.c" % (f // CHUNKS_PER_FILE))
            with open(path, "w") as fh:
                fh.write("\n".join(out))
        # index + entry bitmap
        n = len(self.rom)
        bitmap = bytearray(n // 16)
        for a in self.tr.ins:
            bitmap[a >> 4] |= 1 << ((a >> 1) & 7)
        L = list(hdr)
        L.insert(2, "#include <string.h>")
        L.insert(3, '#include "slotcode.h"')
        for s, e, name in index:
            L.append("void %s(void);" % name)
        L.append("")
        L.append("static const recomp_chunk chunks[%d] = {" % len(index))
        for s, e, name in index:
            L.append("\t{0x%X, 0x%X, %s}," % (s, e, name))
        L.append("};")
        L.append("")
        code = bytearray(n // 8)       # bytes that are part of traced instructions
        live = set()
        for a, (off, size) in LIVE.items():
            live.update(range(a + off, a + off + size))
        for b in self.tr.owner:
            if b < n and b not in live:
                code[b >> 3] |= 1 << (b & 7)
        L.append("static const unsigned char code_bitmap[%d] = {" % len(code))
        for k in range(0, len(code), 32):
            L.append("\t" + ",".join(str(b) for b in code[k:k + 32]) + ",")
        L.append("};")
        L.append("")
        L.append("static const unsigned char entry_bitmap[%d] = {" % len(bitmap))
        for k in range(0, len(bitmap), 32):
            L.append("\t" + ",".join(str(b) for b in bitmap[k:k + 32]) + ",")
        L.append("};")
        L.append("""
/* chunks whose code a mod has changed: the interpreter runs those */
static unsigned char chunk_off[%d];

static int chunk_of(uint32_t pc)
{
    int lo = 0, hi = %d - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (pc < chunks[mid].start) hi = mid - 1;
        else if (pc >= chunks[mid].end) lo = mid + 1;
        else return mid;
    }
    return -1;
}

/* The HBlank vector points into RAM (0xFFC0B0), where the game keeps a `jmp abs.l`
 * whose target it changes (src/rom_00200.s: copied from dat_0046A at boot): that
 * instruction, as the interpreter would run it. */
static void rc_hblank_jmp(void)
{
    CHK(0xFFC0B0u)
    REG_PPC = 0xFFC0B0u; REG_IR = 0x4EF9;
    REG_PC = RD32(0xFFC0B2u);
    USE_CYCLES(%d);
}

recomp_fn recomp_find(uint32_t pc)
{
    if ((pc & 0xFFFFFF) == 0xFFC0B0 && md.ram[0xC0B0] == 0x4E && md.ram[0xC0B1] == 0xF9)
        return rc_hblank_jmp;
    if (pc >= %d)
        return slot_find(pc);           /* a level mod's own routines (slotcode.c) */
    if (!(entry_bitmap[pc >> 4] & (1 << ((pc >> 1) & 7))))
        return 0;
    int c = chunk_of(pc);
    return c >= 0 && !chunk_off[c] ? chunks[c].fn : 0;
}

/* a byte of code changed (data, and the operands read at run time, need nothing) */
int recomp_invalidate(uint32_t addr)
{
    if (addr >= %d || !(code_bitmap[addr >> 3] & (1 << (addr & 7))))
        return 0;
    int c = chunk_of(addr);
    if (c < 0) return 0;
    chunk_off[c] = 1;
    return 1;
}

void recomp_validate_all(void)
{
    slot_reset();
    memset(chunk_off, 0, sizeof(chunk_off));
}
""" % (len(index), len(index), self.mu.lookup(0x4EF9)[1], n, n))
        with open(os.path.join(OUT, "recomp_index.c"), "w") as fh:
            fh.write("\n".join(L))
        total = sum(self.stats[k] for k in ("native", "flow", "special", "call"))
        print("recompiled %d instructions in %d regions: %d specialised, %d control flow, %d status register/exception, %d via interpreter"
              % (total, len(index), self.stats["native"], self.stats["flow"], self.stats["special"], self.stats["call"]))
        print("video: %d fixed commands decoded, %d port accesses as calls, %d drawing routines in C"
              % (self.stats["video_fixed"], self.stats["video_port"], self.stats["draw"]))
        for k, v in sorted(self.callop_names.items(), key=lambda x: -x[1])[:15]:
            print("   interpreter handler %-28s x%d" % (k, v))


# --------------------------------------------------------------------------
# New code a level mod brings (Bonanza Maker's routines past the cartridge's end:
# tile loaders, start routines): built from a few instruction forms whose operands
# are all immediates or absolute long addresses. Each form is generated here like any
# recompiled instruction, with the operands left open; port/src/slotcode.c fills
# them in when the mod is loaded and runs the routine natively.
# --------------------------------------------------------------------------

SLOT_WORDS = [0x1A2B, 0x3C4D, 0x5E6F, 0x7A8B]     # operand words while generating


def slot_forms():
    forms = []
    for r in range(8):
        forms.append((0x41F9 | r << 9, 2))   # lea abs.l,An
        forms.append((0x303C | r << 9, 1))   # move.w #imm,Dn
        forms.append((0x203C | r << 9, 2))   # move.l #imm,Dn
        forms.append((0x3039 | r << 9, 2))   # move.w abs.l,Dn
        forms.append((0x2039 | r << 9, 2))   # move.l abs.l,Dn
    forms += [(0x33FC, 3), (0x23FC, 4),      # move.w/l #imm,abs.l
              (0x4279, 2), (0x42B9, 2)]      # clr.w/l abs.l
    return sorted(forms)


def gen_slot(mu):
    words = SLOT_WORDS
    L = ["/* Generated by tools/recomp.py - do not edit. */", '#include "recomp_rt.h"', '#include "slotcode.h"', ""]
    table = []
    for op, n in slot_forms():
        name, cyc = mu.lookup(op)
        raw = struct.pack(">H", op) + b"".join(struct.pack(">H", w) for w in words[:n])
        body = specialise(mu.bodies[name], op, 0x80000, raw)
        assert body is not None and "return" not in body, hex(op)
        for k in range(n - 1):
            body = body.replace("0x%04X%04Xu" % (words[k], words[k + 1]), "((uint)w[%d] << 16 | w[%d])" % (k, k + 1))
        for k in range(n):
            body = body.replace("0x%04Xu" % words[k], "(uint)w[%d]" % k)
        assert not any(("%04X" % x) in body for x in words), hex(op)
        L.append("static void so_%04X(const uint16_t *w)  /* %s */" % (op, name))
        L.append("{")
        L += [x for x in body.strip("\n").split("\n")]
        L.append("}")
        table.append((op, n, cyc))
    L.append("")
    L.append("const slot_form slot_forms[%d] = {" % len(table))
    for op, n, cyc in table:
        L.append("\t{0x%04X, %d, %d, so_%04X}," % (op, n, cyc, op))
    L.append("};")
    L.append("const int slot_nforms = %d;" % len(table))
    L.append("const int slot_cyc_jsr = %d, slot_cyc_jmp = %d, slot_cyc_rts = %d;"
             % (mu.lookup(0x4EB9)[1], mu.lookup(0x4EF9)[1], mu.lookup(0x4E75)[1]))
    with open(os.path.join(OUT, "recomp_slot.c"), "w") as fh:
        fh.write("\n".join(L) + "\n")
    print("slot code: %d instruction forms" % len(table))


def gen_cycles(mu):
    """The 68000 base cycles of every opcode, as Musashi's m68ki_build_opcode_table makes
    them (the last matching entry of its table wins): exception entry takes the cycles of
    the instruction it replaces off again (CYC_INSTRUCTION)."""
    cyc = bytearray(0x10000)
    for name, mask, match, c in mu.table:
        free = [b for b in range(16) if not (mask >> b) & 1]
        for k in range(1 << len(free)):
            op = match
            for i, b in enumerate(free):
                if (k >> i) & 1:
                    op |= 1 << b
            if (op & mask) == match:
                cyc[op] = c
    L = ["/* Generated by tools/recomp.py - do not edit. */",
         "/* 68000 base cycles per opcode (the table Musashi builds at start-up) */",
         "const unsigned char m68k_rt_cycles[0x10000] = {"]
    for k in range(0, 0x10000, 64):
        L.append(",".join(str(x) for x in cyc[k:k + 64]) + ",")
    L.append("};")
    with open(os.path.join(OUT, "recomp_cycles.c"), "w") as fh:
        fh.write("\n".join(L) + "\n")


def main():
    rom_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "baserom.md")
    rom = open(rom_path, "rb").read()
    import hashlib
    if hashlib.sha1(rom).hexdigest() != disasm.SHA1:
        sys.exit("baserom sha1 mismatch")
    tr = disasm.Tracer(rom)
    tr.run()
    fm = disasm.Formatter(rom, tr)
    g = Gen(rom, tr)
    g.run()
    gen_slot(g.mu)
    gen_cycles(g.mu)


if __name__ == "__main__":
    main()
