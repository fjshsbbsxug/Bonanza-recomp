/* The game's drawing routines, written in C (draw.c): 1 done, 0 not run (the game's
 * routine runs as translated). */
#ifndef DRAW_H
#define DRAW_H
#include <stdint.h>

extern uint64_t draw_stat_c, draw_stat_translated;   /* calls done in C / left to the game's routine */

int  draw_tiles_packed(void);   /* sub_01146 */
int  draw_tiles_raw(void);      /* sub_0119C */
int  draw_text(void);           /* sub_011B8 */
int  draw_text_d7(void);        /* sub_011F2 */
int  draw_text_queue(void);     /* sub_13318 */

#endif
