/* The screen before a stage (screen mode 6, set up at 0x11120): the building, then the
 * things to take there shown one at a time; A, B or C hurries it on. A step a frame
 * (STEP, from the frame at 0x113E6); the second part's set-up (0x11476: the targets'
 * picture loaded with the screen off) stays the game's code. */
#include "rt.h"
#include "../video.h"

#define STEP         0xFF6C1C
#define WAIT         0xFF6C1A   /* (step 2 on: bytes: the targets shown, frames to the next) */
#define SHOWN        0xFF6C1A
#define NEXT_IN      0xFF6C1B
#define BUTTONS      0xFF17B9   /* (byte) the buttons pressed: A, B, C (bits 4-6), start */
#define VDP_MODE2    0xFFC07C
#define VDP_MODE4    0xFFC092

static int hurried(void)
{
    set8(BUTTONS, ram8(BUTTONS) & 0x70);
    return ram8(BUTTONS) != 0;
}

/* (sprites from a table: d0 picture, d1 x, d2 y, d4 height, d3 width each) */
static void sprites_from(uint32_t loop, uint32_t jsr, uint32_t dbra)
{
    for (;;) {
        set_lo(0, mem16(REG_A[0]));
        set_lo(1, mem16(REG_A[0] + 2));
        set_lo(2, mem16(REG_A[0] + 4));
        set_lo(4, mem16(REG_A[0] + 6));
        set_lo(3, mem16(REG_A[0] + 8));
        REG_A[0] += 10;
        call(0x12E64, jsr);
        uint16_t c = lo(REG_D[7]);
        set_lo(7, (uint16_t)(c - 1));
        if (!c) { ran_to(dbra + 4); return; }
        branched(dbra, loop);
    }
}

/* (the targets' frame: the sprite count kept at the targets shown, the targets' boxes
 * on every other 2 frames; then the sprite list's end) */
static int targets_frame(int from_count)
{
    if (from_count) {
        set_lo(7, (uint16_t)(int16_t)(int8_t)ram8(SHOWN));
        set16(SPRITE_COUNT, lo(REG_D[7]));
    }
    set_lo(0, ram16(WAIT));
    if (lo(REG_D[0]) & 2) { branched(0x11920, 0x11942); }
    else {
        REG_A[0] = 0x11C96;
        set_lo(7, 0xF);
        ran_to(0x1192E);
        sprites_from(0x1192E, 0x11938, 0x1193E);
    }
    branched(0x11942, 0x125EC);
    return jump_to(0x125EC);
}

/* sub_11460 (step 0): the building shown; at the wait's end (or hurried) the targets'
 * set-up (0x11476). Its two figures, and the "stage" box blinking (table 0x112AC). */
int logic_stagein_building(void)
{
    if (!start()) return 0;
    begin(0x11460);
    if (hurried()) { branched(0x11468, 0x11476); return jump_to(0x11476); }
    set16(WAIT, (uint16_t)(ram16(WAIT) - 1));
    if (!ram16(WAIT)) { ran_to(0x11476); return jump_to(0x11476); }
    branched(0x11472, 0x1155C);
    set_lo(0, 0);
    call(0x05D88, 0x1155E);
    REG_D[0] = 2;
    call(0x05D88, 0x11566);
    REG_D[7] = ram16(SPRITE_COUNT);
    set_lo(7, (uint16_t)(lo(REG_D[7]) << 3));
    REG_A[0] = 0xFF0000 + REG_D[7];
    set_lo(0, ram16(WAIT));
    if (!(REG_D[0] & 2)) {
        set32(REG_A[0], 0); set32(REG_A[0] + 4, 0);
        REG_A[0] += 8;
        ran_to(0x11598);
        return done();
    }
    branched(0x11588, 0x1159A);
    REG_A[1] = 0x112AC;
    for (;;) {
        set_lo(7, mem16(REG_A[1])); REG_A[1] += 2;
        if (lo(REG_D[7]) == 0xFFFF) { branched(0x115A6, 0x115D2); break; }
        set16(REG_A[0], lo(REG_D[7]));
        set16(REG_A[0] + 6, mem16(REG_A[1])); REG_A[1] += 2;
        set_lo(7, (uint16_t)((ram16(SPRITE_COUNT) + 1) | mem16(REG_A[1]))); REG_A[1] += 2;
        set16(REG_A[0] + 2, lo(REG_D[7]));
        set_lo(7, (uint16_t)(mem16(REG_A[1]) | mem16(REG_A[1] + 2))); REG_A[1] += 4;
        set16(REG_A[0] + 4, lo(REG_D[7]));
        REG_A[0] += 8;
        set16(SPRITE_COUNT, (uint16_t)(ram16(SPRITE_COUNT) + 1));
        branched(0x115D0, 0x115A0);
    }
    return done();
}

/* sub_1180C (step 1): the targets' picture shown a while; then they are counted up */
int logic_stagein_targets(void)
{
    if (!start()) return 0;
    begin(0x1180C);
    if (hurried()) branched(0x11814, 0x11822);
    else {
        set16(WAIT, (uint16_t)(ram16(WAIT) - 1));
        if (ram16(WAIT)) { branched(0x1181E, 0x11916); return targets_frame(0); }
        ran_to(0x11822);
    }
    REG_A[0] = 0x11A20;
    set_lo(0, (uint16_t)(ram16(STAGE_ENTRY) << 1));
    set_lo(0, mem16(0x11A20 + (int16_t)lo(REG_D[0])));
    set16(WAIT, 1);
    set16(STEP, (uint16_t)(ram16(STEP) + 1));
    ran_to(0x11840);
    return done();
}

/* (start pressed or the end: the fade out (5 frames a step), step 4, the sound off) */
static int stagein_leave(void)
{
    stack_put(-4, REG_A[6]);
    set16(0xFF17AE, 5);
    set16(0xFF17B0, 5);
    set16(0xFF17B2, 8);
    set16(STEP, 4);
    set_lo(0, 0xE0);
    call(0x02D54, 0x11900);
    ran_to(0x11906);
    return done();
}

/* sub_11842 (step 2): the targets counted up, one more each so many frames (table
 * 0x11A2E: the stage's count, then each one's sprite) with a sound; then a wait */
int logic_stagein_count(void)
{
    if (!start()) return 0;
    begin(0x11842);
    if (hurried()) { branched(0x1184A, 0x118DA); return stagein_leave(); }
    set8(NEXT_IN, (uint8_t)(ram8(NEXT_IN) - 1));
    if (ram8(NEXT_IN)) { branched(0x11854, 0x11908); return targets_frame(1); }
    set_lo(0, (uint16_t)(ram16(STAGE_ENTRY) << 2));
    REG_A[0] = mem32(0x11A2E + (int16_t)lo(REG_D[0]));
    REG_D[7] = ram8(SHOWN);
    if (lo(REG_D[7]) == mem16(REG_A[0])) {
        branched(0x11872, 0x118B4);
        set8(NEXT_IN, 0x3C);
        set16(STEP, (uint16_t)(ram16(STEP) + 1));
        ran_to(0x118C2);
        return done();
    }
    set8(SHOWN, (uint8_t)(ram8(SHOWN) + 1));
    REG_A[0] += 2;
    ran_to(0x1187E);
    sprites_from(0x1187E, 0x11888, 0x1188E);
    set_lo(0, 0x98);
    call(0x02D54, 0x11896);
    REG_A[0] = 0x11A20;
    set_lo(0, ram16(STAGE_ENTRY));
    REG_D[0] = (REG_D[0] & 0xFFFFFF00u) | (uint8_t)RD8(0x11A20 + (int16_t)lo(REG_D[0]));
    set8(NEXT_IN, (uint8_t)REG_D[0]);
    branched(0x118B0, 0x11916);
    return targets_frame(0);
}

/* sub_118C4 (step 3): the last wait; then the fade out */
int logic_stagein_wait(void)
{
    if (!start()) return 0;
    begin(0x118C4);
    if (hurried()) { branched(0x118CC, 0x118DA); return stagein_leave(); }
    set8(NEXT_IN, (uint8_t)(ram8(NEXT_IN) - 1));
    if (ram8(NEXT_IN)) { branched(0x118D6, 0x11908); return targets_frame(1); }
    ran_to(0x118DA);
    return stagein_leave();
}

/* sub_11946 (step 4): faded out; then the screen off and the stage's start (mode $C) */
int logic_stagein_fade(void)
{
    if (!start()) return 0;
    begin(0x11946);
    REG_A[6] = 0xFF0200;
    set_lo(7, 0x3F);
    call(0x02BB4, 0x11950);
    if (FLAG_Z) { branched(0x11956, 0x119AE); return done(); }
    set16(VDP_MODE2, ram16(VDP_MODE2) & 0x81BF);
    video_reg_word(ram16(VDP_MODE2));
    call(0x00522, 0x1196C);
    set16(VDP_MODE4, ram16(VDP_MODE4) & 0x8CF7);
    video_reg_word(ram16(VDP_MODE4));
    set16(VDP_MODE2, ram16(VDP_MODE2) | 0x8140);
    video_reg_word(ram16(VDP_MODE2));
    call(0x01928, 0x11996);
    set16(GAME_MODE, 0xC);
    set32(SCREEN_RUN, 0x2E06);
    ran_to(0x119AE);
    return done();
}
