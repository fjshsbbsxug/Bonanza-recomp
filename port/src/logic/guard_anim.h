/* Guards turning and their animations (guard_anim.c). Each counts its own time
 * (rts included), from where it is called. */
#ifndef LOGIC_GUARD_ANIM_H
#define LOGIC_GUARD_ANIM_H
#include <stdint.h>

void guard_anim_start(uint16_t o);                     /* sub_1767C */
void guard_face_anim(uint16_t o);                      /* sub_15288 */
uint16_t guard_turn_begin(uint16_t o, uint16_t want, uint16_t frames, int alt);   /* sub_173D6 / sub_173DC */
uint16_t guard_turn_step(uint16_t o);                  /* sub_1763C */

#endif
