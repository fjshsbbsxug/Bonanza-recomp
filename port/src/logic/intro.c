/* The attract mode's opening (screen mode $1C, set up at 0x120E4): the brothers walking
 * with the police cars behind them, the town scrolling by, the two of them in the van,
 * the view scrolling up; start skips it. A step a frame (INTRO_STEP); the steps' set-ups
 * (tiles and maps loaded with the screen off) stay the game's code. */
#include "rt.h"
#include "../video.h"

#define INTRO_STEP   0xFF6C5C
#define SCROLL_B_Y   0xFF6C60   /* the planes' scroll: B's vertical, A's horizontal, */
#define SCROLL_A_X   0xFF6C5E   /*   A's vertical, B's horizontal */
#define SCROLL_A_Y   0xFF6C64
#define SCROLL_B_X   0xFF6C62
#define INTRO_WAIT   0xFF6C1A
#define INTRO_COUNT  0xFF6C1C   /* (the town's palette step) */
#define START        0xFF17B9   /* (byte) start pressed (bit 7), either pad */
#define SKIPPED      0xFF5548
#define VDP_MODE2    0xFFC07C
#define VDP_MODE4    0xFFC092
#define FIGURES      4          /* the brothers, then the three cars: */
#define FIG_PATH     0xFF3456   /* (longs) each one's path: steps of (dx, dy, frames), */
#define FIG_DX       0xFF385E   /*   ended by frames -1 */
#define FIG_DY       0xFF390A
#define FIG_X        0xFF304E
#define FIG_Y        0xFF30FA
#define FIG_TIME     0xFF365A
#define FIG_ANIM     0xFF35AE   /* (2 bytes) the brothers' animation: wait, step */

/* (start pressed: the opening skipped: sound $E0, the last step) */
static int skip(uint32_t bne)
{
    set8(START, ram8(START) & 0x80);
    if (!ram8(START)) return 0;
    branched(bne, 0x12388);
    set16(SKIPPED, 1);
    set16(INTRO_STEP, 6);
    set_lo(0, 0xE0);
    call(0x02D54, 0x1239C);
    ran_to(0x123A2);
    return 1;
}

/* sub_122FE: the opening's frame: the scroll set, then its step (table 0x1236C) */
int logic_intro_frame(void)
{
    if (!start()) return 0;
    begin(0x122FE);
    set16(SPRITE_COUNT, 0);
    set16(SCREEN_WAIT, (uint16_t)(ram16(SCREEN_WAIT) + 1));
    video_at_long(0x40000010);
    video_put(ram16(SCROLL_B_Y));
    video_at_long(0x70000002);
    video_put(ram16(SCROLL_A_X));
    video_at_long(0x40020010);
    video_put(ram16(SCROLL_A_Y));
    video_at_long(0x70020002);
    video_put(ram16(SCROLL_B_X));
    set_lo(0, (uint16_t)(ram16(INTRO_STEP) << 2));
    REG_A[0] = mem32(0x1236C + (int16_t)lo(REG_D[0]));
    ran_to(0x1236C);
    return jump_to(REG_A[0]);
}

/* (the brothers' animation: a step every 4 frames, of 4) */
static uint16_t anim_step(uint32_t a4, uint32_t bpl, uint32_t to)
{
    set8(a4, (uint8_t)(ram8(a4) - 1));
    if (!(ram8(a4) & 0x80)) branched(bpl, to);
    else {
        set8(a4, 3);
        set8(a4 + 1, (uint8_t)((ram8(a4 + 1) + 1) & 3));
        ran_to(to);
    }
    return (uint16_t)(ram8(a4 + 1) << 1);
}
static void sprite(uint16_t pic, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t jsr)
{
    set_lo(0, pic);
    set_lo(1, x);
    set_lo(2, y);
    set_lo(3, w);
    set_lo(4, h);
    call(0x12E64, jsr);
}
/* (a figure moved by its step: across at half speed, the odd half every other frame) */
static void figure_move(void)
{
    set_lo(0, mem16(REG_A[0])); REG_A[0] += 2;
    set_lo(1, ram16(SCREEN_WAIT) & 1);
    set_lo(0, (uint16_t)((uint16_t)(lo(REG_D[0]) + lo(REG_D[1])) >> 1));
    set16(REG_A[2], (uint16_t)(ram16(REG_A[2]) + lo(REG_D[0])));
    set_lo(0, mem16(REG_A[1])); REG_A[1] += 2;
    set16(REG_A[3], (uint16_t)(ram16(REG_A[3]) + lo(REG_D[0])));
}

/* sub_123A4 (step 0): the brothers and the cars along their paths (at the brothers'
 * path's end the next step's set-up, 0x12614) */
int logic_intro_walk(void)
{
    if (!start()) return 0;
    begin(0x123A4);
    if (skip(0x123AC)) return done();
    for (int i = 0; i < FIGURES; i++) {
        uint32_t at = i ? 0x123E2 + 0x28 * (uint32_t)(i - 1) : 0x123AE;
        uint32_t next = i ? at + 0x28 : 0x123E2;
        uint32_t t = FIG_TIME + 2 * (uint32_t)i;
        set16(t, (uint16_t)(ram16(t) - 1));
        if (!(ram16(t) & 0x8000)) { branched(at + 6, next); continue; }
        REG_A[0] = ram32(FIG_PATH + 4 * (uint32_t)i);
        set16(FIG_DX + 2 * (uint32_t)i, mem16(REG_A[0]));
        set16(FIG_DY + 2 * (uint32_t)i, mem16(REG_A[0] + 2));
        set16(t, mem16(REG_A[0] + 4));
        REG_A[0] += 6;
        set32(FIG_PATH + 4 * (uint32_t)i, REG_A[0]);
        if (!i && ram16(t) == 0xFFFF) { branched(0x123DE, 0x12614); return jump_to(0x12614); }
        ran_to(next);
    }
    REG_A[0] = FIG_DX; REG_A[1] = FIG_DY; REG_A[2] = FIG_X; REG_A[3] = FIG_Y;
    REG_A[4] = FIG_ANIM;
    figure_move();                                    /* (the brothers) */
    uint16_t s = anim_step(REG_A[4], 0x12490, 0x124A4);
    REG_A[5] = 0x12764;
    uint16_t x = ram16(REG_A[2]), y = ram16(REG_A[3]);
    sprite((uint16_t)(mem16(0x12764 + s) + 0x180), x, y, 3, 1, 0x124CA);
    sprite(0x140, (uint16_t)(x + 0xD), (uint16_t)(y + 8), 1, 1, 0x124F4);
    REG_A[5] = 0x1276C;
    sprite((uint16_t)(mem16(0x1276C + s) + 0x200), (uint16_t)(x + 0x18), (uint16_t)(y + 8), 2, 2, 0x12526);
    REG_A[2] += 2; REG_A[3] += 2;
    sprite(0x140, (uint16_t)(x + 0x2B), (uint16_t)(y + 0xC), 1, 1, 0x12552);
    REG_D[7] = 2;
    for (;;) {                                        /* (the cars: 2 x 2 sprites each) */
        figure_move();
        x = ram16(REG_A[2]); y = ram16(REG_A[3]);
        sprite(0xE280, x, y, 2, 2, 0x12584);
        sprite(0xEA80, (uint16_t)(x + 0x18), y, 2, 2, 0x125A2);
        sprite(0xF280, x, (uint16_t)(y + 0x18), 2, 2, 0x125C0);
        REG_A[2] += 2; REG_A[3] += 2;
        sprite(0xFA80, (uint16_t)(x + 0x18), (uint16_t)(y + 0x18), 2, 2, 0x125E2);
        uint16_t c = lo(REG_D[7]);
        set_lo(7, (uint16_t)(c - 1));
        if (!c) { ran_to(0x125EC); break; }
        branched(0x125E8, 0x1255A);
    }
    return jump_to(0x125EC);                          /* (the sprite list's end) */
}

/* (steps 1 and 2: the town's palette cycling and the scroll easing in; at the wait's end
 * the next set-up) */
int logic_intro_town(void)
{
    if (!start()) return 0;
    begin(0x128C4);
    if (skip(0x128CC)) return done();
    set16(INTRO_WAIT, (uint16_t)(ram16(INTRO_WAIT) - 1));
    if (ram16(INTRO_WAIT) & 0x8000) { branched(0x128D6, 0x12962); return jump_to(0x12962); }
    if (ram16(INTRO_WAIT) >= 0x30) { branched(0x128E2, 0x128F0); call(0x12E9C, 0x128F0); }
    else { call(0x12ED8, 0x128E6); branched(0x128EC, 0x128F6); }
    set16(INTRO_COUNT, (uint16_t)(ram16(INTRO_COUNT) + 1));
    set_lo(7, (uint16_t)(ram16(INTRO_COUNT) >> 1));
    if (lo(REG_D[7]) != 0x2F) branched(0x12908, 0x12918);
    else { set16(INTRO_COUNT, 0x58); set_lo(7, 0x2C); ran_to(0x12918); }
    call(0x018FC, 0x12918);
    if (!(ram8(INTRO_COUNT + 1) & 1)) { branched(0x12926, 0x125EC); return jump_to(0x125EC); }
    REG_A[0] = 0x12952;                               /* (the police lights, every other */
    REG_D[7] = 3;                                     /*  frame) */
    ran_to(0x12932);
    for (;;) {
        uint16_t x = (uint16_t)(mem16(REG_A[0]) + ram16(SCROLL_A_X));
        uint16_t y = mem16(REG_A[0] + 2);
        REG_A[0] += 4;
        set_lo(0, 0xE0BD); set_lo(1, x); set_lo(2, y);
        REG_D[3] = 1; REG_D[4] = 1;
        call(0x12E64, 0x12944);
        uint16_t c = lo(REG_D[7]);
        set_lo(7, (uint16_t)(c - 1));
        if (!c) { ran_to(0x1294E); break; }
        branched(0x1294A, 0x12932);
    }
    branched(0x1294E, 0x125EC);
    return jump_to(0x125EC);
}
int logic_intro_van(void)
{
    if (!start()) return 0;
    begin(0x12A3A);
    if (skip(0x12A42)) return done();
    set16(INTRO_WAIT, (uint16_t)(ram16(INTRO_WAIT) - 1));
    if (ram16(INTRO_WAIT) & 0x8000) { branched(0x12A4C, 0x12A94); return jump_to(0x12A94); }
    if (ram16(INTRO_WAIT) >= 0x30) { branched(0x12A58, 0x12A66); call(0x12EBA, 0x12A66); }
    else { call(0x12ED8, 0x12A5C); branched(0x12A62, 0x12A6C); }
    set16(INTRO_COUNT, (uint16_t)(ram16(INTRO_COUNT) + 1));
    if (ram16(INTRO_COUNT) != 0x2F) branched(0x12A7A, 0x12A86);
    else { set16(INTRO_COUNT, 0x2C); ran_to(0x12A86); }
    set_lo(7, ram16(INTRO_COUNT));
    call(0x018FC, 0x12A8C);
    ran_to(0x12A92);
    return done();
}

/* sub_12BAE (step 3): one brother walking on alone; at the wait's end the next set-up */
int logic_intro_alone(void)
{
    if (!start()) return 0;
    begin(0x12BAE);
    if (skip(0x12BB6)) return done();
    set16(INTRO_WAIT, (uint16_t)(ram16(INTRO_WAIT) - 1));
    if (!ram16(INTRO_WAIT)) { branched(0x12BC0, 0x12C50); return jump_to(0x12C50); }
    REG_A[2] = FIG_X; REG_A[3] = FIG_Y; REG_A[4] = FIG_ANIM;
    set16(FIG_X, (uint16_t)(ram16(FIG_X) + 1));
    uint16_t s = anim_step(FIG_ANIM, 0x12BDA, 0x12BEE);
    uint16_t x = ram16(FIG_X), y = ram16(FIG_Y);
    REG_A[5] = 0x12764;
    sprite((uint16_t)((mem16(0x12764 + s) + 0x188) | 0x800), x, y, 3, 1, 0x12C14);
    REG_A[5] = 0x1276C;
    sprite((uint16_t)((mem16(0x1276C + s) + 0x209) | 0x800), (uint16_t)(x - 0x18), (uint16_t)(y + 8), 2, 2, 0x12C46);
    branched(0x12C4C, 0x125EC);
    return jump_to(0x125EC);
}

/* sub_12D8A (step 4): the view scrolling up 8 lines a frame; then a wait (step 5) */
int logic_intro_rise(void)
{
    if (!start()) return 0;
    begin(0x12D8A);
    if (skip(0x12D92)) return done();
    set16(INTRO_WAIT, (uint16_t)(ram16(INTRO_WAIT) - 1));
    if (!ram16(INTRO_WAIT)) {
        branched(0x12D9C, 0x12DAE);
        set16(INTRO_WAIT, 0xF0);
        set16(INTRO_STEP, 5);
        ran_to(0x12DBE);
        return done();
    }
    set16(SCROLL_B_Y, (uint16_t)(ram16(SCROLL_B_Y) - 8));
    set16(SCROLL_A_X, (uint16_t)(ram16(SCROLL_A_X) - 8));
    ran_to(0x12DAC);
    return done();
}

/* sub_12DC0 (step 5): the wait; then the end (step 6) */
static int intro_end(void);
int logic_intro_wait(void)
{
    if (!start()) return 0;
    begin(0x12DC0);
    if (skip(0x12DC8)) return done();
    set16(INTRO_WAIT, (uint16_t)(ram16(INTRO_WAIT) - 1));
    if (ram16(INTRO_WAIT)) { ran_to(0x12DD6); return done(); }
    branched(0x12DD2, 0x12DD8);
    set16(INTRO_STEP, 6);
    ran_to(0x12DE0);
    return intro_end();
}

/* sub_12DE0 (step 6): the screen off, the palettes cleared, the shadow and highlight
 * off; then the next screen: the title's options when skipped, else the demo */
static int intro_end(void)
{
    set16(VDP_MODE2, ram16(VDP_MODE2) & 0x81BF);
    video_reg_word(ram16(VDP_MODE2));
    call(0x00522, 0x12DF2);
    call(0x01928, 0x12DF8);
    set16(VDP_MODE4, ram16(VDP_MODE4) & 0x8CF7);
    video_reg_word(ram16(VDP_MODE4));
    set16(VDP_MODE2, ram16(VDP_MODE2) | 0x8140);
    video_reg_word(ram16(VDP_MODE2));
    int skipped = ram16(SKIPPED) != 0;
    if (!skipped) ran_to(0x12E2C); else branched(0x12E28, 0x12E48);
    set32(SCREEN_RUN, 0x2E06);
    set16(GAME_MODE, skipped ? 4 : 0x14);
    set16(SKIPPED, 0);
    ran_to(skipped ? 0x12E62 : 0x12E46);
    return done();
}
int logic_intro_end(void)
{
    if (!start()) return 0;
    begin(0x12DE0);
    return intro_end();
}

/* sub_12E9C / sub_12EBA / sub_12ED8: plane A scrolled across $10 a frame (in its $200)
 * until at 0, at $1F8, or at $100 / $108 */
static int scroll_to(uint32_t r, uint16_t a, uint16_t b, uint32_t beq2, uint32_t rts)
{
    if (!start()) return 0;
    begin(r);
    uint16_t x = ram16(SCROLL_A_X);
    if (x == a) { branched(r + 8, rts); return done(); }
    if (beq2) {
        if (x == b) { branched(beq2, rts); return done(); }
    }
    set16(SCROLL_A_X, (uint16_t)((x + 0x10) & 0x1FF));
    ran_to(rts);
    return done();
}
int logic_intro_scroll0(void)   { return scroll_to(0x12E9C, 0, 0, 0, 0x12EB8); }
int logic_intro_scroll1f8(void) { return scroll_to(0x12EBA, 0x1F8, 0, 0, 0x12ED6); }
int logic_intro_scroll100(void) { return scroll_to(0x12ED8, 0x100, 0x108, 0x12EEC, 0x12F00); }
