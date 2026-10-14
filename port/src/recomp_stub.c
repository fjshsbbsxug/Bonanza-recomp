/* Used when building without recompiled code: everything is interpreted. */
#include "recomp.h"
recomp_fn recomp_find(uint32_t pc) { (void)pc; return 0; }
int recomp_invalidate(uint32_t addr) { (void)addr; return 0; }
void recomp_validate_all(void) {}

/* without port/gen: no recompiled Z80 programs either */
#include "z80recomp.h"
unsigned long z80r_insns;
int z80r_code_changed;
int z80r_program(const uint8_t *zram) { (void)zram; return -1; }
int z80r_run(int program, z80* const z, int *lc, int limit) { (void)program; (void)z; (void)lc; (void)limit; return 0; }
