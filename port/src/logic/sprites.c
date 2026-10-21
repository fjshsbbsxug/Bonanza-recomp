/* The objects' sprites each frame: which objects are near each player's view, their
 * sprites in each view's lists (on the ground floor or higher up), and the lists merged
 * into the sprite table. */
#include "rt.h"

/* the two views: player 1's and player 2's (their top, left, middle across and down) */
static const struct { uint32_t active, top, left, cy, cx; } VIEW[2] = {
    {OBJ_ACTIVE,     0xFF1850, 0xFF1852, 0xFF1854, 0xFF1856},
    {OBJ_ACTIVE + 2, 0xFF2294, 0xFF2296, 0xFF2298, 0xFF229A},
};

/* sub_058B4: object d0 near either view (its player in play): within 144 pixels plus its
 * half width across of the view's middle, and 40 plus its height of its middle down (by
 * its feet and height): OBJ_ON_SCREEN bit 0 / bit 1. (The port's hook there, for the
 * floor alert option, runs before this in the generated code.) */
int logic_object_near_views(void)
{
    if (!start()) return 0;
    begin(0x058B4);
    uint16_t o = lo(REG_D[0]);
    for (int v = 0; v < 2; v++) {
        uint32_t b = 0x058B4 + 0x88 * v;
        REG_D[5] = 0;
        uint16_t on = ram16(VIEW[v].active);
        set_lo(7, on);
        if (!on) { branched(b + 8, b + 0x78); goto mark; }
        if (on == 0x8000) { branched(b + 0x10, b + 0x78); goto mark; }
        {
            uint16_t dx = (uint16_t)(ram16(VIEW[v].cx) - obj16(OBJ_X, o));
            if ((int16_t)dx >= 0) branched(b + 0x24, b + 0x2A);
            else { dx = (uint16_t)-dx; ran_to(b + 0x2A); }
            set_lo(7, dx);
            uint16_t w = (uint16_t)(obj16(OBJ_SIZE_X, o) + 0x90);
            set_lo(6, w);
            if (dx > w) { branched(b + 0x3A, b + 0x78); goto mark; }
            uint16_t dy = (uint16_t)(ram16(VIEW[v].cy) + 0x10 + obj16(OBJ_HEIGHT, o) - obj16(OBJ_Y, o));
            if ((int16_t)dy >= 0) branched(b + 0x5C, b + 0x62);
            else { dy = (uint16_t)-dy; ran_to(b + 0x62); }
            set_lo(6, dy);
            uint16_t h = (uint16_t)(obj16(OBJ_SIZE_Y, o) + 0x28);
            set_lo(7, h);
            if (dy > h) { branched(b + 0x72, b + 0x78); goto mark; }
            REG_D[5] = (uint32_t)(v + 1);
            ran_to(b + 0x78);
        }
mark:
        REG_A[6] = OBJ_ON_SCREEN;
        set_obj16(OBJ_ON_SCREEN, o, (uint16_t)((obj16(OBJ_ON_SCREEN, o) & (v ? 1 : 2)) | lo(REG_D[5])));
        ran_to(b + 0x88);
    }
    return done();
}

/* (a piece's sprite into the lists of the views the object is near: in front (priority)
 * if the object is below 13 high; on the ground floor's lists or the higher ones'; each
 * list holds 64, and a full first list leaves out the second view's too. at: the
 * cmpi.w #$d of the height test; the pieces left (d2) kept on the stack meanwhile) */
static void piece_to_lists(uint16_t o, uint32_t at, uint16_t y1, uint16_t x1, uint16_t y2, uint16_t x2,
                           uint32_t d3)
{
    uint16_t h = obj16(OBJ_HEIGHT, o);
    REG_A[5] = OBJ_HEIGHT;
    if (h >= 0xD) branched(at + 0x6, at + 0x10);
    else { d3 |= 0x8000; ran_to(at + 0x10); }
    REG_D[3] = d3;
    int high = h != 0;
    if (high) { branched(at + 0x14, at + 0x32); ran_to(at + 0x4A); }
    else branched(at + 0x30, at + 0x4A);
    uint32_t list1 = high ? SPRITES_V1_HIGH : SPRITES_V1_LOW, list2 = high ? SPRITES_V2_HIGH : SPRITES_V2_LOW;
    uint32_t n1 = SPRITES_N + (high ? 2 : 0), n2 = SPRITES_N + (high ? 6 : 4);
    REG_A[1] = n1; REG_A[2] = n2; REG_A[3] = list1; REG_A[4] = list2;
    uint32_t c = at + 0x4A;
    uint32_t d2 = REG_D[2];                           /* (movem.l d2, -(a7)) */
    REG_A[5] = OBJ_ON_SCREEN;
    uint16_t on = obj16(OBJ_ON_SCREEN, o);
    if (on & 1) {
        uint16_t n = ram16(n1);
        if (n == 0x40) { branched(c + 0x1C, c + 0x54); goto out; }
        set16(n1, (uint16_t)(n + 1));
        uint32_t e = list1 + 8u * n;
        set16(e, y1); set32(e + 2, d3); set16(e + 6, x1);
        REG_A[3] = e + 6;
        ran_to(c + 0x2C);
    } else branched(c + 0x12, c + 0x2C);
    if (on & 2) {
        uint16_t n = ram16(n2);
        if (n == 0x40) { branched(c + 0x44, c + 0x54); goto out; }
        set16(n2, (uint16_t)(n + 1));
        uint32_t e = list2 + 8u * n;
        set16(e, y2); set32(e + 2, d3); set16(e + 6, x2);
        REG_A[4] = e + 6;
        ran_to(c + 0x54);
    } else branched(c + 0x3A, c + 0x54);
out:
    REG_D[2] = d2;                                    /* (movem.l (a7)+, d2) */
}

/* sub_059C6: object d0's sprites in the lists of the views it is near: its picture's
 * pieces (table 0x24B80, or a level mod's: a count, then each piece's y, x and sprite attributes), each at
 * the object's place in each view (y raised by its height), its attributes ($3C66) added;
 * mirrored (OBJ_MIRRORED): flipped, x from the right by the piece's width. (The port's
 * hook there, leaving out guards far from the picture, runs before this in the
 * generated code.) */
int logic_object_sprites(void)
{
    if (!start()) return 0;
    begin(0x059C6);
    uint16_t o = lo(REG_D[0]);
    int mirrored = obj16(OBJ_MIRRORED, o) != 0;
    uint32_t d1 = REG_D[1];
    if (mirrored) branched(0x059D0, 0x05B1E);         /* (movem.l d1, -(a7)) */
    REG_A[6] = OBJ_FRAME;
    uint16_t pic = obj16(OBJ_FRAME, o);
    set_lo(7, pic);
    if (!pic) {
        if (mirrored) { branched(0x05B2C, 0x05C92); REG_D[1] = d1; ran_to(0x05C96); }
        else branched(0x059DE, 0x05B1C);
        return done();
    }
    /* (the table's address read where the game keeps it: a level mod may move it) */
    uint32_t table = rom32((mirrored ? 0x05B32 : 0x059E4) + 2);
    uint32_t a6 = mem32(table + (int16_t)(uint16_t)(pic << 2));
    uint16_t n = (uint16_t)(mem16(a6) >> 8);
    a6 += 2;
    set_lo(2, n);
    uint32_t loop = mirrored ? 0x05B40 : 0x059F2;
    ran_to(loop);
    for (;;) {
        int8_t py = (int8_t)RD8(a6), px = (int8_t)RD8(a6 + 1);
        uint16_t y = obj16(OBJ_Y, o), x = obj16(OBJ_X, o), h = obj16(OBJ_HEIGHT, o);
        uint16_t y1 = (uint16_t)(py + y - ram16(VIEW[0].top) + 0x105 - h + 0x14);
        uint16_t y2 = (uint16_t)(py + y - ram16(VIEW[1].top) + 0x85 - h + 0x14);
        uint32_t d3 = mem32(a6 + 2);
        uint16_t x1, x2;
        uint32_t at;
        if (!mirrored) {
            x1 = (uint16_t)(px + x - ram16(VIEW[0].left) + 0x80);
            x2 = (uint16_t)(px + x - ram16(VIEW[1].left) + 0x80);
            d3 = (d3 & 0xFFFF0000u) | (uint16_t)((uint16_t)d3 | obj16(OBJ_ATTR, o));
            at = 0x05A76;
        } else {
            d3 ^= 0x800;
            d3 = (d3 & 0xFFFF0000u) | (uint16_t)((uint16_t)d3 | obj16(OBJ_ATTR, o));
            uint16_t wdt = (uint16_t)(((uint16_t)(d3 >> 16) >> 7 & 0x18) + 8);
            x1 = (uint16_t)(x - (uint16_t)(px + wdt) - ram16(VIEW[0].left) + 0x80);
            x2 = (uint16_t)(x - (uint16_t)(px + wdt) - ram16(VIEW[1].left) + 0x80);
            REG_D[1] = (uint32_t)(uint16_t)d3 << 16 | x2;
            at = 0x05BEC;
        }
        ran_to(at);
        REG_D[7] = y1; REG_D[5] = y2; REG_D[6] = x1;
        if (mirrored) set_lo(4, x2); else REG_D[4] = x2;
        a6 += 6;
        REG_A[6] = a6;
        piece_to_lists(o, at, y1, x1, y2, x2, d3);
        uint32_t dbra = at + 0xA2;
        if (n == 0) { ran_to(dbra + 4); break; }
        n--;
        set_lo(2, n);
        branched(dbra, loop);
    }
    set_lo(2, 0xFFFF);
    if (mirrored) { REG_D[1] = d1; ran_to(0x05C96); }
    return done();
}

/* sub_05C98: the lists into the sprite table after the sprites already there ($17A4):
 * view 1's and view 2's ground-floor sprites by turns, then their higher ones, each
 * linked to the next, up to 64 in all; the last one ends the links (none: an empty one) */
int logic_sprites_merge(void)
{
    if (!start()) return 0;
    begin(0x05C98);
    uint16_t n = ram16(SPRITE_COUNT);
    uint32_t a0 = SPRITE_TABLE + 8u * n;
    ran_to(0x05CC2);
    for (int part = 0; part < 2; part++) {
        uint32_t l1 = part ? SPRITES_V1_HIGH : SPRITES_V1_LOW, l2 = part ? SPRITES_V2_HIGH : SPRITES_V2_LOW;
        uint16_t c1 = ram16(SPRITES_N + (part ? 2 : 0)), c2 = ram16(SPRITES_N + (part ? 6 : 4));
        uint32_t b = part ? 0x05D24 - 0x5CC2 : 0;     /* (the second part's code, 0x62 on) */
        if (part) ran_to(0x05D24);
        for (;;) {
            if (c1) {
                if (n == 0x40) { branched(0x05CCC + b, 0x05D6E); goto end; }
                n++;
                set32(a0, (uint32_t)(n & 0x3F) + ram32(l1));
                set32(a0 + 4, ram32(l1 + 4));
                a0 += 8; l1 += 8; c1--;
                ran_to(0x05CE2 + b);
            } else branched(0x05CC4 + b, 0x05CE2 + b);
            if (c2) {
                branched(0x05CE4 + b, 0x05CF0 + b);
                if (n == 0x40) { branched(0x05CF4 + b, 0x05D6E); goto end; }
                n++;
                set32(a0, (uint32_t)(n & 0x3F) + ram32(l2));
                set32(a0 + 4, ram32(l2 + 4));
                a0 += 8; l2 += 8; c2--;
                ran_to(0x05D0A + b);
                branched(0x05D0A + b, 0x05CC2 + b);
                continue;
            }
            if (!c1) { branched(0x05CEA + b, part ? 0x05D6E : 0x05D0C); break; }
            ran_to(0x05CEE + b);
            branched(0x05CEE + b, 0x05CC2 + b);
        }
    }
end:
    set_lo(0, n);
    set_lo(1, 0); set_lo(2, 0);
    REG_A[0] = a0;
    if (!n) {
        set32(SPRITE_TABLE, 0); set32(SPRITE_TABLE + 4, 0);
        REG_A[0] = SPRITE_TABLE + 4;
        ran_to(0x05D7E);
    } else {
        branched(0x05D70, 0x05D80);
        set16(a0 - 6, ram16(a0 - 6) & 0xFFC0);
        ran_to(0x05D86);
    }
    return done();
}

/* sub_0582E: the frame's sprites: the four lists emptied; the objects being drawn
 * (OBJ_ACTIVE 2) first, then those in play (1): near which views (sub_058B4), and their
 * sprites there (sub_059C6); the lists merged (sub_05C98) */
int logic_sprites_frame(void)
{
    if (!start()) return 0;
    begin(0x0582E);
    for (int k = 0; k < 4; k++) set16(SPRITES_N + 2 * k, 0);
    REG_A[0] = OBJ_ACTIVE;
    for (int pass = 0; pass < 2; pass++) {
        uint32_t b = 0x05852 + 0x30 * pass;
        uint16_t want = pass ? 1 : 2;
        REG_D[0] = 0;
        set_lo(1, 0x55);
        ran_to(b);
        for (;;) {
            uint16_t o = lo(REG_D[0]);
            if (obj16(OBJ_ACTIVE, o) != want) branched(b + 6, b + 0x24);
            else {
                call(0x058B4, b + 0xA);
                o = lo(REG_D[0]);
                REG_A[6] = OBJ_ON_SCREEN;
                if (!obj16(OBJ_ON_SCREEN, o)) branched(b + 0x1A, b + 0x24);
                else { call(0x059C6, b + 0x1E); o = lo(REG_D[0]); ran_to(b + 0x24); }
            }
            set_lo(0, (uint16_t)(o + 2));
            uint16_t c = lo(REG_D[1]);
            set_lo(1, (uint16_t)(c - 1));
            if (c == 0) { ran_to(b + 0x2A); break; }
            ran_to(b + 0x26);
            branched(b + 0x26, b);
        }
    }
    call(0x05C98, 0x058AC);
    ran_to(0x058B2);
    return done();
}

/* sub_12E64: a sprite at the sprite table's end (SPRITE_COUNT): y d2, x d1, attributes
 * d0, d3 by d4 tiles' size (in its size byte), linked to the next */
static void sprite_add(void)
{
    REG_A[6] = SPRITE_TABLE;
    uint16_t n = ram16(SPRITE_COUNT);
    set_lo(5, (uint16_t)(n << 3));
    uint32_t e = SPRITE_TABLE + (uint32_t)(int32_t)(int16_t)lo(REG_D[5]);
    set16(e, lo(REG_D[2]));
    set16(e + 4, lo(REG_D[0]));
    set16(e + 6, lo(REG_D[1]));
    uint16_t size = (uint16_t)(lo(REG_D[3]) << 8);
    uint16_t h = (uint16_t)((uint16_t)(lo(REG_D[4]) << 2) << 8);
    n = (uint16_t)(ram16(SPRITE_COUNT) + 1);
    set16(SPRITE_COUNT, n);
    set_lo(4, n);
    set_lo(3, (uint16_t)(size | h | n));
    set16(e + 2, lo(REG_D[3]));
    flags_of16(lo(REG_D[3]));
    ran_to(0x12E9A);
}
int logic_sprite_add(void)
{
    if (!start()) return 0;
    begin(0x12E64);
    sprite_add();
    return done();
}

/* sub_13CEA: a blinking sprite at 162, 183 (its picture every 8 frames from table
 * 0x13CE2, palette line 1, in front) */
int logic_blink_sprite(void)
{
    if (!start()) return 0;
    begin(0x13CEA);
    set_lo(0, (uint16_t)((ram16(FRAME_COUNT) & 0x18) >> 2));
    REG_A[0] = 0x13CE2;
    set_lo(0, (uint16_t)(rom16(0x13CE2 + lo(REG_D[0])) | 0xA000));
    set_lo(1, 0xA2);
    set_lo(2, 0xB7);
    REG_D[3] = 1;
    REG_D[4] = 1;
    call(0x12E64, 0x13D0E);
    ran_to(0x13D14);
    return done();
}

/* sub_05D88: object d0's picture as sprites straight on the screen (the messages: x
 * 192..319 only), each piece at the object's place, linked on; mirrored: x from the
 * right by the piece's width, flipped */
int logic_object_picture(void)
{
    if (!start()) return 0;
    begin(0x05D88);
    uint16_t o = lo(REG_D[0]);
    REG_A[4] = OBJ_X;
    uint16_t x = obj16(OBJ_X, o);
    if (x < 0xC0) { branched(0x05D94, 0x05E26); return done(); }
    if (x >= 0x140) { branched(0x05D9E, 0x05E26); return done(); }
    REG_A[6] = OBJ_FRAME;
    uint16_t pic = obj16(OBJ_FRAME, o);
    set_lo(7, pic);
    if (!pic) { branched(0x05DAC, 0x05E26); return done(); }
    /* (the table's address read where the game keeps it: a level mod may move it) */
    uint32_t a6 = mem32(rom32(0x05DB2 + 2) + (int16_t)(uint16_t)(pic << 2));
    set_lo(4, (uint16_t)(mem16(a6) >> 8));
    a6 += 2;
    uint32_t a5 = SPRITE_TABLE + (uint32_t)(int32_t)(int16_t)(uint16_t)(ram16(SPRITE_COUNT) << 3);
    REG_A[4] = OBJ_MIRRORED;
    int mirrored = obj16(OBJ_MIRRORED, o) != 0;
    uint32_t loop = mirrored ? 0x05E28 : 0x05DDE, dbra = mirrored ? 0x05E80 : 0x05E22;
    if (mirrored) branched(0x05DDA, 0x05E28); else ran_to(0x05DDE);
    for (;;) {
        set16(SPRITE_COUNT, (uint16_t)(ram16(SPRITE_COUNT) + 1));
        int8_t py = (int8_t)RD8(a6), px = (int8_t)RD8(a6 + 1);
        uint16_t size = mem16(a6 + 2), attr = mem16(a6 + 4);
        set16(a5, (uint16_t)(py + obj16(OBJ_Y, o)));
        uint16_t sx;
        if (!mirrored) sx = (uint16_t)(px + obj16(OBJ_X, o));
        else sx = (uint16_t)(obj16(OBJ_X, o) - (uint16_t)((((size >> 7) & 0x18) + 8) + px));
        set16(a5 + 6, sx);
        set_lo(6, (uint16_t)(ram16(SPRITE_COUNT) & 0x3F));
        set_lo(7, (uint16_t)(size + lo(REG_D[6])));
        set16(a5 + 2, lo(REG_D[7]));
        if (mirrored) { set_lo(7, attr ^ 0x800); set16(a5 + 4, lo(REG_D[7])); }
        else set16(a5 + 4, attr);
        a6 += 6;
        a5 += 8;
        uint16_t c = lo(REG_D[4]);
        set_lo(4, (uint16_t)(c - 1));
        if (!c) { ran_to(dbra + 4); break; }
        branched(dbra, loop);
    }
    REG_A[4] = OBJ_X;
    REG_A[5] = a5;
    REG_A[6] = a6;
    return done();
}
