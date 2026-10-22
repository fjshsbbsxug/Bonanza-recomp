/* The game's logic in C: time, calls to the game's routines, checks (rt.h). */
#include "rt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint64_t logic_stat_c;
int logic_off = -1;

/* ---- time */

static int cyc;            /* the routine's time so far */
static uint32_t pc_at;     /* the next instruction whose time is not counted yet */

static int instr_time(uint32_t a) { return game.m68k_cost[a >> 1]; }

void begin(uint32_t a) { cyc = 0; pc_at = a; }

void jumped_to(uint32_t a) { pc_at = a; }

void cyc_back(int n) { cyc -= n; }

uint32_t pc_now(void) { return pc_at; }

void ran_to(uint32_t b)
{
    for (; pc_at < b; pc_at += 2)
        cyc += instr_time(pc_at);
}

void branched(uint32_t a, uint32_t b)
{
    ran_to(a);
    uint16_t op = rom16(a);
    int t = instr_time(a);                 /* (the table: as when not taken) */
    if ((op & 0xF0F8) == 0x50C8) t -= 4;   /* dbcc looping: 10 */
    else if ((op & 0xF000) == 0x6000 && (op & 0x0F00) > 0x0100)
        t += (op & 0xFF) ? 2 : -2;          /* bcc taken: 10 */
    cyc += t;
    pc_at = b;
}

int done(void)
{
    USE_CYCLES(cyc);
    cyc = 0;
    return 1;
}

int jump_to(uint32_t addr)
{
    USE_CYCLES(cyc);
    cyc = 0;
    REG_PC = addr;
    return 2;
}

void call(uint32_t addr, uint32_t jsr_at)
{
    uint16_t op = rom16(jsr_at);
    uint32_t next;
    if ((op & 0xFF00) == 0x6100) {
        next = jsr_at + ((op & 0xFF) ? 2 : 4);                                   /* bsr.s / bsr.w */
    } else {
        int mode = (op >> 3) & 7, reg = op & 7;                                  /* jsr <ea> */
        next = jsr_at + (mode == 2 ? 2 : mode == 7 && reg == 1 ? 6 : 4);
    }
    ran_to(next);                          /* (the jsr's own time) */
    int saved_pc = (int)pc_at;
    USE_CYCLES(cyc);                       /* the time so far, before the routine's */
    cyc = 0;
    cpu_call(addr, next);
    pc_at = (uint32_t)saved_pc;
}

/* a port hook at pc, within a routine in C (its time counted up to pc first): when it
 * goes on elsewhere (2), the game's code runs from there until it comes back to ret */
int hook_at(uint32_t pc, uint32_t ret)
{
    if (!game_hooks) return 0;
    ran_to(pc);
    USE_CYCLES(cyc);
    cyc = 0;
    uint32_t sp = REG_A[7];
    int h = game_hook(pc);
    if (h != 2) return h;
    cpu_run_to(ret, sp);
    pc_at = ret;
    return 2;
}

uint32_t jsr_target(uint32_t at)
{
    uint16_t op = rom16(at);
    if (op == 0x4EB9 || op == 0x4EF9) return rom32(at + 2);            /* jsr / jmp abs.l */
    if (op == 0x4EB8) return (uint32_t)(int32_t)(int16_t)rom16(at + 2);  /* jsr abs.w */
    if (op == 0x4EBA) return at + 2 + (int16_t)rom16(at + 2);           /* jsr (d16,pc) */
    if (op == 0x6100) return at + 2 + (int16_t)rom16(at + 2);           /* bsr.w */
    if ((op & 0xFF00) == 0x6100) return at + 2 + (int8_t)(op & 0xFF);  /* bsr.s */
    fprintf(stderr, "logic: no jsr at %06X\n", at);
    abort();
}

int player_jump(int p, uint32_t a, uint32_t jmp)
{
    uint32_t at = P(p, jmp), to = jsr_target(at);
    if (!p && to != a) {
        fprintf(stderr, "logic: the jmp at %06X goes to %06X, not %06X\n", jmp, to, a);
        abort();
    }
    ran_to(at + 6);
    return jump_to(to);
}

uint32_t player_imm32(int p, uint32_t a, uint32_t instr)
{
    uint32_t v = rom32(P(p, instr) + 2);
    if (!p && v != a) {
        fprintf(stderr, "logic: the long at %06X is %06X, not %06X\n", instr, v, a);
        abort();
    }
    return v;
}

void player_call(int p, uint32_t a, uint32_t jsr)
{
    uint32_t at = P(p, jsr), to = jsr_target(at);
    if (!p && to != a) {
        fprintf(stderr, "logic: the jsr at %06X calls %06X, not %06X\n", jsr, to, a);
        abort();
    }
    call(to, at);
}

uint32_t run_part(uint32_t addr, const uint32_t *stops, int n)
{
    USE_CYCLES(cyc);
    cyc = 0;
    uint32_t at = cpu_run_part(addr, stops, n);
    pc_at = at;
    return at;
}

/* ---- the players' code */

#include "player2_map.h"

static uint32_t look_up(const uint32_t (*map)[2], int n, uint32_t a, const char *what)
{
    int lo_ = 0, hi = n - 1;
    while (lo_ <= hi) {
        int m = (lo_ + hi) / 2;
        if (map[m][0] == a) return map[m][1];
        if (map[m][0] < a) lo_ = m + 1; else hi = m - 1;
    }
    fprintf(stderr, "logic: no player 2 %s for %06X (tools/player_map.py)\n", what, a);
    abort();
}

uint32_t player2_at(uint32_t a)
{
    return look_up(player2_places, (int)(sizeof(player2_places) / sizeof(player2_places[0])), a, "place");
}

uint32_t player2_ram(uint32_t a)
{
    return look_up(player2_words, (int)(sizeof(player2_words) / sizeof(player2_words[0])), a, "word");
}

uint32_t player2_table(uint32_t a)
{
    return look_up(player2_tables, (int)(sizeof(player2_tables) / sizeof(player2_tables[0])), a, "table");
}

/* ---- checks */

void scramble(uint32_t regs)
{
    static int on = -1;
    static uint32_t rng = 0x9E3779B9u;
    if (on < 0) on = getenv("LOGIC_SCRAMBLE") != 0;
    if (!on) return;
    for (int r = 0; r < 15; r++)
        if (regs & (1u << r)) {
            rng = rng * 1664525u + 1013904223u;
            REG_DA[r] = rng;
        }
    if (regs & SCRAMBLE_FLAGS) {
        rng = rng * 1664525u + 1013904223u;
        FLAG_X = rng & 0x100; FLAG_N = rng & 0x80; FLAG_Z = rng & 0x10000; FLAG_V = rng & 0x80; FLAG_C = rng & 0x100;
    }
}

static struct { const char *name; uint64_t n; } calls[1024];

void count_named(const char *name)
{
    for (int i = 0; i < 1024; i++)
        if (calls[i].name == name || !calls[i].name) { calls[i].name = name; calls[i].n++; break; }
}

int start_named(const char *name)
{
    if (logic_off < 0) logic_off = getenv("BONANZA_TRANSLATED") != 0;
    static int trace = -1;                             /* LOGIC_TRACE=1: each part as it starts, */
    if (trace < 0) trace = getenv("LOGIC_TRACE") != 0; /* with the frame's time left (both ways) */
    if (trace) fprintf(stderr, "logic %s %d\n", name, (int)GET_CYCLES());
    if (logic_off) return 0;
    static const char *skip = (const char *)-1;       /* LOGIC_SKIP=a,b: these as the game's code */
    if (skip == (const char *)-1) skip = getenv("LOGIC_SKIP");
    if (skip) {
        size_t n = strlen(name);
        for (const char *s = strstr(skip, name); s; s = strstr(s + 1, name))
            if ((s == skip || s[-1] == ',') && (s[n] == ',' || !s[n])) return 0;
    }
    logic_stat_c++;
    count_named(name);
    cyc = 0;
    return 1;
}

/* ---- long routines: the machine kept, and put back when one runs past the frame */

#include "../vdp.h"
#include <stddef.h>

static struct {
    m68ki_cpu_core cpu;
    int cycles, irq_dirty;
    uint32_t call_ret;
    uint8_t md[sizeof(md_t) - offsetof(md_t, ram)];
    uint8_t vdp[offsetof(vdp_t, fbmd)];
} kept;
static int long_depth;
uint64_t logic_long_back;                  /* (LOGICSTATS: how often one was put back) */

int start_long_named(const char *name, uint32_t addr)
{
    static uint32_t bypass_at;
    if (bypass_at == addr) { bypass_at = 0; return 0; }
    static int ref = -1;
    if (ref < 0) ref = getenv("LOGIC_LONG_REF") != 0;
    if (ref && !logic_off) {               /* (the game's code for it, in one go: from addr */
        uint32_t sp = REG_A[7];            /*  to its rts) */
        uint32_t ret = m68ki_read_32(sp);
        bypass_at = addr;
        REG_PC = addr;
        cpu_run_to(ret, sp + 4);
        return 2;                          /* (done: on at REG_PC, its return) */
    }
    if (!start_named(name)) return 0;
    if (long_depth++) return 1;            /* (within another: that one keeps the machine) */
    kept.cpu = m68ki_cpu;
    kept.cycles = GET_CYCLES();
    kept.irq_dirty = cpu_irq_dirty;
    kept.call_ret = cpu_call_ret;
    memcpy(kept.md, (uint8_t *)&md + offsetof(md_t, ram), sizeof kept.md);
    memcpy(kept.vdp, &vdp, sizeof kept.vdp);
    return 1;
}

/* (the time taken: back to the start when it ran past the frame's end; 1 when not) */
static int long_end(void)
{
    USE_CYCLES(cyc);
    cyc = 0;
    if (--long_depth) return 1;
    if (GET_CYCLES() > 0) return 1;
    static int keep = -1;                  /* LOGIC_LONG_KEEP=1 (tests): never put back, so */
    if (keep < 0) keep = getenv("LOGIC_LONG_KEEP") != 0;   /* the C runs: only the frame */
    if (keep) return 1;                    /* it ran past differs then */
    m68ki_cpu = kept.cpu;
    SET_CYCLES(kept.cycles);
    cpu_irq_dirty = kept.irq_dirty;
    cpu_call_ret = kept.call_ret;
    memcpy((uint8_t *)&md + offsetof(md_t, ram), kept.md, sizeof kept.md);
    memcpy(&vdp, kept.vdp, sizeof kept.vdp);
    logic_long_back++;
    return 0;
}
int done_long(void) { return long_end() ? 1 : 0; }
int jump_long(uint32_t addr)
{
    if (!long_end()) return 0;
    REG_PC = addr;
    return 2;
}

void logic_report(FILE *f)
{
    fprintf(f, "  (long routines put back: %llu)\n", (unsigned long long)logic_long_back);
    for (int i = 0; i < 1024 && calls[i].name; i++)
        fprintf(f, "  %-28s %llu\n", calls[i].name, (unsigned long long)calls[i].n);
}
