/* The guards' script ops (guard_ops.c). */
#ifndef LOGIC_GUARD_OPS_H
#define LOGIC_GUARD_OPS_H
#include <stdint.h>

#define NEXT_OP      0x147C2u    /* the next op, now */
#define AFTER_SCRIPT 0x144AEu    /* the rest of the guard's frame */
#define FRAME_OVER   0x14782u    /* the guard's frame is over */

typedef uint32_t (*guard_op)(uint16_t o, uint32_t at);
extern const guard_op guard_op_start[65];   /* by op / 4; 0: still the game's code */
extern const guard_op guard_op_going[65];

#endif
