/* Interface to the statically recompiled 68000 code (port/gen/). */
#ifndef RECOMP_H
#define RECOMP_H
#include <stdint.h>

typedef void (*recomp_fn)(void);

/* Returns the recompiled function that can resume execution at pc, or NULL. */
recomp_fn recomp_find(uint32_t pc);

/* A mod changed the code at addr: from now on the interpreter runs the recompiled block
 * holding it (1 if there was one). recomp_validate_all() undoes all of that. */
int recomp_invalidate(uint32_t addr);
void recomp_validate_all(void);

#endif
