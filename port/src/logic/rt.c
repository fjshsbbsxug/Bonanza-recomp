/* The game's logic in C: time, calls to the game's routines, checks (rt.h). */
#include "rt.h"
#include <stdio.h>
#include <stdlib.h>

uint64_t logic_stat_c;
int logic_off = -1;

/* ---- time */

static int cyc;            /* the routine's time so far */
static uint32_t pc_at;     /* the next instruction whose time is not counted yet */

static int instr_time(uint32_t a) { return game.m68k_cost[a >> 1]; }

void begin(uint32_t a) { cyc = 0; pc_at = a; }

void jumped_to(uint32_t a) { pc_at = a; }

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

void call(uint32_t addr, uint32_t jsr_at)
{
    uint16_t op = rom16(jsr_at);
    uint32_t next = jsr_at + ((op & 0xFF00) == 0x6100 ? ((op & 0xFF) ? 2 : 4)    /* bsr */
                              : (op & 0x3F) == 0x39 ? 6 : 4);                    /* jsr abs.l / abs.w, (pc) */
    ran_to(next);                          /* (the jsr's own time) */
    int saved_pc = (int)pc_at;
    USE_CYCLES(cyc);                       /* the time so far, before the routine's */
    cyc = 0;
    cpu_call(addr, next);
    pc_at = (uint32_t)saved_pc;
}

uint32_t run_part(uint32_t addr, const uint32_t *stops, int n)
{
    USE_CYCLES(cyc);
    cyc = 0;
    uint32_t at = cpu_run_part(addr, stops, n);
    pc_at = at;
    return at;
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

static struct { const char *name; uint64_t n; } calls[256];

void count_named(const char *name)
{
    for (int i = 0; i < 256; i++)
        if (calls[i].name == name || !calls[i].name) { calls[i].name = name; calls[i].n++; break; }
}

int start_named(const char *name)
{
    if (logic_off < 0) logic_off = getenv("BONANZA_TRANSLATED") != 0;
    if (logic_off) return 0;
    logic_stat_c++;
    count_named(name);
    cyc = 0;
    return 1;
}

void logic_report(FILE *f)
{
    for (int i = 0; i < 256 && calls[i].name; i++)
        fprintf(f, "  %-28s %llu\n", calls[i].name, (unsigned long long)calls[i].n);
}
