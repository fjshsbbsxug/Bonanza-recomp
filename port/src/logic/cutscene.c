/* The story cutscene (the screen at 0x23948: the robbers' van and the guards; its
 * pictures by the video routines in draw.c): a step a frame (TITLE_STATE), its script
 * (table 0x3BB4A, times 0x3BB00). */
#include "rt.h"
#include "../video.h"
#include "setup.h"

#define STEP             0xFF6C5C
#define SCROLL_A         0xFF6C54   /* the planes' scroll: A's and B's vertical, the */
#define SCROLL_B         0xFF6C56   /*   horizontal one */
#define SCROLL_X         0xFF6C58
#define FIGURE           0xFF6C5A   /* the running figure's step */
#define SCRIPT_AT        0xFF6C68   /* the script's place, and the frame of its next */
#define SCRIPT_NEXT      0xFF6C66

static int scene1_setup(void);
static int scene2_setup(void);

/* sub_23AB6: the cutscene's frame: the scroll set, then its step (table 0x23B10) */
int logic_cutscene_frame(void)
{
    if (!start()) return 0;
    begin(0x23AB6);
    set16(SPRITE_COUNT, 0);
    set16(FRAME_COUNT, (uint16_t)(ram16(FRAME_COUNT) + 1));
    video_at_long(0x40000010);
    video_put(ram16(SCROLL_A));
    video_at_long(0x40020010);
    video_put(ram16(SCROLL_B));
    video_at_long(0x70020002);
    video_put(ram16(SCROLL_X));
    set_lo(0, (uint16_t)(ram16(STEP) << 2));
    REG_A[0] = rom32(0x23B10 + (int16_t)lo(REG_D[0]));
    ran_to(0x23B10);                                  /* (jmp (a0)) */
    return jump_to(REG_A[0]);
}

/* sub_23B28 (step 0): the wait; then the scene set up (its video part, 0x23B32) */
int logic_cutscene_wait(void)
{
    if (!start()) return 0;
    begin(0x23B28);
    uint16_t t = (uint16_t)(ram16(SCREEN_WAIT) - 1);
    set16(SCREEN_WAIT, t);
    if (t) { branched(0x23B2E, 0x23BF2); return done(); }
    ran_to(0x23B32);
    return scene1_setup();
}

/* (the scene scrolling down every other frame) */
static int cutscene_scroll(void)
{
    if (!(ram8(FRAME_COUNT + 1) & 1)) { branched(0x23C4A, 0x23C54); return done(); }
    set16(SCROLL_A, (uint16_t)(ram16(SCROLL_A) + 1));
    ran_to(0x23C54);
    return done();
}

/* sub_23C0C (step 1): faded up (the scroll going on); then step 2, for 296 frames */
int logic_cutscene_fade_up(void)
{
    if (!start()) return 0;
    begin(0x23C0C);
    REG_A[6] = 0xFF0200; REG_A[5] = 0xFF0720; set_lo(7, 0x3F);
    call(0x02AEA, 0x23C1C);
    if (FLAG_Z) { branched(0x23C22, 0x23C42); return cutscene_scroll(); }
    set16(STEP, 2);
    set16(SCREEN_WAIT, 0x128);
    ran_to(0x23C36);
    return done();
}

/* sub_23C38 (step 2): scrolling; at the time's end the fade out (step 3) */
int logic_cutscene_scroll(void)
{
    if (!start()) return 0;
    begin(0x23C38);
    uint16_t t = (uint16_t)(ram16(SCREEN_WAIT) - 1);
    set16(SCREEN_WAIT, t);
    if (t) { ran_to(0x23C42); return cutscene_scroll(); }
    branched(0x23C3E, 0x23C56);
    fade_speed(0x10, 0x10, 8);
    set16(STEP, 3);
    ran_to(0x23C78);
    return done();
}

/* sub_23C7A (step 3): faded out (all but the last 5 colors); then the second scene
 * (its video part, 0x23C8E) */
int logic_cutscene_fade_out(void)
{
    if (!start()) return 0;
    begin(0x23C7A);
    REG_A[6] = 0xFF0200; set_lo(7, 0x3A);
    call(0x02BB4, 0x23C84);
    if (FLAG_Z) { branched(0x23C8A, 0x23D72); return done(); }
    ran_to(0x23C8E);
    return scene2_setup();
}

/* (the second scene's frame from 0x23D96: the figure running (its picture every 4
 * frames, table 0x23EFE), the van's lights (every 4 frames), the 16 objects' pictures
 * (sub_05D88), the guards' sprites (sub_125EC); the shadow and highlight on every other
 * frame (the video parts at 0x23E16 / 0x23E2A)) */
static int scene_rest(void);
static int cutscene_scene(void)
{
    set_lo(0, ram16(FRAME_COUNT) & 3);
    if (!lo(REG_D[0])) {
        set16(OBJ_FRAME, 0);
        uint16_t f = (uint16_t)(ram16(FIGURE) + 1);
        set16(FIGURE, f);
        if (f < 3) branched(0x23DBA, 0x23DDE);
        else { set16(FIGURE, 0); ran_to(0x23DC4); branched(0x23DC4, 0x23DDE); }
    } else {
        branched(0x23DA0, 0x23DC8);
        set_lo(0, (uint16_t)(ram16(FIGURE) << 1));
        REG_A[0] = 0x23EFE;
        set16(OBJ_FRAME, rom16(0x23EFE + (int16_t)lo(REG_D[0])));
        ran_to(0x23DDE);
    }
    set_lo(0, (uint16_t)(((ram16(FRAME_COUNT) & 4) >> 2) + 0x3B9));
    set16(OBJ_FRAME + 2, lo(REG_D[0]));
    REG_D[0] = 0;
    REG_D[1] = 0xF;
    ran_to(0x23DF8);
    for (;;) {
        call(0x05D88, 0x23DF8);
        set_lo(0, (uint16_t)(lo(REG_D[0]) + 2));
        uint16_t c = lo(REG_D[1]);
        set_lo(1, (uint16_t)(c - 1));
        if (!c) { ran_to(0x23E04); break; }
        branched(0x23E00, 0x23DF8);
    }
    call(0x125EC, 0x23E04);
    if (!(ram8(FRAME_COUNT + 1) & 1)) { branched(0x23E12, 0x23E2A); return jump_to(0x23E2A); }
    ran_to(0x23E16);
    return jump_to(0x23E16);
}

/* sub_23E74: the script: at its next time, its next command (table 0x3BB4A), the time
 * after (0x3BB00); the scroll across by object 0's step */
static int script_due(void)
{
    set_lo(0, ram16(FRAME_COUNT));
    return lo(REG_D[0]) >= ram16(SCRIPT_NEXT);
}
static void script_scroll(void)
{
    set_lo(0, ram16(OBJ_DX));
    set16(SCROLL_X, (uint16_t)(ram16(SCROLL_X) + lo(REG_D[0])));
    ran_to(0x23EBE);
}
/* sub_23E74: the script: at its next time, its next command (table 0x3BB4A: some run
 * past a frame's end, so the game's code calls it, back at 0x23E98), the time after
 * (0x3BB00); the scroll across by object 0's step */
int logic_cutscene_script(void)
{
    if (!start()) return 0;
    begin(0x23E74);
    if (!script_due()) { branched(0x23E80, 0x23EB2); script_scroll(); return done(); }
    set_lo(0, (uint16_t)(ram16(SCRIPT_AT) << 1));
    REG_A[0] = mem32(0x3BB4A + (int16_t)lo(REG_D[0]));
    ran_to(0x23E98);
    REG_A[7] -= 4; set32(REG_A[7], 0x23E98);
    return jump_to(REG_A[0]);
}
int logic_cutscene_script_after(void)
{
    if (!start()) return 0;
    begin(0x23E98);
    set16(SCRIPT_AT, (uint16_t)(ram16(SCRIPT_AT) + 2));
    set_lo(0, ram16(SCRIPT_AT));
    REG_A[0] = 0x3BB00;
    set16(SCRIPT_NEXT, mem16(0x3BB00 + (int16_t)lo(REG_D[0])));
    ran_to(0x23EB2);
    script_scroll();
    return done();
}

/* sub_23D80 (step 4): the second scene: its script and the guards' frame (sub_14232);
 * at the time's end the fade out (step 5, its video part at 0x23E60) */
int logic_cutscene_scene(void)
{
    if (!start()) return 0;
    begin(0x23D80);
    uint16_t t = (uint16_t)(ram16(SCREEN_WAIT) - 1);
    set16(SCREEN_WAIT, t);
    if (!t) {
        branched(0x23D86, 0x23E3E);
        set16(STEP, 5);
        fade_speed(0x10, 0x10, 8);
        ran_to(0x23E60);
        return jump_to(0x23E60);
    }
    ran_to(0x23D90);                                  /* (jsr sub_23E74) */
    REG_A[7] -= 4; set32(REG_A[7], 0x23D90);
    jumped_to(0x23E74);
    if (script_due()) {                               /* (its command: the game's code
                                                       *  calls it; on at 0x23D90) */
        set_lo(0, (uint16_t)(ram16(SCRIPT_AT) << 1));
        REG_A[0] = mem32(0x3BB4A + (int16_t)lo(REG_D[0]));
        ran_to(0x23E98);
        REG_A[7] -= 4; set32(REG_A[7], 0x23E98);
        return jump_to(REG_A[0]);
    }
    branched(0x23E80, 0x23EB2);
    script_scroll();
    ran_to(0x23EC0);
    REG_A[7] += 4;
    jumped_to(0x23D90);
    return scene_rest();
}

/* (sub_23D80 from 0x23D90, after the script's command) */
int logic_cutscene_scene_rest(void)
{
    if (!start()) return 0;
    begin(0x23D90);
    return scene_rest();
}
static int scene_rest(void)
{
    call(0x14232, 0x23D90);
    return cutscene_scene();
}

/* sub_23EC0 (step 5): faded out (the scene going on); then the attract mode again
 * (stage 1, the screen dispatcher, mode 0) */
int logic_cutscene_end(void)
{
    if (!start()) return 0;
    begin(0x23EC0);
    REG_A[6] = 0xFF0200; set_lo(7, 0x3F);
    call(0x02BB4, 0x23ECA);
    if (FLAG_Z) { ran_to(0x23ED4); branched(0x23ED4, 0x23D96); return cutscene_scene(); }
    branched(0x23ED0, 0x23EDA);
    set16(STAGE_ENTRY, 1);
    set32(SCREEN_RUN, 0x2E06);
    set16(GAME_MODE, 0);
    set16(0xFF5548, 0);
    ran_to(0x23EFC);
    return done();
}

/* The script's commands (table 0x3BB4A); those loading tiles stay the game's code */
#define FIGURE_GO        0xFF2FAE   /* (words) a figure's animation going */

/* a block of the map's tiles drawn (sub_28A90, its table at a6), the tile step $40 */
static void script_rect(uint32_t at, uint32_t table)
{
    REG_A[6] = table;
    set16(0xFF1830, 0x40);
    call(0x28A90, at + 0xE);
}
#define SCRIPT_RECT(name, at, table)                                         \
    int logic_##name(void)                                                   \
    {                                                                        \
        if (!start()) return 0;                                              \
        begin(at);                                                           \
        script_rect(at, table);                                              \
        ran_to(at + 0x14);                                                   \
        return done();                                                       \
    }
SCRIPT_RECT(script_rect1, 0x3BC9A, 0x3C0E2)    /* sub_3BC9A ... */
SCRIPT_RECT(script_rect2, 0x3BCB0, 0x3C0EE)
SCRIPT_RECT(script_rect3, 0x3BCC6, 0x3C0FA)
SCRIPT_RECT(script_rect4, 0x3BCDC, 0x3C106)
SCRIPT_RECT(script_rect5, 0x3BCF2, 0x3C112)
SCRIPT_RECT(script_rect6, 0x3BD08, 0x3C11E)
SCRIPT_RECT(script_rect7, 0x3BD1E, 0x3C12A)
SCRIPT_RECT(script_rect8, 0x3BD34, 0x3C136)    /* ... sub_3BD34 */

/* sub_3BC56: the title's block, by the console's version (overseas, 60 Hz) */
int logic_script_title(void)
{
    if (!start()) return 0;
    begin(0x3BC56);
    REG_D[5] = (REG_D[5] & 0xFFFFFF00u) | (uint8_t)RD8(0xA10001);
    if ((REG_D[5] & 0x80) && !(REG_D[5] & 0x40)) {
        ran_to(0x3BC6C);
        script_rect(0x3BC6C, 0x3C0D6);
        ran_to(0x3BC80);
        return done();
    }
    if (!(REG_D[5] & 0x80)) branched(0x3BC60, 0x3BC82);
    else branched(0x3BC68, 0x3BC82);
    script_rect(0x3BC82, 0x3C0CA);
    ran_to(0x3BC96);
    return done();
}

/* sub_3C0A2: the last block, and the scroll across starting (object 0's step -1) */
int logic_script_last(void)
{
    if (!start()) return 0;
    begin(0x3C0A2);
    script_rect(0x3C0A2, 0x3C142);
    set16(OBJ_DX, 0xFFFF);
    ran_to(0x3C0BE);
    return done();
}

/* sub_3BC4C / sub_3BDBE / sub_3C0C0: the scroll across going (-1) or stopped */
int logic_script_scroll_go(void)
{
    if (!start()) return 0;
    begin(0x3BC4C);
    set16(OBJ_DX, 0xFFFF);
    ran_to(0x3BC54);
    return done();
}
int logic_script_scroll_stop(void)
{
    if (!start()) return 0;
    begin(0x3BDBE);
    set16(OBJ_DX, 0);
    ran_to(0x3BDC6);
    return done();
}
int logic_script_scroll_stop2(void)
{
    if (!start()) return 0;
    begin(0x3C0C0);
    set16(OBJ_DX, 0);
    ran_to(0x3C0C8);
    return done();
}

/* sub_3BDC8: two figures' animations going */
int logic_script_figures(void)
{
    if (!start()) return 0;
    begin(0x3BDC8);
    set16(FIGURE_GO + 8, 3);
    set16(FIGURE_GO + 10, 3);
    ran_to(0x3BDD8);
    return done();
}

/* sub_3BBDE / sub_3BC98: nothing */
int logic_script_none(void)
{
    if (!start()) return 0;
    begin(REG_PC);
    return done();
}

/* (0x23B32: the first scene set up with the screen off: its tiles and two map blocks;
 * then its colours (sub_018FC: a frame can end there, so the game's code calls it; on at
 * 0x23BB6): the scroll, a slow fade up, step 1) */
static int scene1_setup(void)
{
    screen_off();
    call(0x00522, 0x23B44);
    call(0x01928, 0x23B4A);
    tiles(0x55100, 0x40000000, 0xBE, 0, 0x23B64);
    tiles(0x55F14, 0x70200002, 0x37, 0, 0x23B7E);
    map_rect(0x23C00, 0x23B92);
    map_rect(0x23BF4, 0x23BA6);
    set_lo(7, 0x25);
    ran_to(0x23BB6);
    REG_A[7] -= 4; set32(REG_A[7], 0x23BB6);
    return jump_to(0x018FC);
}
int logic_cutscene_setup1(void)
{
    if (!start()) return 0;
    begin(0x23BB6);
    set16(SCROLL_A, 0x150);
    fade_speed(0x10, 0x10, 8);
    set16(STEP, 1);
    screen_on();
    ran_to(0x23BF2);
    return done();
}

/* (0x23C8E: the second scene set up: its tiles and map block, its music, its time
 * ($1130 frames), the scroll, step 4; then its colours, as above (on at 0x23D60)) */
static int scene2_setup(void)
{
    screen_off();
    call(0x00522, 0x23CA0);
    call(0x01928, 0x23CA6);
    tiles(0x296FA, 0x60000002, 0xE8, 0, 0x23CC0);
    tiles(0x6C600, 0x40000001, 0x75, 0, 0x23CDA);
    tiles(0x6A000, 0x4EC00001, 0x8A, 0, 0x23CF4);
    tiles(0x2C552, 0x50000002, 0x45, 0, 0x23D0E);
    map_rect(0x23D74, 0x23D22);
    set_lo(0, 0x86);                                  /* (its music: a frame can end here */
    ran_to(0x23D32);                                  /*  too; on at 0x23D32) */
    REG_A[7] -= 4; set32(REG_A[7], 0x23D32);
    return jump_to(0x02D54);
}
int logic_cutscene_setup2a(void)
{
    if (!start()) return 0;
    begin(0x23D32);
    set16(SCREEN_WAIT, 0x1130);
    set16(SCROLL_B, 0x1CF);
    set16(FRAME_COUNT, 0);
    set16(FIGURE, 0);
    set16(STEP, 4);
    set_lo(7, 0x30);
    ran_to(0x23D60);
    REG_A[7] -= 4; set32(REG_A[7], 0x23D60);
    return jump_to(0x018FC);
}
int logic_cutscene_setup2(void)
{
    if (!start()) return 0;
    begin(0x23D60);
    screen_on();
    ran_to(0x23D72);
    return done();
}
