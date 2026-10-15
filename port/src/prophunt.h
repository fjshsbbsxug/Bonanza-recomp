/* Prop Hunt: an online game mode chosen by the host (prophunt.c). */
#ifndef PROPHUNT_H
#define PROPHUNT_H
#include <stdint.h>

/* the game's rules: both machines call these around every frame they run, in lockstep
 * (they read only the players' buttons and the game's RAM, so both stay the same) */
void ph_reset(void);              /* a new online game */
void ph_pre_frame(void);          /* before md_run_frame: may change md.pad and RAM */
void ph_post_frame(void);         /* after md_run_frame */

/* the picture (each machine its own) */
int  ph_active(void);             /* a round is on (or about to start) */
int  ph_hidden_player(void);      /* the player drawn as a disguise, or -1 */
/* the disguise: frame and attribute to draw at that player's place; 0 if none */
int  ph_disguise(uint16_t *frame, uint16_t *attr);
/* the banner (round, role, time, score, messages) on the 32-bit picture */
void ph_overlay(uint32_t *fb, int stride, int width, int height, int local_player);

#endif
