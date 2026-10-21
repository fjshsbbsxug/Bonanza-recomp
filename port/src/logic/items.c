/* The items each frame (sub_1AFDA): objects $26..$45, the loot and the stages' gadgets,
 * each by its kind's routine (table 0x1B0CC). */
#include "rt.h"
#include <stdio.h>
#include "map.h"
#include "objects.h"

/* (the routines that return their answer in the Z flag: nonzero, as bne sees it) */
static int found(uint32_t routine, uint32_t jsr) { call(routine, jsr); return FLAG_Z != 0; }

/* ---------------------------------------------------------------- the loot (kind 0) */

/* who takes it: player p touching it on foot (or already taking loot) takes it, a
 * request to the state that picks it up; loot already counted once (dropped again)
 * goes straight onto his back (kind $18 / $1C), protected a while. The answer: the
 * player who took it, or -1 */
static int loot_taker(uint16_t o, int p)
{
    uint32_t b = p ? 0x6C : 0;
    uint16_t st = ram16(p ? P2_STATE : P1_STATE);
    set_lo(7, st);
    if (st == 0x8007) { branched(0x1B1FA + b, 0x1B248 + b); return p; }
    if (!st) branched(0x1B1F2 + b, 0x1B206 + b);
    else if (st != 1) { branched(0x1B202 + b, p ? 0x1B502 : 0x1B250); return -1; }
    else ran_to(0x1B206 + b);
    REG_A[6] = ITEM_SCORED;
    if (obj16(ITEM_SCORED, o)) {
        uint32_t d0 = REG_D[0];
        set_lo(0, (uint16_t)(2 * p));
        set_lo(7, p ? 0x1C : 0x18);
        call(0x18736, 0x1B220 + b);
        set_lo(7, 0x3C);
        call(p ? 0x09EF4 : 0x09EDA, 0x1B22A + b);
        REG_D[0] = d0;
        branched(0x1B234 + b, 0x1B248 + b);
    } else {
        branched(0x1B210 + b, 0x1B236 + b);
        set_lo(7, 0x8007);
        uint32_t d6 = REG_D[6];
        call(p ? 0x09894 : 0x09846, 0x1B23E + b);
        REG_D[6] = d6;
        ran_to(0x1B248 + b);
    }
    return p;
}

/* sub_1B0FC: a piece of loot. State 0: set up (its size, its touch points, its
 * sparkle); 1: lying there, taken by a player touching it (5000 points the first time;
 * all taken: the "way out" message and the way out open); 2: taken (not drawn); 3:
 * thrown out of a shot player's hands, flying across, falling (table 0x1B532) until it
 * lands on a floor. It sparkles every 4 frames. */
static void loot(uint16_t o)
{
    uint16_t st = obj16(ITEM_STATE, o);
    REG_A[6] = ITEM_STATE;
    set_lo(7, st);
    if (!st) {
        set_obj16(ITEM_STATE, o, 1);
        set_obj16(OBJ_SIZE_X, o, 4);
        set_obj16(OBJ_SIZE_Y, o, 0xB);
        set_obj16(OBJ_PASS, o, 0);
        set_obj16(OBJ_ACTIVE, o, 1);
        set_lo(7, (uint16_t)(2 * o));
        set_obj32(OBJ_POINTS_A, o, 0xFFDCFFD8);
        set_obj32(OBJ_POINTS_B, o, 0xFFDCFFE8);
        set_obj32(OBJ_POINTS_C, o, 0xFFE4FFD8);
        set_obj32(OBJ_POINTS_D, o, 0xFFE4FFE8);
        set_obj16(OBJ_PICTURE, o, obj16(OBJ_FRAME, o));
        set_obj16(ITEM_SPARKLE, o, obj16(OBJ_ANIM, o));
        set_obj16(OBJ_LEFT, o, obj16(OBJ_ANIM, o));
        REG_A[5] = OBJ_ANIM;
        REG_A[6] = OBJ_LEFT;
        branched(0x1B1BA, 0x1B502);
        goto sparkle;
    }
    branched(0x1B106, 0x1B1BE);
    if (st == 1) {
        REG_D[7] = 0;
        REG_A[6] = OBJ_HEIGHT;
        if (!obj16(OBJ_HEIGHT, o)) branched(0x1B1D2, 0x1B1DA);
        else { set_lo(7, 0x10); ran_to(0x1B1DA); }
        set_obj16(OBJ_HEIGHT, o, lo(REG_D[7]));
        uint16_t touch = (uint16_t)players_touching(o, 0x1B1DE);
        int who = -1;
        if (!(touch & 1)) branched(0x1B1E8, 0x1B250);
        else {
            ran_to(0x1B1EC);
            who = loot_taker(o, 0);
            if (who == 0) { REG_D[7] = 0; REG_D[6] = 0; branched(0x1B24C, 0x1B2B8); }
        }
        if (who < 0) {
            if (!(touch & 2)) { branched(0x1B254, 0x1B502); goto sparkle; }
            ran_to(0x1B258);
            who = loot_taker(o, 1);
            if (who < 0) goto sparkle;
            REG_D[7] = 2; REG_D[6] = 1;
            ran_to(0x1B2B8);
        }
        /* taken: the points, once */
        if (ram16(DEMO)) branched(0x1B2BE, 0x1B2F2);
        else if (obj16(ITEM_SCORED, o)) { REG_A[6] = ITEM_SCORED; branched(0x1B2CC, 0x1B2F2); }
        else {
            uint32_t d0 = REG_D[0];
            set_lo(0, lo(REG_D[6]));
            REG_D[1] = 0x5000;
            call(0x11D6A, 0x1B2DC);
            REG_D[0] = d0;
            set_obj16(ITEM_SCORED, o, 1);
            ran_to(0x1B2F2);
        }
        set_obj16(ITEM_TAKER, o, lo(REG_D[7]));
        set_obj16(ITEM_STATE, o, 2);
        REG_A[6] = LOOT_TAKEN;
        uint16_t n = (uint16_t)(ram16(LOOT_TAKEN) + 1);
        set_lo(7, n);
        set16(LOOT_TAKEN, n);
        if (n != ram16(LOOT_COUNT)) { branched(0x1B31A, 0x1B502); goto sparkle; }
        set_lo(7, 0x24);
        call(0x18736, 0x1B322);
        o = lo(REG_D[0]);
        set16(LOOT_ALL, 1);
        branched(0x1B330, 0x1B502);
        goto sparkle;
    }
    branched(0x1B1C2, 0x1B334);
    if (st == 2) {
        set_obj16(OBJ_FRAME, o, 0);
        REG_A[6] = OBJ_FRAME;
        branched(0x1B346, 0x1B530);
        goto out;
    }
    branched(0x1B338, 0x1B34A);

    /* flying across: stopped by something (sub_1AD5E, as wide as 32) or a wall */
    REG_A[6] = ITEM_FLY_LEFT;
    if (!obj16(ITEM_FLY_LEFT, o)) branched(0x1B354, 0x1B43A);
    else {
        set_obj16(ITEM_FLY_LEFT, o, (uint16_t)(obj16(ITEM_FLY_LEFT, o) - 1));
        uint16_t dx = obj16(OBJ_DX, o);
        set_lo(7, dx); set_lo(6, dx);
        int left = (dx & 0x8000) != 0;
        uint32_t b = left ? 0 : 0x62;
        if (!left) branched(0x1B368, 0x1B3CE);
        uint32_t d7 = REG_D[7];                       /* (kept on the stack) */
        uint16_t w = obj16(OBJ_SIZE_X, o);
        set_lo(6, w);
        uint32_t d6 = REG_D[6];
        set_obj16(OBJ_SIZE_X, o, 0x20);
        set_obj16(OBJ_PASS, o, 0);
        call(0x1AD5E, 0x1B38E + b);
        REG_D[6] = d6;
        REG_A[5] = OBJ_PASS; REG_A[6] = OBJ_SIZE_X;
        set_obj16(OBJ_SIZE_X, o, w);
        set_obj16(OBJ_PASS, o, 1);
        uint16_t hit = lo(REG_D[7]);
        REG_D[7] = d7;
        if (hit) { branched(0x1B3A8 + b, 0x1B43A); goto down; }
        REG_D[6] = 0;
        set_lo(3, 0xFFD8);
        if (found(left ? 0x176B8 : 0x17738, 0x1B3B2 + b)) { branched(0x1B3B8 + b, 0x1B43A); goto down; }
        set_lo(3, 0xFFE8);
        if (found(left ? 0x177B8 : 0x17820, 0x1B3C0 + b)) { branched(0x1B3C6 + b, 0x1B43A); goto down; }
        if (left) branched(0x1B3CA, 0x1B42C); else ran_to(0x1B42C);
        REG_A[6] = OBJ_X;
        set_lo(7, (uint16_t)(obj16(OBJ_X, o) + lo(REG_D[7])));
        set_obj16(OBJ_X, o, lo(REG_D[7]));
        ran_to(0x1B43A);
    }
down:
    /* falling (rising): a floor below lands it, on the floor's line (table 0x1B566) */
    {
        uint16_t dy = obj16(OBJ_DY, o);
        REG_A[6] = OBJ_DY;
        set_lo(6, dy); set_lo(7, dy);
        if (dy & 0x8000) {
            REG_D[7] = 0;
            set_lo(3, (uint16_t)(0xFFD8 + lo(REG_D[6])));
            if (found(0x176B8, 0x1B452)) { branched(0x1B458, 0x1B4A0); goto step; }
            set_lo(3, (uint16_t)(0xFFD8 + lo(REG_D[6])));
            if (found(0x176B8, 0x1B462)) { branched(0x1B468, 0x1B4A0); goto step; }
            branched(0x1B46C, 0x1B492);
        } else {
            branched(0x1B446, 0x1B470);
            REG_D[7] = 0;
            set_lo(3, (uint16_t)(0xFFE8 + lo(REG_D[6])));
            if (found(0x177B8, 0x1B478)) { branched(0x1B47E, 0x1B4D2); goto landed; }
            set_lo(3, (uint16_t)(0xFFE8 + lo(REG_D[6])));
            if (found(0x17820, 0x1B488)) { branched(0x1B48E, 0x1B4D2); goto landed; }
            ran_to(0x1B492);
        }
    }
move_y:
    REG_A[4] = OBJ_Y;
    set_lo(6, (uint16_t)(obj16(OBJ_Y, o) + lo(REG_D[6])));
    set_obj16(OBJ_Y, o, lo(REG_D[6]));
    ran_to(0x1B4A0);
step:
    {
        REG_A[6] = ITEM_FALL_LEFT;
        uint16_t left = (uint16_t)(obj16(ITEM_FALL_LEFT, o) - 1);
        set_obj16(ITEM_FALL_LEFT, o, left);
        if (left) { branched(0x1B4AA, 0x1B502); goto sparkle; }
        REG_A[3] = ITEM_FALL_AT;
        set_lo(7, obj16(ITEM_FALL_AT, o));
        uint32_t a5 = 0x1B532 + (uint32_t)(int32_t)(int16_t)lo(REG_D[7]);
        REG_A[4] = OBJ_DY;
        set_obj16(OBJ_DY, o, rom16(a5));
        set_obj16(ITEM_FALL_LEFT, o, rom16(a5 + 2));
        REG_A[5] = a5 + 2;
        set_obj16(ITEM_FALL_AT, o, (uint16_t)(obj16(ITEM_FALL_AT, o) + 4));
        branched(0x1B4D0, 0x1B502);
        goto sparkle;
    }
landed:
    set_obj16(ITEM_STATE, o, 1);
    set_obj16(OBJ_PASS, o, 0);
    set_lo(6, (uint16_t)((obj16(OBJ_Y, o) & 7) << 1));
    REG_A[6] = 0x1B566;
    set_lo(6, rom16(0x1B566 + (int16_t)lo(REG_D[6])));
    branched(0x1B500, 0x1B492);
    goto move_y;

sparkle:
    set_lo(7, ram16(FRAME_COUNT) & 3);
    if (lo(REG_D[7])) branched(0x1B50C, 0x1B530);
    else {
        uint16_t s = obj16(ITEM_SPARKLE, o);
        set_lo(7, (uint16_t)(obj16(OBJ_FRAME, o) + s));
        set_obj16(OBJ_FRAME, o, lo(REG_D[7]));
        set_lo(6, (uint16_t)-s);
        set_obj16(ITEM_SPARKLE, o, lo(REG_D[6]));
        REG_A[6] = OBJ_FRAME;
        REG_A[5] = ITEM_SPARKLE;
        ran_to(0x1B530);
    }
out:
    ran_to(0x1B532);                                  /* (its rts) */
}

/* ---------------------------------------------------------------- doors (kind 4) */

#define DOOR_ANY_HEIGHT  0xFFA3B6   /* (nonzero: touching a door at any height) */
#define DOORWAY_TAKEN    0xFFA3B8   /* a guard stands in the doorway */
#define DOOR_SIDE        0xFFA3AC   /* the side the last one touching was on */

/* (a small routine called by the jsr at `jsr`: its time, rts included) */
#define CALLED(jsr, routine, rts, body) do { into(jsr, routine); body; ran_to((rts) + 2); jumped_to((jsr) + 6); } while (0)

/* sub_1AC4A: does object d1 touch door d0? (the door not letting shots through; d1 not
 * either, unless it is a guard at 1 ($8DC4); at the same height unless DOOR_ANY_HEIGHT.)
 * If so its door (P1_DOOR's table) is d0, and the side it is on noted ($3E6A). */
static void door_touch(void)
{
    uint16_t a = lo(REG_D[0]), b = lo(REG_D[1]);
    REG_A[6] = OBJ_PASS;
    if (obj16(OBJ_PASS, a)) { branched(0x1AC54, 0x1AD02); return; }
    REG_A[5] = 0xFF8DC4;
    if (obj16(0xFF8DC4, b) == 1) branched(0x1AC64, 0x1AC70);
    else if (obj16(OBJ_PASS, b)) { branched(0x1AC6C, 0x1AD02); return; }
    else ran_to(0x1AC70);
    if (ram16(DOOR_ANY_HEIGHT)) branched(0x1AC76, 0x1AC8C);
    else {
        REG_A[6] = OBJ_HEIGHT;
        set_lo(6, obj16(OBJ_HEIGHT, a));
        if (lo(REG_D[6]) != obj16(OBJ_HEIGHT, b)) { branched(0x1AC88, 0x1AD02); return; }
        ran_to(0x1AC8C);
    }
    set_lo(5, 2);
    REG_A[6] = OBJ_X;
    uint16_t d = (uint16_t)(obj16(OBJ_X, a) - obj16(OBJ_X, b));
    if (d & 0x8000) { d = (uint16_t)-d; set_lo(5, 1); ran_to(0x1ACA8); }
    else branched(0x1AC9E, 0x1ACA8);
    set_lo(6, d);
    set16(DOOR_SIDE, lo(REG_D[5]));
    REG_A[6] = OBJ_SIZE_X;
    set_lo(5, (uint16_t)(obj16(OBJ_SIZE_X, a) + obj16(OBJ_SIZE_X, b)));
    if (d >= lo(REG_D[5])) { branched(0x1ACBE, 0x1AD02); return; }
    REG_A[6] = OBJ_Y;
    d = (uint16_t)(obj16(OBJ_Y, a) - obj16(OBJ_Y, b));
    if (d & 0x8000) { d = (uint16_t)-d; ran_to(0x1ACD6); }
    else branched(0x1ACD0, 0x1ACD6);
    set_lo(6, d);
    REG_A[6] = OBJ_SIZE_Y;
    set_lo(5, (uint16_t)(obj16(OBJ_SIZE_Y, a) + obj16(OBJ_SIZE_Y, b)));
    if (d >= lo(REG_D[5])) { branched(0x1ACE6, 0x1AD02); return; }
    set_obj16(P1_DOOR, b, a);
    set_obj16(0xFF3E6A, b, ram16(DOOR_SIDE));
    REG_A[6] = 0xFF3E6A;
    ran_to(0x1AD02);
}
static void door_touch_at(uint32_t jsr) { CALLED(jsr, 0x1AC4A, 0x1AD02, door_touch()); }

/* sub_1AB92 / sub_1ABEE: player p (in play, at no door yet) touching door d0 (on a
 * box too: at height 0 unless 16); if so, he touches something on that side */
static void player_at_door(int p)
{
    uint32_t b = p ? 0x5C : 0;
    if (!obj16(OBJ_ACTIVE, 2 * p)) { branched(0x1AB98 + b, 0x1ABEC + b); return; }
    if (obj16(P1_DOOR, 2 * p)) { branched(0x1ABA2 + b, 0x1ABEC + b); return; }
    uint16_t h = obj16(OBJ_HEIGHT, 2 * p);
    set_lo(6, h);
    if (h == 0x10) branched(0x1ABB0 + b, 0x1ABBA + b);
    else { set_obj16(OBJ_HEIGHT, 2 * p, 0); ran_to(0x1ABBA + b); }
    uint32_t d1 = REG_D[1], d6 = REG_D[6];
    REG_D[1] = (uint32_t)(2 * p);
    door_touch_at(0x1ABC0 + b);
    REG_D[1] = d1; REG_D[6] = d6;
    set_obj16(OBJ_HEIGHT, 2 * p, lo(REG_D[6]));
    if (!obj16(P1_DOOR, 2 * p)) { branched(0x1ABD6 + b, 0x1ABEC + b); return; }
    set16(P1_TOUCHING + 2 * p, 1);
    set16(0xFFA3B2 + 2 * p, ram16(DOOR_SIDE));
    ran_to(0x1ABEC + b);
}
static void player_at_door_at(int p, uint32_t jsr)
{
    uint32_t r = p ? 0x1ABEE : 0x1AB92;
    CALLED(jsr, r, r + 0x5A, player_at_door(p));
}

/* (the guards in view, objects 6..37, each: d1, counted down in d2) */
static int guards_next(uint32_t dbra, uint32_t loop)
{
    set_lo(1, (uint16_t)(lo(REG_D[1]) + 2));
    uint16_t c = lo(REG_D[2]);
    set_lo(2, (uint16_t)(c - 1));
    if (!c) { ran_to(dbra + 4); return 0; }
    branched(dbra, loop);
    return 1;
}

/* sub_1AA62: the guards in view at door d0 as it opens towards them (their side's
 * animation, table 0x1AB0C) are hit by it: event $18 (the guard opening it, $9F2A,
 * noted at $9614; the door at $A46C) */
static void door_hits_guards(void)
{
    uint32_t d1 = REG_D[1];
    set16(DOORWAY_TAKEN, 0);
    set_lo(1, 0xC);
    set_lo(2, 0x1F);
    ran_to(0x1AA74);
    do {
        uint16_t g = lo(REG_D[1]), d = lo(REG_D[0]);
        REG_A[6] = OBJ_ACTIVE;
        if (!obj16(OBJ_ACTIVE, g)) { branched(0x1AA7E, 0x1AB00); continue; }
        REG_A[6] = OBJ_ON_SCREEN;
        if (!obj16(OBJ_ON_SCREEN, g)) { branched(0x1AA8C, 0x1AB00); continue; }
        door_touch_at(0x1AA90);
        g = lo(REG_D[1]); d = lo(REG_D[0]);
        REG_A[6] = P1_DOOR;
        if (obj16(P1_DOOR, g) != d) { branched(0x1AAA0, 0x1AB00); continue; }
        REG_A[6] = ITEM_STATE;
        if (obj16(ITEM_STATE, d) != 1) { branched(0x1AAB0, 0x1AB00); continue; }
        set_lo(7, rom16(0x1AB0C + (int16_t)(uint16_t)(obj16(0xFF3E6A, g) << 1)));
        REG_A[6] = OBJ_ANIM;
        if (lo(REG_D[7]) != obj16(OBJ_ANIM, d)) { branched(0x1AAD2, 0x1AB00); continue; }
        set_lo(7, obj16(0xFF9F2A, d));
        set_obj16(0xFF9614, g, lo(REG_D[7]));
        set_obj16(0xFFA46C, g, d);
        REG_A[6] = 0xFFA46C;
        set_lo(6, g);
        set_lo(7, 0x18);
        call(0x17170, 0x1AAFA);
        ran_to(0x1AB00);
    } while (guards_next(0x1AB02, 0x1AA74));
    REG_D[1] = d1;
    ran_to(0x1AB0A);
}

/* sub_1AB12: a guard in view in door d0's doorway (not on a box): DOORWAY_TAKEN */
static void doorway_taken(void)
{
    uint32_t d1 = REG_D[1];
    set16(DOORWAY_TAKEN, 0);
    set_lo(1, 0xC);
    set_lo(2, 0x1F);
    ran_to(0x1AB24);
    do {
        uint16_t g = lo(REG_D[1]);
        REG_A[6] = OBJ_ACTIVE;
        if (!obj16(OBJ_ACTIVE, g)) { branched(0x1AB2E, 0x1AB86); continue; }
        REG_A[6] = OBJ_ON_SCREEN;
        if (!obj16(OBJ_ON_SCREEN, g)) { branched(0x1AB3C, 0x1AB86); continue; }
        REG_A[6] = OBJ_PASS;
        set_lo(7, obj16(OBJ_PASS, g));
        uint32_t d7 = REG_D[7];
        set_obj16(OBJ_PASS, g, 0);
        door_touch_at(0x1AB52);
        REG_D[7] = d7;
        REG_A[6] = OBJ_PASS;
        g = lo(REG_D[1]);
        set_obj16(OBJ_PASS, g, lo(REG_D[7]));
        REG_A[6] = P1_DOOR;
        if (obj16(P1_DOOR, g) != lo(REG_D[0])) { branched(0x1AB6A, 0x1AB86); continue; }
        REG_A[6] = OBJ_HEIGHT;
        if (obj16(OBJ_HEIGHT, g) == 0x10) { branched(0x1AB7A, 0x1AB86); continue; }
        set16(DOORWAY_TAKEN, 1);
        ran_to(0x1AB86);
    } while (guards_next(0x1AB88, 0x1AB24));
    REG_D[1] = d1;
    ran_to(0x1AB90);
}

/* (a player at door d0 as it opens towards him: knocked over, state $800E) */
static void door_knocks(int p, uint32_t at)
{
    if (obj16(P1_DOOR, 2 * p) != lo(REG_D[0])) { branched(at, at + 0x34); return; }
    set_lo(7, rom16(0x1AB0C + (int16_t)(uint16_t)(obj16(0xFF3E6A, 2 * p) << 1)));
    REG_A[6] = OBJ_ANIM;
    if (lo(REG_D[7]) != obj16(OBJ_ANIM, lo(REG_D[0]))) { branched(at + 0x1E, at + 0x34); return; }
    set_lo(7, 0x800E);
    uint32_t d6 = REG_D[6];
    call(p ? 0x098B8 : 0x0986A, at + 0x2A);
    REG_D[6] = d6;
    ran_to(at + 0x34);
}

/* (a player knocked over by door d0 no longer at it once it turned the other way:
 * table 0x1B806) */
static void door_lets_go(int p, uint32_t at)
{
    if (ram16(p ? P2_STATE : P1_STATE) != 0x800E) { branched(at, at + 0x28); return; }
    set_lo(7, rom16(0x1B806 + (int16_t)(uint16_t)(obj16(0xFF3E6A, 2 * p) << 1)));
    REG_A[6] = OBJ_ANIM;
    if (lo(REG_D[7]) == obj16(OBJ_ANIM, lo(REG_D[0]))) { branched(at + 0x1E, at + 0x28); return; }
    set16(P1_DOOR + 2 * p, 0);
    ran_to(at + 0x28);
}

/* (the door's sound) */
static void door_sound(uint32_t jsr)
{
    uint32_t d0 = REG_D[0];
    set_lo(0, 0x9C);
    call(0x02D54, jsr);
    REG_D[0] = d0;
}

/* sub_1B576: a door. 0: shut (who touches it noted); 1: a guard opens it ($9F2A, its
 * side): it swings away from him, knocking over a player or guard behind it, a sound;
 * 2: open, until nobody stands in it, then it shuts (3), shut at the animation's end
 * (a sound). Its animation runs every frame; it is always drawn. */
static void door(uint16_t o)
{
    uint16_t st = obj16(ITEM_STATE, o);
    REG_A[6] = ITEM_STATE;
    set_lo(7, st);
    if (!st) {
        set_obj16(OBJ_SIZE_X, o, 4);
        set_obj16(OBJ_SIZE_Y, o, 0x14);
        set16(DOOR_ANY_HEIGHT, 0);
        player_at_door_at(0, 0x1B5A2);
        player_at_door_at(1, 0x1B5A8);
        set16(DOOR_ANY_HEIGHT, 0);
        CALLED(0x1B5B6, 0x1AA62, 0x1AB0A, door_hits_guards());
        branched(0x1B5BC, 0x1B7F2);
    } else if (branched(0x1B580, 0x1B5C0), st == 1) {
        set_obj16(OBJ_SIZE_X, o, 0x14);
        set_lo(6, obj16(0xFF9F2A, o));
        set_lo(6, obj16(0xFF3E6A, lo(REG_D[6])));
        set_lo(7, 2);
        if (lo(REG_D[6]) == 1) branched(0x1B5F0, 0x1B5F8);
        else { set_lo(7, 4); ran_to(0x1B5F8); }
        into(0x1B5F8, 0x1B088);
        anim_start(o, lo(REG_D[7]));
        jumped_to(0x1B5FE);
        set_obj16(OBJ_HEIGHT, o, 0x10);
        set16(DOOR_ANY_HEIGHT, 1);
        player_at_door_at(0, 0x1B612);
        door_knocks(0, 0x1B61E);
        set16(DOOR_ANY_HEIGHT, 1);
        player_at_door_at(1, 0x1B65A);
        door_knocks(1, 0x1B666);
        set16(DOOR_ANY_HEIGHT, 1);
        CALLED(0x1B6A2, 0x1AA62, 0x1AB0A, door_hits_guards());
        set_obj16(ITEM_STATE, o, 2);
        door_sound(0x1B6BC);
        branched(0x1B6C6, 0x1B7F2);
    } else if (branched(0x1B5C4, 0x1B6CA), st == 2) {
        set_obj16(OBJ_SIZE_X, o, 0x14);
        set16(DOOR_ANY_HEIGHT, 1);
        player_at_door_at(0, 0x1B6E6);
        player_at_door_at(1, 0x1B6EC);
        CALLED(0x1B6F2, 0x1AB12, 0x1AB90, doorway_taken());
        door_lets_go(0, 0x1B700);
        door_lets_go(1, 0x1B730);
        /* still in the doorway: a player at it (unless on a box), a guard */
        if (ram16(P1_DOOR) != lo(REG_D[0])) branched(0x1B75E, 0x1B76E);
        else if (ram16(OBJ_HEIGHT) != 0x10) { branched(0x1B76A, 0x1B7F2); goto anim; }
        else ran_to(0x1B76E);
        if (ram16(P1_DOOR + 2) != lo(REG_D[0])) branched(0x1B774, 0x1B784);
        else if (ram16(OBJ_HEIGHT + 2) != 0x10) { branched(0x1B780, 0x1B7F2); goto anim; }
        else ran_to(0x1B784);
        if (ram16(DOORWAY_TAKEN)) { branched(0x1B78A, 0x1B7F2); goto anim; }
        REG_A[6] = OBJ_ANIM;
        set_lo(7, (uint16_t)(obj16(OBJ_ANIM, o) + 1));
        into(0x1B79A, 0x1B088);
        anim_start(o, lo(REG_D[7]));
        jumped_to(0x1B7A0);
        set_obj16(ITEM_STATE, o, 3);
        REG_A[6] = ITEM_STATE;
        branched(0x1B7AC, 0x1B7F2);
    } else {
        branched(0x1B6CE, 0x1B7B0);
        REG_A[6] = OBJ_ANIM_TIME;
        set_lo(7, obj16(OBJ_ANIM_TIME, o));
        if (!(lo(REG_D[7]) & 0x8000)) { branched(0x1B7BA, 0x1B7F2); goto anim; }
        set_lo(7, 1);
        into(0x1B7C2, 0x1B088);
        anim_start(o, 1);
        jumped_to(0x1B7C8);
        set_obj16(ITEM_STATE, o, 0);
        set_obj16(OBJ_HEIGHT, o, 0);
        REG_A[6] = OBJ_HEIGHT;
        door_sound(0x1B7E8);
        ran_to(0x1B7F2);
    }
anim:
    into(0x1B7F2, 0x1B032);
    anim_step(o);
    jumped_to(0x1B7F8);
    set_obj16(OBJ_ACTIVE, o, 2);
    REG_A[6] = OBJ_ACTIVE;
    ran_to(0x1B806);                                  /* (its rts) */
}

/* ---------------------------------------------------------------- springboards (kind 8) */

#define SPRING_ON        0xFFA2FE   /* bits: player 1 (1), player 2 (2) on it */

/* (where each player's part of the springboard's code is: it is not the same length) */
static const struct spring_at {
    uint32_t same, end, not_b, bra_b, held, carried, jump, drop, near, bpl, abs_done, bcc, on,
             anim, beq7, out, after;
} SPRING[2] = {
    {0x1B8D8, 0x1B99E, 0x1B8E8, 0x1B8FE, 0x1B908, 0x1B912, 0x1B91A, 0x1B922, 0x1B926, 0x1B932,
     0x1B938, 0x1B93C, 0x1B940, 0x1B956, 0x1B970, 0x1B978, 0x1B982},
    {0x1B9B4, 0x1BA74, 0x1B9C4, 0x1B9DA, 0x1B9E4, 0x1B9EE, 0x1B9F6, 0x1B9FE, 0x1BA02, 0x1BA10,
     0x1BA16, 0x1BA1A, 0x1BA1E, 0x1BA34, 0x1BA4E, 0x1BA52, 0x1BA58},
};

/* (player p's touching it changed: in state $800B he is put at its x; landing on it
 * from a jump or a drop (within 15 across), or held / carried: he is on it, its
 * animation starts, he is taken over (unless picking up loot) and let go of what moved
 * him) */
static void spring_touch(uint16_t o, int p)
{
    const struct spring_at *at = &SPRING[p];
    uint16_t bit = (uint16_t)(p ? 2 : 1);
    set_lo(7, lo(REG_D[6]) & bit);
    REG_A[6] = SPRING_ON;
    set_lo(5, obj16(SPRING_ON, o) & bit);
    if (lo(REG_D[7]) == lo(REG_D[5])) { branched(at->same, at->end); return; }
    uint32_t state = p ? P2_STATE : P1_STATE;
    REG_A[6] = state;
    uint16_t st = ram16(state);
    set_lo(7, st);
    if (st == 0x800B) {
        REG_A[6] = OBJ_X;
        set_obj16(OBJ_X, 2 * p, obj16(OBJ_X, o));
        set16(p ? 0xFF2CC8 : 0xFF2284, 0);
        branched(at->bra_b, at->end);
        return;
    }
    branched(at->not_b, at->not_b + 0x1A);
    if (ram16(0xFF56E2 + 0xAA * p)) branched(at->held, at->on);
    else if (ram16(0xFF56B2 + 0xAA * p)) branched(at->carried, at->on);
    else {
        if (st == 0x8002) branched(at->jump, at->near);
        else if (st != 0x8014) { branched(at->drop, at->end); return; }
        else ran_to(at->near);
        REG_A[6] = OBJ_X;
        uint16_t d = (uint16_t)(obj16(OBJ_X, 2 * p) - obj16(OBJ_X, o));
        if (d & 0x8000) { d = (uint16_t)-d; ran_to(at->abs_done); }
        else branched(at->bpl, at->abs_done);
        set_lo(7, d);
        if (d >= 0xF) { branched(at->bcc, at->end); return; }
        ran_to(at->on);
    }
    REG_A[6] = SPRING_ON;
    set_lo(7, obj16(SPRING_ON, o) | bit);
    set_obj16(SPRING_ON, o, lo(REG_D[7]));
    set_lo(7, 6);
    into(at->anim, 0x1B088);
    anim_start(o, 6);
    jumped_to(at->anim + 6);
    set_obj16(ITEM_STATE, o, 2);
    REG_A[6] = ITEM_STATE;
    if (ram16(state) == 0x8007) branched(at->beq7, at->after);
    else {
        uint32_t d6 = REG_D[6];
        call(p ? 0x09A1C : 0x099D6, at->out);
        if (!p) REG_D[6] = d6;                        /* (player 2's code does not keep d6) */
        ran_to(at->after);
    }
    set16(0xFF56B2 + 0xAA * p, 0);
    set16(0xFF56A4 + 0xAA * p, 0);
    set_lo(7, ram16(0xFF17C0 + 2 * p) & 0x8080);
    set16(0xFF17C0 + 2 * p, lo(REG_D[7]));
    ran_to(at->end);
}

/* (player p sent off the springboard: dropping, state $8014, a sound) */
static void spring_launch(uint16_t o, int p, uint32_t jsr)
{
    uint32_t d0 = REG_D[0], d1 = REG_D[1], d7 = REG_D[7];
    set_lo(7, 0x8014);
    uint32_t d6 = REG_D[6];
    call(p ? 0x09894 : 0x09846, jsr);
    REG_D[6] = d6;
    set16(0xFF56B2 + 0xAA * p, 0);
    REG_A[6] = ITEM_STATE;
    set_obj16(ITEM_STATE, lo(REG_D[0]), 0);
    (void)o;
    set_lo(0, 0xAF);
    call(0x02D54, jsr + 0x1E);
    REG_D[0] = d0; REG_D[1] = d1; REG_D[7] = d7;
}

/* sub_1B882: a springboard (run out of view too). A player landing on it (or held,
 * carried) is taken over and sinks with it (table 0x1BBC0 from its y), then at the
 * animation's 4th step it throws him up, dropping. */
static void springboard(uint16_t o)
{
    REG_A[6] = ITEM_STATE;
    if (!obj16(ITEM_STATE, o)) {
        set_obj16(ITEM_STATE, o, 1);
        set_obj16(OBJ_SIZE_X, o, 0xC);
        set_obj16(OBJ_SIZE_Y, o, 0xE);
        set_obj16(SPRING_ON, o, 0);
        REG_A[6] = SPRING_ON;
        branched(0x1B8B8, 0x1BBB8);
        goto anim;
    }
    branched(0x1B88C, 0x1B8BC);
    players_touching(o, 0x1B8BC);
    spring_touch(o, 0);
    spring_touch(o, 1);
    REG_A[6] = ITEM_STATE;
    if (obj16(ITEM_STATE, o) != 2) { branched(0x1BA80, 0x1BBB8); goto anim; }
    REG_A[6] = OBJ_ANIM_AT;
    set_lo(7, obj16(OBJ_ANIM_AT, o));
    if (lo(REG_D[7]) != 0x10) {
        /* sinking with it */
        REG_A[6] = SPRING_ON;
        set_lo(6, obj16(SPRING_ON, o));
        for (int p = 0; p < 2; p++) {
            if (!(lo(REG_D[6]) & (p ? 2 : 1))) {
                if (p) { branched(0x1BAE4, 0x1BBB8); goto anim; }
                branched(0x1BAA4, 0x1BAE0);
                continue;
            }
            set_lo(7, obj16(OBJ_ANIM_AT, o) >> 1);
            REG_A[6] = 0x1BBC0 + (uint32_t)(int32_t)(int16_t)lo(REG_D[7]);
            REG_A[5] = OBJ_Y;
            set_lo(7, (uint16_t)(obj16(OBJ_Y, o) + rom16(REG_A[6])));
            set_obj16(OBJ_Y, 2 * p, lo(REG_D[7]));
            set_lo(7, ram16(0xFF17C0 + 2 * p) & 0x8080);
            set16(0xFF17C0 + 2 * p, lo(REG_D[7]));
            set16(0xFF56BC + 0xAA * p, 1);
            if (p) branched(0x1BB22, 0x1BBB8);
            else ran_to(0x1BAE0);
        }
        goto anim;
    }
    /* thrown up */
    branched(0x1BA92, 0x1BB26);
    REG_A[6] = SPRING_ON;
    set_lo(7, obj16(SPRING_ON, o));
    if (!(lo(REG_D[7]) & 1)) branched(0x1BB34, 0x1BB74);
    else {
        set_lo(7, lo(REG_D[7]) & 2);
        set_obj16(SPRING_ON, o, lo(REG_D[7]));
        spring_launch(o, 0, 0x1BB4C);
        ran_to(0x1BB74);
    }
    if (!(lo(REG_D[7]) & 2)) { branched(0x1BB78, 0x1BBB8); goto anim; }
    set_lo(7, lo(REG_D[7]) & 1);
    /* (into SPRING_ON, or - player 1 thrown too - into its state: a6 as left) */
    set16(REG_A[6] + (uint32_t)(int32_t)(int16_t)lo(REG_D[0]), lo(REG_D[7]));
    spring_launch(o, 1, 0x1BB90);
    ran_to(0x1BBB8);
anim:
    into(0x1BBB8, 0x1B032);
    anim_step(o);
    jumped_to(0x1BBBE);
    ran_to(0x1BBC0);                                  /* (its rts) */
}

/* ---------------------------------------------------------------- rides (kind $C) */

#define RIDE_FRAMES      0xFF4FE2   /* frames counted (odd ones move a pixel more) */
#define RIDE_RIDER       0xFF49D6   /* the player riding (0 / 2) */
#define RIDE_LEFT        0xFF4A82   /* frames of the ride left */
#define RIDE_HOLD        0xFF4B2E   /* frames before the rider's view settles */

/* (player p gets on: taken over, state $800A, standing in it, on a box's height) */
static void ride_board(uint16_t o, int p)
{
    uint32_t b = p ? 0x7A : 0;
    set8(0xFF3A63 + 2 * p, 0);
    uint32_t d6 = REG_D[6];
    set_lo(7, 0x800A);
    call(p ? 0x09894 : 0x09846, 0x1BC68 + b);
    REG_D[6] = d6;
    REG_A[6] = OBJ_Y;
    set_lo(7, (uint16_t)(obj16(OBJ_Y, o) + 0x1C));
    set_obj16(OBJ_Y, 2 * p, lo(REG_D[7]));
    set8(0xFF3A62 + 2 * p, 2);
    set_obj16(OBJ_HEIGHT, 2 * p, 0x10);
    set_obj16(RIDE_RIDER, o, (uint16_t)(2 * p));
    set_obj16(RIDE_LEFT, o, 0x8C);
    set_obj16(RIDE_HOLD, o, 0x18);
    REG_A[6] = RIDE_HOLD;
    set16(0xFFA398 + 2 * p, o);
}

/* sub_1BBE6: a ride. 1: waiting; a player touching it (unless in state $800B) gets
 * on (a sound); 2: it carries him along (DX, DY: half a pixel a frame) for 140
 * frames, his view held after 24; 3: it goes back where it started, and he is let
 * out, 16 across, back on the floor, to his climbing code (0xC952 / 0xED82). */
static void ride(uint16_t o)
{
    set_obj16(RIDE_FRAMES, o, (uint16_t)(obj16(RIDE_FRAMES, o) + 1));
    REG_A[6] = ITEM_STATE;
    uint16_t st = obj16(ITEM_STATE, o);
    set_lo(7, st);
    if (!st) {
        set_obj16(ITEM_STATE, o, 1);
        set_obj16(OBJ_SIZE_X, o, 1);
        set_obj16(OBJ_SIZE_Y, o, 0xC);
        set_obj16(OBJ_DX, o, 3);
        set_obj16(OBJ_DY, o, 1);
        REG_A[6] = OBJ_DY;
        branched(0x1BC34, 0x1BEF6);
        goto out;
    }
    branched(0x1BBFA, 0x1BC38);
    if (st == 1) {
        uint16_t touch = (uint16_t)players_touching(o, 0x1BC40);
        if (!(touch & 1)) branched(0x1BC4A, 0x1BCC0);
        else if (ram16(P1_STATE) == 0x800B) branched(0x1BC56, 0x1BCC0);
        else { ride_board(o, 0); branched(0x1BCBC, 0x1BD38); goto boarded; }
        if (!(touch & 2)) { branched(0x1BCC4, 0x1BEF6); goto out; }
        if (ram16(P2_STATE) == 0x800B) { branched(0x1BCD0, 0x1BEF6); goto out; }
        ride_board(o, 1);
        ran_to(0x1BD38);
boarded:
        set_obj16(ITEM_STATE, o, 2);
        set_obj16(RIDE_FRAMES, o, 0);
        REG_A[6] = RIDE_FRAMES;
        {
            uint32_t d0 = REG_D[0];
            set_lo(0, 0x9F);
            call(0x02D54, 0x1BD56);
            REG_D[0] = d0;
        }
        branched(0x1BD60, 0x1BEF6);
        goto out;
    }
    branched(0x1BC3C, 0x1BD64);
    if (st == 2) {
        uint16_t hold = (uint16_t)(obj16(RIDE_HOLD, o) - 1);
        set_obj16(RIDE_HOLD, o, hold);
        if (!(hold & 0x8000)) branched(0x1BD76, 0x1BDA8);
        else {
            int p2 = obj16(RIDE_RIDER, o) != 0;
            REG_A[5] = p2 ? 0xFF5766 : 0xFF56BC;
            REG_A[4] = p2 ? 0xFF22B4 : 0xFF1870;
            REG_A[6] = RIDE_RIDER;
            if (p2) ran_to(0x1BDA0); else branched(0x1BD90, 0x1BDA0);
            set16(REG_A[5], 1);
            set16(REG_A[4], 0xFFFE);
            ran_to(0x1BDA8);
        }
        uint16_t rider = obj16(RIDE_RIDER, o);
        set_lo(7, rider);
        uint16_t odd = obj16(RIDE_FRAMES, o) & 1;
        set_lo(6, (uint16_t)((uint16_t)(odd + obj16(OBJ_DX, o)) >> 1));
        set_lo(6, (uint16_t)(obj16(OBJ_X, o) + lo(REG_D[6])));
        set_obj16(OBJ_X, o, lo(REG_D[6]));
        set_obj16(OBJ_X, rider, lo(REG_D[6]));
        set_lo(6, (uint16_t)((uint16_t)(odd + obj16(OBJ_DY, o)) >> 1));
        set_lo(5, lo(REG_D[6]));
        set_lo(6, (uint16_t)(obj16(OBJ_Y, o) + lo(REG_D[6])));
        set_obj16(OBJ_Y, o, lo(REG_D[6]));
        set_lo(5, (uint16_t)(obj16(OBJ_Y, rider) + lo(REG_D[5])));
        set_obj16(OBJ_Y, rider, lo(REG_D[5]));
        uint16_t left = (uint16_t)(obj16(RIDE_LEFT, o) - 1);
        set_obj16(RIDE_LEFT, o, left);
        if (left) { ran_to(0x1BE1E); branched(0x1BE1E, 0x1BEF6); goto out; }
        branched(0x1BE1A, 0x1BE22);
        set_obj16(ITEM_STATE, o, 3);
        REG_A[6] = ITEM_STATE;
        branched(0x1BE2E, 0x1BEF6);
        goto out;
    }
    branched(0x1BD68, 0x1BE32);
    /* back where it started; the rider let out */
    set_obj16(OBJ_X, o, obj16(0xFF508E, o));
    set_obj16(OBJ_Y, o, obj16(0xFF513A, o));
    REG_A[5] = 0xFF513A;
    set_lo(7, obj16(RIDE_RIDER, o));
    {
        uint32_t d0 = REG_D[0], d1 = REG_D[1], d7 = REG_D[7];
        set_lo(0, lo(REG_D[7]));
        for (int k = 0; k < 5; k++) {
            REG_D[1] = (uint32_t)(0x14 + 4 * k);
            call(0x1A108, 0x1BE68 + 8 * k);
        }
        REG_D[0] = d0; REG_D[1] = d1; REG_D[7] = d7;
    }
    uint16_t rider = lo(REG_D[7]);
    REG_A[6] = OBJ_HEIGHT;
    set_lo(6, (uint16_t)(obj16(OBJ_X, rider) + 0x10));
    set_obj16(OBJ_X, rider, lo(REG_D[6]));
    set_obj16(OBJ_HEIGHT, rider, 0);
    if (!rider) {
        set32(0xFF568A, 0xC952);
        set32(0xFF5690, 0xC952);
        set16(0xFFA398, 0);
        branched(0x1BECE, 0x1BEEA);
    } else {
        branched(0x1BEB0, 0x1BED0);
        set32(0xFF5734, 0xED82);
        set32(0xFF573A, 0xED82);
        set16(0xFFA39A, 0);
        ran_to(0x1BEEA);
    }
    set_obj16(ITEM_STATE, o, 1);
    REG_A[6] = ITEM_STATE;
    ran_to(0x1BEF6);
out:
    ran_to(0x1BEF8);                                  /* (its rts) */
}

/* ---------------------------------------------------------------- trip things (kind $10) */

#define TRIP_WAIT        0xFF4A82   /* frames before it trips anyone again */
#define TRIP_SIDE        0xFFA272   /* the tripped player's facing ($3A62's byte) */
#define TRIP_SLIDE       0xFF49D6   /* frames it slides */
#define TRAP_LEFT        0xFF49D6   /* (traps) frames before it goes */

/* (player p touching it trips (state $8005, asked); did he start tripping just now?
 * b: where this kind's copy of the code is from kind $10's; end: its rts) */
static int trip_player(uint16_t o, int p, uint32_t b, uint32_t end)
{
    b += p ? 0x56 : 0;
    uint32_t state = p ? P2_STATE : P1_STATE, fail = p ? end : 0x1BFAE + b;
    if (!(lo(REG_D[6]) & (p ? 2 : 1))) { branched(0x1BF5C + b, fail); return 0; }
    if (ram16(0xFF56BE + 0xAA * p)) { branched(0x1BF66 + b, fail); return 0; }
    set_lo(5, ram16(state));
    REG_D[7] = ram8(0xFF3A62 + 2 * p);
    REG_A[6] = TRIP_SIDE;
    set_obj16(TRIP_SIDE, o, lo(REG_D[7]));
    set_lo(7, 0x8005);
    uint32_t d5 = REG_D[5], d6 = REG_D[6];
    call(p ? 0x09894 : 0x09846, 0x1BF8A + b);
    REG_D[5] = d5; REG_D[6] = d6;
    if (ram16(state) != 0x8005) { branched(0x1BF9C + b, fail); return 0; }
    if (ram16(state) == lo(REG_D[5])) { branched(0x1BFA6 + b, fail); return 0; }
    if (p) ran_to(0x1C000 + b - 0x56); else branched(0x1BFAA + b, 0x1C000 + b);
    return 1;
}

/* (state 0, and state 1's wait and the players tripping, the same for kinds $10 and
 * $14 (b: the code's place, end: its rts, sliding: its state 2): 1 when it is kicked,
 * 2 in a later state, 0 when done) */
static int trip_start(uint16_t o, uint32_t b, uint32_t end, uint32_t sliding)
{
    REG_A[6] = ITEM_STATE;
    uint16_t st = obj16(ITEM_STATE, o);
    set_lo(7, st);
    if (!st) {
        set_obj16(ITEM_STATE, o, 1);
        set_obj16(OBJ_SIZE_X, o, 3);
        set_obj16(OBJ_SIZE_Y, o, 4);
        set_obj16(TRIP_WAIT, o, 0x78);
        REG_A[6] = TRIP_WAIT;
        branched(0x1BF30 + b, end);
        return 0;
    }
    branched(0x1BF02 + b, 0x1BF34 + b);
    if (st != 1) { branched(0x1BF38 + b, sliding); return 2; }
    REG_A[6] = TRIP_WAIT;
    if (obj16(TRIP_WAIT, o)) {
        set_obj16(TRIP_WAIT, o, (uint16_t)(obj16(TRIP_WAIT, o) - 1));
        branched(0x1BF4E + b, end);
        return 0;
    }
    branched(0x1BF46 + b, 0x1BF52 + b);
    players_touching(o, 0x1BF52 + b);
    return trip_player(o, 0, b, end) || trip_player(o, 1, b, end);
}

/* (kicked: its animation, its way (2 a frame), the frames it slides, a sound) */
static void trip_kick(uint16_t o, uint32_t anim_jsr, uint16_t frames, uint16_t sound, uint32_t end)
{
    set_obj16(OBJ_DX, o, lo(REG_D[6]));
    set_obj16(TRIP_SLIDE, o, frames);
    into(anim_jsr, 0x1B088);
    anim_start(o, lo(REG_D[7]));
    jumped_to(anim_jsr + 6);
    uint32_t d0 = REG_D[0];
    set_lo(0, sound);
    call(0x02D54, anim_jsr + 0xE);
    REG_D[0] = d0;
    set_obj16(ITEM_STATE, o, 2);
    REG_A[6] = ITEM_STATE;
    branched(anim_jsr + 0x24, end);
}

/* (sliding: is there a wall ahead (32 up from its feet, the cell's wall test) or else
 * a door? at: the jsr of the cell's test) */
static int trip_blocked(uint16_t o, uint32_t at, uint32_t wall_to)
{
    REG_A[6] = OBJ_DX;
    set_lo(7, obj16(OBJ_DX, o));
    uint32_t d0 = REG_D[0], d1 = REG_D[1], d7 = REG_D[7];
    set_lo(4, lo(REG_D[0]));
    REG_A[6] = OBJ_Y;
    set_lo(0, (uint16_t)(obj16(OBJ_X, o) - 0x20 + lo(REG_D[7])));
    set_lo(1, (uint16_t)(obj16(OBJ_Y, o) - 0x20));
    call(0x1016A, at);
    REG_A[6] = OBJ_SIZE_Y;
    set_lo(3, (uint16_t)(obj16(OBJ_SIZE_Y, lo(REG_D[4])) - 0x20));
    int wall = found(0x101F2, at + 0x14);
    REG_D[0] = d0; REG_D[1] = d1; REG_D[7] = d7;
    if (wall) { branched(at + 0x1E, wall_to); return 1; }
    call(0x1AD04, at + 0x26);
    uint16_t door = lo(REG_D[7]);
    REG_D[7] = d7;
    return door ? 2 : 0;
}

/* (lying again, harmless for 120 frames) */
static void trip_rest(uint16_t o)
{
    set_obj16(OBJ_FRAME, o, obj16(OBJ_PICTURE, o));
    set_obj16(ITEM_STATE, o, 1);
    set_obj16(TRIP_WAIT, o, 0x78);
    REG_A[5] = OBJ_PICTURE;
    REG_A[6] = TRIP_WAIT;
}

/* sub_1BEF8: a thing on the floor players trip on: one touching it (not already
 * shot at it...) trips, and it slides away from him (its animation $533E, the next one
 * the other way; 2 a frame for 28 frames, turned by a wall or a door, a sound), then
 * lies again, harmless for 120 frames. */
static void trip_thing(uint16_t o)
{
    int r = trip_start(o, 0, 0x1C12C, 0x1C062);
    if (r == 1) {
        /* kicked away from him */
        REG_A[6] = TRIP_SIDE;
        set_lo(7, obj16(0xFF533E, o));
        set_lo(6, 0xFFFE);
        if (obj16(TRIP_SIDE, o) == 1) branched(0x1C01A, 0x1C024);
        else { set_lo(7, (uint16_t)(lo(REG_D[7]) + 1)); set_lo(6, 2); ran_to(0x1C024); }
        trip_kick(o, 0x1C03A, 0x1C, 0xA2, 0x1C12C);
    } else if (r == 2) {
        if (obj16(ITEM_STATE, o) != 2) { branched(0x1C066, 0x1C12A); ran_to(0x1C12C); goto out; }
        int b = trip_blocked(o, 0x1C098, 0x1C0CE);
        if (!b) branched(0x1C0CA, 0x1C0DA);
        else {
            if (b == 2) ran_to(0x1C0CE);
            set_lo(7, (uint16_t)-lo(REG_D[7]));       /* turned round */
            REG_A[6] = OBJ_DX;
            set_obj16(OBJ_DX, o, lo(REG_D[7]));
        }
        REG_A[6] = OBJ_X;
        set_lo(7, (uint16_t)(obj16(OBJ_X, o) + lo(REG_D[7])));
        set_obj16(OBJ_X, o, lo(REG_D[7]));
        REG_A[6] = TRIP_SLIDE;
        uint16_t left = (uint16_t)(obj16(TRIP_SLIDE, o) - 1);
        set_obj16(TRIP_SLIDE, o, left);
        if (left) branched(0x1C0F2, 0x1C120);
        else { trip_rest(o); ran_to(0x1C120); }
        into(0x1C120, 0x1B032);
        anim_step(o);
        jumped_to(0x1C126);
        branched(0x1C126, 0x1C12C);
    }
out:
    ran_to(0x1C12E);                                  /* (its rts) */
}

/* sub_1C12E: the same, sliding away for 32 frames (animation $10, the other way $F,
 * another sound), and stopped by a wall or a door. */
static void trip_thing2(uint16_t o)
{
    int r = trip_start(o, 0x236, 0x1C352, 0x1C294);
    if (r == 1) {
        set_lo(7, 0x10);
        set_lo(6, 2);
        REG_A[6] = TRIP_SIDE;
        if (obj16(TRIP_SIDE, o) == 1) branched(0x1C24A, 0x1C256);
        else { set_lo(7, 0xF); set_lo(6, 0xFFFE); ran_to(0x1C256); }
        trip_kick(o, 0x1C26C, 0x20, 0xA5, 0x1C352);
    } else if (r == 2) {
        if (obj16(ITEM_STATE, o) != 2) { branched(0x1C298, 0x1C350); ran_to(0x1C352); goto out; }
        int b = trip_blocked(o, 0x1C2CA, 0x1C31C);
        if (b == 2) branched(0x1C2FC, 0x1C31C);
        if (!b) {
            ran_to(0x1C300);
            REG_A[6] = OBJ_X;
            set_lo(7, (uint16_t)(obj16(OBJ_X, o) + lo(REG_D[7])));
            set_obj16(OBJ_X, o, lo(REG_D[7]));
            REG_A[6] = TRIP_SLIDE;
            uint16_t left = (uint16_t)(obj16(TRIP_SLIDE, o) - 1);
            set_obj16(TRIP_SLIDE, o, left);
            if (left) { branched(0x1C318, 0x1C346); goto anim; }
            ran_to(0x1C31C);
        }
        trip_rest(o);
        ran_to(0x1C346);
anim:
        into(0x1C346, 0x1B032);
        anim_step(o);
        jumped_to(0x1C34C);
        branched(0x1C34C, 0x1C352);
    }
out:
    ran_to(0x1C354);                                  /* (its rts) */
}

/* ---------------------------------------------------------------- sticky traps (kind $18) */

/* (player p touching it is stuck (state $8012, asked; if it took): the trap and his side
 * noted ($A38C, $A390), the trap done in 6 frames) */
static int trap_catches(uint16_t o, int p)
{
    uint32_t b = p ? 0x62 : 0;
    uint32_t state = p ? P2_STATE : P1_STATE;
    set_lo(6, ram16(state));
    uint32_t d6 = REG_D[6];
    set_lo(7, 0x8012);
    call(p ? 0x09894 : 0x09846, 0x1C3B4 + b);
    REG_D[6] = d6;
    if (ram16(state) == lo(REG_D[6])) { branched(0x1C3C4 + b, 0x1C492); return 0; }
    set16(0xFFA390 + 2 * p, ram16(0xFFA3BC + 2 * p));
    set16(0xFFA38C + 2 * p, o);
    set_obj16(TRAP_LEFT, o, 6);
    set_obj16(ITEM_STATE, o, 2);
    set_obj16(0xFFA0CE, o, (uint16_t)(2 * p));
    REG_A[6] = 0xFFA0CE;
    branched(0x1C3FC + b, 0x1C492);
    return 1;
}

/* sub_1C354: a sticky trap (always drawn): a player touching it is stuck; it goes 6
 * frames later. */
static void trap(uint16_t o)
{
    REG_A[6] = ITEM_STATE;
    uint16_t st = obj16(ITEM_STATE, o);
    set_lo(7, st);
    if (!st) {
        set_obj16(ITEM_STATE, o, 1);
        set_obj16(OBJ_SIZE_X, o, 4);
        set_obj16(OBJ_SIZE_Y, o, 2);
        set_obj16(OBJ_ACTIVE, o, 2);
        REG_A[6] = OBJ_ACTIVE;
        branched(0x1C38C, 0x1C492);
    } else if (branched(0x1C35E, 0x1C390), st == 1) {
        uint16_t touch = (uint16_t)players_touching(o, 0x1C398);
        if (touch & 1) { ran_to(0x1C3A6); trap_catches(o, 0); }
        else if (branched(0x1C3A2, 0x1C400), touch & 2) { ran_to(0x1C408); trap_catches(o, 1); }
        else branched(0x1C404, 0x1C492);
    } else if (branched(0x1C394, 0x1C462), st == 2) {
        REG_A[6] = TRAP_LEFT;
        uint16_t left = (uint16_t)(obj16(TRAP_LEFT, o) - 1);
        set_obj16(TRAP_LEFT, o, left);
        if (left) branched(0x1C474, 0x1C492);
        else {
            set_obj16(ITEM_STATE, o, 3);
            REG_A[6] = ITEM_STATE;
            branched(0x1C484, 0x1C492);
        }
    } else {
        branched(0x1C466, 0x1C488);
        set_obj16(OBJ_FRAME, o, 0);
        REG_A[6] = OBJ_FRAME;
        ran_to(0x1C492);
    }
    ran_to(0x1C494);                                  /* (its rts) */
}

/* ---------------------------------------------------------------- presses (kind $1C) */

#define PRESS_AT         0xFF49D6   /* where it is in its moves (table 0x1C602: dy, crushes) */
#define PRESS_MAKE1      0xFF4A82   /* frames before it makes its part $40 */
#define PRESS_MAKE2      0xFF4B2E   /*   ... $44 */

/* (player p touching it crushed: state $8018, asked) */
static void press_crushes(int p, uint32_t at)
{
    uint16_t bit = (uint16_t)(p ? 2 : 1);
    uint32_t end = at + 0x20;
    if (!(lo(REG_D[6]) & bit)) { branched(at, end); return; }
    if (ram16(0xFF56BE + 0xAA * p)) { branched(at + 0xA, end); return; }
    set_lo(7, 0x8018);
    uint32_t d6 = REG_D[6];
    call(p ? 0x09894 : 0x09846, at + 0x16);
    REG_D[6] = d6;
    ran_to(end);
}

/* sub_1C494: a press (always drawn). Set up, it makes its part (kind $40) and waits
 * (state 1) until it is set going: then it makes its parts $40 and $44 (after 1 and 9
 * frames) and moves by its table; on the steps that crush, players touching it (shots
 * passing or not; the game puts player 2's back wrong) are crushed. At the table's end
 * it starts again. */
static void press(uint16_t o)
{
    REG_A[6] = ITEM_STATE;
    uint16_t st = obj16(ITEM_STATE, o);
    set_lo(7, st);
    if (!st) {
        set_obj16(ITEM_STATE, o, 1);
        set_obj16(OBJ_SIZE_X, o, 0x10);
        set_obj16(OBJ_SIZE_Y, o, 8);
        set_obj16(PRESS_AT, o, 0);
        set_obj16(PRESS_MAKE1, o, 1);
        set_obj16(PRESS_MAKE2, o, 9);
        set_obj16(OBJ_ACTIVE, o, 2);
        REG_A[6] = OBJ_ACTIVE;
        set_lo(7, 0x40);
        call(0x18736, 0x1C4F2);
        branched(0x1C4F8, 0x1C5F4);
        goto out;
    }
    branched(0x1C49E, 0x1C4FC);
    if (st == 1) { ran_to(0x1C508); branched(0x1C508, 0x1C5F4); goto out; }
    branched(0x1C500, 0x1C50C);
    REG_A[6] = PRESS_MAKE1;
    set_obj16(PRESS_MAKE1, o, (uint16_t)(obj16(PRESS_MAKE1, o) - 1));
    if (obj16(PRESS_MAKE1, o)) branched(0x1C516, 0x1C524);
    else { set_lo(7, 0x40); call(0x18736, 0x1C51E); o = lo(REG_D[0]); ran_to(0x1C524); }
    REG_A[6] = PRESS_MAKE2;
    set_obj16(PRESS_MAKE2, o, (uint16_t)(obj16(PRESS_MAKE2, o) - 1));
    if (obj16(PRESS_MAKE2, o)) branched(0x1C52E, 0x1C53C);
    else { set_lo(7, 0x44); call(0x18736, 0x1C536); o = lo(REG_D[0]); ran_to(0x1C53C); }
    REG_A[6] = PRESS_AT;
    set_lo(7, obj16(PRESS_AT, o));
    REG_A[5] = 0x1C602;
    set_lo(6, rom16(0x1C602 + (int16_t)lo(REG_D[7])));
    set_lo(7, rom16(0x1C604 + (int16_t)lo(REG_D[7])));
    if (lo(REG_D[6]) == 0x8000) {
        branched(0x1C556, 0x1C5F6);
        set_obj16(ITEM_STATE, o, 0);
        REG_A[6] = ITEM_STATE;
        ran_to(0x1C602);                              /* (its rts) */
        return;
    }
    REG_A[5] = OBJ_Y;
    set_lo(6, (uint16_t)(obj16(OBJ_Y, o) + lo(REG_D[6])));
    set_obj16(OBJ_Y, o, lo(REG_D[6]));
    set_obj16(PRESS_AT, o, (uint16_t)(obj16(PRESS_AT, o) + 4));
    if (!lo(REG_D[7])) { branched(0x1C56E, 0x1C5F4); goto out; }
    {
        REG_A[6] = OBJ_PASS;
        set_lo(5, ram16(OBJ_PASS));
        set_lo(4, ram16(OBJ_PASS + 2));
        set16(OBJ_PASS, 0);
        set16(OBJ_PASS + 2, 0);
        uint32_t d4 = REG_D[4], d5 = REG_D[5];
        players_touching(o, 0x1C58C);
        REG_D[4] = d4; REG_D[5] = d5;
        REG_A[6] = OBJ_PASS;
        set16(OBJ_PASS, lo(REG_D[5]));
        set16(OBJ_PASS, lo(REG_D[4]));                /* (the game's: player 2's not put back) */
    }
    press_crushes(0, 0x1C5A2);
    press_crushes(1, 0x1C5C6);
    set16(0xFFA3BA, 1);
    call(0x1A7D8, 0x1C5EE);
    ran_to(0x1C5F4);
out:
    ran_to(0x1C5F6);                                  /* (its rts) */
}

/* ---------------------------------------------------------------- holes (kind $20) */

#define HOLE_WAIT        0xFF4A82   /* frames before it takes anyone */
#define HOLE_LEFT        0xFF49D6   /* frames it holds him */
#define HOLE_WHO         0xFFA042   /* who is in it (0 / 2) */

/* (player p touching it falls in (state $8006, asked): did he?) */
static int hole_takes(int p)
{
    uint32_t b = p ? 0x40 : 0, end = 0x1C8B4, fail = p ? end : 0x1C77A;
    if (!(lo(REG_D[6]) & (p ? 2 : 1))) { branched(0x1C73E + b, fail); return 0; }
    if (ram16(0xFF56BE + 0xAA * p)) { branched(0x1C748 + b, fail); return 0; }
    set_lo(7, 0x8006);
    uint32_t d6 = REG_D[6];
    call(p ? 0x09894 : 0x09846, 0x1C754 + b);
    REG_D[6] = d6;
    if (ram16(p ? P2_STATE : P1_STATE) != 0x8006) { branched(0x1C766 + b, fail); return 0; }
    return 1;
}

/* sub_1C6E2: a hole (in the floor): every 60 frames it can take a player touching it:
 * he falls in (state $8006), put at its x - 5, facing out, held 120 frames (its
 * animations $11, $13, a sound), then let out ($12) and taken over no more. */
static void hole(uint16_t o)
{
    REG_A[6] = ITEM_STATE;
    uint16_t st = obj16(ITEM_STATE, o);
    set_lo(7, st);
    if (!st) {
        set_obj16(ITEM_STATE, o, 1);
        set_obj16(OBJ_SIZE_X, o, 1);
        set_obj16(OBJ_SIZE_Y, o, 4);
        set_obj16(HOLE_WAIT, o, 0x3C);
        REG_A[6] = HOLE_WAIT;
        branched(0x1C71A, 0x1C8B4);
        goto out;
    }
    branched(0x1C6EC, 0x1C71E);
    if (st == 1) {
        REG_A[6] = HOLE_WAIT;
        uint16_t w = (uint16_t)(obj16(HOLE_WAIT, o) - 1);
        set_obj16(HOLE_WAIT, o, w);
        if (!(w & 0x8000)) { branched(0x1C730, 0x1C8B4); goto out; }
        players_touching(o, 0x1C734);
        if (hole_takes(0)) { REG_D[7] = 0; set_obj16(HOLE_WHO, o, 0); branched(0x1C776, 0x1C7B6); }
        else if (hole_takes(1)) { REG_D[7] = 2; set_obj16(HOLE_WHO, o, 2); ran_to(0x1C7B6); }
        else goto out;
        REG_A[6] = OBJ_X;
        uint16_t who = lo(REG_D[7]);
        set8(0xFF3A62 + (int16_t)who, 2);
        set_lo(6, (uint16_t)(obj16(OBJ_X, o) - 5));
        set_obj16(OBJ_X, who, lo(REG_D[6]));
        set_lo(7, 0x11);
        into(0x1C7D8, 0x1B088);
        anim_start(o, 0x11);
        jumped_to(0x1C7DE);
        set_obj16(HOLE_LEFT, o, 0x78);
        {
            uint32_t d0 = REG_D[0];
            set_lo(0, 0x9E);
            call(0x02D54, 0x1C7F2);
            REG_D[0] = d0;
        }
        set_obj16(ITEM_STATE, o, 2);
        REG_A[6] = ITEM_STATE;
        branched(0x1C808, 0x1C8B4);
        goto out;
    }
    branched(0x1C722, 0x1C80C);
    if (st == 2) {
        REG_A[6] = OBJ_ANIM_TIME;
        set_lo(7, obj16(OBJ_ANIM_TIME, o));
        if (!(lo(REG_D[7]) & 0x8000)) branched(0x1C81E, 0x1C82C);
        else {
            set_lo(7, 0x13);
            into(0x1C826, 0x1B088);
            anim_start(o, 0x13);
            jumped_to(0x1C82C);
        }
        REG_A[6] = HOLE_LEFT;
        set_obj16(HOLE_LEFT, o, (uint16_t)(obj16(HOLE_LEFT, o) - 1));
        if (obj16(HOLE_LEFT, o)) {
            branched(0x1C836, 0x1C87E);
            into(0x1C87E, 0x1B032);
            anim_step(o);
            jumped_to(0x1C884);
            branched(0x1C884, 0x1C8B4);
            goto out;
        }
        set_lo(7, 0x12);
        into(0x1C83E, 0x1B088);
        anim_start(o, 0x12);
        jumped_to(0x1C844);
        set_obj16(ITEM_STATE, o, 3);
        REG_A[6] = HOLE_WHO;
        if (obj16(HOLE_WHO, o)) {
            branched(0x1C85A, 0x1C874);
            call(0x09A1C, 0x1C874);
            branched(0x1C87A, 0x1C8B4);
        } else if (ram16(P1_STATE) != 0x8006) branched(0x1C866, 0x1C8B4);
        else { call(0x099D6, 0x1C86A); branched(0x1C870, 0x1C8B4); }
        goto out;
    }
    branched(0x1C810, 0x1C888);
    into(0x1C888, 0x1B032);
    anim_step(o);
    jumped_to(0x1C88E);
    REG_A[6] = OBJ_ANIM_TIME;
    set_lo(7, obj16(OBJ_ANIM_TIME, o));
    if (!(lo(REG_D[7]) & 0x8000)) branched(0x1C898, 0x1C8B4);
    else {
        set_obj16(HOLE_WAIT, o, 0x3C);
        set_obj16(ITEM_STATE, o, 1);
        REG_A[6] = ITEM_STATE;
        ran_to(0x1C8B4);
    }
out:
    ran_to(0x1C8B6);                                  /* (its rts) */
}

/* ---------------------------------------------------------------- hiding places (kind $24) */

#define HIDE_WAIT        0xFF49D6   /* frames before anyone can hide in it (counts on below 0) */

/* sub_1C8B6: a hiding place: after its first second, a player touching it is asked to
 * hide in it (state $800D; it noted at $A394 / $A396), player 1 first. */
static void hiding_place(uint16_t o)
{
    REG_A[6] = ITEM_STATE;
    uint16_t st = obj16(ITEM_STATE, o);
    set_lo(7, st);
    if (!st) {
        set_obj16(ITEM_STATE, o, 1);
        set_obj16(OBJ_SIZE_X, o, 0xD);
        set_obj16(OBJ_SIZE_Y, o, 8);
        set_obj16(HIDE_WAIT, o, 0x3C);
        REG_A[6] = HIDE_WAIT;
        branched(0x1C8EE, 0x1C948);
    } else if (branched(0x1C8C0, 0x1C8F2), st != 1) {
        branched(0x1C8F6, 0x1C946);
        ran_to(0x1C948);
    } else {
        REG_A[6] = HIDE_WAIT;
        uint16_t w = (uint16_t)(obj16(HIDE_WAIT, o) - 1);
        set_obj16(HIDE_WAIT, o, w);
        if (!(w & 0x8000)) branched(0x1C904, 0x1C948);
        else {
            uint16_t touch = (uint16_t)players_touching(o, 0x1C908);
            int p = (touch & 1) ? 0 : (touch & 2) ? 1 : -1;
            if (p) branched(0x1C912, 0x1C92A);
            if (p < 0) branched(0x1C92E, 0x1C948);
            else {
                set16(0xFFA394 + 2 * p, o);
                set_lo(7, 0x800D);
                call(p ? 0x09894 : 0x09846, p ? 0x1C93C : 0x1C920);
                branched(p ? 0x1C942 : 0x1C926, 0x1C948);
            }
        }
    }
    ran_to(0x1C94A);                                  /* (its rts) */
}

/* ---------------------------------------------------------------- trapdoors (kind $28) */

#define TRAPDOOR_LANE    0xFF47D2   /* (long) its place in the map's lanes (0xFF643E, or
                                       0xFF645A for one on the upper lanes) */
#define TRAPDOOR_LEFT    0xFF49D6   /* frames to its next step */

/* (players on it - not on a box, not in state $800B: touching it at its own height,
 * as tall as 24 - sub_1A8F2) */
static void trapdoor_touch(uint16_t o, uint32_t jsr)
{
    set_obj16(OBJ_SIZE_X, o, 1);
    set_obj16(OBJ_SIZE_Y, o, 4);
    call(0x1A8F2, jsr);
    set_obj16(OBJ_SIZE_X, o, 1);
    set_obj16(OBJ_SIZE_Y, o, 0x18);
    REG_A[6] = OBJ_SIZE_Y;
}
static int trapdoor_on(int p, uint32_t at, uint32_t fail)
{
    if (!(lo(REG_D[6]) & (p ? 2 : 1))) { branched(at, fail); return 0; }
    if (obj16(OBJ_HEIGHT, 2 * p)) { branched(at + 0xA, fail); return 0; }
    if (ram16(p ? P2_STATE : P1_STATE) == 0x800B) { branched(at + 0x16, fail); return 0; }
    return 1;
}

/* (its lane's walls there: 5 lanes' bytes, 4 apart) */
static void trapdoor_walls(uint16_t o, uint8_t v)
{
    set_lo(7, (uint16_t)(2 * o));
    REG_A[6] = obj32(TRAPDOOR_LANE, o);
    for (int k = -1; k < 4; k++) set8(REG_A[6] + 4 * k, v);
}

/* sub_1C94A: a trapdoor: a player standing on it (2 frames) opens it (animation $16):
 * the floor there goes from the map's lanes and he falls through (his script 0xB858 /
 * 0xDD4C, falling 16 / 12, taken over); 30 frames later it shuts again ($17). */
static void trapdoor(uint16_t o)
{
    REG_A[6] = ITEM_STATE;
    uint16_t st = obj16(ITEM_STATE, o);
    set_lo(7, st);
    if (!st) {
        set_obj16(ITEM_STATE, o, 1);
        set_obj16(OBJ_SIZE_X, o, 0);
        set_obj16(OBJ_SIZE_Y, o, 0x18);
        REG_D[7] = 0xFF643E;
        REG_A[6] = 0xFF3A62;
        if (!ram8(0xFF3A62 + (int16_t)o)) branched(0x1C986, 0x1C990);
        else { REG_D[7] = 0xFF645A; ran_to(0x1C990); }
        set_lo(6, (uint16_t)(2 * o));
        set_obj32(TRAPDOOR_LANE, o, REG_D[7]);
        REG_A[6] = TRAPDOOR_LANE;
        branched(0x1C99E, 0x1CC2E);
        goto out;
    }
    branched(0x1C954, 0x1C9A2);
    if (st == 1) {
        trapdoor_touch(o, 0x1C9C2);
        for (int p = 0; p < 2; p++) {
            uint32_t at = p ? 0x1CA22 : 0x1C9E4, fail = p ? 0x1CC2E : 0x1CA1E;
            if (!trapdoor_on(p, at, fail)) continue;
            set_obj16(TRAPDOOR_LEFT, o, 2);
            set_obj16(ITEM_STATE, o, 2);
            REG_A[6] = ITEM_STATE;
            if (p) branched(0x1CA5C, 0x1CC2E); else ran_to(0x1CA1E);
        }
        goto out;
    }
    branched(0x1C9A6, 0x1CA60);
    if (st == 2) {
        REG_A[6] = TRAPDOOR_LEFT;
        set_obj16(TRAPDOOR_LEFT, o, (uint16_t)(obj16(TRAPDOOR_LEFT, o) - 1));
        if (obj16(TRAPDOOR_LEFT, o)) { branched(0x1CA72, 0x1CC2E); goto out; }
        set_lo(7, 0x16);
        into(0x1CA7A, 0x1B088);
        anim_start(o, 0x16);
        jumped_to(0x1CA80);
        set_obj16(ITEM_STATE, o, 3);
        REG_A[6] = ITEM_STATE;
        branched(0x1CA8C, 0x1CC2E);
        goto out;
    }
    branched(0x1CA64, 0x1CA90);
    if (st == 3) {
        /* open: the floor goes, and who stands on it falls */
        set_obj16(ITEM_STATE, o, 4);
        trapdoor_walls(o, 0);
        trapdoor_touch(o, 0x1CAE8);
        for (int p = 0; p < 2; p++) {
            uint32_t at = p ? 0x1CB50 : 0x1CB0A, fail = p ? 0x1CC28 : 0x1CB4C;
            if (!trapdoor_on(p, at, fail)) continue;
            uint32_t d0 = REG_D[0], d1 = REG_D[1], d6 = REG_D[6];
            REG_A[6] = p ? 0x0DD4C : 0x0B858;
            set16(P1_LIFT + 2 * p, p ? 0xC : 0x10);
            call(p ? 0x09A8C : 0x09A62, at + 0x2C);
            call(p ? 0x09A1C : 0x099D6, at + 0x32);
            set_obj16(OBJ_HEIGHT, 2 * p, 0);
            REG_D[0] = d0; REG_D[1] = d1; REG_D[6] = d6;
            if (p) branched(0x1CB92, 0x1CC28); else ran_to(0x1CB4C);
        }
        goto anim;
    }
    branched(0x1CA94, 0x1CB96);
    if (st == 4) {
        set_obj16(ITEM_STATE, o, 5);
        set_obj16(TRAPDOOR_LEFT, o, 0x1E);
        REG_A[6] = TRAPDOOR_LEFT;
        branched(0x1CBB6, 0x1CC28);
        goto anim;
    }
    branched(0x1CB9A, 0x1CBBA);
    /* shutting */
    REG_A[6] = OBJ_ANIM;
    if (obj16(OBJ_ANIM, o) == 0x17) branched(0x1CBC6, 0x1CBE2);
    else {
        REG_A[6] = OBJ_ANIM_TIME;
        set_lo(7, obj16(OBJ_ANIM_TIME, o));
        if (!(lo(REG_D[7]) & 0x8000)) { branched(0x1CBD4, 0x1CC28); goto anim; }
        set_lo(7, 0x17);
        into(0x1CBDC, 0x1B088);
        anim_start(o, 0x17);
        jumped_to(0x1CBE2);
    }
    REG_A[6] = TRAPDOOR_LEFT;
    set_obj16(TRAPDOOR_LEFT, o, (uint16_t)(obj16(TRAPDOOR_LEFT, o) - 1));
    if (obj16(TRAPDOOR_LEFT, o)) { branched(0x1CBEC, 0x1CC28); goto anim; }
    trapdoor_walls(o, 1);
    set_obj16(ITEM_STATE, o, 1);
    REG_A[6] = ITEM_STATE;
    ran_to(0x1CC28);
anim:
    into(0x1CC28, 0x1B032);
    anim_step(o);
    jumped_to(0x1CC2E);
out:
    ran_to(0x1CC30);                                  /* (its rts) */
}

/* ---------------------------------------------------------------- the way out (kind $2C) */

/* sub_1CC30: the way out (noted at $A5C4): until all the loot is taken, players
 * touching it are stopped as by a door ($A3A8 / $A3AA: at the way out). */
static void way_out(uint16_t o)
{
    REG_A[6] = ITEM_STATE;
    set_lo(7, obj16(ITEM_STATE, o));
    if (!lo(REG_D[7])) {
        set_obj16(ITEM_STATE, o, 1);
        set_obj16(OBJ_SIZE_X, o, 6);
        set_obj16(OBJ_SIZE_Y, o, 0x10);
        REG_A[6] = OBJ_SIZE_Y;
        set16(0xFFA5C4, o);
        branched(0x1CC62, 0x1CCBA);
    } else if (branched(0x1CC3A, 0x1CC66), ram16(LOOT_ALL)) {
        branched(0x1CC6C, 0x1CCBA);
    } else {
        uint16_t touch = (uint16_t)players_touching(o, 0x1CC70);
        for (int p = 0; p < 2; p++) {
            if (!(touch & (p ? 2 : 1))) { branched(p ? 0x1CC9C : 0x1CC7A, p ? 0x1CCBA : 0x1CC98); continue; }
            set16(P1_TOUCHING + 2 * p, 1);
            set16(0xFFA3B2 + 2 * p, ram16(0xFFA3BC + 2 * p));
            set16(0xFFA3A8 + 2 * p, 1);
            ran_to(p ? 0x1CCBA : 0x1CC98);
        }
    }
    ran_to(0x1CCBC);                                  /* (its rts) */
}

/* ---------------------------------------------------------------- the frame */

typedef void (*item_fn)(uint16_t o);
static const item_fn ITEM[12] = { loot, door, springboard, ride, trip_thing, trip_thing2, trap, press, hole, hiding_place, trapdoor, way_out };

/* sub_1AFDA: each item in play, near a view (kind 8 always), runs its kind's routine */
int logic_items_frame(void)
{
    if (!start()) return 0;
    begin(0x1AFDA);
    set_lo(0, 0x4C);
    set_lo(1, 0x1F);
    ran_to(0x1AFE2);
    for (;;) {
        uint16_t o = lo(REG_D[0]);
        REG_A[6] = OBJ_ACTIVE;
        if (!obj16(OBJ_ACTIVE, o)) branched(0x1AFEC, 0x1B02A);
        else {
            REG_A[6] = ITEM_KIND;
            int run = 1;
            if (obj16(ITEM_KIND, o) == 8) branched(0x1AFFC, 0x1B00E);
            else {
                REG_A[6] = OBJ_ON_SCREEN;
                if (!obj16(OBJ_ON_SCREEN, o)) { branched(0x1B00A, 0x1B02A); run = 0; }
                else ran_to(0x1B00E);
            }
            if (run) {
                uint16_t kind = obj16(ITEM_KIND, o);
                set_lo(7, kind);
                REG_A[6] = rom32(0x1B0CC + (int16_t)kind);
                uint32_t d0 = REG_D[0], d1 = REG_D[1];
                item_fn f = !(kind & 3) && kind < 4 * 12 ? ITEM[kind >> 2] : 0;
                {
                    static char names[2][13][24];
                    char *nm = names[!f][kind / 4 < 13 ? kind / 4 : 12];
                    if (!nm[0]) snprintf(nm, 24, "item kind %02X%s", kind, f ? "" : " (68000)");
                    count_named(nm);
                }
                if (f) {
                    ran_to(0x1B026);                  /* (jsr (a6)) */
                    jumped_to(REG_A[6]);
                    f(o);
                    jumped_to(0x1B026);
                } else call(REG_A[6], 0x1B024);
                REG_D[0] = d0; REG_D[1] = d1;
                ran_to(0x1B02A);
            }
        }
        set_lo(0, (uint16_t)(o + 2));
        uint16_t c = lo(REG_D[1]);
        set_lo(1, (uint16_t)(c - 1));
        if (!c) { ran_to(0x1B030); break; }
        branched(0x1B02C, 0x1AFE2);
    }
    return done();
}
