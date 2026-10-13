/* 68000 execution: recompiled code where available, Musashi interpreter otherwise. */
#include "md.h"
#include "vdp.h"
#include "recomp.h"
#include "gamemods.h"
#include "extrastages.h"
#include "../ext/musashi/m68k.h"
#include "../ext/musashi/m68kcpu.h"
#include "../ext/musashi/m68kops.h"

static int line_budget;   /* cycles requested for the current slice */
static int carry;         /* overshoot from the previous slice */

uint64_t cpu_stat_recomp_insns, cpu_stat_interp_insns;

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

unsigned int m68k_read_disassembler_8(unsigned int a) { return md_read8(a); }
unsigned int m68k_read_disassembler_16(unsigned int a) { return m68k_read_memory_16(a); }
unsigned int m68k_read_disassembler_32(unsigned int a) { return m68k_read_memory_32(a); }

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
    static int done;
    if (!done) {
        m68k_init();
        done = 1;
    }
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
}

void cpu_reset(void)
{
    m68k_pulse_reset();
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
    m68k_set_irq(irq_level);
    m68ki_check_interrupts();
}

uint32_t cpu_get_pc(void) { return REG_PC; }

int cpu_line_cycles(void)
{
    return line_budget - GET_CYCLES();
}

/* Execute one instruction with the interpreter (without touching the cycle pool). */
static void interp_step(void)
{
#ifdef INTERP_TRACE
    if (md.use_recomp) {
        static unsigned char seen[0x40000];
        if (REG_PC < 0x80000 && !seen[REG_PC >> 1]) {
            seen[REG_PC >> 1] = 1;
            fprintf(stderr, "interp %06x\n", REG_PC);
        }
    }
#endif
    if (game_hooks && (REG_PC == GAME_HOOK_VISIBILITY || REG_PC == GAME_HOOK_SPRITES || REG_PC == GAME_HOOK_STAGE) && game_hook(REG_PC)) {
        REG_PC = m68ki_pull_32();      /* the routine was handled: return from it */
        USE_CYCLES(16);
        return;
    }
    REG_PPC = REG_PC;
    REG_IR = m68ki_read_imm_16();
    m68ki_instruction_jump_table[REG_IR]();
    USE_CYCLES(CYC_INSTRUCTION[REG_IR]);
    cpu_stat_interp_insns++;
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
        recomp_fn fn = md.use_recomp ? recomp_find(REG_PC) : 0;
        if (fn)
            fn();
        else
            interp_step();
    }
    carry = CPU_STOPPED ? 0 : -GET_CYCLES();
    /* keep cpu_line_cycles() meaningful until the next slice */
    line_budget = cycles;
    SET_CYCLES(0);
}
