/* 68000 execution: all of it is the recompiled game code (port/gen); there is no
 * interpreter. The CPU's state, reset and interrupt entry are in m68k_rt.c. */
#include "md.h"
#include "vdp.h"
#include "recomp.h"
#include "gamemods.h"
#include "extrastages.h"
#include "../ext/musashi/m68k.h"
#include "../ext/musashi/m68kcpu.h"
#include "m68k_rt.h"

static int line_budget;   /* cycles requested for the current slice */
static int carry;         /* overshoot from the previous slice */

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
    carry = 0;
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

int cpu_line_cycles(void)
{
    return line_budget - GET_CYCLES();
}

/* PC where there is no native code (never, for the game, its Bonanza Maker mods and the
 * Lost Levels): the 68000's exception for an instruction it cannot run; the game's own
 * error handler takes it from there. */
static void no_code(void)
{
    if (cpu_stat_no_code++ < 8)
        fprintf(stderr, "68000: no native code at %06X\n", REG_PC);
    m68k_rt_no_code();
}

void cpu_run(int cycles)
{
    line_budget = cycles - carry;
    SET_CYCLES(line_budget);
    if (cpu_irq_dirty)
        cpu_apply_irq();
    if (CPU_STOPPED) {
        SET_CYCLES(0);
        carry = 0;
        return;
    }
    while (GET_CYCLES() > 0) {
        if (cpu_irq_dirty)
            cpu_apply_irq();
        if (CPU_STOPPED)
            break;
        recomp_fn fn = recomp_find(REG_PC);
        if (fn)
            fn();
        else
            no_code();
    }
    carry = CPU_STOPPED ? 0 : -GET_CYCLES();
    /* keep cpu_line_cycles() meaningful until the next slice */
    line_budget = cycles;
    SET_CYCLES(0);
}
