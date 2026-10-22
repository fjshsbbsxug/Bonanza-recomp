/* The time and the map's marks each frame (sub_105CC): the time counted down and drawn,
 * what comes when it runs out (the players out, then back in if they have lives left),
 * the time left going into points at the way out, and the HUD's sprites: each player's
 * mark and the guards' on the map, and the map's fixed sprites. */
#include "rt.h"
#include "../video.h"

#define TICKS        0x110F4   /* (bytes, by difficulty, every other) frames a second lasts */
#define HURRY        0x110FA   /* (longs, by difficulty) the time the hurry sound comes */
#define MAP_PLACE    0x10D4C   /* (by stage entry: y, x) where the map is on the screen */
#define MAP_GUARDS   0x10DBC   /* (by stage entry: first, last) the objects marked on it */
#define MAP_SPRITES  0x10D04   /* the map's fixed sprites */

/* sub_10912: the map's 9 fixed sprites at a0, each linked to the next */
static void hud_fixed(void)
{
    uint32_t a0 = REG_A[0], a6 = MAP_SPRITES;
    for (int k = 0; k < 9; k++) {
        set16(a0, rom16(a6));
        uint16_t c = (uint16_t)(ram16(SPRITE_COUNT) + 1);
        set16(SPRITE_COUNT, c);
        set_lo(7, (uint16_t)(c + rom16(a6 + 2)));
        set16(a0 + 2, lo(REG_D[7]));
        set32(a0 + 4, rom32(a6 + 4));
        a0 += 8; a6 += 8;
    }
    REG_A[0] = a0;
    REG_A[6] = a6;
}

/* the time's three digits (a0: the timer) into the HUD's tiles; at: its addq.l #1, a0 */
static void hud_digits(uint32_t at)
{
    uint32_t a0 = REG_A[0] + 1;
    video_at_long(0x7E8A0003);
    REG_A[1] = 0xC00000;
    for (int k = 0; k < 3; k++) {
        if (k == 1) video_at_long(0x7E8E0003);
        set_lo(1, (uint16_t)(0x8520 + ram8(a0++)));
        video_put(lo(REG_D[1]));
    }
    REG_A[0] = a0;
    ran_to(at + 0x40);
}

/* a0 at the sprite table's end (the count times 8, kept below 256: the game's) */
static void sprites_end_byte(void)
{
    REG_D[0] = (uint32_t)(uint16_t)(ram16(SPRITE_COUNT) << 3) & 0xFF;
    REG_A[0] = SPRITE_TABLE + REG_D[0];
}

/* y / 10 by counting tens (loop: its subi.w #$a) */
static uint16_t tens(uint16_t y, uint32_t loop)
{
    uint16_t k = 0;
    REG_D[7] = 0;
    ran_to(loop);
    for (;;) {
        y = (uint16_t)(y - 10);
        if (y & 0x8000) { branched(loop + 4, loop + 0xC); break; }
        k++;
        branched(loop + 0xA, loop);
    }
    set_lo(7, k);
    return k;
}

/* a mark at the map: y / 10 and x / 16 placed by the stage's map */
static void mark(uint16_t y10, uint16_t x16, uint16_t attr)
{
    uint32_t a0 = REG_A[0];
    REG_D[7] = (uint32_t)(uint16_t)(ram16(STAGE_ENTRY) << 2);
    REG_D[1] = (uint16_t)(y10 + rom16(MAP_PLACE + (int16_t)lo(REG_D[7])));
    REG_D[2] = (uint16_t)(x16 + rom16(MAP_PLACE + 2 + (int16_t)lo(REG_D[7])));
    set16(a0, lo(REG_D[1]));
    set_lo(7, (uint16_t)(ram16(SPRITE_COUNT) + 1));
    set16(a0 + 2, lo(REG_D[7]));
    set16(a0 + 4, attr);
    set16(a0 + 6, lo(REG_D[2]));
    REG_A[0] = a0 + 8;
    set16(SPRITE_COUNT, (uint16_t)(ram16(SPRITE_COUNT) + 1));
}

/* (a player out when the time ran out: put back where he started ($508E...) and his
 * stage's routine for it; one caught at that moment: a "caught" message unless the way
 * out came) */
static void player_time_out(int p)
{
    uint32_t beq = p ? 0x10BA2 : 0x10AF8, end = p ? 0x10C40 : 0x10B98, j = p ? 0x10C18 : 0x10B70;
    if (!obj16(OBJ_ACTIVE, 2 * p)) { branched(beq, end); return; }
    if (ram16(p ? P2_STATE : P1_STATE) == 0x8017) {
        uint32_t c = p ? 0x10C24 : 0x10B7C;
        branched(beq + 0xC, c);
        if (ram16(ESCAPE)) { branched(c + 6, end); return; }
        call(0x187CE, c + 0xA);
        REG_D[0] = p ? 2 : 0;
        set_lo(7, 0x30);
        call(0x18736, c + 0x16);
        ran_to(end);
        return;
    }
    set16(P1_LIVES + 2 * p, ram16(P1_LIVES_KEPT + 2 * p));
    set16(P1_OUT + 2 * p, 1);
    set16(OBJ_ACTIVE + 2 * p, 0x8000);
    if (!p) REG_D[0] = 0;
    set_lo(7, (uint16_t)(ram16(PLAYERS_OUT) | (p ? 2 : 1)));
    set16(PLAYERS_OUT, lo(REG_D[7]));
    set16(OBJ_X + 2 * p, ram16(0xFF508E + 2 * p));
    set16(OBJ_Y + 2 * p, ram16(0xFF513A + 2 * p));
    set16(OBJ_HEIGHT + 2 * p, ram16(0xFF51E6 + 2 * p));
    set16(0xFF3A62 + 2 * p, ram16(0xFF53EA + 2 * p));
    set_lo(0, (uint16_t)((ram16(STAGE_ENTRY) - 1) << 2));
    REG_A[0] = rom32((p ? 0x0459A : 0x04566) + (int16_t)lo(REG_D[0]));
    call(REG_A[0], j);
    call(p ? 0x11D50 : 0x11D36, j + 2);
    branched(j + 8, end);
}

int logic_hud_frame(void)
{
    if (!start()) return 0;
    begin(0x105CC);
    if (ram16(DEMO)) {
        branched(0x105D2, 0x108FA);
        goto fixed;
    }
    REG_A[0] = TIMER;
    if (ram16(ESCAPING)) {
        branched(0x105E2, 0x109DE);
        goto escaping;
    }
    if (ram16(TIME_FROZEN)) { branched(0x105EC, 0x106E4); goto digits; }
    if (ram8(TIME_UP) == 0x7F) { branched(0x105F8, 0x10C62); goto back_in; }
    if ((int16_t)ram16(TIME_UP) < 0) { branched(0x10602, 0x10AB8); goto up_wait; }
    if (ram16(TIME_UP)) { branched(0x1060C, 0x10AE4); goto up_count; }

    /* the time runs while either player plays (in play, not taken over, not caught) */
    if (!ram8(OBJ_ACTIVE + 1)) branched(0x10616, 0x10630);
    else if (ram8(P1_TAKEN_OVER)) branched(0x10620, 0x10630);
    else if (ram16(P1_STATE) != 0x8017) { branched(0x1062C, 0x10650); goto tick; }
    else ran_to(0x10630);
    if (!ram8(OBJ_ACTIVE + 3)) { branched(0x10636, 0x106E4); goto digits; }
    if (ram8(P1_TAKEN_OVER + 0xAA)) { branched(0x10640, 0x106E4); goto digits; }
    if (ram16(P2_STATE) == 0x8017) { branched(0x1064C, 0x106E4); goto digits; }
    ran_to(0x10650);
tick:
    {
        uint8_t t = (uint8_t)(ram8(TIMER) - 1);
        set8(TIMER, t);
        if (t) { branched(0x10652, 0x106E4); goto digits; }
        REG_A[1] = TICKS;
        set_lo(0, (uint16_t)(ram16(DIFFICULTY) << 1));
        set8(TIMER, md.rom[TICKS + (int16_t)lo(REG_D[0])]);
        uint8_t v = (uint8_t)(ram8(TIMER + 3) - 1);
        set8(TIMER + 3, v);
        if (!(v & 0x80)) { branched(0x1066E, 0x106C0); goto hurry; }
        v = (uint8_t)(ram8(TIMER + 2) - 1);
        set8(TIMER + 2, v);
        if (!(v & 0x80)) { branched(0x10676, 0x106BA); goto ones; }
        v = (uint8_t)(ram8(TIMER + 1) - 1);
        set8(TIMER + 1, v);
        if (!(v & 0x80)) { branched(0x1067E, 0x106B4); goto tens; }
        /* the time ran out: "time up", the lives kept */
        set32(TIMER, 0);
        set16(TIME_UP, 0x8030);
        REG_D[0] = 0;
        set_lo(7, 0x50);
        call(0x18736, 0x10696);
        set16(P1_LIVES_KEPT, ram16(P1_LIVES));
        set16(P1_LIVES_KEPT + 2, ram16(P1_LIVES + 2));
        branched(0x106B0, 0x106E4);
        goto digits;
tens:
        set8(TIMER + 2, 5);
ones:
        set8(TIMER + 3, 9);
        ran_to(0x106C0);
hurry:
        REG_A[1] = HURRY;
        set_lo(0, (uint16_t)(ram16(DIFFICULTY) << 2));
        REG_D[0] = rom32(HURRY + (int16_t)lo(REG_D[0]));
        if (REG_D[0] != ram32(REG_A[0])) { branched(0x106D4, 0x106E4); goto digits; }
        REG_D[0] = 0;
        set_lo(7, 0x20);
        call(0x18736, 0x106DE);
        ran_to(0x106E4);
    }
digits:
    hud_digits(0x106E4);
    /* the players' marks on the map */
    sprites_end_byte();
    REG_A[2] = MAP_PLACE;
    REG_A[1] = OBJ_ACTIVE;
    for (int p = 0; p < 2; p++) {
        uint32_t b = 0x70 * p;
        if (!obj16(OBJ_ACTIVE, 2 * p)) { branched(0x10748 + b, 0x107B4 + b); continue; }
        uint16_t x16 = (uint16_t)(obj16(OBJ_X, 2 * p) >> 4);
        set_lo(2, x16);
        uint16_t y10 = tens(obj16(OBJ_Y, 2 * p), 0x1075C + b);
        set_lo(1, y10);
        set16(HUD_P1_Y + 0x16 * p, y10);
        set16(HUD_P1_X + 0x16 * p, x16);
        mark(y10, x16, (uint16_t)(0x8510 + p));
        ran_to(0x107B4 + b);
    }
    /* the guards': every fourth from a different one each 2 frames */
    {
        REG_D[6] = (uint16_t)(ram16(FRAME_COUNT) & 7) >> 1;
        REG_A[1] = MAP_GUARDS;
        REG_D[7] = (uint32_t)(uint16_t)(ram16(STAGE_ENTRY) << 2);
        set_lo(0, rom16(MAP_GUARDS + 2 + (int16_t)lo(REG_D[7])));
        set_lo(7, (uint16_t)(rom16(MAP_GUARDS + (int16_t)lo(REG_D[7])) + lo(REG_D[6])));
        uint32_t off = 0x4C + 2 * REG_D[7];
        uint32_t a1 = OBJ_ACTIVE + off, a4 = OBJ_Y + off, a5 = OBJ_X + off, a6 = 0xFF9E9E + off;
        ran_to(0x1088A);
        for (;;) {
            if (!ram16(a1)) branched(0x1088C, 0x108EC);
            else if (ram16(a6) == 2) branched(0x10894, 0x108EC);
            else {
                uint16_t y10 = tens(ram16(a4), 0x1089C);
                set_lo(1, y10);
                uint16_t x16 = (uint16_t)(ram16(a5) >> 4);
                set_lo(2, x16);
                mark(y10, x16, 0x8567);
                ran_to(0x108EC);
            }
            a1 += 8; a4 += 8; a5 += 8; a6 += 8;
            set_lo(6, (uint16_t)(lo(REG_D[6]) + 4));
            if ((uint16_t)(lo(REG_D[0]) - lo(REG_D[6])) & 0x8000) { ran_to(0x108FA); break; }
            branched(0x108F8, 0x1088A);
        }
        REG_A[1] = a1; REG_A[4] = a4; REG_A[5] = a5; REG_A[6] = a6;
    }
fixed:
    set_lo(0, (uint16_t)(ram16(SPRITE_COUNT) << 3));
    REG_A[0] = SPRITE_TABLE + (uint32_t)(int32_t)(int16_t)lo(REG_D[0]);
    ran_to(0x10910);                                  /* (jsr $10912) */
    jumped_to(0x10912);
    hud_fixed();
    ran_to(0x109DE);                                  /* (its rts) */
    jumped_to(0x10910);
    return done();

escaping:
    /* the time left goes into points: a second a frame, 256 points each */
    set8(TIMER, 0x3C);
    {
        uint8_t v = (uint8_t)(ram8(TIMER + 3) - 1);
        set8(TIMER + 3, v);
        if (!(v & 0x80)) { branched(0x109EC, 0x10A1A); goto points; }
        v = (uint8_t)(ram8(TIMER + 2) - 1);
        set8(TIMER + 2, v);
        if (!(v & 0x80)) { branched(0x109F4, 0x10A14); goto e_ones; }
        v = (uint8_t)(ram8(TIMER + 1) - 1);
        set8(TIMER + 1, v);
        if (!(v & 0x80)) { branched(0x10A00, 0x10A0E); goto e_tens; }
        set32(TIMER, 0);
        branched(0x10A0A, 0x10AA4);
        if (!ram16(STAGE_DONE)) branched(0x10AAA, 0x10AB6);
        else { set16(STAGE_LEFT, 1); ran_to(0x10AB6); }
        branched(0x10AB6, 0x10A4A);
        goto e_digits;
e_tens:
        set8(TIMER + 2, 5);
e_ones:
        set8(TIMER + 3, 9);
        ran_to(0x10A1A);
    }
points:
    if (!obj16(OBJ_ACTIVE, 0)) branched(0x10A20, 0x10A32);
    else { REG_D[0] = 0; REG_D[1] = 0x100; call(0x11D6A, 0x10A2C); }
    if (!obj16(OBJ_ACTIVE, 2)) branched(0x10A38, 0x10A4A);
    else { REG_D[0] = 1; REG_D[1] = 0x100; call(0x11D6A, 0x10A44); }
e_digits:
    hud_digits(0x10A4A);
    sprites_end_byte();
    branched(0x10AA0, 0x10912);
    goto fixed_end;

up_wait:
    {
        uint16_t v = (uint16_t)(ram16(TIME_UP) - 1);
        set16(TIME_UP, v);
        if (v & 0x8000) {
            sprites_end_byte();
            branched(0x10AD8, 0x10912);
            goto fixed_end;
        }
        branched(0x10ABE, 0x10ADC);
        set16(TIME_UP, 0x168);
        ran_to(0x10AE4);
    }
up_count:
    {
        uint16_t v = (uint16_t)(ram16(TIME_UP) - 1);
        set16(TIME_UP, v);
        if (v) { branched(0x10AEA, 0x10C48); goto tail; }
        /* the players out */
        REG_A[0] = OBJ_ACTIVE;
        player_time_out(0);
        REG_A[0] = OBJ_ACTIVE;
        player_time_out(1);
        set16(TIME_UP, 0x7F14);
        ran_to(0x10C48);
        goto tail;
    }

back_in:
    {
        uint8_t c = (uint8_t)(ram8(TIME_UP + 1) - 1);
        set8(TIME_UP + 1, c);
        if (c) { branched(0x10C68, 0x10C48); goto tail; }
        set16(TIME_UP, 0);
        if (ram16(ESCAPE)) { branched(0x10C76, 0x10C48); goto tail; }
        if (ram16(OBJ_ACTIVE) == 1) { branched(0x10C80, 0x10C48); goto tail; }
        if (ram16(OBJ_ACTIVE + 2) == 1) { branched(0x10C8A, 0x10C48); goto tail; }
        /* the players back in (those with lives left start again), the time again */
        for (int p = 0; p < 2; p++) {
            uint32_t b = 0x28 * p;
            uint16_t on = (uint16_t)(ram16(OBJ_ACTIVE + 2 * p) & 0x8000) >> 15;
            set16(OBJ_ACTIVE + 2 * p, on);
            set_lo(0, on);
            if (!ram16(P1_LIVES + 2 * p)) branched(0x10CA8 + b, 0x10CB4 + b);
            else { REG_D[0] = 0; call(p ? 0x08F26 : 0x08E74, 0x10CAE + b); }
        }
        call(0x1A67E, 0x10CDC);
        REG_A[0] = TICKS;
        set_lo(0, (uint16_t)(ram16(DIFFICULTY) << 1));
        set32(TIMER, 0x30000);
        set8(TIMER, md.rom[TICKS + (int16_t)lo(REG_D[0])]);
        branched(0x10D00, 0x10C48);
    }
tail:
    sprites_end_byte();
    branched(0x10C5E, 0x10912);
fixed_end:
    hud_fixed();
    ran_to(0x109DC);
    return done();
}

/* ---------------------------------------------------------------- points */

/* (abcd: the bytes' decimal digits added, with X; X and C as it leaves them) */
static uint8_t bcd_add(uint8_t dst, uint8_t src)
{
    uint res = (uint)(src & 0xF) + (dst & 0xF) + (FLAG_X ? 1 : 0);
    FLAG_V = ~res;
    if (res > 9) res += 6;
    res += (uint)(src & 0xF0) + (dst & 0xF0);
    FLAG_X = FLAG_C = (res > 0x99) << 8;
    if (FLAG_C) res -= 0xA0;
    FLAG_V &= res;
    FLAG_N = NFLAG_8(res);
    res &= 0xFF;
    FLAG_Z |= res;
    return (uint8_t)res;
}

/* (n bytes ending before `dst` and `src`, added in decimal from the last: abcd -(src),-(dst)) */
static void bcd_add_bytes(uint32_t dst, uint32_t src, int n)
{
    for (int k = 1; k <= n; k++) set8(dst - k, bcd_add(ram8(dst - k), ram8(src - k)));
}

/* sub_11D88 / sub_11E18: points d1 (decimal) to player p's score (at most 99999999),
 * shown again; reaching the next extra life's score ($C0FC, 4 digits on from the top):
 * a life more, the next one so many points on ($C0FE), shown */
static int add_points(int p)
{
    uint32_t q = p ? 0xC : 0, b = p ? 0x90 : 0;
    uint32_t score = 0xFFC0F4 + q, add = 0xFFC0F8 + q, next = 0xFFC0FC + q, step = 0xFFC0FE + q;
    begin(0x11D88 + b);
    set32(add, REG_D[1]);
    REG_A[6] = add; REG_A[5] = next;
    FLAG_X = 0;
    bcd_add_bytes(add, next, 4);
    REG_A[6] = score; REG_A[5] = add;
    if (!FLAG_C) branched(0x11DA6 + b, 0x11DB4 + b);
    else { set32(score, 0x99999999); ran_to(0x11DB4 + b); }
    set16(0xFF6C2E + (p ? 0x16 : 0), 1);
    REG_D[0] = (uint32_t)ram16(next) << 16;
    if (REG_D[0] > ram32(score)) { branched(0x11DCC + b, 0x11DF6 + b); return done(); }
    FLAG_X = 0;
    bcd_add_bytes(step, step + 2, 2);
    REG_A[0] = next; REG_A[1] = step;
    uint8_t s = FLAG_C ? 0xFF : 0;
    REG_D[0] = (REG_D[0] & 0xFFFFFF00u) | s;
    set8(next, ram8(next) | s);
    set16(P1_LIVES + 2 * p, (uint16_t)(ram16(P1_LIVES + 2 * p) + 1));
    set16(0xFF6C30 + (p ? 0x16 : 0), 1);
    ran_to(0x11DF6 + b);
    return done();
}
int logic_add_points_p1(void) { if (!start()) return 0; return add_points(0); }
int logic_add_points_p2(void) { if (!start()) return 0; return add_points(1); }

/* sub_11D6A: points d1 to player d0 (sub_11D88 / sub_11E18), the registers kept */
int logic_add_points(void)
{
    if (!start()) return 0;
    begin(0x11D6A);
    static const int regs[9] = {2, 3, 4, 5, 6, 7, 8 + 4, 8 + 5, 8 + 6};
    uint32_t kept[9];
    for (int k = 0; k < 9; k++) {
        kept[k] = regs[k] < 8 ? REG_D[regs[k]] : REG_A[regs[k] - 8];
        stack_put(-36 + 4 * k, kept[k]);              /* (movem.l d2-d7/a4-a6, -(a7)) */
    }
    REG_A[7] -= 36;
    set_lo(0, (uint16_t)(lo(REG_D[0]) << 2));
    REG_A[6] = rom32(0x11D80 + (int16_t)lo(REG_D[0]));
    call(REG_A[6], 0x11D78);
    REG_A[7] += 36;
    for (int k = 0; k < 9; k++) {
        if (regs[k] < 8) REG_D[regs[k]] = kept[k]; else REG_A[regs[k] - 8] = kept[k];
    }
    ran_to(0x11D7E);
    return done();
}

/* sub_11D36 / sub_11D50: a player's points drawn as they are (no points added), and his
 * HUD marked to redraw */
static int points_redraw(int p)
{
    if (!start()) return 0;
    uint32_t b = p ? 0x1A : 0;
    begin(0x11D36 + b);
    REG_D[0] = (uint32_t)p;
    REG_D[1] = 0;
    call(0x11D6A, 0x11D3A + b);
    set16(p ? 0xFF6C44 : 0xFF6C2E, 1);
    set16(p ? 0xFF6C46 : 0xFF6C30, 1);
    ran_to(0x11D4E + b);
    return done();
}
int logic_points_redraw(void)    { return points_redraw(0); }
int logic_points_redraw_p2(void) { return points_redraw(1); }
