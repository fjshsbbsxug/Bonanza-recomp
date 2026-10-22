/* The screen before a stage (screen mode 6, set up at 0x11120): the building, then the
 * things to take there shown one at a time; A, B or C hurries it on. A step a frame
 * (STEP, from the frame at 0x113E6); the second part's set-up (0x11476: the targets'
 * picture loaded with the screen off) stays the game's code. */
#include "rt.h"
#include "../video.h"
#include "setup.h"

#define STEP         0xFF6C1C
#define WAIT         0xFF6C1A   /* (step 2 on: bytes: the targets shown, frames to the next) */
#define SHOWN        0xFF6C1A
#define NEXT_IN      0xFF6C1B
#define BUTTONS      0xFF17B9   /* (byte) the buttons pressed: A, B, C (bits 4-6), start */
#define VDP_MODE2    REG_MODE2
#define VDP_MODE4    REG_MODE4

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

/* sub_24938: the coffee break's frame (set up at 0x24536, its frame at 0x248DE): the
 * guards' frame (sub_14232), the 17 objects' pictures, the shadow and highlight every
 * other frame; at the time's end (or A, B or C) the screen off and the next stage's
 * screen (mode $18) */
int logic_coffee_frame(void)
{
    if (!start()) return 0;
    begin(0x24938);
    set_lo(0, ram16(0xFF17B8));
    int end = hurried();
    if (end) branched(0x24946, 0x249A4);
    else {
        set16(WAIT, (uint16_t)(ram16(WAIT) - 1));
        if (!ram16(WAIT)) { end = 1; branched(0x24950, 0x249A4); }
    }
    if (end) {
        set16(VDP_MODE2, ram16(VDP_MODE2) & 0x81BF);
        video_reg_word(ram16(VDP_MODE2));
        call(0x00522, 0x249B6);
        set16(VDP_MODE4, ram16(VDP_MODE4) & 0x8CF7);
        video_reg_word(ram16(VDP_MODE4));
        set16(VDP_MODE2, ram16(VDP_MODE2) | 0x8140);
        video_reg_word(ram16(VDP_MODE2));
        call(0x01928, 0x249E0);
        set16(STAGE_ENTRY, (uint16_t)(ram16(STAGE_ENTRY) + 1));
        set16(GAME_MODE, 0x18);
        set32(SCREEN_RUN, 0x2E06);
        ran_to(0x249FE);
        return done();
    }
    call(0x14232, 0x24954);
    REG_D[0] = 0;
    REG_D[1] = 0x10;
    ran_to(0x2495E);
    for (;;) {
        call(0x05D88, 0x2495E);
        set_lo(0, (uint16_t)(lo(REG_D[0]) + 2));
        uint16_t c = lo(REG_D[1]);
        set_lo(1, (uint16_t)(c - 1));
        if (!c) { ran_to(0x2496A); break; }
        branched(0x24966, 0x2495E);
    }
    call(0x125EC, 0x2496A);
    if (ram8(FRAME_COUNT + 1) & 1) {
        set16(VDP_MODE4, ram16(VDP_MODE4) | 0x8C08);
        video_reg_word(ram16(VDP_MODE4));
        ran_to(0x2498E);
    } else {
        branched(0x24978, 0x24990);
        set16(VDP_MODE4, ram16(VDP_MODE4) & 0x8CF7);
        video_reg_word(ram16(VDP_MODE4));
        ran_to(0x249A2);
    }
    return done();
}

/* ---------------------------------------------------------------- the coffee break's tiles */

struct tile_load { uint32_t src, cmd; uint16_t n; uint8_t raw; uint32_t jsr; };

static int tile_loads(uint32_t r, const struct tile_load *t, int n, uint32_t rts)
{
    if (!start()) return 0;
    begin(r);
    for (int k = 0; k < n; k++) tiles(t[k].src, t[k].cmd, t[k].n, t[k].raw, t[k].jsr);
    ran_to(rts);
    return done();
}

/* sub_246B6 / sub_24720 / sub_2478A: the tiles of the coffee breaks after stages 4, 8
 * and 12 (table 0x246A6 by the stage entry) */
int logic_coffee_tiles4(void)
{
    static const struct tile_load t[] = {
        {0x5A000, 0x60000000, 0x27, 1, 0x246CA}, {0x61040, 0x70000000, 0x2C, 1, 0x246E4},
        {0x6AAB0, 0x60200001, 0x8B, 0, 0x246FE}, {0x6B334, 0x6E800003, 0x11, 0, 0x24718},
    };
    return tile_loads(0x246B6, t, 4, 0x2471E);
}
int logic_coffee_tiles8(void)
{
    static const struct tile_load t[] = {
        {0x60900, 0x55400002, 0x05, 1, 0x24734}, {0x67DE0, 0x56000002, 0x05, 1, 0x2474E},
        {0x2958C, 0x51E00002, 0x1A, 0, 0x24768}, {0x6BC9E, 0x71A00001, 0x3A, 0, 0x24782},
    };
    return tile_loads(0x24720, t, 4, 0x24788);
}
int logic_coffee_tiles12(void)
{
    static const struct tile_load t[] = {
        {0x5A000, 0x60000000, 0x27, 1, 0x2479E}, {0x61040, 0x70000000, 0x2C, 1, 0x247B8},
        {0x5C740, 0x65000000, 0x3F, 1, 0x247D2}, {0x63BA0, 0x75A00000, 0x3F, 1, 0x247EC},
        {0x6C600, 0x40000001, 0x75, 0, 0x24806}, {0x6CDEC, 0x7AC00001, 0x09, 0, 0x24820},
        {0x6CEB0, 0x46800002, 0x0C, 0, 0x2483A}, {0x6CF58, 0x70C00003, 0x11, 0, 0x24854},
        {0x6D08C, 0x48200002, 0x0B, 0, 0x2486E}, {0x6D2EA, 0x44000002, 0x0B, 0, 0x24888},
        {0x6A000, 0x4EC00001, 0x8A, 0, 0x248A2}, {0x6A7D0, 0x49A00002, 0x18, 0, 0x248BC},
        {0x6A94A, 0x6B200003, 0x0D, 0, 0x248D6},
    };
    return tile_loads(0x2478A, t, 13, 0x248DC);
}

/* ---------------------------------------------------------------- the set-ups (long: a
 * frame can end inside them, see begin_long) */

/* (the "stage" boxes' sprites from table 0x112AC at a0, each linked to the next, until
 * -1; the loop's places at `loop`, its beq at `beq` to `out`, its bra at `bra`) */
static void stage_boxes(uint32_t loop, uint32_t beq, uint32_t out, uint32_t bra)
{
    REG_A[1] = 0x112AC;
    ran_to(loop);
    for (;;) {
        set_lo(7, mem16(REG_A[1])); REG_A[1] += 2;
        if (lo(REG_D[7]) == 0xFFFF) { branched(beq, out); return; }
        set16(REG_A[0], lo(REG_D[7]));
        set16(REG_A[0] + 6, mem16(REG_A[1])); REG_A[1] += 2;
        set_lo(7, (uint16_t)((ram16(SPRITE_COUNT) + 1) | mem16(REG_A[1]))); REG_A[1] += 2;
        set16(REG_A[0] + 2, lo(REG_D[7]));
        set_lo(7, (uint16_t)(mem16(REG_A[1]) | mem16(REG_A[1] + 2))); REG_A[1] += 4;
        set16(REG_A[0] + 4, lo(REG_D[7]));
        REG_A[0] += 8;
        set16(SPRITE_COUNT, (uint16_t)(ram16(SPRITE_COUNT) + 1));
        branched(bra, loop);
    }
}

/* sub_11120: the building's screen set up: its tiles, its map block, the "stage" boxes,
 * the two figures' places and pictures (by the console's version and the stage), the
 * shadow and highlight, 240 frames, its frame (0x113E6), its music and colours */
int logic_stagein_setup(void)
{
    begin_long(0x11120);
    screen_off();
    call(0x01928, 0x11132);
    call(0x00522, 0x11138);
    tiles(0x5447E, 0x40000000, 0xFF, 0, 0x11152);
    tiles(0x296FA, 0x60000000, 0xE8, 0, 0x1116C);
    tiles(0x40020, 0x40000001, 0xFF, 0, 0x11186);
    map_block(0x7DFC0, 0x7DD48, 0xC000, 0xF, 0xF, 0x111B2);
    REG_A[0] = 0xFF0000;
    stage_boxes(0x111C4, 0x111CA, 0x111F6, 0x111F4);
    set16(0xFF304E, 0x100);
    set16(0xFF30FA, 0xC0);
    set16(0xFF3050, 0x100);
    set16(0xFF30FC, 0xAC);
    REG_D[5] = (REG_D[5] & 0xFFFFFF00u) | (uint8_t)(RD8(0xA10001) & 0x80);
    if ((uint8_t)REG_D[5]) { REG_A[0] = 0x113AE; branched(0x11228, 0x11230); }
    else { branched(0x11220, 0x1122C); REG_A[0] = 0x11392; ran_to(0x11230); }
    set_lo(0, (uint16_t)(ram16(STAGE_ENTRY) << 1));
    set16(OBJ_FRAME, mem16(REG_A[0] + (int16_t)lo(REG_D[0])));
    REG_A[0] = 0x113CA;
    set16(OBJ_FRAME + 2, mem16(0x113CA + (int16_t)lo(REG_D[0])));
    reg_keep(REG_MODE4, ram16(REG_MODE4) | 0x8C08);
    set16(WAIT, 0xF0);
    set16(STEP, 0);
    set32(SCREEN_RUN, 0x113E6);
    set_lo(0, 0x87);
    call(0x02D54, 0x1127A);
    REG_A[0] = 0x11376;
    set_lo(0, (uint16_t)(ram16(STAGE_ENTRY) << 1));
    set_lo(7, mem16(0x11376 + (int16_t)lo(REG_D[0])));
    call(0x018FC, 0x11292);
    screen_on();
    ran_to(0x112AA);
    return done_long();
}

/* sub_11476 (step 0's end): the targets' screen: the stage's tiles (table 0x119B0), the
 * targets' frame tiles and colours (0x115D4), the shadow and highlight; then the targets'
 * picture (0x114F0, a video part) */
int logic_stagein_targets_setup(void)
{
    begin_long(0x11476);
    screen_off();
    call(0x00522, 0x11488);
    screen_on();
    set_lo(0, (uint16_t)(ram16(STAGE_ENTRY) << 2));
    REG_A[0] = mem32(0x119B0 + (int16_t)lo(REG_D[0]));
    call(REG_A[0], 0x114B0);
    tiles(0x51CFA, 0x40000002, 0x1D, 0, 0x114C6);
    REG_A[0] = 0x115D4;
    set_lo(0, (uint16_t)(lo(REG_D[0]) >> 1));
    set_lo(7, mem16(0x115D4 + (int16_t)lo(REG_D[0])));
    call(0x018FC, 0x114D6);
    reg_keep(REG_MODE4, ram16(REG_MODE4) | 0x8C08);
    ran_to(0x114F0);
    return rest_long(0x114F0);
}

/* sub_24536: the coffee break set up: the screen off, the work memory cleared (sub_00470:
 * a frame ends inside it, so the game's code runs it; on at 0x2454E) */
int logic_coffee_setup(void)
{
    if (!start()) return 0;
    begin(0x24536);
    screen_off();
    ran_to(0x2454E);
    REG_A[7] -= 4; set32(REG_A[7], 0x2454E);
    return jump_to(0x00470);
}

/* (0x2454E, long) the palettes, sprites and planes cleared, the stage's map, objects and
 * guards as for play, its tiles (and the break's own, table 0x246A6), its map block and
 * colours, its music; 240 frames, the scroll, its frame (0x248DE); the figures' animations
 * going (table 0x2469E: how many), the scroll set, the screen on */
int logic_coffee_setup2(void)
{
    begin_long(0x2454E);
    call(0x01928, 0x2454E);
    call(0x00522, 0x24554);
    call(0x006FA, 0x2455A);
    call(0x0FF22, 0x24560);
    call(0x0156C, 0x24566);
    call(0x141F8, 0x2456C);
    reg_keep(REG_MODE4, 0x8C00);
    planes_size(0x9011);
    tiles(0x55100, 0x40000000, 0xBE, 0, 0x245B0);
    REG_D[0] &= 0xFFFF0000u;
    set_lo(0, ram16(STAGE_ENTRY));
    REG_A[0] = mem32(0x246A6 + (int16_t)lo(REG_D[0]));
    call(REG_A[0], 0x245C6);
    map_rect(0x24692, 0x245D6);
    set_lo(7, 0x30);
    call(0x018FC, 0x245E0);
    set_lo(0, 0x8C);
    call(0x02D54, 0x245EA);
    set16(WAIT, 0xF0);
    set16(STEP, 0);
    set16(FRAME_COUNT, 0);
    set16(0xFF6C54, 0x224);
    set16(0xFF6C56, 0x1CF);
    set32(SCREEN_RUN, 0x248DE);
    set_lo(0, 0);
    set_lo(0, (uint16_t)(ram16(STAGE_ENTRY) >> 1));
    REG_A[0] = 0x2469E;
    set_lo(0, mem16(0x2469E + (int16_t)lo(REG_D[0])));
    REG_A[0] = 0xFF2FAE;
    ran_to(0x2463C);
    for (;;) {
        set16(REG_A[0], 3);
        REG_A[0] += 2;
        uint16_t c = lo(REG_D[0]);
        set_lo(0, (uint16_t)(c - 1));
        if (!c) { ran_to(0x24644); break; }
        branched(0x24640, 0x2463C);
    }
    scroll_put(0x40000010, ram16(0xFF6C54));
    scroll_put(0x40020010, ram16(0xFF6C56));
    scroll_put(0x70020002, 0);
    screen_on();
    ran_to(0x24690);
    return done_long();
}
