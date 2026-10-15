#!/usr/bin/env python3
"""Static recompiler for the Z80 sound driver of Bonanza Bros. -> C (port/gen/z80_driver.c).

    python3 tools/z80recomp.py [baserom.md]

The 68000 copies one program to the Z80 at boot (sub_02C76: 0x118B bytes from ROM
0x33000 to Z80 0x0000, never changed afterwards): a Sega SMPS-style driver. Its code is
traced from the reset and interrupt entries, the RST vectors it calls and its three jump
tables, and every traced instruction becomes C:

  * the instruction's own C is taken from the Z80 core the port uses
    (port/ext/z80/z80.c): the case of its opcode in exec_opcode / _ed / _ddfd, or the
    whole CB / DD CB decoder with the opcode fixed, so results, flags, cycles and the R
    register are those of that core;
  * its operand bytes are built in (read in the same order the core reads them);
  * after each instruction the core's interrupt check runs and the line's cycle
    count is updated, exactly as port/src/md.c's step loop does, and the code gives
    control back at the same points (the 228-cycle line budget).

Calls and returns use the real Z80 stack in Z80 RAM (the driver pushes return addresses
and pops frames itself), so every jump goes through a dispatch switch on PC.
"""
import hashlib
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
Z80C = os.path.join(ROOT, "port", "ext", "z80", "z80.c")
OUT = os.path.join(ROOT, "port", "gen", "z80_driver.c")
SHA1 = "f7313ecaf7143873597e3a6ebf58d542d06c3ef3"

DRIVER_ROM, DRIVER_LEN = 0x33000, 0x118B
# the standard Sega boot stub the 68000 copies first (sub_0021E, ROM 0x2E2): it clears Z80
# RAM, writes `jp (hl)` (0xE9) over its own first byte and spins on it until the driver
# is loaded; before and after that write it is two programs
STUB_ROM, STUB_LEN = 0x2E2, 0x26
# entries the tracer cannot see: the reset and interrupt vectors, the three jump tables
# (commands E0-E5, coordination flags E0-FF, meta flags FF xx) and the return addresses
# the driver pushes itself before jumping into a flag handler
ROOTS = [0x0000, 0x0038, 0x0B3B, 0x0D45]
TABLES = [(0x0602, 6), (0x0D50, 32), (0x0D90, 8)]


# --------------------------------------------------------------------------
# the Z80 core's opcode bodies
# --------------------------------------------------------------------------

def function_body(src, name):
    m = re.search(r"^void %s\([^)]*\)\s*\{" % name, src, re.M)
    i = m.end()
    depth = 1
    j = i
    while depth:
        c = src[j]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
        j += 1
    return src[i:j - 1]


def strip_comments(s):
    s = re.sub(r"/\*.*?\*/", "", s, flags=re.S)
    return re.sub(r"//[^\n]*", "", s)


def switch_cases(body):
    """case bodies of the function's `switch (opcode)`: {opcode: text}, default text"""
    body = strip_comments(body)
    i = body.index("switch (opcode) {") + len("switch (opcode) {")
    depth, j = 0, i
    segs = []            # (labels, start)
    cur_labels, start = [], None
    while True:
        if depth == 0:
            m = re.match(r"\s*(case\s+(0x[0-9A-Fa-f]+)\s*:|default\s*:)", body[j:])
            if m:
                if start is not None and body[start:j].strip():
                    segs.append((cur_labels, body[start:j]))
                    cur_labels = []
                cur_labels.append(int(m.group(2), 16) if m.group(2) else "default")
                j += m.end()
                start = j
                continue
        c = body[j]
        if c == "{":
            depth += 1
        elif c == "}":
            if depth == 0:
                segs.append((cur_labels, body[start:j]))
                break
            depth -= 1
        j += 1
    cases = {}
    for labels, text in segs:
        t = text.strip()
        # the case ends with a top-level break
        if not t.endswith("break;"):
            raise SystemExit("case %s does not end with break" % labels)
        t = t[: -len("break;")].rstrip()
        for lab in labels:
            cases[lab] = t
    return cases


class Core:
    def __init__(self):
        src = open(Z80C).read()
        self.c00 = switch_cases(function_body(src, "exec_opcode"))
        self.ced = switch_cases(function_body(src, "exec_opcode_ed"))
        self.cdd = switch_cases(function_body(src, "exec_opcode_ddfd"))
        self.cb = strip_comments(function_body(src, "exec_opcode_cb"))
        self.dcb = strip_comments(function_body(src, "exec_opcode_dcb"))
        for b in (self.cb, self.dcb):
            assert "return" not in b and "nextb" not in b and "nextw" not in b


# --------------------------------------------------------------------------
# instruction lengths and flow (from the same bodies: the operand reads they make)
# --------------------------------------------------------------------------

def operand_bytes(text):
    n = len(re.findall(r"\bnextb\(z\)", text)) + 2 * len(re.findall(r"\bnextw\(z\)", text))
    n += len(re.findall(r"\bIZD\b", text))
    n += 2 * len(re.findall(r"\bcond_(?:jump|call)\(", text)) + len(re.findall(r"\bcond_jr\(", text))
    return n


class Insn:
    def __init__(self, addr, raw, kind, text):
        self.addr, self.raw, self.kind, self.text = addr, raw, kind, text
        self.size = len(raw)


def decode(core, mem, a):
    op = mem[a]
    if op == 0xCB:
        return Insn(a, mem[a:a + 2], "cb", None)
    if op == 0xED:
        sub = mem[a + 1]
        t = core.ced.get(sub)
        if t is None:
            return None
        return Insn(a, mem[a:a + 2 + operand_bytes(t)], "ed", t)
    if op in (0xDD, 0xFD):
        sub = mem[a + 1]
        if sub == 0xCB:
            return Insn(a, mem[a:a + 4], "ddcb", None)
        t = core.cdd.get(sub)
        if t is not None:
            return Insn(a, mem[a:a + 2 + operand_bytes(t)], "dd", t)
        t = core.c00.get(sub)
        if t is None or sub in (0xCB, 0xED, 0xDD, 0xFD):
            return None
        return Insn(a, mem[a:a + 2 + operand_bytes(t)], "dd00", t)
    t = core.c00.get(op)
    if t is None:
        return None
    return Insn(a, mem[a:a + 1 + operand_bytes(t)], "00", t)


def w16(b, k):
    return b[k] | b[k + 1] << 8


def flow(i):
    """(targets, falls through)"""
    op = i.raw[0]
    if i.kind == "00":
        if op == 0xC3:
            return [w16(i.raw, 1)], False
        if op in (0xC2, 0xCA, 0xD2, 0xDA, 0xE2, 0xEA, 0xF2, 0xFA, 0xCD, 0xC4, 0xCC, 0xD4, 0xDC, 0xE4, 0xEC, 0xF4, 0xFC):
            return [w16(i.raw, 1)], True
        if op == 0x18:
            return [(i.addr + 2 + (i.raw[1] ^ 0x80) - 0x80) & 0xFFFF], False
        if op in (0x10, 0x20, 0x28, 0x30, 0x38):
            return [(i.addr + 2 + (i.raw[1] ^ 0x80) - 0x80) & 0xFFFF], True
        if op == 0xC9 or op == 0xE9:
            return [], False
        if (op & 0xC7) == 0xC7:
            return [op & 0x38], True
        return [], True
    if i.kind == "ed" and i.raw[1] in (0x45, 0x4D, 0x55, 0x5D, 0x65, 0x6D, 0x75, 0x7D):
        return [], False
    if i.kind == "dd" and i.raw[1] == 0xE9:
        return [], False
    return [], True


def trace(core, mem, length, roots, tables):
    ins = {}
    todo = list(roots)
    for t, n in tables:
        todo += [w16(mem, t + 2 * k) for k in range(n)]
    owner = {}
    while todo:
        a = todo.pop()
        while a < length and a not in ins:
            if a in owner:
                raise SystemExit("jump into the middle of the instruction at %04X (%04X)" % (owner[a], a))
            i = decode(core, mem, a)
            if i is None:
                raise SystemExit("unknown instruction at %04X: %s" % (a, mem[a:a + 4].hex()))
            ins[a] = i
            for k in range(1, i.size):
                owner[a + k] = a
            targets, falls = flow(i)
            todo += [t for t in targets if t < length]
            if not falls:
                break
            a += i.size
    return ins


# --------------------------------------------------------------------------
# C
# --------------------------------------------------------------------------

def operands(text):
    """the instruction's C with its operand reads taken from its own bytes (same order)"""
    t = re.sub(r"\bIZD\b", "displace(z, *iz, nb(z, &o))", text)
    t = re.sub(r"\bnextb\(z\)", "nb(z, &o)", t)
    t = re.sub(r"\bnextw\(z\)", "nw(z, &o)", t)
    t = re.sub(r"\bcond_(jump|call|jr)\(z, ", r"cond_\1_o(z, &o, ", t)
    return t


def emit(core, ins):
    L = []
    for a in sorted(ins):
        i = ins[a]
        nxt = a + i.size
        op = i.raw[0]
        head = "L_%04X: if (*lc >= limit) return 1;" % a
        ob = ", ".join("0x%02X" % b for b in i.raw[1:]) or "0"
        body = ["{ const uint8_t ob[] = {%s}; opst o = {ob, 0}; unsigned long c0 = z->cyc; (void)o;" % ob]
        if i.kind == "00":
            body.append("  { const uint8_t opcode = 0x%02X; z->pc = 0x%04X; z->cyc += cyc_00[opcode]; inc_r(z); %s }"
                        % (op, (a + 1) & 0xFFFF, operands(i.text)))
        elif i.kind == "cb":
            body.append("  { z->pc = 0x%04X; z->cyc += cyc_00[0xCB]; inc_r(z); nb(z, &o);" % ((a + 1) & 0xFFFF))
            body.append("    { const uint8_t opcode = 0x%02X; %s } }" % (i.raw[1], core.cb))
        elif i.kind == "ed":
            body.append("  { z->pc = 0x%04X; z->cyc += cyc_00[0xED]; inc_r(z); nb(z, &o);" % ((a + 1) & 0xFFFF))
            body.append("    { const uint8_t opcode = 0x%02X; z->cyc += cyc_ed[opcode]; inc_r(z); %s } }"
                        % (i.raw[1], operands(i.text)))
        else:
            iz = "&z->ix" if op == 0xDD else "&z->iy"
            body.append("  { uint16_t* const iz = %s; z->pc = 0x%04X; z->cyc += cyc_00[0x%02X]; inc_r(z); nb(z, &o);"
                        % (iz, (a + 1) & 0xFFFF, op))
            if i.kind == "ddcb":
                body.append("    { const uint8_t opcode = 0xCB; z->cyc += cyc_ddfd[opcode]; inc_r(z);"
                            " uint16_t addr = displace(z, *iz, nb(z, &o)); nb(z, &o);")
                body.append("      { const uint8_t opcode = 0x%02X; %s } } }" % (i.raw[3], core.dcb))
            elif i.kind == "dd":
                body.append("    { const uint8_t opcode = 0x%02X; z->cyc += cyc_ddfd[opcode]; inc_r(z); %s } }"
                            % (i.raw[1], operands(i.text)))
            else:   # any other DD/FD opcode: the plain one, R counted once
                body.append("    { const uint8_t opcode = 0x%02X; z->cyc += cyc_ddfd[opcode]; inc_r(z);" % i.raw[1])
                body.append("      { z->cyc += cyc_00[opcode]; inc_r(z); %s }" % operands(i.text))
                body.append("      z->r = (z->r & 0x80) | ((z->r - 1) & 0x7f); } }")
        body.append("  process_interrupts(z);")
        body.append("  *lc += (int)(z->cyc - c0); z80r_insns++; }")
        body.append("if (z->pc != 0x%04X) goto dispatch;" % (nxt & 0xFFFF))
        L.append(head)
        L += ["\t" + x for x in body]
        targets, falls = flow(i)
        if falls and nxt not in ins:
            L.append("\tgoto dispatch;")
    return L


PRELUDE = r'''/* Generated by tools/z80recomp.py - do not edit. */
/* The Z80's operations from the core in port/ext/z80/z80.c (MIT licence), copied here
 * without its interpreter: the instructions below are its opcode cases, fixed for each
 * instruction of the Z80's programs. */
#include <stdio.h>
#include <string.h>
#include "z80recomp.h"
'''

POSTLUDE = r'''
#define GET_BIT(n, val) (((val) >> (n)) & 1)
#define IZH (*iz >> 8)
#define IZL (*iz & 0xFF)

unsigned long z80r_insns;
int z80r_code_changed;

/* the instruction's operand bytes, read in the order the core reads them */
typedef struct { const uint8_t *b; int k; } opst;
static inline uint8_t nb(z80* const z, opst *o) { z->pc++; return o->b[o->k++]; }
static inline uint16_t nw(z80* const z, opst *o) { z->pc += 2; o->k += 2; return o->b[o->k - 2] | o->b[o->k - 1] << 8; }
static inline void cond_jump_o(z80* const z, opst *o, bool condition) {
  const uint16_t addr = nw(z, o);
  if (condition) jump(z, addr);
  z->mem_ptr = addr;
}
static inline void cond_call_o(z80* const z, opst *o, bool condition) {
  const uint16_t addr = nw(z, o);
  if (condition) { call(z, addr); z->cyc += 7; }
  z->mem_ptr = addr;
}
static inline void cond_jr_o(z80* const z, opst *o, bool condition) {
  const int8_t b = nb(z, o);
  if (condition) { jr(z, b); z->cyc += 5; }
}
'''


def core_helpers():
    """z80.c up to its interface, without the opcode interpreter: the operations, the
    cycle tables and the interrupt entry (mode 0, executing an opcode from the bus, is
    not used: the programs run in mode 1); then z80_init / z80_gen_int as z80r_*"""
    src = open(Z80C).read()
    head = src[:src.index("// MARK: interface")]
    head = re.sub(r"static void exec_opcode\w*\([^;]*\);\n", "", head)
    head = re.sub(r"static void exec_opcode_dcb\(\s*z80\* const z, const uint8_t opcode, const uint16_t addr\);\n", "", head)
    assert "exec_opcode(z, z->int_data);" in head
    head = head.replace("exec_opcode(z, z->int_data);", "fprintf(stderr, \"z80: interrupt mode 0 is not supported\\n\");")
    assert "exec_opcode" not in head, "interpreter left in the helpers"
    head = head.replace('#include "z80.h"', "")
    iface = src[src.index("// MARK: interface"):]
    init = re.search(r"void z80_init\(z80\* const z\) \{.*?\n\}\n", iface, re.S).group(0)
    gen = re.search(r"void z80_gen_int\(z80\* const z, uint8_t data\) \{.*?\n\}\n", iface, re.S).group(0)
    return head + "\n" + init.replace("z80_init", "z80r_init") + "\n" + gen.replace("z80_gen_int", "z80r_gen_int")



def emit_program(core, name, mem, length, roots, tables):
    ins = trace(core, mem, length, roots, tables)
    L = ["/* run from z->pc while *lc < limit: 1 the line is done, 0 PC is outside this code,",
         " * 2 the code bytes changed (the caller looks again at what is in Z80 RAM) */",
         "static int run_%s(z80* const z, int *lc, int limit)" % name, "{", "dispatch:",
         "\tif (z80r_code_changed) return 2;", "\tif (z->halted) return 0;", "\tswitch (z->pc) {"]
    for a in sorted(ins):
        L.append("\tcase 0x%04X: goto L_%04X;" % (a, a))
    L.append("\tdefault: return 0;")
    L.append("\t}")
    L += emit(core, ins)
    L.append("\treturn 0;")
    L.append("}")
    print("z80 %s: %d instructions, %d code bytes" % (name, len(ins), sum(i.size for i in ins.values())))
    return L


def image(L, name, data):
    L.append("static const uint8_t %s[%d] = {" % (name, len(data)))
    for k in range(0, len(data), 24):
        L.append("\t" + ",".join("%d" % b for b in data[k:k + 24]) + ",")
    L.append("};")


def main():
    global OUT
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if "--out" in sys.argv:                 # the directory the game's code goes to
        d = sys.argv[sys.argv.index("--out") + 1]
        args = [a for a in args if a != d]
        OUT = os.path.join(d, "z80_driver.c")
    rom_path = args[0] if args else os.path.join(ROOT, "baserom.md")
    rom = open(rom_path, "rb").read()
    if hashlib.sha1(rom).hexdigest() != SHA1:
        sys.exit("baserom sha1 mismatch")
    core = Core()
    drv = bytearray(0x10000)
    drv[:DRIVER_LEN] = rom[DRIVER_ROM:DRIVER_ROM + DRIVER_LEN]
    stub = bytearray(0x10000)
    stub[:STUB_LEN] = rom[STUB_ROM:STUB_ROM + STUB_LEN]
    spin = bytearray(stub)
    spin[0] = 0xE9
    L = [PRELUDE, core_helpers(), POSTLUDE]
    image(L, "img_driver", drv[:DRIVER_LEN])
    image(L, "img_stub", stub[:STUB_LEN])
    L += emit_program(core, "driver", drv, DRIVER_LEN, ROOTS, TABLES)
    L += emit_program(core, "stub", stub, STUB_LEN, [0], [])
    L += emit_program(core, "spin", spin, 1, [0], [])
    L.append('''
/* which program is in Z80 RAM: the driver, the boot stub before or after it wrote over
 * its first byte, or none of them (-1: the interpreter runs it) */
int z80r_program(const uint8_t *zram)
{
    if (!memcmp(zram, img_driver, sizeof(img_driver))) return 0;
    if (!memcmp(zram + 1, img_stub + 1, sizeof(img_stub) - 1)) {
        if (zram[0] == img_stub[0]) return 1;
        if (zram[0] == 0xE9) return 2;
    }
    return -1;
}

int z80r_run(int program, z80* const z, int *lc, int limit)
{
    z80r_code_changed = 0;
    switch (program) {
    case 0: return run_driver(z, lc, limit);
    case 1: return run_stub(z, lc, limit);
    case 2: return run_spin(z, lc, limit);
    }
    return 0;
}''')
    with open(OUT, "w") as fh:
        fh.write("\n".join(L) + "\n")


if __name__ == "__main__":
    main()
