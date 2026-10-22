/* The screens around the game: the screen dispatcher, the Sega logo and title screen,
 * the title menu (1 player, 2 players, options), the OPTIONS menu, and the attract
 * demo's start. Their pictures are drawn by the video routines (draw.c, the segments
 * the recompiler marks); this is what they do between. */
#include "rt.h"
#include "../video.h"

#define TITLE_STATE      0xFF6C5C   /* the title screen's (and the cutscenes') step */
#define TITLE_SCROLL     0xFF6C54
#define MENU_ITEM        0xFF554A   /* the menu's item under the cursor */
#define MENU_PLAYERS     0xFF554C   /* 1 or 2 players chosen */
#define MENU_FROM_DEMO   0xFF5548
#define FADE             0xFF17AE   /* the fade: frames to its next step, per step, steps */
#define VDP_MODE2        0xFFC07C   /* (the video register 1 the game keeps) */

/* (the fade set going: movem.l a6, -(a7); lea FADE, a6; three words; movem.l (a7)+, a6) */
static void fade_set(uint16_t wait, uint16_t step, uint16_t steps)
{
    stack_put(-4, REG_A[6]);
    set16(FADE, wait);
    set16(FADE + 2, step);
    set16(FADE + 4, steps);
}

/* (a routine's tail that is a video segment, or another screen routine: the game's code
 * runs on from there, its rts this one's) */
static int go_on_at(uint32_t at, uint32_t to)
{
    branched(at, to);
    return jump_to(to);
}

/* (the screen's routine changed: SCREEN_RUN) */
static void screen_next(uint32_t r) { set32(SCREEN_RUN, r); }

/* ---------------------------------------------------------------- the dispatcher */

/* sub_02E06: the screen set up by GAME_MODE (table 0x2E14: logo, title, menu, a stage,
 * options, the attract demo, extra stages, the ending), called by a jsr of the C's own:
 * a stage's set-up takes several frames, which end inside it as in the game's code */
int logic_screen_mode(void)
{
    if (!start()) return 0;
    begin(0x02E06);
    set_lo(0, ram16(GAME_MODE));
    REG_A[0] = rom32(0x02E14 + (int16_t)lo(REG_D[0]));
    ran_to(0x02E12);                                  /* (jsr (a0)) */
    REG_A[7] -= 4;
    set32(REG_A[7], 0x02E12);
    return jump_to(REG_A[0]);
}

/* ---------------------------------------------------------------- the Sega logo */

/* sub_02F4A: once the text jobs are done, the logo fades up ($2F7A) */
int logic_logo_wait(void)
{
    if (!start()) return 0;
    begin(0x02F4A);
    if (ram16(0xFF554E)) { branched(0x02F50, 0x02F78); return done(); }
    screen_next(0x2F7A);
    fade_set(5, 5, 8);
    ran_to(0x02F78);
    return done();
}

/* sub_02F7A: the logo's colors (0x2FA0) faded up; then it shines ($3012) */
int logic_logo_fade_up(void)
{
    if (!start()) return 0;
    begin(0x02F7A);
    REG_A[6] = 0xFF0200; REG_A[5] = 0x02FA0; set_lo(7, 0xF);
    call(0x02AEA, 0x02F8A);
    if (FLAG_Z) { branched(0x02F90, 0x02F9E); return done(); }
    screen_next(0x3012);
    ran_to(0x02F9E);
    return done();
}

/* sub_02FC0: the logo faded out; then the title (mode 4, sub_02FD4's video part) */
int logic_logo_fade_out(void)
{
    if (!start()) return 0;
    begin(0x02FC0);
    REG_A[6] = 0xFF0200; set_lo(7, 0xF);
    call(0x02BB4, 0x02FCA);
    if (FLAG_Z) { branched(0x02FD0, 0x03010); return done(); }
    ran_to(0x02FD4);
    return jump_to(0x02FD4);
}

/* sub_03050: the logo's shine: every 3 frames, 11 colors from the next place in the
 * table at 0x308C (2 back each time) into the palette, until it runs out */
static void logo_shine(void)
{
    uint16_t t = (uint16_t)(ram16(0xFF17C8) - 1);
    set16(0xFF17C8, t);
    if (t) { branched(0x03056, 0x0308A); return; }
    set16(0xFF17C8, 3);
    set_lo(0, ram16(0xFF17C6));
    if (lo(REG_D[0]) & 0x8000) { branched(0x03068, 0x0308A); return; }
    set16(0xFF17C6, (uint16_t)(ram16(0xFF17C6) - 2));
    uint32_t a0 = 0x0308C + (uint32_t)(int32_t)(int16_t)lo(REG_D[0]), a1 = 0xFF0204;
    ran_to(0x0307C);
    for (int k = 0; k < 11; k++, a0 += 2, a1 += 2) {
        set16(a1, mem16(a0));
        if (k < 10) branched(0x0307E, 0x0307C);
    }
    set_lo(0, 0xFFFF);
    REG_A[0] = a0; REG_A[1] = a1;
    set16(0xFF17C4, 1);
    ran_to(0x0308A);
}
int logic_logo_shine(void) { if (!start()) return 0; begin(0x03050); logo_shine(); return done(); }

/* sub_03012: the logo shines until its time is up or start is pressed (only start kept
 * of the buttons pressed); then it fades out ($2FC0) */
int logic_logo_frame(void)
{
    if (!start()) return 0;
    begin(0x03012);
    set8(0xFF17B9, ram8(0xFF17B9) & 0x80);
    if (ram8(0xFF17B9)) branched(0x0301A, 0x0302A);
    else {
        ran_to(0x03020);                              /* (bsr.b $3050) */
        stack_put(-4, 0x03020);
        jumped_to(0x03050);
        logo_shine();
        ran_to(0x0308C);                              /* (its rts) */
        jumped_to(0x03020);
        uint16_t t = (uint16_t)(ram16(SCREEN_WAIT) - 1);
        set16(SCREEN_WAIT, t);
        if (t) { branched(0x03026, 0x0304E); return done(); }
        ran_to(0x0302A);
    }
    screen_next(0x2FC0);
    fade_set(3, 3, 8);
    ran_to(0x0304E);
    return done();
}

/* ---------------------------------------------------------------- the title screen */

/* sub_03622: the title's colors cycled (presets $26.. by a counter, a step every 4) */
static void title_colors(uint32_t jsr)
{
    set8(0xFF6C5A, (uint8_t)((ram8(0xFF6C5A) + 1) & 0xF));
    set_lo(7, 0x26);
    set_lo(0, ram8(0xFF6C5A));
    set_lo(0, (uint16_t)(lo(REG_D[0]) >> 2));
    set_lo(7, (uint16_t)(lo(REG_D[7]) + lo(REG_D[0])));
    call(0x018FC, jsr);
}
int logic_title_colors(void)
{
    if (!start()) return 0;
    begin(0x03622);
    title_colors(0x03640);
    ran_to(0x03646);
    return done();
}

/* sub_03648: the title's running figure: its two sprites (table 0x3724 by its step) at
 * object 0's place, the step on every 8 frames; it moves left a pixel every 4 frames
 * ($3050 / $3052 counting) */
int logic_title_runner(void)
{
    if (!start()) return 0;
    begin(0x03648);
    uint8_t step = ram8(0xFF6C5B);
    for (int k = 0; k < 2; k++) {
        set_lo(1, ram16(OBJ_Y));
        set_lo(2, k ? 0x700 : 0x701);
        REG_A[0] = 0x03724;
        set_lo(3, (uint16_t)(rom16(0x03724 + ((uint16_t)(step << 1) & 0xFF)) + (k ? 0x8008 : 0x8000)));
        REG_D[3] &= 0xFFFFu;
        set_lo(4, (uint16_t)(ram16(OBJ_X) + (k ? 8 : 0)));
        REG_A[0] = SPRITE_TABLE;
        set16(SPRITE_TABLE + 8 * k, lo(REG_D[1]));
        set16(SPRITE_TABLE + 8 * k + 2, lo(REG_D[2]));
        set16(SPRITE_TABLE + 8 * k + 4, lo(REG_D[3]));
        set16(SPRITE_TABLE + 8 * k + 6, lo(REG_D[4]));
    }
    set16(OBJ_ANIM_AT, (uint16_t)((ram16(OBJ_ANIM_AT) + 1) & 7));
    if (ram16(OBJ_ANIM_AT)) branched(0x036D8, 0x036EA);
    else { set8(0xFF6C5B, (uint8_t)((ram8(0xFF6C5B) + 1) & 3)); ran_to(0x036EA); }
    set_lo(0, ram16(OBJ_ANIM_AT) & 1);
    if (lo(REG_D[0])) branched(0x036F4, 0x036F8); else ran_to(0x036F8);
    set16(OBJ_X + 2, (uint16_t)((ram16(OBJ_X + 2) + 1) & 1));
    if (!ram16(OBJ_X + 2)) { branched(0x03706, 0x03722); return done(); }
    set16(OBJ_X + 4, (uint16_t)((ram16(OBJ_X + 4) + 1) & 1));
    if (!ram16(OBJ_X + 4)) { branched(0x03718, 0x03722); return done(); }
    set16(OBJ_X, (uint16_t)(ram16(OBJ_X) - 1));
    ran_to(0x03722);
    return done();
}

/* (start pressed on the title: the fade out, a sound; to the menu ($5548 set)) */
static void title_start(void)
{
    fade_set(2, 2, 8);
    set_lo(0, 0xE0);
    call(0x02D54, 0x034A2);
    set16(MENU_FROM_DEMO, 1);
    set16(TITLE_STATE, 3);
    ran_to(0x034B8);
}
int logic_title_start(void) { if (!start()) return 0; begin(0x03484); title_start(); return done(); }

/* (the title going on: the scroll on while object 1's counter says, the runner, colors) */
static int title_go_on(void)
{
    if (!ram16(OBJ_X + 2)) branched(0x03500, 0x0350A);
    else { set16(TITLE_SCROLL, (uint16_t)(ram16(TITLE_SCROLL) + 1)); ran_to(0x0350A); }
    call(0x03648, 0x0350A);
    call(0x03622, 0x03510);
    ran_to(0x03516);
    return done();
}

/* sub_034BA (step 0): faded up from black; then step 1, for 402 frames */
int logic_title_fade_up(void)
{
    if (!start()) return 0;
    begin(0x034BA);
    REG_A[6] = 0xFF0200; REG_A[5] = 0xFF0720; set_lo(7, 0x3F);
    call(0x02AEA, 0x034CA);
    if (FLAG_Z) { branched(0x034D0, 0x034FA); return title_go_on(); }
    set16(TITLE_STATE, 1);
    set16(SCREEN_WAIT, 0x192);
    ran_to(0x034E4);
    return done();
}

/* sub_034E6 (step 1): start goes to the menu; at the time's end "PUSH START" shown
 * (step 2, 360 frames) */
int logic_title_scroll(void)
{
    if (!start()) return 0;
    begin(0x034E6);
    set8(0xFF17B9, ram8(0xFF17B9) & 0x80);
    if (ram8(0xFF17B9)) { branched(0x034EE, 0x03484); title_start(); return done(); }
    uint16_t t = (uint16_t)(ram16(SCREEN_WAIT) - 1);
    set16(SCREEN_WAIT, t);
    if (t) { ran_to(0x034FA); return title_go_on(); }
    branched(0x034F6, 0x03518);
    REG_A[6] = 0x03388;
    call(0x011B8, 0x0351C);
    call(0x03622, 0x03522);
    set16(TITLE_STATE, 2);
    set16(SCREEN_WAIT, 0x168);
    ran_to(0x03538);
    return done();
}

/* sub_0353A (step 2): "PUSH START" blinking (every 8 frames), start to the menu; at the
 * time's end the fade out (step 3) */
int logic_title_push_start(void)
{
    if (!start()) return 0;
    begin(0x0353A);
    set8(0xFF17B9, ram8(0xFF17B9) & 0x80);
    if (ram8(0xFF17B9)) { branched(0x03542, 0x03484); title_start(); return done(); }
    call(0x03648, 0x03546);
    uint16_t t = (uint16_t)(ram16(SCREEN_WAIT) - 1);
    set16(SCREEN_WAIT, t);
    if (!t) {
        branched(0x03552, 0x0357C);
        fade_set(2, 2, 8);
        set16(TITLE_STATE, 3);
        ran_to(0x0359E);
        return done();
    }
    if (!(ram8(SCREEN_WAIT + 1) & 8)) { branched(0x0355E, 0x0356A); REG_A[6] = 0x0336E; }
    else { REG_A[6] = 0x03354; ran_to(0x03566); branched(0x03566, 0x0356E); }
    call(0x011B8, 0x0356E);
    call(0x03622, 0x03574);
    ran_to(0x0357A);
    return done();
}

/* sub_035A0 (step 3): faded out (the runner still going); then step 4 */
int logic_title_fade_out(void)
{
    if (!start()) return 0;
    begin(0x035A0);
    REG_A[6] = 0xFF0200; set_lo(7, 0x3F);
    call(0x02BB4, 0x035AA);
    if (FLAG_Z) return go_on_at(0x035B0, 0x03648);
    set16(TITLE_STATE, 4);
    ran_to(0x035BC);
    return done();
}

/* ---------------------------------------------------------------- the title menu */

/* sub_0372C: the menu shown (its texts at 0x3784, 0x3A1E), its palette, nothing chosen */
int logic_menu_start(void)
{
    if (!start()) return 0;
    begin(0x0372C);
    REG_A[6] = 0x03784;
    set_lo(7, 0x8000);
    for (int k = 0; k < 3; k++) call(0x011F2, 0x03734 + 6 * k);
    REG_A[6] = 0x03A1E;
    call(0x011B8, 0x0374A);
    REG_D[7] = 0;
    call(0x018FC, 0x03752);
    REG_D[0] = 0;
    set16(MENU_ITEM, 0);
    set16(MENU_FROM_DEMO, 0);
    set16(TITLE_STATE, 0);
    for (int k = 0; k < 3; k++) set16(0xFF182A + 2 * k, 0);
    REG_A[6] = 0xFF182E;
    screen_next(0x37C0);
    ran_to(0x03782);
    return done();
}

/* sub_037C0: the menu's step (table 0x37D2) */
int logic_menu_frame(void)
{
    if (!start()) return 0;
    begin(0x037C0);
    set_lo(0, (uint16_t)(ram16(TITLE_STATE) << 2));
    REG_A[0] = rom32(0x037D2 + (int16_t)lo(REG_D[0]));
    ran_to(0x037D2);                                  /* (jmp (a0)) */
    return jump_to(REG_A[0]);
}

/* sub_037DE (step 0): the menu's lines drawn (the second one dimmed unless $C066 is
 * $1A); up and down move the cursor over its three lines (the second skipped unless
 * $C066 is $1A); start chooses: 1 player if player 1 pressed it, else 2; the fade out
 * (step 1) */
int logic_menu_choose(void)
{
    if (!start()) return 0;
    begin(0x037DE);
    REG_A[6] = 0xFF182A;
    set_lo(7, 0x8000);
    set16(0xFF182A, 0x8000);
    set_lo(7, 0x8000);
    if (ram8(0xFFC066) == 0x1A) branched(0x037F8, 0x03800);
    else { set_lo(7, 0xA000); ran_to(0x03800); }
    set16(0xFF182C, lo(REG_D[7]));
    REG_A[6] = 0x03784;
    set_lo(7, ram16(0xFF182A));
    call(0x011F2, 0x0380E);
    set_lo(7, ram16(0xFF182C));
    call(0x011F2, 0x0381A);
    set_lo(0, ram16(0xFF17B8) & 0xFF);
    uint8_t pad = (uint8_t)lo(REG_D[0]);
    if (!pad) { branched(0x0382A, 0x038DE); return done(); }
    set_lo(1, pad & 0x80);
    if (pad & 0x80) {
        branched(0x03834, 0x038E0);
        set_lo(7, 1);
        set_lo(6, (uint16_t)((lo(REG_D[6]) & 0xFF00) | (ram8(0xFF17BB) & 0x80)));
        if (lo(REG_D[6]) & 0xFF) branched(0x038EE, 0x038F6);
        else { set_lo(7, 2); ran_to(0x038F6); }
        set16(MENU_PLAYERS, lo(REG_D[7]));
        fade_set(1, 1, 8);
        set16(TITLE_STATE, 1);
        ran_to(0x0391E);
        return done();
    }
    int two = ram8(0xFFC066) == 0x1A;
    set_lo(1, pad & 1);
    if (pad & 1) {
        uint16_t i = (uint16_t)(ram16(MENU_ITEM) - 1);
        set16(MENU_ITEM, i);
        if (i & 0x8000) { set16(MENU_ITEM, 2); ran_to(0x03854); branched(0x03854, 0x038BC); }
        else if (branched(0x03848, 0x03858), two) branched(0x03860, 0x038BC);
        else if (i != 1) branched(0x0386C, 0x038BC);
        else { set16(MENU_ITEM, 0); ran_to(0x03876); branched(0x03876, 0x038BC); }
    } else {
        branched(0x0383E, 0x03878);
        set_lo(1, pad & 2);
        if (!(pad & 2)) { branched(0x0387E, 0x038DE); return done(); }
        uint16_t i = (uint16_t)(ram16(MENU_ITEM) + 1);
        set16(MENU_ITEM, i);
        if (i == 3) { set16(MENU_ITEM, 0); ran_to(0x0389A); branched(0x0389A, 0x038BC); }
        else if (branched(0x03890, 0x0389E), two) branched(0x038A6, 0x038BC);
        else if (i != 1) branched(0x038B2, 0x038BC);
        else { set16(MENU_ITEM, 2); ran_to(0x038BC); }
    }
    set_lo(7, (uint16_t)(ram16(MENU_ITEM) << 2));
    REG_A[6] = rom32(0x03920 + (int16_t)lo(REG_D[7]));
    for (int k = 0; k < 3; k++) call(0x011B8, 0x038CC + 6 * k);
    ran_to(0x038DE);
    return done();
}

/* sub_0392C (step 1): faded out; then step 2 (sub_0394A: the choice made) */
int logic_menu_fade_out(void)
{
    if (!start()) return 0;
    begin(0x0392C);
    REG_A[6] = 0xFF0200; set_lo(7, 0x3F);
    call(0x02BB4, 0x03936);
    if (FLAG_Z) { branched(0x0393C, 0x03948); return done(); }
    set16(TITLE_STATE, 2);
    ran_to(0x03948);
    return done();
}

/* ---------------------------------------------------------------- the OPTIONS menu */

/* sub_03BA6: the OPTIONS screen faded up (colors 0x33A0); then its frame ($3BCC) */
int logic_options_fade_up(void)
{
    if (!start()) return 0;
    begin(0x03BA6);
    REG_A[6] = 0xFF0200; REG_A[5] = 0x033A0; set_lo(7, 0x3F);
    call(0x02AEA, 0x03BB6);
    if (FLAG_Z) { branched(0x03BBC, 0x03BCA); return done(); }
    screen_next(0x3BCC);
    ran_to(0x03BCA);
    return done();
}

/* (start on the OPTIONS screen, or EXIT chosen: the fade out ($3F6A)) */
static void options_exit(void)
{
    screen_next(0x3F6A);
    fade_set(5, 5, 8);
    ran_to(0x03E50);
}

/* sub_03BCC: the OPTIONS menu, every 8 frames: start leaves; up and down move the
 * cursor over its lines (wrapping at the last one: an operand the port may change, for
 * the STAGE line), the line under it drawn plain then highlighted; left and right
 * change its value (the line's routine, table 0x3CB0) */
int logic_options_frame(void)
{
    if (!start()) return 0;
    begin(0x03BCC);
    set16(FRAME_COUNT, (uint16_t)(ram16(FRAME_COUNT) + 1));
    REG_D[7] = 0;
    set_lo(0, ram16(FRAME_COUNT) & 7);
    if (lo(REG_D[0])) { branched(0x03BDE, 0x03C90); goto change; }
    REG_D[0] = ram8(0xFF17B8);
    if (!REG_D[0]) { branched(0x03BEE, 0x03C90); goto change; }
    {
        uint8_t pad = (uint8_t)REG_D[0];
        set_lo(1, pad & 0x80);
        if (pad & 0x80) { branched(0x03BF8, 0x03E2C); options_exit(); return done(); }
        uint32_t d7 = REG_D[7];
        stack_put(-4, d7);
        REG_A[7] -= 4;
        set_lo(7, 0x8000);
        call(0x03E7A, 0x03C04);
        REG_A[7] += 4;
        REG_D[7] = d7;
        set_lo(1, pad & 1);
        if (pad & 1) {
            if (!ram16(MENU_ITEM)) { set16(MENU_ITEM, rom16(0x03C22 + 2)); ran_to(0x03C2A); branched(0x03C2A, 0x03C90); }
            else { branched(0x03C1E, 0x03C2E); set16(MENU_ITEM, (uint16_t)(ram16(MENU_ITEM) - 1)); ran_to(0x03C34); branched(0x03C34, 0x03C90); }
            goto change;
        }
        branched(0x03C14, 0x03C38);
        set_lo(1, pad & 2);
        if (pad & 2) {
            set_lo(1, rom16(0x03C42 + 2));
            if (lo(REG_D[1]) == ram16(MENU_ITEM)) { set16(MENU_ITEM, 0); ran_to(0x03C58); branched(0x03C58, 0x03C90); }
            else { branched(0x03C4C, 0x03C5C); set16(MENU_ITEM, (uint16_t)(ram16(MENU_ITEM) + 1)); ran_to(0x03C62); branched(0x03C62, 0x03C90); }
            goto change;
        }
        branched(0x03C3E, 0x03C66);
        set_lo(1, pad & 4);
        if (pad & 4) { set_lo(7, 0xFFFF); ran_to(0x03C74); branched(0x03C74, 0x03C90); goto change; }
        branched(0x03C6C, 0x03C78);
        set_lo(1, pad & 8);
        if (pad & 8) { set_lo(7, 1); ran_to(0x03C86); branched(0x03C86, 0x03C90); }
        else { branched(0x03C7E, 0x03C88); ran_to(0x03C90); }
    }
change:
    /* (bsr $3CA0: the line's routine by the cursor) */
    ran_to(0x03C94);
    stack_put(-4, 0x03C94);
    REG_A[7] -= 4;
    jumped_to(0x03CA0);
    set_lo(0, (uint16_t)(ram16(MENU_ITEM) << 2));
    REG_A[0] = rom32(0x03CB0 + (int16_t)lo(REG_D[0]));
    call(REG_A[0], 0x03CAC);
    ran_to(0x03CB0);                                  /* (its rts) */
    REG_A[7] += 4;
    jumped_to(0x03C94);
    set_lo(7, 0xA000);
    call(0x03E7A, 0x03C98);
    ran_to(0x03C9E);
    return done();
}

/* sub_03CA0: the cursor's line's routine (table 0x3CB0) */
int logic_options_line(void)
{
    if (!start()) return 0;
    begin(0x03CA0);
    set_lo(0, (uint16_t)(ram16(MENU_ITEM) << 2));
    REG_A[0] = rom32(0x03CB0 + (int16_t)lo(REG_D[0]));
    call(REG_A[0], 0x03CAC);
    ran_to(0x03CAE);
    return done();
}

/* sub_03CC8: LEVEL changed by d7 (EASY, NORMAL, HARD, round). (Insane mode's hook runs
 * before this in the generated code.) */
int logic_option_level(void)
{
    if (!start()) return 0;
    begin(0x03CC8);
    if (!lo(REG_D[7])) { branched(0x03CCA, 0x03CF4); return done(); }
    REG_A[0] = DIFFICULTY;
    uint16_t v = (uint16_t)(ram16(DIFFICULTY) + lo(REG_D[7]));
    set16(DIFFICULTY, v);
    if (v == 0xFFFF) { set16(DIFFICULTY, 2); ran_to(0x03CE2); return done(); }
    branched(0x03CDA, 0x03CE4);
    if (v != 3) { branched(0x03CE8, 0x03CF4); return done(); }
    set16(DIFFICULTY, 0);
    ran_to(0x03CF4);
    return done();
}

/* sub_03CF6: PLAYERS (lives, $C052) changed by 2 d7: 3, 5, 7, round */
int logic_option_lives(void)
{
    if (!start()) return 0;
    begin(0x03CF6);
    if (!lo(REG_D[7])) { branched(0x03CF8, 0x03D20); return done(); }
    REG_A[0] = 0xFFC052;
    set_lo(7, (uint16_t)(lo(REG_D[7]) * 2));
    uint16_t v = (uint16_t)(ram16(0xFFC052) + lo(REG_D[7]));
    set16(0xFFC052, v);
    if (v == 1) { set16(0xFFC052, 7); ran_to(0x03D12); return done(); }
    branched(0x03D0A, 0x03D14);
    if (v != 9) { branched(0x03D18, 0x03D20); return done(); }
    set16(0xFFC052, 3);
    ran_to(0x03D20);
    return done();
}

/* (sbcd: the bytes' decimal digits subtracted, with X) */
static uint8_t bcd_sub(uint8_t dst, uint8_t src)
{
    uint res = (uint)(dst & 0xF) - (src & 0xF) - (FLAG_X ? 1 : 0);
    FLAG_V = ~res;
    if (res > 9) res -= 6;
    res += (uint)(dst & 0xF0) - (src & 0xF0);
    FLAG_X = FLAG_C = (res > 0x99) << 8;
    if (FLAG_C) res += 0xA0;
    res &= 0xFF;
    FLAG_V &= res;
    FLAG_N = NFLAG_8(res);
    FLAG_Z |= res;
    return (uint8_t)res;
}
static uint8_t bcd_add1(uint8_t dst)
{
    uint res = (uint)(dst & 0xF) + 1 + (FLAG_X ? 1 : 0);
    FLAG_V = ~res;
    if (res > 9) res += 6;
    res += (uint)(dst & 0xF0);
    FLAG_X = FLAG_C = (res > 0x99) << 8;
    if (FLAG_C) res -= 0xA0;
    FLAG_V &= res;
    FLAG_N = NFLAG_8(res);
    res &= 0xFF;
    FLAG_Z |= res;
    return (uint8_t)res;
}

/* sub_03D22: the SOUND TEST's number ($C050, decimal 0..44): A, B or C plays its sound
 * (table 0x3D8C); left and right change it, round */
int logic_option_sound(void)
{
    if (!start()) return 0;
    begin(0x03D22);
    set_lo(1, ram16(0xFFC050));
    REG_A[6] = 0x03D8C;
    REG_D[0] = (uint8_t)RD8(0x03D8C + (int16_t)lo(REG_D[1]));
    if (!lo(REG_D[7])) {
        set_lo(1, (uint16_t)((lo(REG_D[1]) & 0xFF00) | (ram8(0xFF17B9) & 0x70)));
        if (!(lo(REG_D[1]) & 0xFF)) { branched(0x03D42, 0x03D4C); return done(); }
        call(0x02D54, 0x03D46);
        ran_to(0x03D4C);
        return done();
    }
    branched(0x03D34, 0x03D4E);
    REG_A[0] = 0xFFC050;
    set_lo(0, ram16(0xFFC050));
    uint8_t v;
    if (lo(REG_D[7]) == 1) {
        set_lo(1, 1);
        FLAG_X = 0;                                   /* (addq.w #1: no carry) */
        v = bcd_add1((uint8_t)lo(REG_D[0]));
        set_lo(0, (uint16_t)((lo(REG_D[0]) & 0xFF00) | v));
        if (lo(REG_D[0]) == 0x45) { set_lo(0, 0); ran_to(0x03D72); branched(0x03D72, 0x03D88); }
        else branched(0x03D6A, 0x03D88);
    } else {
        branched(0x03D5A, 0x03D74);
        set_lo(1, 1);
        FLAG_X = 0;
        v = bcd_sub((uint8_t)lo(REG_D[0]), 1);
        set_lo(0, (uint16_t)((lo(REG_D[0]) & 0xFF00) | v));
        if (v == 0x99) { set_lo(0, 0x44); ran_to(0x03D88); }
        else branched(0x03D80, 0x03D88);
    }
    set16(0xFFC050, lo(REG_D[0]));
    ran_to(0x03D8A);
    return done();
}

/* sub_03DD2: CONTROL changed by d7 (two ways, round); the buttons' bits (table 0x3E16) */
int logic_option_control(void)
{
    if (!start()) return 0;
    begin(0x03DD2);
    if (!lo(REG_D[7])) { branched(0x03DD4, 0x03E14); return done(); }
    REG_A[0] = BUTTONS_SWAPPED;
    uint16_t v = (uint16_t)(ram16(BUTTONS_SWAPPED) + lo(REG_D[7]));
    set16(BUTTONS_SWAPPED, v);
    if (v == 0xFFFF) { set16(BUTTONS_SWAPPED, 1); ran_to(0x03DEC); branched(0x03DEC, 0x03DFA); }
    else if (branched(0x03DE4, 0x03DEE), v != 2) branched(0x03DF2, 0x03DFA);
    else { set16(BUTTONS_SWAPPED, 0); ran_to(0x03DFA); }
    set_lo(0, (uint16_t)(ram16(BUTTONS_SWAPPED) << 2));
    REG_A[0] = 0x03E16 + (uint32_t)(int32_t)(int16_t)lo(REG_D[0]);
    for (int k = 0; k < 3; k++) set8(PAD_MAP + k, (uint8_t)RD8(REG_A[0] + k));
    REG_A[0] += 2;
    REG_A[1] = PAD_MAP + 2;
    ran_to(0x03E14);
    return done();
}

/* sub_03E1E: EXIT: A, B or C leaves */
int logic_option_exit(void)
{
    if (!start()) return 0;
    begin(0x03E1E);
    set_lo(0, (uint16_t)((lo(REG_D[0]) & 0xFF00) | (ram8(0xFF17B9) & 0x70)));
    if (!(lo(REG_D[0]) & 0xFF)) { branched(0x03E28, 0x03E50); return done(); }
    options_exit();
    return done();
}

/* sub_03E52: STAGE (with the stage select code) changed by d7: 1..14, round */
int logic_option_stage(void)
{
    if (!start()) return 0;
    begin(0x03E52);
    if (!lo(REG_D[7])) { branched(0x03E54, 0x03E78); return done(); }
    REG_A[0] = STAGE_ENTRY;
    uint16_t v = (uint16_t)(ram16(STAGE_ENTRY) + lo(REG_D[7]));
    set16(STAGE_ENTRY, v);
    if (!v) { set16(STAGE_ENTRY, 0xE); ran_to(0x03E6A); branched(0x03E6A, 0x03E78); }
    else if (branched(0x03E62, 0x03E6C), v != 0xF) branched(0x03E70, 0x03E78);
    else { set16(STAGE_ENTRY, 1); ran_to(0x03E78); }
    return done();
}

/* sub_03E7A: the cursor's line drawn in d7's colors (its routine: table 0x3E92) */
int logic_options_draw_line(void)
{
    if (!start()) return 0;
    begin(0x03E7A);
    uint32_t d0 = REG_D[0], a0 = REG_A[0];
    stack_put(-8, d0); stack_put(-4, a0);
    REG_A[7] -= 8;
    set_lo(0, (uint16_t)(ram16(MENU_ITEM) << 2));
    REG_A[0] = rom32(0x03E92 + (int16_t)lo(REG_D[0]));
    call(REG_A[0], 0x03E8A);
    REG_A[7] += 8;
    REG_D[0] = d0; REG_A[0] = a0;
    ran_to(0x03E90);
    return done();
}

/* sub_03F14 / sub_03F3C / sub_03F48: the CONTROL, EXIT and STAGE lines drawn */
int logic_options_draw_control(void)
{
    if (!start()) return 0;
    begin(0x03F14);
    REG_A[6] = 0x04008;
    call(0x011F2, 0x03F18);
    set_lo(0, (uint16_t)(ram16(BUTTONS_SWAPPED) << 2));
    REG_A[6] = rom32(0x0415C + (int16_t)lo(REG_D[0]));
    call(0x011F2, 0x03F2E);
    call(0x011F2, 0x03F34);
    ran_to(0x03F3A);
    return done();
}
int logic_options_draw_exit(void)
{
    if (!start()) return 0;
    begin(0x03F3C);
    REG_A[6] = 0x04016;
    call(0x011F2, 0x03F40);
    ran_to(0x03F46);
    return done();
}
int logic_options_draw_stage(void)
{
    if (!start()) return 0;
    begin(0x03F48);
    REG_A[6] = 0x04020;
    call(0x011F2, 0x03F4C);
    set_lo(0, (uint16_t)(ram16(STAGE_ENTRY) << 2));
    REG_A[6] = rom32(0x0402C + (int16_t)lo(REG_D[0]));
    call(0x011F2, 0x03F62);
    ran_to(0x03F68);
    return done();
}

/* sub_03F6A: the OPTIONS screen faded out; then back to the title ($3F8A) */
int logic_options_fade_out(void)
{
    if (!start()) return 0;
    begin(0x03F6A);
    REG_A[6] = 0xFF0200; set_lo(7, 0x3F);
    call(0x02BB4, 0x03F74);
    if (FLAG_Z) { branched(0x03F7A, 0x03F88); return done(); }
    screen_next(0x3F8A);
    ran_to(0x03F88);
    return done();
}

/* ---------------------------------------------------------------- the attract demo */

/* sub_041C4: the demo set up: stage 1, both players; the stage set up (sub_0430A, by a
 * jsr of the C's own: it takes several frames); then sub_041C4 from 0x041DA */
int logic_demo_setup(void)
{
    if (!start()) return 0;
    begin(0x041C4);
    set16(STAGE_ENTRY, 1);
    set16(PLAYERS_OUT, 3);
    ran_to(0x041DA);
    REG_A[7] -= 4;
    set32(REG_A[7], 0x041DA);
    return jump_to(0x0430A);
}

/* sub_041C4 from 0x041DA: no sound waiting; after the stage's wait, $4200; the
 * recorded pads (0x3D000, 0x3D800) */
int logic_demo_setup2(void)
{
    if (!start()) return 0;
    begin(0x041DA);
    set16(SOUND_REQUEST, 0);
    set32(SCREEN_NEXT, 0x4200);
    set32(0xFF17FE, 0x3D000);
    set32(0xFF1802, 0x3D800);
    ran_to(0x041FE);
    return done();
}

/* sub_0442A: the stage's start: its fade up (job 0x4476), its frame from now on
 * (sub_05124), the screen on */
static void stage_go(uint32_t r)
{
    REG_A[6] = 0x04476;
    call(0x0141A, r + 4);
    set16(0xFF1816, 0);
    screen_next(0x5124);
    set16(VDP_MODE2, ram16(VDP_MODE2) | 0x8140);
    video_reg_word(ram16(VDP_MODE2));
    set16(0xFF1814, 1);
    ran_to(r + 0x34);
}
int logic_stage_go(void) { if (!start()) return 0; begin(0x0442A); stage_go(0x0442A); return done(); }

/* sub_04200: the demo's stage started (sub_0442A); then its video part (0x04206) */
int logic_demo_go(void)
{
    if (!start()) return 0;
    begin(0x04200);
    ran_to(0x04206);
    stack_put(-4, 0x04206);
    jumped_to(0x0442A);
    stage_go(0x0442A);
    ran_to(0x04460);                                  /* (its rts) */
    jumped_to(0x04206);
    return jump_to(0x04206);
}
