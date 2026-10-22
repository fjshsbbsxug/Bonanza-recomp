/* What the screens' set-ups do, with the screen off: video registers the game keeps a copy
 * of, tiles loaded (packed or raw), blocks of the map drawn, the fade's speed. Each takes
 * the place of the jsr it stands for, for the time. */
#ifndef LOGIC_SETUP_H
#define LOGIC_SETUP_H
#include "rt.h"
#include "../video.h"

#define REG_MODE2    0xFFC07C   /* (the video registers the game keeps: 1, */
#define REG_MODE4    0xFFC092   /*   12 (shadow and highlight, the width), */
#define REG_SIZE     0xFFC09A   /*   16 (the planes' size)) */
#define REG_FLAGS    0xFFC0A0

static inline void reg_keep(uint32_t a, uint16_t v) { set16(a, v); video_reg_word(v); }
static inline void screen_off(void) { reg_keep(REG_MODE2, ram16(REG_MODE2) & 0x81BF); }
static inline void screen_on(void)  { reg_keep(REG_MODE2, ram16(REG_MODE2) | 0x8140); }

/* (the planes' size kept, $FFC0A0 bit 16 set: "move.l; bset.b #$10, d0; move.l") */
static inline void planes_size(uint16_t v)
{
    set16(REG_SIZE, v);
    REG_D[0] = ram32(REG_FLAGS) | 0x10000;
    set32(REG_FLAGS, REG_D[0]);
}

/* n + 1 tiles from src (sub_01146 packed / sub_0119C raw) to the video address cmd */
static inline void tiles(uint32_t src, uint32_t cmd, uint16_t n, int raw, uint32_t jsr)
{
    REG_A[6] = src;
    video_at_long(cmd);
    set_lo(7, n);
    call(raw ? 0x0119C : 0x01146, jsr);
}

/* a block of the map's tiles (sub_28A90, its table at a6), the tile step $40 */
static inline void map_rect(uint32_t table, uint32_t jsr)
{
    REG_A[6] = table;
    set16(0xFF1830, 0x40);
    call(0x28A90, jsr);
}

/* a map block drawn by sub_282A4: its job at $FF2CD4 (the map, the blocks, the place,
 * columns and rows), the map's width 16 */
static inline void map_block(uint32_t map, uint32_t blocks, uint16_t at, uint16_t cols,
                             uint16_t rows, uint32_t jsr)
{
    REG_A[0] = 0xFF2CD4;
    set32(0xFF2CD4, map);
    set32(0xFF2CD8, blocks);
    set16(0xFF2CDC, at);
    set16(0xFF2CDE, cols);
    set16(0xFF2CE0, rows);
    REG_A[0] = 0xFF2CE2;
    set16(0xFF2CCC, 0x10);
    call(0x282A4, jsr);
}

/* the fade's frames a step, its step, its steps (a6 kept on the stack meanwhile) */
static inline void fade_speed(uint16_t wait, uint16_t step, uint16_t steps)
{
    stack_put(-4, REG_A[6]);
    set16(0xFF17AE, wait);
    set16(0xFF17B0, step);
    set16(0xFF17B2, steps);
}

/* the planes' scroll: a word to the video address cmd */
static inline void scroll_put(uint32_t cmd, uint16_t v) { video_at_long(cmd); video_put(v); }

#endif
