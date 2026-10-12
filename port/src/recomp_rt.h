/* Runtime support for the generated code in port/gen/. */
#ifndef RECOMP_RT_H
#define RECOMP_RT_H

#include "md.h"
#include "recomp.h"
#include "gamemods.h"
#include "m68k.h"
#include "m68kcpu.h"
#include "m68kops.h"

extern int cpu_irq_dirty;
extern uint64_t cpu_stat_recomp_insns;

/* Every instruction starts with this: give control back to the scheduler when the
 * time slice is used up or an interrupt line changed (same points as the interpreter). */
#define CHK(a) \
    if (GET_CYCLES() <= 0 || cpu_irq_dirty || CPU_STOPPED) { REG_PC = (a); return; } \
    cpu_stat_recomp_insns++;

/* Run one instruction through the reference interpreter handler. */
#define CALLOP(a, op) do { \
    REG_PPC = (a); REG_PC = (a) + 2; REG_IR = (op); \
    m68ki_instruction_jump_table[op](); \
    USE_CYCLES(CYC_INSTRUCTION[op]); \
} while (0)

/* Native instructions: base cycle count from the interpreter's table. */
#define CYC(op) USE_CYCLES(CYC_INSTRUCTION[op])

/* memory */
#define RD8(a)  m68ki_read_8(a)
#define RD16(a) m68ki_read_16(a)
#define RD32(a) m68ki_read_32(a)
#define WR8(a, v)  m68ki_write_8((a), (v))
#define WR16(a, v) m68ki_write_16((a), (v))
#define WR32(a, v) m68ki_write_32((a), (v))

typedef struct {
    uint32_t start, end;
    recomp_fn fn;
} recomp_chunk;

#endif
