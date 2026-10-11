/* Used when building without recompiled code: everything is interpreted. */
#include "recomp.h"
recomp_fn recomp_find(uint32_t pc) { (void)pc; return 0; }
