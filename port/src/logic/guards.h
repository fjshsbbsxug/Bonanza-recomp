/* Guards (guards.c). */
#ifndef LOGIC_GUARDS_H
#define LOGIC_GUARDS_H
#include <stdint.h>

/* sub_16BE8: guard o looks for the players (LOOK_FOUND); called by the jsr at jsr (its time) */
void guard_look(uint16_t o, uint32_t jsr);

#endif
