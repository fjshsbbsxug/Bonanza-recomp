/* Interface to the statically recompiled 68000 code (port/gen/). */
#ifndef RECOMP_H
#define RECOMP_H
#include <stdint.h>

typedef void (*recomp_fn)(void);

/* Returns the recompiled function that can resume execution at pc, or NULL. */
recomp_fn recomp_find(uint32_t pc);

#endif
