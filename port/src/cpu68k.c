/* 68000 execution: all of it is the recompiled game code (port/gen); there is no
 * interpreter. The CPU's state, reset and interrupt entry are in m68k_rt.c. */
#include "md.h"
#include "game.h"
#include "vdp.h"
#include "recomp.h"
#include "gamemods.h"
#include "extrastages.h"
#include "../ext/musashi/m68k.h"
#include "../ext/musashi/m68kcpu.h"
#include "m68k_rt.h"

uint64_t cpu_stat_recomp_insns, cpu_stat_no_code;

/* ---- Musashi memory interface ---- */

unsigned int m68k_read_memory_8(unsigned int a) { return md_read8(a); }

unsigned int m68k_read_memory_16(unsigned int a)
{
    a &= 0xFFFFFF;
    if (a < 0x400000)
        return (md.rom[a] << 8) | md.rom[a + 1];
    if (a >= 0xE00000)
        return (md.ram[a & 0xFFFF] << 8) | md.ram[(a + 1) & 0xFFFF];
    return md_read16(a);
}

unsigned int m68k_read_memory_32(unsigned int a)
{
    return (m68k_read_memory_16(a) << 16) | m68k_read_memory_16(a + 2);
}

void m68k_write_memory_8(unsigned int a, unsigned int v) { md_write8(a, v); }

void m68k_write_memory_16(unsigned int a, unsigned int v)
{
    a &= 0xFFFFFF;
    if (a >= 0xE00000) {
        md.ram[a & 0xFFFF] = (uint8_t)(v >> 8);
        md.ram[(a + 1) & 0xFFFF] = (uint8_t)v;
        return;
    }
    md_write16(a, v);
}

void m68k_write_memory_32(unsigned int a, unsigned int v)
{
    m68k_write_memory_16(a, v >> 16);
    m68k_write_memory_16(a + 2, v & 0xFFFF);
}


#include <stdio.h>
#include <stdlib.h>
int md_int_ack(int level)
{
    static int ilog = -1;
    if (ilog < 0) ilog = getenv("INTLOG") != NULL;
    if (ilog) fprintf(stderr, "int %d line %d frame %u pc %06x sr %04x\n", level, md.line, md.frame, REG_PC, m68ki_get_sr());
    vdp_ack_irq(level);
    return M68K_INT_ACK_AUTOVECTOR;
}

/* ---- control ---- */

void cpu_init(void)
{
    m68k_rt_init(md_int_ack);
}

void cpu_reset(void)
{
    m68k_rt_reset();
}

/* Interrupt line changes are applied between instructions only. */
int cpu_irq_dirty;
static int irq_level;

void cpu_set_irq(int level)
{
    irq_level = level;
    cpu_irq_dirty = 1;
}

void cpu_apply_irq(void)
{
    cpu_irq_dirty = 0;
    m68k_rt_set_irq(irq_level);
    m68k_rt_check_interrupts();
}

uint32_t cpu_get_pc(void) { return REG_PC; }
uint32_t cpu_get_sp(void) { return REG_SP; }

/* PC where there is no native code (never, for the game, its Bonanza Maker mods and the
 * Lost Levels): the 68000's exception for an instruction it cannot run; the game's own
 * error handler takes it from there. */
static void no_code(void)
{
    if (cpu_stat_no_code++ < 8)
        fprintf(stderr, "68000: no native code at %06X\n", REG_PC);
    m68k_rt_no_code();
}

/* ---- the frame driver (md.c): the game's code run as functions, not time-sliced ----
 * The game sits in its reset code's loop (0x45C) between frames; everything it does
 * runs from its frame routine (the vertical blank, 0x98E) and its band routines (the
 * line interrupts, through 0xFFC0B0). */
static int idle_pc(uint32_t pc) { return pc >= 0x45C && pc < 0x46A; }

/* the one place the frame routine waits for the picture: the play screen's routine
 * (0x879C) waits for the top band's routine to set 0xFF17B6 - its logic goes on while
 * the next picture is drawn, below its top band */
static int at_wait(void) { return REG_PC >= 0x879C && REG_PC < 0x87A4 && !CPU_STOPPED; }
static int wait_flag(void) { return md.ram[0x17B6] | md.ram[0x17B7]; }
int cpu_waiting(void) { return at_wait() && !wait_flag(); }
int cpu_wait_over(void) { return at_wait() && wait_flag(); }

static void step(void)
{
    if (cpu_irq_dirty)
        cpu_apply_irq();
    if (CPU_STOPPED)
        return;
    recomp_fn fn = game.recomp_find(REG_PC);
    if (fn)
        fn();
    else
        no_code();
}

/* run until the code is back in the idle loop, or up to the given time (a routine that
 * takes longer, such as a stage being set up, goes on next time): 1 when idle */
int cpu_run_until_idle(int cycles)
{
    SET_CYCLES(cycles);
    if (cpu_irq_dirty)
        cpu_apply_irq();
    while (GET_CYCLES() > 0 && !CPU_STOPPED) {
        if ((idle_pc(REG_PC) || cpu_waiting()) && !cpu_irq_dirty)
            break;
        step();
    }
    int idle = idle_pc(REG_PC);
    if (getenv("LOADLOG") && (idle || cpu_waiting()))
        fprintf(stderr, "load: frame %u mode %04X used %d of %d\n", md.frame,
                md.ram[0xC070] << 8 | md.ram[0xC071], cycles - GET_CYCLES(), cycles);
    if (!idle && !cpu_waiting() && getenv("LAGLOG"))
        fprintf(stderr, "lag: frame %u pc %06X mode %04X\n", md.frame, REG_PC, md.ram[0xC070] << 8 | md.ram[0xC071]);
    SET_CYCLES(0);
    return idle;
}

uint32_t cpu_call_ret;

void cpu_call(uint32_t addr, uint32_t ret)
{
    uint32_t sp0 = REG_SP, ret0 = cpu_call_ret;
    cpu_call_ret = ret;                      /* (the translated code's rts comes back here) */
    m68ki_push_32(ret);
    REG_PC = addr;
    /* (the frame's time may run out within it: the routine goes on to its end, its
     * time counted against the frame all the same) */
    const int more = 1 << 28;
    int added = more;
    SET_CYCLES(GET_CYCLES() + more);
    while (!(REG_PC == ret && REG_SP == sp0) && !CPU_STOPPED) {
        if (GET_CYCLES() <= 0) {             /* (never, in practice) */
            SET_CYCLES(GET_CYCLES() + more);
            added += more;
        }
        step();
    }
    SET_CYCLES(GET_CYCLES() - added);
    cpu_call_ret = ret0;
}

/* the game's code run from REG_PC (set up by a port hook that pushed its own returns)
 * until it comes back to ret with the stack at sp, as cpu_call() does */
void cpu_run_to(uint32_t ret, uint32_t sp)
{
    uint32_t ret0 = cpu_call_ret;
    cpu_call_ret = ret;
    const int more = 1 << 28;
    int added = more;
    SET_CYCLES(GET_CYCLES() + more);
    while (!(REG_PC == ret && REG_SP == sp) && !CPU_STOPPED) {
        if (GET_CYCLES() <= 0) {
            SET_CYCLES(GET_CYCLES() + more);
            added += more;
        }
        step();
    }
    SET_CYCLES(GET_CYCLES() - added);
    cpu_call_ret = ret0;
}

int cpu_stopping;
static const uint32_t *part_stops;
static int part_n, part_left;
static uint32_t part_start;

int cpu_stop_here(uint32_t addr)
{
    if (!part_left) {                        /* (the place it starts from is not an end) */
        part_left = 1;
        if (addr == part_start) return 0;
    }
    for (int i = 0; i < part_n; i++)
        if (part_stops[i] == addr) return 1;
    return 0;
}

uint32_t cpu_run_part(uint32_t addr, const uint32_t *stops, int n)
{
    const uint32_t *ps = part_stops;
    int pn = part_n, pl = part_left, was = cpu_stopping;
    uint32_t pst = part_start;
    part_stops = stops; part_n = n; part_left = 0; part_start = addr;
    cpu_stopping = 1;
    REG_PC = addr;
    const int more = 1 << 28;
    int added = more;
    SET_CYCLES(GET_CYCLES() + more);
    for (;;) {
        if (GET_CYCLES() <= 0) {
            SET_CYCLES(GET_CYCLES() + more);
            added += more;
        }
        step();
        int at_stop = 0;
        for (int i = 0; i < n; i++)
            if (REG_PC == stops[i]) at_stop = 1;
        if (at_stop && part_left) break;
        if (CPU_STOPPED) break;
    }
    SET_CYCLES(GET_CYCLES() - added);
    part_stops = ps; part_n = pn; part_left = pl; part_start = pst;
    cpu_stopping = was;
    return REG_PC;
}

/* take a waiting interrupt (if the code accepts it now) and run its routine until it
 * returns to what it interrupted: a function call at that point */
void cpu_run_interrupt(void)
{
    uint32_t pc0 = REG_PC, sp0 = REG_SP;
    cpu_apply_irq();
    if (REG_PC == pc0 && REG_SP == sp0)
        return;                              /* not taken (masked, or none) */
    SET_CYCLES(MD_CYC68K_LINE * MD_LINES);
    while (GET_CYCLES() > 0 && !CPU_STOPPED && !(REG_PC == pc0 && REG_SP == sp0))
        step();
    SET_CYCLES(0);
}

