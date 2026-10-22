/* The frame during play (sub_05124, the screen's routine while a stage is played): the
 * pause, every part of the game's frame in turn, and what follows a frame: the stage
 * done, players out of lives, the game over.
 *
 * Its views' part (sub_08576) waits for the frame's end inside, so it is run as the
 * game's code would: the C goes there by a jsr of its own, and the rest of the frame
 * (from 0x05312, where that returns) is a second part. */
#include "rt.h"
#include "../video.h"

#define NO_PAUSE         0xFF180A   /* (the way out or time up showing: no pausing) */
#define PAUSE_MSG        0xFF9E08   /* the "PAUSE" message (kind $54) */
#define GAME_OVER        0xFF6C6E   /* (bytes) player 1's game over, player 2's at +2 (at +1 /
                                       +3: each one's game over showing) */

/* (player p out of lives (P1_OUT 1: his last life gone): out of play, the other one
 * left; if one was holding him, he is put beside him, 16 across, unless more than 16
 * apart. P1_OUT 2: his game over ends (sub_12F02 / sub_13354). His count drawn
 * (sub_11EDC / sub_11FB8).) */
static const struct out_at {
    uint32_t test, two, bne_lives, beq_holder, bpl, bpl_to, bcc, bra, end_two, count, next;
} OUT[2] = {
    {0x0532E, 0x0533A, 0x0534A, 0x05372, 0x05386, 0x05390, 0x05394, 0x053A0, 0x053A2, 0x053AE, 0x053B4},
    {0x053BA, 0x053C6, 0x053D6, 0x053FE, 0x05414, 0x0541E, 0x05422, 0x05430, 0x05432, 0x0543E, 0x05444},
};
static void player_out(int p)
{
    const struct out_at *at = &OUT[p];
    uint32_t out = P1_OUT + 2 * p;
    if (!ram16(out)) { branched(at->test, at->next); return; }
    if (ram16(out) == 2) {
        branched(at->two, at->end_two);
        call(p ? 0x13354 : 0x12F02, at->end_two);
        set16(out, 0);
        ran_to(at->count);
    } else {
        set16(out, 0);
        uint16_t lives = (uint16_t)(ram16(P1_LIVES + 2 * p) - 1);
        set16(P1_LIVES + 2 * p, lives);
        if (lives) branched(at->bne_lives, at->count);
        else {
            set16(OBJ_ACTIVE + 2 * p, 0);
            set_lo(7, ram16(PLAYERS_OUT) & (p ? 1 : 2));
            set16(PLAYERS_OUT, lo(REG_D[7]));
            set16(out, 2);
            set_lo(7, ram16(0xFF3DBE + 2 * p));
            uint16_t holder = lo(REG_D[7]);
            if (!holder) branched(at->beq_holder, at->count);
            else {
                set_lo(5, 0xFFF0);
                REG_A[6] = OBJ_X;
                uint16_t d = (uint16_t)(obj16(OBJ_X, holder) - obj16(OBJ_X, 2 * p));
                if (!(d & 0x8000)) branched(at->bpl, at->bpl_to);
                else { set_lo(5, 0x10); d = (uint16_t)-d; ran_to(at->bpl_to); }
                set_lo(6, d);
                if (d >= 0x10) branched(at->bcc, at->count);
                else {
                    set_lo(6, (uint16_t)(obj16(OBJ_X, holder) + lo(REG_D[5])));
                    set_obj16(OBJ_X, 2 * p, lo(REG_D[6]));
                    branched(at->bra, at->count);
                }
            }
        }
    }
    call(p ? 0x11FB8 : 0x11EDC, at->count);
    ran_to(at->next);
}

/* (after the frame's parts: the stage done - the next stage's screen ($2E06, mode $18);
 * players out of lives; both players' game over (sub_05496), else the sprites (sub_0582E)) */
static void play_frame_end(void)
{
    if (ram16(STAGE_LEFT)) {
        branched(0x05324, 0x05470);
        set16(0xFF1814, 0);
        set16(STAGE_ENTRY, (uint16_t)(ram16(STAGE_ENTRY) + 1));
        set32(SCREEN_RUN, 0x2E06);
        set16(GAME_MODE, 0x18);
        ran_to(0x0548E);
        return;
    }
    player_out(0);
    player_out(1);
    REG_A[6] = GAME_OVER;
    if (!ram8(GAME_OVER)) branched(0x0544E, 0x05468);
    else if (!ram8(GAME_OVER + 2)) branched(0x05456, 0x05468);
    else {
        set16(0xFF1814, 0);
        call(0x05496, 0x05460);
        ran_to(0x05466);
        return;
    }
    call(0x0582E, 0x05468);
    ran_to(0x0546E);
}

/* sub_05124 to its views' part: the frame counted; start (by a player in play) pauses
 * or goes on - "PAUSE" shown, the music held (or let go, the message gone). Paused: only
 * the pictures - the message, the HUD's sprites, the players' game over or standing
 * still, the loot sparkling - and the frame's end. Else every part in turn: the items,
 * the guards, the players, the objects; then the views' part, by a jsr of the C's own. */
int logic_play_frame(void)
{
    if (!start()) return 0;
    begin(0x05124);
    set16(FRAME_COUNT, (uint16_t)(ram16(FRAME_COUNT) + 1));
    if (ram16(NO_PAUSE)) { branched(0x05130, 0x052B4); goto run; }
    REG_D[7] = 0;
    REG_D[6] = 0;
    if (!ram16(OBJ_ACTIVE)) branched(0x0513E, 0x0514C);
    else { set_lo(7, ram8(P1_PAD_PRESSED) & 0x80); ran_to(0x0514C); }
    if (!ram16(OBJ_ACTIVE + 2)) branched(0x05152, 0x05160);
    else { set_lo(6, ram8(P1_PAD_PRESSED + 2) & 0x80); ran_to(0x05160); }
    set_lo(7, (uint16_t)(lo(REG_D[7]) | lo(REG_D[6])));
    if (!(lo(REG_D[7]) & 0xFF)) branched(0x05162, 0x051C8);
    else {
        set_lo(0, 0x80);
        uint16_t held = ram16(ANIMS_HELD) ^ 1;
        set16(ANIMS_HELD, held);
        if (!held) {
            /* going on */
            call(0x02D76, 0x05176);
            call(0x1050C, 0x0517C);
            set_lo(7, ram16(PAUSE_MSG));
            REG_A[6] = OBJ_ACTIVE;
            set_obj16(OBJ_ACTIVE, lo(REG_D[7]), 0);
            branched(0x05192, 0x051D2);
            goto paused;
        }
        branched(0x05172, 0x05196);
        REG_A[6] = 0x05792;
        for (int k = 0; k < 4; k++) call(0x011B8, 0x0519A + 6 * k);
        REG_D[0] = 0;
        set_lo(7, 0x54);
        call(0x18736, 0x051B8);
        set_lo(0, 1);
        call(0x02D76, 0x051C2);
        ran_to(0x051C8);
    }
    if (!ram16(ANIMS_HELD)) { branched(0x051CE, 0x052B4); goto run; }
    ran_to(0x051D2);
paused:
    set16(SPRITE_COUNT, 0);
    set_lo(0, ram16(PAUSE_MSG));
    call(0x05D88, 0x051DE);
    set_lo(0, (uint16_t)(ram16(SPRITE_COUNT) << 3));
    REG_A[0] = SPRITE_TABLE + (uint32_t)(int32_t)(int16_t)lo(REG_D[0]);
    call(0x10912, 0x051F4);
    for (int p = 0; p < 2; p++) {
        uint32_t b = p ? 0x24 : 0;
        if (ram8(0xFF6C6F + 2 * p)) {
            call(p ? 0x13354 : 0x12F02, 0x05204 + b);
            branched(0x0520A + b, 0x0521E + b);
        } else {
            branched(0x05200 + b, 0x0520E + b);
            set_lo(7, ram16(OBJ_ACTIVE + 2 * p));
            if (lo(REG_D[7])) branched(0x05214 + b, 0x0521E + b);
            else { call(p ? 0x13ADC : 0x137A6, 0x05218 + b); ran_to(0x0521E + b); }
        }
    }
    call(0x0932A, 0x05242);
    call(0x09408, 0x05248);
    set_lo(0, 0x4C);
    set_lo(1, 0x1F);
    ran_to(0x0525A);
    for (;;) {
        uint16_t o = lo(REG_D[0]);
        REG_A[6] = OBJ_ACTIVE;
        if (!obj16(OBJ_ACTIVE, o)) branched(0x05264, 0x052AA);
        else {
            REG_A[6] = ITEM_KIND;
            int go = 1;
            if (obj16(ITEM_KIND, o) == 8) branched(0x05274, 0x05286);
            else {
                REG_A[6] = OBJ_ON_SCREEN;
                if (!obj16(OBJ_ON_SCREEN, o)) { branched(0x05282, 0x052AA); go = 0; }
                else ran_to(0x05286);
            }
            if (go) {
                REG_A[6] = ITEM_KIND;
                set_lo(7, obj16(ITEM_KIND, o));
                if (lo(REG_D[7])) branched(0x05290, 0x052AA);
                else if (REG_A[6] = ITEM_STATE, obj16(ITEM_STATE, o) == 2) branched(0x052A0, 0x052AA);
                else { call(0x1B502, 0x052A4); ran_to(0x052AA); }   /* (the loot's sparkle) */
            }
        }
        set_lo(0, (uint16_t)(lo(REG_D[0]) + 2));
        uint16_t c = lo(REG_D[1]);
        set_lo(1, (uint16_t)(c - 1));
        if (!c) { ran_to(0x052B0); break; }
        branched(0x052AC, 0x0525A);
    }
    branched(0x052B0, 0x0531E);
    play_frame_end();
    return done();

run:
    set16(SPRITE_COUNT, 0);
    set16(0xFF56BC, 0);
    set16(0xFF5766, 0);
    set16(0xFF186E, 0);
    set16(0xFF1870, 0);
    set16(0xFF22B2, 0);
    set16(0xFF22B4, 0);
    set_lo(7, ram16(0xFF566A));
    if (!lo(REG_D[7])) branched(0x052E4, 0x052EE);
    else { set16(0xFF566A, (uint16_t)(lo(REG_D[7]) - 1)); ran_to(0x052EE); }
    call(0x1AFDA, 0x052EE);
    call(0x14232, 0x052F4);
    call(0x09026, 0x052FA);
    call(0x091A8, 0x05300);
    call(0x187EA, 0x05306);
    /* jsr $8576 (its wait for the frame's end inside): it returns to 0x05312 */
    ran_to(0x05312);
    REG_A[7] -= 4;
    set32(REG_A[7], 0x05312);
    return jump_to(0x08576);
}

/* sub_05124 from 0x05312 (after its views' part): the time (sub_105CC), sub_13F18,
 * and the frame's end */
int logic_play_frame2(void)
{
    if (!start()) return 0;
    begin(0x05312);
    call(0x105CC, 0x05312);
    call(0x13F18, 0x05318);
    play_frame_end();
    return done();
}

/* ---------------------------------------------------------------- against a wall */

#define WALL_FX          0xFF6CA2   /* (player 1's; player 2's +2) 1 asked for, 2 running */
#define WALL_FX_END      0xFF6CAE   /*   1 its path at its end, 2 to be taken away */
#define WALL_FX_TIME     0xFF6CB2   /*   frames left on the path's step */
#define WALL_FX_PATH     0xFF6CB6   /*   (long) the path's step: dx, dy, frames */
#define WALL_FX_X        0xFF6CA6   /*   where it is (+4: y) */

static const struct wall_fx_at {
    uint32_t beq_off, off_to, beq_run, run, beq_end, end, end_rts, bne_start, start, beq_one,
             bpl, bmi, stop, beq_last, bra_run, after;
} WALL_AT[2] = {
    {0x13F1E, 0x14006, 0x13F2A, 0x13F74, 0x13F36, 0x140E8, 0x140FC, 0x13F42, 0x13FCE, 0x13F4E,
     0x13F58, 0x13F70, 0x13FFE, 0x13F82, 0x13FCA, 0x14006},
    {0x1400C, 0x140DC, 0x14018, 0x14062, 0x14024, 0x140FE, 0x14112, 0x14030, 0x140BC, 0x1403C,
     0x14046, 0x1405E, 0x140DE, 0x14070, 0x140B8, 0x140DC},
};

/* (player p's: 1 when the routine ends there) */
static int wall_fx(int p)
{
    const struct wall_fx_at *at = &WALL_AT[p];
    uint32_t q = 2 * p, o = 4 + 2 * p;                /* (its object: 2 / 3) */
    uint16_t st = ram16(WALL_FX + q);
    if (!st) { branched(at->beq_off, at->off_to); return p; }
    if (st == 2) { branched(at->beq_run, at->run); goto run; }
    if (ram16(WALL_FX_END + q) == 2) { branched(at->beq_end, at->end); goto end; }
    if (ram16(p ? P2_STATE : P1_STATE) != 0x8003) {
        /* it starts: its path by his facing (table 0x13FF2) */
        branched(at->bne_start, at->start);
        set16(WALL_FX + q, 2);
        set_lo(0, (uint16_t)((lo(REG_D[0]) & 0xFF00) | ram8(P1_FACING + q)));
        set_lo(0, (uint16_t)((lo(REG_D[0]) << 2) & 0xFF));
        REG_A[0] = 0x13FF2;
        set32(WALL_FX_PATH + 4 * p, rom32(0x13FF2 + (int16_t)lo(REG_D[0])));
        if (p) { ran_to(0x140DC); return 1; }
        branched(0x13FEE, 0x14006);
        return 0;
    }
    if (ram16(WALL_FX_END + q) == 1) { branched(at->beq_one, at->run); goto run; }
    {
        uint16_t t = (uint16_t)(ram16(WALL_FX_TIME + q) - 1);
        set16(WALL_FX_TIME + q, t);
        if (!(t & 0x8000)) { branched(at->bpl, at->run); goto run; }
        set32(WALL_FX_PATH + 4 * p, ram32(WALL_FX_PATH + 4 * p) + 6);
        REG_A[0] = ram32(WALL_FX_PATH + 4 * p);
        t = mem16(REG_A[0] + 4);
        set16(WALL_FX_TIME + q, t);
        if (t & 0x8000) {
            branched(at->bmi, at->stop);
            set16(WALL_FX_END + q, 1);
            if (p) { ran_to(0x140E6); return 1; }
            ran_to(0x14006);
            return 0;
        }
        ran_to(at->run);
    }
run:
    {
        REG_A[0] = ram32(WALL_FX_PATH + 4 * p);
        uint16_t dx = mem16(REG_A[0]);
        REG_D[0] = dx;
        if (dx == 0x8000) { REG_A[0] += 2; branched(at->beq_last, at->end); goto end; }
        set16(WALL_FX_X + q, (uint16_t)(ram16(WALL_FX_X + q) + dx));
        REG_D[0] = mem16(REG_A[0] + 2);
        REG_A[0] += 4;
        set16(WALL_FX_X + 4 + q, (uint16_t)(ram16(WALL_FX_X + 4 + q) + lo(REG_D[0])));
        set_obj16(OBJ_ACTIVE, o, 1);
        set_obj16(OBJ_X, o, ram16(WALL_FX_X + q));
        set_obj16(OBJ_Y, o, ram16(WALL_FX_X + 4 + q));
        set_obj16(OBJ_HEIGHT, o, obj16(OBJ_HEIGHT, 2 * p));
        set_lo(0, obj16(OBJ_HEIGHT, 2 * p));
        set_obj16(OBJ_FRAME, o, 0x164);
        branched(at->bra_run, at->after);
        return p;
    }
end:
    set16(WALL_FX + q, 0);
    set16(WALL_FX_END + q, 0);
    set_obj16(OBJ_ACTIVE, o, 0);
    ran_to(at->end_rts);
    return 1;
}

/* sub_13F18: each player's effect against a wall: asked for, it starts on a path by his
 * facing while he stays against it, and goes when its path ends (player 1's ending
 * this frame: player 2's not looked at) */
int logic_wall_fx(void)
{
    if (!start()) return 0;
    begin(0x13F18);
    if (!wall_fx(0)) wall_fx(1);
    return done();
}

/* sub_13E00 / sub_13E8C: player p's effect against a wall asked for: a sound, at his
 * height less 58 (52), 62 (66) across on the side he faces, its path that way (0x14114
 * / 0x14186); facing neither side, where the table's empty first entry leads */
static int wall_fx_start(int p)
{
    uint32_t b = p ? 0x8C : 0, q = 2 * p;
    begin(0x13E00 + b);
    set_lo(0, 0xA8);
    call(0x02D54, 0x13E04 + b);
    set16(WALL_FX + q, 1);
    set16(WALL_FX_END + q, 0);
    set16(WALL_FX_TIME + q, 0);
    set_lo(0, (uint16_t)(obj16(OBJ_Y, (uint16_t)q) - (p ? 0x34 : 0x3A)));
    set16(WALL_FX_X + 4 + q, lo(REG_D[0]));
    set_lo(0, (uint16_t)((lo(REG_D[0]) & 0xFF00) | ram8(P1_FACING + q)));
    set_lo(0, (uint16_t)((lo(REG_D[0]) << 2) & 0xFF));
    uint32_t table = 0x13E48 + b;
    REG_A[0] = rom32(table + (int16_t)lo(REG_D[0]));
    ran_to(table);                                    /* (jmp (a0)) */
    uint32_t left = 0x13E54 + b, right = left + (p ? 0x1C : 0x1C);
    if (REG_A[0] != left && REG_A[0] != right) return jump_to(REG_A[0]);
    jumped_to(REG_A[0]);
    int r = REG_A[0] == right;
    set_lo(0, (uint16_t)(obj16(OBJ_X, (uint16_t)q) + (r ? -1 : 1) * (p ? 0x42 : 0x3E)));
    set16(WALL_FX_X + q, lo(REG_D[0]));
    set32(WALL_FX_PATH + 4 * p, r ? 0x14186 : 0x14114);
    ran_to(REG_A[0] + 0x1A);
    return done();
}
int logic_wall_fx_start_p1(void) { if (!start()) return 0; return wall_fx_start(0); }
int logic_wall_fx_start_p2(void) { if (!start()) return 0; return wall_fx_start(1); }

/* ---------------------------------------------------------------- the attract demo */

#define DEMO_LEFT        0xFF566E   /* the demo's frames left */
#define DEMO_PADS        0xFF17FE   /* (longs) where each player's pad goes on in the recording */

/* sub_0427E (the screen's routine in the attract demo): start pressed or the demo's time
 * up: it ends (sub_05496); else the players' pads played back from the recording (or,
 * not in the demo, recorded), and the frame during play (sub_05124, by a jsr of the C's
 * own: it waits for the frame's end inside) */
int logic_demo_frame(void)
{
    if (!start()) return 0;
    begin(0x0427E);
    set_lo(7, (uint16_t)((lo(REG_D[7]) & 0xFF00) | (ram8(0xFF17B9) & 0x80)));
    if (lo(REG_D[7]) & 0xFF) { branched(0x04288, 0x042F6); goto end; }
    {
        uint16_t left = (uint16_t)(ram16(DEMO_LEFT) - 1);
        set16(DEMO_LEFT, left);
        if (!left) { branched(0x04292, 0x042F6); goto end; }
    }
    for (int p = 0; p < 2; p++) {
        uint32_t at = DEMO_PADS + 4 * p, a6 = ram32(at);
        if (ram16(DEMO) == 1) set16(P1_PAD + 2 * p, mem16(a6));
        else WR16(a6, ram16(P1_PAD + 2 * p));
        set32(at, a6 + 2);
        REG_A[6] = a6 + 2;
    }
    if (ram16(DEMO) == 1) { branched(0x0429E, 0x042CA); ran_to(0x042EE); }
    else { ran_to(0x042C6); branched(0x042C6, 0x042EE); }
    ran_to(0x042F4);                                  /* (jsr $5124) */
    REG_A[7] -= 4;
    set32(REG_A[7], 0x042F4);
    return jump_to(0x05124);
end:
    set16(0xFF1814, 0);
    set16(TIME_FROZEN, 0);
    call(0x05496, 0x04302);
    ran_to(0x04308);
    return done();
}

/* sub_1A67E: after the players went out: the message ($1822) gone, pausing allowed,
 * the loot put back (sub_0AE02), each player still in the game back in play */
int logic_players_back(void)
{
    if (!start()) return 0;
    begin(0x1A67E);
    set_lo(7, ram16(0xFF1822));
    if (!lo(REG_D[7])) { branched(0x1A684, 0x1A6D4); return done(); }
    REG_A[6] = OBJ_ACTIVE;
    set_obj16(OBJ_ACTIVE, lo(REG_D[7]), 0);
    set16(0xFF1822, 0);
    set16(0xFF180A, 0);
    call(0x0AE02, 0x1A69E);
    for (int p = 0; p < 2; p++) {
        uint32_t b = p ? 0x18 : 0;
        if (!ram16(OBJ_ACTIVE + 2 * p)) branched(0x1A6AA + b, 0x1A6BC + b);
        else {
            set16(OBJ_ACTIVE + 2 * p, 1);
            set16(0xFF6C6A + 2 * p, 0);
            ran_to(0x1A6BC + b);
        }
    }
    return done();
}

/* sub_05496: both players' games over: the screen off, the map's column jobs and the
 * job list cleared, stage 1 again, the music stopped, the palettes cleared; then the
 * screens from the logo (mode 0) */
int logic_games_over(void)
{
    if (!start()) return 0;
    begin(0x05496);
    set16(0xFFC07C, ram16(0xFFC07C) & 0x81BF);
    video_reg_word(ram16(0xFFC07C));
    call(0x0181A, 0x054A8);
    call(0x013FE, 0x054AE);
    set16(STAGE_ENTRY, 1);
    set_lo(0, 0xE1);
    call(0x02D54, 0x054C0);
    call(0x01928, 0x054C6);
    set16(GAME_MODE, 0);
    set32(SCREEN_RUN, 0x2E06);
    ran_to(0x054DE);
    return done();
}
