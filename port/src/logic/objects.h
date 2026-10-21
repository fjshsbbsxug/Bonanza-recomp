/* The objects' small shared routines (objects.c), as called by a jsr: each takes the
 * routine's time, rts included; the caller goes on with jumped_to(jsr + 6). */
#ifndef LOGIC_OBJECTS_H
#define LOGIC_OBJECTS_H
#include <stdint.h>

void into(uint32_t jsr, uint32_t routine);    /* the jsr's time; the routine's code next */
void anim_start(uint16_t o, uint16_t anim);  /* sub_1B088 */
void anim_step(uint16_t o);                  /* sub_1B032 */

#endif
