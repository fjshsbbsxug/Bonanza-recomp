/* The game's drawing routines, written in C (draw.c): 1 done (the caller does the
 * return), 2 done and gone on into the game's code (REG_PC), 0 not run (the game's
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
int  draw_hud_frame(void);      /* sub_1044A */
int  draw_hud_stage_name(void); /* sub_1050C */
int  draw_hud_number_p1(void);  /* sub_11EDC */
int  draw_hud_number_p2(void);  /* sub_11FB8 */
int  draw_hud_score_p1(void);   /* sub_11DF8 */
int  draw_hud_score_p2(void);   /* sub_11E88 */
int  draw_tile_loads(void);     /* sub_0135A */
int  draw_frame_113E6(void);
int  draw_frame_03420(void);
int  draw_frame_122FE(void);
int  draw_frame_248DE(void);
/* parts of the vertical blank (sub_0098E), each going on in the game's code */
int  draw_vblank_entry(void);       /* 0x98E */
int  draw_vblank_scroll_9C0(void);  /* 0x9C0 */
int  draw_vblank_scroll(void);      /* 0x9C6 */
int  draw_vblank_sprites(void);     /* 0xA2C */
int  draw_vblank_palette(void);     /* 0xAD2 */
int  draw_vblank_hud(void);         /* 0xB58 */
int  draw_hud_time_106E4(void);
int  draw_hud_time_10A4A(void);
int  draw_vram_at_d6(void);     /* sub_28AD0 */
int  draw_map_rect(void);       /* sub_28A90 */
int  draw_map_column(void);     /* sub_01836 */
int  draw_sprite_list(void);    /* sub_006CA */
int  draw_sprites_clear(void);  /* sub_0066A */
int  draw_targets_picture(void); /* 0x114F0 */
int  draw_map_block(void);      /* sub_282A4 */

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
