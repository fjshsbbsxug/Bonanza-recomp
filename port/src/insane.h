/* Insane mode (insane.c): a harder game, a fourth LEVEL in the game's OPTIONS menu once
 * unlocked (by beating the game on Hard). */
#ifndef INSANE_H
#define INSANE_H
#include <stdint.h>

int  insane_active(void);         /* on, unlocked, and not an online game */
void insane_reset(void);          /* machine reset */
void insane_taint(void);          /* the game was started some other way (stage select, test play) */
/* after each frame the frontend runs on its own: the mode's rules on the game's RAM and
 * script data, and the unlock when the game is beaten on Hard; 1 when Insane mode was
 * just earned (save the settings) */
int  insane_frame(void);
int  insane_owns(uint32_t pc);    /* the game hooks below are Insane mode's */
int  insane_hook(uint32_t pc);    /* ... handled (gamemods.c), as game_hook */
/* "INSANE MODE UNLOCKED!" for a while after earning it */
void insane_overlay(uint32_t *fb, int stride, int width, int height);
int  insane_overlay_on(void);

#define INSANE_HOOK_P1_HIT    0x18BC2u   /* player 1's shot hits a guard: jsr sub_17170 */
#define INSANE_HOOK_P2_HIT    0x18EAEu
#define INSANE_HOOK_P1_SHIELD 0x18B5Cu   /* ... and a shield stopped it (after the clink) */
#define INSANE_HOOK_P2_SHIELD 0x18E40u
#define INSANE_HOOK_P1_TRIP   0xC330u    /* tripped on a can: jsr sub_09F0E (40 frames' protection) */
#define INSANE_HOOK_P2_TRIP   0xE7AAu    /* ... player 2: jsr sub_09F28 */
#define INSANE_HOOK_P1_GETUP  0xC366u    /* sub_0C366: lying until the protection runs out */
#define INSANE_HOOK_P2_GETUP  0xE7DEu
#define INSANE_HOOK_RUN_STEP  0x156C0u   /* script op 48 (running): a frame's step from $385E */
#define INSANE_HOOK_LANE_STEP 0x16752u   /* ... op F0 (changing lane) */
#define INSANE_HOOK_LOOK      0x16C2Eu   /* sub_16BE8: an unalerted guard looks for players */
#define INSANE_HOOK_LEVEL     0x3CC8u    /* sub_03CC8: OPTIONS, LEVEL changed by d7 */
#define INSANE_HOOK_LEVEL_DRAW1 0x3B22u  /* the menu drawing LEVEL's value (jsr sub_011F2) */
#define INSANE_HOOK_LEVEL_DRAW2 0x3EC4u  /* ... and redrawing it */

#endif
