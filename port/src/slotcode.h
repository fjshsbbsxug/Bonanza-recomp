/* New 68000 code that level mods bring, run natively. See slotcode.c. */
#ifndef SLOTCODE_H
#define SLOTCODE_H
#include <stdint.h>
#include "recomp.h"

/* an instruction form whose operands are filled in later (generated: port/gen/recomp_slot.c) */
typedef struct {
    uint16_t op;
    uint8_t nwords;                     /* operand words after the opcode */
    uint8_t cyc;
    void (*fn)(const uint16_t *w);
} slot_form;

extern const slot_form slot_forms[];
extern const int slot_nforms;

recomp_fn slot_find(uint32_t pc);       /* native code for a mod's routine at pc, or 0 */
void slot_reset(void);                  /* the ROM changed (mods applied): forget what was read */

#endif
