/* The Z80's programs, recompiled to C (port/gen/z80_driver.c, tools/z80recomp.py):
 * the sound driver and the boot stub that runs before it is loaded. */
#ifndef Z80RECOMP_H
#define Z80RECOMP_H
#include <stdint.h>
#include "../ext/z80/z80.h"

#define Z80R_LEN 0x118B                      /* bytes of Z80 RAM the programs occupy */
extern unsigned long z80r_insns;             /* instructions run */
extern int z80r_code_changed;                /* set when those bytes are written */

void z80r_init(z80* const z);               /* power on / reset */
void z80r_gen_int(z80* const z, uint8_t data); /* the INT line */
int z80r_program(const uint8_t *zram);      /* which program is in Z80 RAM, -1: none */
/* run program from z->pc while *lc < limit (adding each instruction's cycles to *lc,
 * as the step loop does); 1: the line is done, 0: PC is outside its code, 2: its code
 * bytes were written (look again with z80r_program) */
int z80r_run(int program, z80* const z, int *lc, int limit);

#endif
