/* The game's drawing routines, written in C (draw.c): 1 done (the caller does the
 * return), 2 done and gone on into the game's code (REG_PC), 0 not run (the game's
 * routine runs as translated). */
#ifndef DRAW_H
#define DRAW_H
#include <stdint.h>

extern uint64_t draw_stat_c, draw_stat_translated;   /* calls done in C / left to the game's routine */

extern int  draw_tiles_packed(void);   /* sub_01146 */
extern int  draw_tiles_raw(void);      /* sub_0119C */
extern int  draw_text(void);           /* sub_011B8 */
extern int  draw_text_d7(void);        /* sub_011F2 */
extern int  draw_text_queue(void);     /* sub_13318 */
extern int  draw_text_queue2(void);    /* sub_1376A */
extern int  draw_registers(void);      /* sub_0070A */
extern int  draw_vram_at(void);        /* sub_2848C */
extern int  draw_no_window(void);      /* sub_006FA */
extern int  draw_clear_00628(void);    /* sub_00628 */
extern int  draw_clear_planes(void);   /* sub_0052E */
extern int  draw_video_init(void);     /* sub_00484 */
extern int  draw_columns_p1(void);     /* sub_089C0 */
extern int  draw_rows_p1(void);        /* sub_08B50 */
extern int  draw_columns_p2(void);     /* sub_08C1A */
extern int  draw_rows_p2(void);        /* sub_08DAA */
extern int  draw_hex_digits(void);     /* sub_11EA8 */
extern int  draw_vram_copy(void);      /* sub_28200 */
extern int  draw_hud_frame(void);      /* sub_1044A */
extern int  draw_hud_stage_name(void); /* sub_1050C */
extern int  draw_hud_number_p1(void);  /* sub_11EDC */
extern int  draw_hud_number_p2(void);  /* sub_11FB8 */
extern int  draw_hud_score_p1(void);   /* sub_11DF8 */
extern int  draw_hud_score_p2(void);   /* sub_11E88 */
extern int  draw_tile_loads(void);     /* sub_0135A */
extern int  draw_frame_113E6(void);
extern int  draw_frame_03420(void);
extern int  draw_frame_122FE(void);
extern int  draw_frame_248DE(void);
/* parts of the vertical blank (sub_0098E), each going on in the game's code */
extern int  draw_vblank_entry(void);       /* 0x98E */
extern int  draw_vblank_scroll_9C0(void);  /* 0x9C0 */
extern int  draw_vblank_scroll(void);      /* 0x9C6 */
extern int  draw_vblank_sprites(void);     /* 0xA2C */
extern int  draw_vblank_palette(void);     /* 0xAD2 */
extern int  draw_vblank_hud(void);         /* 0xB58 */
extern int  draw_hud_time_106E4(void);
extern int  draw_hud_time_10A4A(void);
extern int  draw_vram_at_d6(void);     /* sub_28AD0 */
extern int  draw_map_rect(void);       /* sub_28A90 */
extern int  draw_map_column(void);     /* sub_01836 */
extern int  draw_sprite_list(void);    /* sub_006CA */
extern int  draw_sprites_clear(void);  /* sub_0066A */
extern int  draw_targets_picture(void); /* 0x114F0 */
extern int  draw_map_block(void);      /* sub_282A4 */
extern int  draw_map_block_each(void); /* sub_28386 */
extern int  draw_load_list(uint32_t at, int n);   /* the tile load lists (tools/recomp.py) */
extern int  draw_tile_stream_p1(void); /* sub_082D2 */
extern int  draw_tile_stream_p2(void); /* sub_08510 */
extern int  draw_window_regs(void);    /* sub_006EA */
extern int  draw_hex_number(void);     /* sub_01268 */
extern int  draw_title_clear(void);
extern int  draw_start_up(void);       /* sub_0021E to 0x2AA */
extern int  draw_boot_status(void);    /* sub_0021E at 0x318 */
extern int  draw_title_rows(void);     /* sub_02E34 at 0x2EEC */
/* the screen set-ups: straight runs of the game's routines from the address named */
extern int  draw_seg_00366(void);
extern int  draw_seg_0042E(void);
extern int  draw_seg_02E34(void);
extern int  draw_seg_02E4C(void);
extern int  draw_seg_02F0E(void);
extern int  draw_seg_02FD4(void);
extern int  draw_seg_0318A(void);
extern int  draw_seg_031A8(void);
extern int  draw_seg_031F4(void);
extern int  draw_seg_035BE(void);
extern int  draw_seg_0394A(void);
extern int  draw_seg_03A78(void);
extern int  draw_seg_03B1E(void);
extern int  draw_seg_03ECC(void);
extern int  draw_seg_03EF0(void);
extern int  draw_seg_03F8A(void);
extern int  draw_seg_04206(void);
extern int  draw_seg_0430A(void);
extern int  draw_seg_04434(void);
extern int  draw_seg_050C4(void);
extern int  draw_seg_05496(void);
extern int  draw_seg_0BE1E(void);
extern int  draw_seg_11120(void);
extern int  draw_seg_11138(void);
extern int  draw_seg_1124C(void);
extern int  draw_seg_11298(void);
extern int  draw_seg_11476(void);
extern int  draw_seg_114B2(void);
extern int  draw_seg_114DC(void);
extern int  draw_seg_1195A(void);
extern int  draw_seg_120E4(void);
extern int  draw_seg_121D2(void);
extern int  draw_seg_12614(void);
extern int  draw_seg_12652(void);
extern int  draw_seg_12730(void);
extern int  draw_seg_12A94(void);
extern int  draw_seg_12B5E(void);
extern int  draw_seg_12C50(void);
extern int  draw_seg_12C72(void);
extern int  draw_seg_12C9A(void);
extern int  draw_seg_12CE4(void);
extern int  draw_seg_12D56(void);
extern int  draw_seg_12DD8(void);
extern int  draw_seg_12DFE(void);
extern int  draw_seg_23948(void);
extern int  draw_seg_2396C(void);
extern int  draw_seg_239B8(void);
extern int  draw_seg_23A22(void);
extern int  draw_seg_23A8A(void);
extern int  draw_seg_23AC2(void);
extern int  draw_seg_23B32(void);
extern int  draw_seg_23B50(void);
extern int  draw_seg_23BD8(void);
extern int  draw_seg_23C8E(void);
extern int  draw_seg_23CAC(void);
extern int  draw_seg_23D60(void);
extern int  draw_seg_23E16(void);
extern int  draw_seg_23E2A(void);
extern int  draw_seg_23E60(void);
extern int  draw_seg_24536(void);
extern int  draw_seg_24572(void);
extern int  draw_seg_24644(void);
extern int  draw_seg_2497C(void);
extern int  draw_seg_24990(void);
extern int  draw_seg_249A4(void);
extern int  draw_seg_3BBFC(void);
extern int  draw_seg_3BC28(void);
extern int  draw_seg_3BD80(void);
extern int  draw_seg_3BE2E(void);
extern int  draw_seg_3BE86(void);
extern int  draw_seg_3BF32(void);
extern int  draw_seg_3C07E(void);    /* sub_02E34 at 0x2E7C */

/* the split screen's line effects (sub_00C1E..sub_010E8); the caller does the rte */
extern int  draw_band_00C1E(void);
extern int  draw_band_00C32(void);
extern int  draw_band_00CD8(void);
extern int  draw_band_00D2C(void);
extern int  draw_band_00D74(void);
extern int  draw_band_00DAC(void);
extern int  draw_band_00DEC(void);
extern int  draw_band_00E34(void);
extern int  draw_band_00E7C(void);
extern int  draw_band_00EC4(void);
extern int  draw_band_00F5A(void);
extern int  draw_band_00FA0(void);
extern int  draw_band_00FE8(void);
extern int  draw_band_01020(void);
extern int  draw_band_01058(void);
extern int  draw_band_010A0(void);
extern int  draw_band_010E8(void);

#endif
