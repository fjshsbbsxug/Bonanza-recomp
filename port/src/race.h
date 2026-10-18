/* Race: an online game mode the host chooses (race.c). */
#ifndef RACE_H
#define RACE_H
#include <stdint.h>

void race_reset(void);            /* a new online game */
int  race_on(void);

/* every game frame's turn (the frontend): this player's buttons; what to do now */
enum {
    RACE_HOLD,                    /* no game frame (voting, counting down, the result) */
    RACE_START,                   /* reset the machine and start stage entry *entry */
    RACE_RUN                      /* run a frame (race_pre_frame / race_post_frame around it) */
};
int  race_update(uint8_t pad, int *entry);
void race_pre_frame(void);        /* before md_run_frame: may change RAM */
void race_post_frame(void);       /* after it */

/* the vote, the countdown, the result and the opponent's progress, on the picture */
void race_overlay(uint32_t *fb, int stride, int width, int height);

#endif
