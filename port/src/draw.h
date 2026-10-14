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
int  draw_text_queue2(void);    /* sub_1376A */
int  draw_registers(void);      /* sub_0070A */
int  draw_vram_at(void);        /* sub_2848C */
int  draw_no_window(void);      /* sub_006FA */
int  draw_clear_00628(void);    /* sub_00628 */
int  draw_clear_planes(void);   /* sub_0052E */
int  draw_video_init(void);     /* sub_00484 */
int  draw_columns_p1(void);     /* sub_089C0 */
int  draw_rows_p1(void);        /* sub_08B50 */
int  draw_columns_p2(void);     /* sub_08C1A */
int  draw_rows_p2(void);        /* sub_08DAA */
int  draw_hex_digits(void);     /* sub_11EA8 */
int  draw_vram_copy(void);      /* sub_28200 */

/* the split screen's line effects (sub_00C1E..sub_010E8); the caller does the rte */
int  draw_band_00C1E(void);
int  draw_band_00C32(void);
int  draw_band_00CD8(void);
int  draw_band_00D2C(void);
int  draw_band_00D74(void);
int  draw_band_00DAC(void);
int  draw_band_00DEC(void);
int  draw_band_00E34(void);
int  draw_band_00E7C(void);
int  draw_band_00EC4(void);
int  draw_band_00F5A(void);
int  draw_band_00FA0(void);
int  draw_band_00FE8(void);
int  draw_band_01020(void);
int  draw_band_01058(void);
int  draw_band_010A0(void);
int  draw_band_010E8(void);

#endif
