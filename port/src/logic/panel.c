/* The players' panels during play: a player's game over, the count down to join in again
 * with a credit, joining in. The game has the code twice: player 2's 0x452 bytes on from
 * player 1's for the panel's steps, 0x336 / 0x324 for the game over's check, with a few
 * differences (marked). */
#include "rt.h"

#define GAME_OVER    0xFF6C6E   /* (bytes, player 2's at +2) the game over; at +1 the panel's */
#define PANEL_STEP   0xFF6C6F   /*   step */
#define PANEL_DRAW   0xFF6C6A   /* (word, +2) the panel's drawing asked for: 1 put up, 2 off */
#define PANEL_WAIT   0xFF6C72   /* (word, +2; as bytes: frames, then seconds left) */
#define PANEL_SHOWN  0xFF6C32   /* (player 2's 0xFF6C48) "insert coin" up */
#define CREDITS      0xFFC078
#define LIVES_OPTION 0xFFC04E
#define PLAYERS_IN   0xFFC076   /* (bits) the players in the game */
#define MAP_WIDTH    0xFF2CCC
#define ROW_JOB      0xFF2CD4

static uint8_t rd8(uint32_t a) { return (uint8_t)RD8(a); }
static uint32_t slot_add_at(int p) { return p ? 0x13744 : 0x132F2; }

/* (the panel's 16 map columns redrawn (sub_017AC, table a6), the map's width kept) */
static void columns_redrawn(uint32_t table, uint32_t jsr)
{
    set_lo(0, ram16(MAP_WIDTH));
    uint32_t d0 = REG_D[0];
    REG_A[7] -= 4; set32(REG_A[7], d0);
    set_lo(7, 0x10);
    REG_A[6] = table;
    call(0x017AC, jsr);
    REG_D[0] = ram32(REG_A[7]); REG_A[7] += 4;
    set16(MAP_WIDTH, lo(REG_D[0]));
}

/* (a player joins in with a credit: the credit used, the lives set by the options, the
 * player in the game; the panel's sprite and the points drawn) */
static void join_sprite(int p, uint32_t table, uint32_t jsr)
{
    set_lo(0, (uint16_t)((ram16(FRAME_COUNT) & 0x18) >> 2));
    set_lo(0, (uint16_t)(mem16(table + (int16_t)lo(REG_D[0])) | 0xA000));
    set_lo(1, 1);
    set_lo(2, 1);
    REG_D[3] = 1;
    REG_D[4] = 1;
    call(0x12E64, jsr);
}
static void join_start(void)
{
    set16(CREDITS, (uint16_t)(ram16(CREDITS) - 1));
}
static void join_lives(int p)
{
    REG_A[0] = 0x110F4;
    set_lo(0, (uint16_t)(ram16(LIVES_OPTION) << 1));
    set32(0xFF6C4A, 0x30000);
    set8(0xFF6C4A, rd8(0x110F4 + (int16_t)lo(REG_D[0])));
    set16(p ? 0xFFC056 : 0xFFC054, ram16(0xFFC052));
    set_lo(7, (uint16_t)(ram16(PLAYERS_IN) | (p ? 2 : 1)));
    set16(PLAYERS_IN, lo(REG_D[7]));
}
static void join_end(int p, uint32_t jsr)
{
    set32(p ? 0xFFC100 : 0xFFC0F4, 0);
    set16(p ? 0xFFC108 : 0xFFC0FC, 0x20);
    set16(p ? 0xFFC10A : 0xFFC0FE, 0xF000);
    set32(p ? 0xFF6C34 : 0xFF6C1E, 0);
    set16(p ? 0xFF6C3C : 0xFF6C26, 0);
    call(p ? 0x11D50 : 0x11D36, jsr);
}

/* ---------------------------------------------------------------- the game over's check */

/* sub_137A6 (player 2's sub_13ADC): run each frame while the player is out: with the
 * game over not yet up, its count down, then it put up (and the other player's panel
 * moved aside if he is out too); with it up, "insert coin" / the credits, and joining in
 * with start. Returns the Z flag its last instruction leaves (player 2's panel tests it),
 * or 2 when it goes on in the game's code (the long map block drawn). */
static int over_check(int p)
{
    uint32_t k = p ? 0x336 : 0, m = p ? 0x324 : 0;
    uint32_t over = GAME_OVER + 2 * p, wait = PANEL_WAIT + 2 * p, draw = PANEL_DRAW + 2 * p;
    if (rd8(over)) { branched(0x137AC + k, 0x1392E + m); goto credits; }
    if (rd8(PANEL_STEP + 2 * p)) { branched(0x137B6 + k, 0x137D6 + k); goto shown; }
    set16(wait, (uint16_t)(ram16(wait) - 1));
    if (ram16(wait)) { branched(0x137C0 + k, 0x137D4 + k); return 0; }
    set16(draw, 2);
    set8(over, 1);
    ran_to(0x137D4 + k);
    return 0;
shown:
    if (rd8(GAME_OVER + 2 * !p)) {                    /* (the other one's game over up) */
        if (!p) {
            branched(0x137DC, 0x13884);
            set16(draw, 1);
            set8(over, 1);
            ran_to(0x13894);
            return 0;
        }
        branched(0x13B12, 0x13BA2);                   /* (player 2's: to the end below) */
        goto end;
    }
    set_lo(7, (uint16_t)(0x46 + p));
    call(0x01550, 0x137E4 + k);
    set_lo(7, (uint16_t)(0x52 + p));
    call(0x01550, 0x137EE + k);
    if (rd8(0xFFC064 + p) != 0xF) {
        REG_A[6] = 0x13896 + m;
        call(slot_add_at(p), 0x13804 + k);
        REG_A[6] = 0x138C6 + m;
        call(slot_add_at(p), 0x1380E + k);
        branched(0x13814 + k, 0x1382C + k);
    } else {
        branched(0x137FC + k, 0x13818 + k);
        REG_A[6] = 0x138AE + m;
        call(slot_add_at(p), 0x1381C + k);
        REG_A[6] = 0x138C6 + m;
        call(slot_add_at(p), 0x13826 + k);
        ran_to(0x1382C + k);
    }
    {                                                 /* (the panel's map block drawn: */
        set_lo(0, ram16(MAP_WIDTH));                  /*  long, so the game's code calls */
        uint32_t d0 = REG_D[0];                       /*  it; on at 0x13862) */
        REG_A[7] -= 4; set32(REG_A[7], d0);
        set32(ROW_JOB, p ? 0x2E6F0 : 0x2E650);
        set32(ROW_JOB + 4, p ? 0x2E4D8 : 0x2E598);
        set16(ROW_JOB + 8, p ? 0xE540 : 0xE500);
        set16(ROW_JOB + 10, 0xF);
        set16(ROW_JOB + 12, 4);
        REG_A[0] = ROW_JOB + 14;
        set16(MAP_WIDTH, 0x10);
        ran_to(0x13862 + k);
        REG_A[7] -= 4; set32(REG_A[7], 0x13862 + k);
        jump_to(0x28386);
        return 2;
    }
end:
    set16(wait, 0x4A);
    set16(draw, 1);
    set16(over, 0);
    ran_to(0x13882 + k);
    return 1;

credits:
    if (ram16(CREDITS)) branched(0x13934 + m, 0x13954 + m);
    else if (ram16(PANEL_SHOWN + (p ? 0x16 : 0))) branched(0x1393E + m, 0x13954 + m);
    else {
        REG_A[6] = 0x138AE + m;
        call(slot_add_at(p), 0x13946 + m);
        set16(PANEL_SHOWN + (p ? 0x16 : 0), 1);
        ran_to(0x13954 + m);
    }
    set_lo(0, (uint16_t)(ram16(CREDITS) << 2));
    REG_A[6] = mem32(0x138DE + m + (int16_t)lo(REG_D[0]));
    call(slot_add_at(p), 0x13964 + m);
    {
        uint32_t st = p ? 0xFF5682 : 0xFF572C;        /* (the player's state, and its next) */
        uint32_t f = 0x139B8 + m;
        if (ram16(st)) { branched(0x13970 + m, f); goto blink; }
        if (ram16(st + 6) == 0x8017) { branched(0x1397C + m, f); goto blink; }
        if (ram16(0xFF1806)) { branched(0x13986 + m, f); goto blink; }
        /* (player 2's code tests these two the other way round) */
        int no_credit = !ram16(CREDITS), full = rd8(0xFFC064 + p) == 0xF;
        if (p ? full : no_credit) { branched(0x13992 + m, f); goto blink; }
        if (p ? no_credit : full) { branched(0x1399E + m, f); goto blink; }
        if (ram16(0xFF6C4E)) { branched(0x139A8 + m, f); goto blink; }
        if (rd8(p ? 0xFF17BD : 0xFF17BB) & 0x80) { branched(0x139B4 + m, 0x139F2 + m); goto join; }
        ran_to(f);
    }
blink:
    call(0x139C6 + m, 0x139B8 + m);
    ran_to(0x139BC + m);
    return 0;
join:
    join_start();
    join_lives(p);
    set16(0xFF304E + 2 * p, ram16(0xFF508E + 2 * p));
    set16(0xFF30FA + 2 * p, ram16(0xFF513A + 2 * p));
    set16(0xFF31A6 + 2 * p, ram16(0xFF51E6 + 2 * p));
    set16(0xFF3A62 + 2 * p, ram16(0xFF53EA + 2 * p));
    set_lo(0, (uint16_t)((ram16(STAGE_ENTRY) - 1) << 2));
    REG_A[0] = mem32((p ? 0x0459A : 0x04566) + (int16_t)lo(REG_D[0]));
    call(REG_A[0], 0x13A6E + m);
    set16(draw, 1);
    set16(over, 7);
    set16(wait, 0x3C);
    join_sprite(p, 0x139BE + m, 0x13AAC + m);
    join_end(p, 0x13AD4 + m);
    ran_to(0x13ADA + m);
    return 0;
}
static int over_check_logic(int p)
{
    if (!start()) return 0;
    begin(p ? 0x13ADC : 0x137A6);
    int z = over_check(p);
    if (z == 2) return 2;
    FLAG_Z = z ? 0 : 1;
    return done();
}
/* (sub_137A6 / sub_13ADC from 0x13862, after the panel's map block) */
static int over_check_after(int p)
{
    if (!start()) return 0;
    uint32_t k = p ? 0x336 : 0;
    begin(0x13862 + k);
    REG_D[0] = ram32(REG_A[7]); REG_A[7] += 4;
    set16(MAP_WIDTH, lo(REG_D[0]));
    set16(PANEL_WAIT + 2 * p, 0x4A);
    set16(PANEL_DRAW + 2 * p, 1);
    set16(GAME_OVER + 2 * p, 0);
    ran_to(0x13882 + k);
    FLAG_Z = 0;
    return done();
}
int logic_over_check_after(void)    { return over_check_after(0); }
int logic_over_check_after_p2(void) { return over_check_after(1); }
int logic_over_check(void)    { return over_check_logic(0); }
int logic_over_check_p2(void) { return over_check_logic(1); }

/* ---------------------------------------------------------------- the panel's steps */

/* sub_12F02 (player 2's sub_13354): the panel's step (PANEL_STEP, table 0x12F1A): the
 * game over put up, waited on, taken off; "continue" and its count down; joining in */
static int panel_step(int p)
{
    uint32_t o = p ? 0x452 : 0;
    uint32_t wait = PANEL_WAIT + 2 * p, draw = PANEL_DRAW + 2 * p, step = PANEL_STEP + 2 * p;
    uint32_t over = GAME_OVER + 2 * p;
    set_lo(0, (uint16_t)((lo(REG_D[0]) & 0xFF00) | rd8(step)));
    REG_D[0] = (uint32_t)(uint16_t)(lo(REG_D[0]) << 2) & 0xFF;
    REG_A[0] = mem32(0x12F1A + o + REG_D[0]);
    ran_to(0x12F1A + o);
    jumped_to(REG_A[0]);
    switch (REG_A[0] - o) {
    case 0x12F3A:                                     /* (0: the game over up) */
        set_lo(7, (uint16_t)(0x44 + p));                  /* (text: game over) */
        call(0x01550, 0x12F3E + o);
        set16(wait, 0xAC);
        set8(step, 1);
        set16(draw, 1);
        columns_redrawn(0x12F80 + o, 0x12F6E + o);
        ran_to(0x12F7E + o);
        return 1;
    case 0x12F8E:                                     /* (1: shown; then off) */
        set16(wait, (uint16_t)(ram16(wait) - 1));
        if (ram16(wait)) { branched(0x12F94 + o, 0x12FB0 + o); return 1; }
        set16(wait, 0xF0);
        set8(step, 2);
        set16(draw, 2);
        ran_to(0x12FB0 + o);
        return 1;
    case 0x12FB2:                                     /* (2: then "continue" up) */
        set16(wait, (uint16_t)(ram16(wait) - 1));
        if (ram16(wait)) { branched(0x12FB8 + o, 0x13000 + o); return 1; }
        set_lo(7, (uint16_t)(0x46 + p));
        call(0x01550, 0x12FC0 + o);
        set16(wait, p ? 0x40 : 0x30);                 /* (player 2's longer) */
        set8(step, 3);
        set16(draw, 1);
        columns_redrawn(0x13002 + o, 0x12FF0 + o);
        ran_to(0x13000 + o);
        return 1;
    case 0x13034:                                     /* (3 and 4: with no credits, the */
    case 0x13078: {                                   /*  game over's check) */
        uint32_t s = REG_A[0] - o == 0x13034 ? 0 : 0x44;
        if (!ram16(CREDITS)) {
            call(p ? 0x13ADC : 0x137A6, 0x1303E + s + o);
            /* (player 2's code has a bne after the check, where player 1's has a bra) */
            if (!p || FLAG_Z) { branched(0x13044 + s + o, (s ? 0x130C0 : 0x13076) + o); return 1; }
            ran_to(0x13048 + s + o);
        } else branched(0x1303A + s + o, 0x13048 + s + o);
        set16(wait, (uint16_t)(ram16(wait) - 1));
        if (ram16(wait)) { branched(0x1304E + s + o, (s ? 0x130C0 : 0x13076) + o); return 1; }
        if (!s) {
            set16(wait, 0x4A);
            set8(step, 4);
            REG_A[6] = 0x13010 + o;
            call(slot_add_at(p), 0x13066 + o);
            set_lo(7, (uint16_t)(0x52 + p));
            call(0x01550, 0x13070 + o);
            ran_to(0x13076 + o);
        } else {
            set8(wait, 1);
            set8(wait + 1, 0xA);
            set8(step, 5);
            set16(draw, 2);
            REG_A[6] = 0x13022 + o;
            call(slot_add_at(p), 0x130BA + o);
            ran_to(0x130C0 + o);
        }
        return 1;
    }
    case 0x130C2: {                                   /* (5: "continue" counting down) */
        uint32_t st = p ? 0xFF5682 : 0xFF572C, f = 0x13102 + o;
        if (ram16(st)) branched(0x130C8 + o, f);
        else if (ram16(st + 6) == 0x8017) branched(0x130D4 + o, f);
        else if (ram16(0xFF6C4E)) branched(0x130DE + o, f);
        else if (ram16(0xFF1806)) branched(0x130E8 + o, f);
        else if (!ram16(CREDITS)) { branched(0x130F2 + o, 0x1312E + o); goto check; }
        else if (rd8(p ? 0xFF17BD : 0xFF17BB) & 0x80) { branched(0x130FE + o, 0x131AE + o); goto join; }
        else ran_to(f);
        call(p ? 0x13CEA : 0x139C6, f);
        if (ram16(0xFF1806)) { branched(0x1310C + o, 0x13134 + o); return 1; }
        if (ram16(0xFF6C4E)) { branched(0x13116 + o, 0x13134 + o); return 1; }
        set8(wait, (uint8_t)(rd8(wait) - 1));
        if (rd8(wait)) { branched(0x13120 + o, 0x13134 + o); return 1; }
        set8(wait + 1, (uint8_t)(rd8(wait + 1) - 1));
        if (!(rd8(wait + 1) & 0x80)) {
            branched(0x1312A + o, 0x13136 + o);
            set8(wait, 0x3C);
            REG_D[0] = rd8(wait + 1);
            set_lo(0, (uint16_t)(lo(REG_D[0]) << 2));
            REG_A[6] = mem32(0x13266 + o + (int16_t)lo(REG_D[0]));
            call(slot_add_at(p), 0x13150 + o);
            ran_to(0x13156 + o);
            return 1;
        }
        ran_to(0x1312E + o);
    check:
        call(p ? 0x13ADC : 0x137A6, 0x1312E + o);
        ran_to(0x13134 + o);
        return 1;
    join:
        join_start();
        call(0x198EA, 0x131B4 + o);
        join_lives(p);
        call(0x1B80E, 0x131F4 + o);
        set16(draw, 1);
        set16(over, 6);
        set16(wait, 0x3C);
        join_sprite(p, p ? 0x13CE2 : 0x139BE, 0x13236 + o);
        join_end(p, 0x1325E + o);
        ran_to(0x13264 + o);
        return 1;
    }
    case 0x13158:                                     /* (6 and 7: back in play after */
    case 0x13186: {                                   /*  joining in) */
        uint32_t s = REG_A[0] - o == 0x13158 ? 0 : 0x2E;
        set16(wait, (uint16_t)(ram16(wait) - 1));
        if (ram16(wait)) { branched(0x1315E + s + o, (s ? 0x131AC : 0x13184) + o); return 1; }
        REG_D[0] = s ? 0 : 1;
        call(p ? 0x08F26 : 0x08E74, 0x13164 + s + o);
        if (!s) call(p ? 0x09A1C : 0x099D6, 0x1316A + o);
        set16(over, 0);
        set16(draw, 0);
        set16(p ? 0xFF2EF8 : 0xFF2EF6, 1);
        ran_to((s ? 0x131AC : 0x13184) + o);
        return 1;
    }
    }
    return jump_to(REG_A[0]);                         /* (a step a mod added) */
}
static int panel_step_logic(int p)
{
    if (!start()) return 0;
    begin(p ? 0x13354 : 0x12F02);
    if (panel_step(p) == 1) return done();
    return 2;
}
int logic_panel_step(void)    { return panel_step_logic(0); }
int logic_panel_step_p2(void) { return panel_step_logic(1); }
