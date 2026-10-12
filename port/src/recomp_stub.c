/* Used when building without recompiled code: everything is interpreted. */
#include "recomp.h"
recomp_fn recomp_find(uint32_t pc) { (void)pc; return 0; }
int recomp_invalidate(uint32_t addr) { (void)addr; return 0; }
void recomp_validate_all(void) {}
