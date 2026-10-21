/* Objects: taking them away, placing them. */
#include "rt.h"
#include <stdio.h>

/* sub_0AE8A / sub_0AEA6: the object player 1 / 2 left to be taken away goes */
static int take_away(uint32_t slot, uint32_t routine)
{
    if (!start()) return 0;
    uint16_t o = ram16(slot);
    set_lo(7, o);                                     /* (for the callers) */
    begin(routine);
    if (o) {
        set16(slot, 0);
        set_obj16(OBJ_ACTIVE, o, 0);
        REG_A[6] = OBJ_ACTIVE;
        ran_to(routine + 0x1A);
    } else {
        branched(routine + 6, routine + 0x1A);
    }
    return done();
}

int logic_take_away_p1(void) { return take_away(P1_TAKE_AWAY, 0x0AE8A); }
int logic_take_away_p2(void) { return take_away(P2_TAKE_AWAY, 0x0AEA6); }

/* sub_19C00 / sub_19C26: object d0 goes to object $9E02's place, 32 / 64 pixels above */
static int place_above(int up, uint32_t routine)
{
    if (!start()) return 0;
    uint16_t from = ram16(0xFF9E02), o = lo(REG_D[0]);
    set_obj16(OBJ_X, o, obj16(OBJ_X, from));
    uint16_t y = (uint16_t)(obj16(OBJ_Y, from) - up);
    set_obj16(OBJ_Y, o, y);

    set_lo(2, from);                                  /* (for the callers) */
    set_lo(7, y);
    REG_A[6] = OBJ_Y;
    begin(routine);
    ran_to(routine + 0x24);
    return done();
}

int logic_place_above32(void) { return place_above(0x20, 0x19C00); }
int logic_place_above64(void) { return place_above(0x40, 0x19C26); }

/* ================================================================ objects 0x46..0x55 */

/* the time of sub_1016A (the cell) and sub_101F2 / sub_10274 (a wall test) as the game's
 * code calls them, from map.c */
#include "map.h"

/* sub_18736: object o makes an object of kind d7 (times 4): in the first free slot, its
 * kind and maker noted; the kind's start routine (0x18772: as the game's code, or in C)
 * makes it. No free slot: nothing. */
typedef void (*obj_fn)(uint16_t maker, uint16_t o);
typedef void (*obj_frame_fn)(uint16_t o);
static const obj_fn obj_start[23];
static const obj_frame_fn obj_frame[23];

int logic_make_object(void)
{
    if (!start()) return 0;
    uint16_t maker = lo(REG_D[0]), kind = lo(REG_D[7]);
    begin(0x18736);
    ran_to(0x18744);
    uint16_t s = OBJS_FIRST;
    int n = 0xF;
    for (;;) {
        if (!obj16(OBJ_ACTIVE, s)) { branched(0x18748, 0x18754); break; }
        s = (uint16_t)(s + 2);
        if (n == 0) {                                 /* no room */
            ran_to(0x18752);
            set_lo(2, s);
            set_lo(6, 0xFFFF);
            return done();
        }
        n--;
        ran_to(0x1874E);
        branched(0x1874E, 0x18744);
    }
    set_lo(2, s);
    set_lo(6, (uint16_t)n);
    set_obj16(OBJ_KIND, s, kind);
    set_obj16(OBJ_OWNER, s, maker);
    uint32_t start_at = rom32(0x18772 + (int16_t)kind);
    REG_A[6] = start_at;
    ran_to(0x18772);                                  /* (to its jmp) */
    /* the kind's start routine (its rts is this routine's) */
    obj_fn c = kind < 23 * 4 ? obj_start[kind / 4] : 0;
    if (!c || start_at != rom32(0x18772 + (int16_t)kind)) return jump_to(start_at);
    jumped_to(start_at);
    c(maker, s);
    cyc_back(16);                                     /* (its rts: counted by the caller) */
    return done();
}

/* ---------------------------------------------------------------- kinds 0 and 4 */

/* kinds 0 / 4 (sub_18880 / sub_18916): something shown over guard `maker`'s head (frame
 * $166 / $167, 22 / 20 pixels up) for 60 frames; the guard's wait ends with it */
static void over_head_start(uint16_t maker, uint16_t s, uint16_t frame, uint16_t up, uint32_t r)
{
    set_obj16(OBJ_X, s, obj16(OBJ_X, maker));
    set_obj16(OBJ_Y, s, (uint16_t)(obj16(OBJ_Y, maker) - up));
    set_obj16(OBJ_HEIGHT, s, obj16(OBJ_HEIGHT, maker));
    set_obj16(OBJ_FRAME, s, frame);
    set_obj16(OBJ_TIMER, s, 0x3C);
    set_obj16(GUARD_CARRIES, maker, s);
    set_obj16(OBJ_ACTIVE, s, 1);
    ran_to(r + 0x5E);                                 /* (its rts too) */
}
static void kind0_start(uint16_t maker, uint16_t s) { over_head_start(maker, s, 0x166, 0x16, 0x18880); }
static void kind4_start(uint16_t maker, uint16_t s) { over_head_start(maker, s, 0x167, 0x14, 0x18916); }

static void over_head_frame(uint16_t s, uint32_t r)
{
    uint16_t t = (uint16_t)(obj16(OBJ_TIMER, s) - 1);
    set_obj16(OBJ_TIMER, s, t);
    if (t) { branched(r + 0xA, r + 0x36); ran_to(r + 0x38); return; }
    uint16_t g = obj16(OBJ_OWNER, s);
    set_obj16(GUARD_WAIT, g, 0);
    set_obj16(GUARD_CARRIES, g, 0);
    set_obj16(OBJ_ACTIVE, s, 0);
    ran_to(r + 0x38);
}
static void kind0_frame(uint16_t s) { over_head_frame(s, 0x188DE); }
static void kind4_frame(uint16_t s) { over_head_frame(s, 0x18974); }

/* ---------------------------------------------------------------- the players' shots */

/* Where each player's shot code is (the time; player 2's has the same steps, in places
 * further on, and one more: a shot stopped by a shield also tells the guard who shot,
 * if it has seen nobody). */
struct shot_at {
    uint32_t start, limit_operand, shots, frame_word;
    uint32_t r, remove, bne_defl, defl, bne_flick, j_map, j_wall, bne_wall, bne_flick2, rts;
    uint32_t j_hit, beq_nohit, beq_noshield, bne_side, j_sound, shield_hook, shield_bne, shield_go,
             shield_bra, hit, bpl, bpl_to, hit_hook, hit_bra, j_other, beq_other, beq_free, defl_beq;
    uint16_t shot_by;
};
static const struct shot_at SHOT[2] = {
    {0x189AC, 0x189AE, P1_SHOTS, 0x168,
     0x18A6E, 0x18A88, 0x18AA4, 0x18BE8, 0x18AC6, 0x18AE4, 0x18AEE, 0x18AF8, 0x18B08, 0x18BE6,
     0x18B12, 0x18B1A, 0x18B28, 0x18B46, 0x18B52, 0x18B5C, 0x18B66, 0x18B6C,
     0x18B8A, 0x18B8E, 0x18BB4, 0x18BBE, 0x18BC2, 0x18BC8, 0x18BCC, 0x18BD4, 0x18BE2, 0x18BFC, 0},
    {0x18C90, 0x18C92, P2_SHOTS, 0x169,
     0x18D52, 0x18D6C, 0x18D88, 0x18ED4, 0x18DAA, 0x18DC8, 0x18DD2, 0x18DDC, 0x18DEC, 0x18ED2,
     0x18DF6, 0x18DFE, 0x18E0C, 0x18E2A, 0x18E36, 0x18E40, 0x18E4A, 0x18E58,
     0x18E76, 0x18E7A, 0x18EA0, 0x18EAA, 0x18EAE, 0x18EB4, 0x18EB8, 0x18EC0, 0x18ECE, 0x18EE8, 2},
};

/* kinds 8 / $C (sub_189AC / sub_18C90): player 1's / 2's shot, from where he stands, 2
 * pixels a frame his way; not when he has as many in flight as he may (2: the operand
 * is read from the ROM, Insane mode makes it 1) */
static void shot_start(int p, uint16_t maker, uint16_t s)
{
    const struct shot_at *A = &SHOT[p];
    if (ram16(A->shots) == rom16(A->limit_operand)) {
        branched(A->start + 8, A->start + 0xC0);
        ran_to(A->start + 0xC2);
        return;
    }
    set_obj16(OBJ_X, s, obj16(OBJ_X, maker));
    set_obj16(OBJ_Y, s, obj16(OBJ_Y, maker));
    set_obj16(OBJ_HEIGHT, s, obj16(OBJ_HEIGHT, maker));
    set_obj16(OBJ_FRAME, s, (uint16_t)A->frame_word);
    set_obj16(OBJ_FLICKER, s, ram16(SHOT_TOGGLE));
    set16(SHOT_TOGGLE, ram16(SHOT_TOGGLE) ^ 1);
    set_obj16(OBJ_ACTIVE, s, 1);
    set_obj16(OBJ_ON_SCREEN, s, 3);
    set_obj16(OBJ_DEFLECTED, s, 0);
    int left = md.ram[(0x3A62 + (int16_t)maker) & 0xFFFF] == 1;   /* (his facing, a byte) */
    if (left) branched(A->start + 0x8A, A->start + 0x92);
    else ran_to(A->start + 0x92);
    set_obj16(OBJ_DX, s, left ? 0xFFFE : 2);
    set16(A->shots, (uint16_t)(ram16(A->shots) + 1));
    set_obj16(0xFF3706, s, 0);
    set_obj16(0xFF37B2, s, 0);
    set_lo(0, 0xB0);                                  /* (left so for the callers) */
    call(0x02D54, A->start + 0xBA);                   /* its sound */
    ran_to(A->start + 0xC2);
}
static void kind8_start(uint16_t maker, uint16_t s) { shot_start(0, maker, s); }
static void kindC_start(uint16_t maker, uint16_t s) { shot_start(1, maker, s); }

/* the shot's frame (sub_18A6E / sub_18D52): out of view: gone. Flying: 2 pixels on; on
 * every other frame (flicker) it stops at walls and hits guards: a guard with its shield
 * up on the side it comes from stops it (a clink; it flies off along a path; player 2's
 * tells the guard), else the guard is shot (event 4; which way it was shot from, $A3C0);
 * missing guards, it may hit something else (sub_1AD04). */
static void shot_frame(int p, uint16_t s)
{
    const struct shot_at *A = &SHOT[p];
    jumped_to(A->r);
    set_obj16(OBJ_FLICKER, s, obj16(OBJ_FLICKER, s) ^ 1);
    if (!obj16(OBJ_ON_SCREEN, s)) {
        ran_to(A->remove);
        goto remove;
    }
    branched(A->remove - 4, A->remove + 0x12);
    if (obj16(OBJ_DEFLECTED, s)) {                    /* flying off along its path */
        branched(A->bne_defl, A->defl);
        uint32_t path = obj32(GUARD_PATH, s);
        uint16_t dx = mem16(path);
        if (dx == 0x8000) { branched(A->defl_beq, A->remove); goto remove; }
        set_obj16(OBJ_X, s, (uint16_t)(obj16(OBJ_X, s) + dx));
        set_obj16(OBJ_Y, s, (uint16_t)(obj16(OBJ_Y, s) + mem16(path + 2)));
        set_obj32(GUARD_PATH, s, path + 4);
        ran_to(A->defl + 0x3C);
        return;
    }
    uint16_t x = (uint16_t)(obj16(OBJ_X, s) + obj16(OBJ_DX, s));
    if (obj16(OBJ_FLICKER, s)) {
        branched(A->bne_flick, A->bne_wall + 2);
    } else {
        int n;
        uint16_t y = (uint16_t)(obj16(OBJ_Y, s) - 0x20);
        struct cell c = map_cell((uint16_t)(x - 0x20), y, &n);
        int up = obj16(OBJ_HEIGHT, s) >= 8;
        int wall = (up ? c.walls_up : c.walls) & map_lane((uint16_t)(obj16(OBJ_Y, s) - 0x20));
        ran_to(A->j_map + 6);
        map_cell_time(n, 1);
        jumped_to(A->j_map + 6);
        ran_to(A->j_wall + 6);
        wall_test_time(0x101F2, up);
        jumped_to(A->j_wall + 6);
        if (wall) { branched(A->bne_wall, A->remove); goto remove; }
        ran_to(A->bne_wall + 2);
    }
    set_obj16(OBJ_X, s, x);
    if (obj16(OBJ_FLICKER, s)) { branched(A->bne_flick2, A->rts); goto out; }

    /* a guard hit? */
    set16(0xFFA3BA, 0);
    REG_D[0] = s;
    call(0x1A764, A->j_hit);
    if (!lo(REG_D[7])) {
        branched(A->beq_nohit, A->j_other);
        call(0x1AD04, A->j_other);
        if (!lo(REG_D[7])) { branched(A->beq_other, A->rts); goto out; }
        if (!obj16(0xFF9E9E, lo(REG_D[6]))) { branched(A->beq_free, A->remove); goto remove; }
        ran_to(A->rts);
        goto out;
    }
    uint16_t g = lo(REG_D[6]);
    if (obj16(0xFF7690, g)) {                         /* its shield up */
        uint16_t side = rom16(0x18C24 + (int16_t)(uint16_t)(obj16(GUARD_FACING_SHOWN, g) << 3));
        if (side == ram16(0xFFA3AC)) {                /* ... on that side: stopped */
            ran_to(A->j_sound);
            uint32_t d0 = REG_D[0];
            set_lo(0, 0x9B);
            call(0x02D54, A->j_sound);
            REG_D[0] = d0;
            REG_D[0] = s;
            ran_to(A->shield_hook);
            if (game_hooks) {                         /* (Insane mode: sent back at him) */
                int h = game_hook(A->shield_hook);
                if (h == 1) { cyc_back(0); goto out_hook; }
            }
            if (obj16(GUARD_SEEN, g)) {
                branched(A->shield_bne, A->shield_go);
            } else if (p) {                           /* player 2's: the guard is told */
                set_lo(7, 0xC);
                REG_D[6] = (REG_D[6] & 0xFFFF0000u) | g;
                call(0x17170, A->shield_bne + 8);
                jumped_to(A->shield_go);
            } else {
                ran_to(A->shield_go);
            }
            set_obj32(GUARD_PATH, s, ram32(0xFF931E));
            set_obj16(OBJ_DEFLECTED, s, 1);
            branched(A->shield_bra, A->rts);
            goto out;
        }
        branched(A->bne_side, A->hit);
    } else {
        branched(A->beq_noshield, A->hit);
    }
    set_obj16(0xFF9614, g, A->shot_by);
    uint16_t from = 2;
    if ((int16_t)obj16(OBJ_DX, s) < 0) { from = 0; ran_to(A->bpl_to); }
    else branched(A->bpl, A->bpl_to);
    set_obj16(0xFFA3C0, g, from);
    set_lo(7, 4);
    REG_D[0] = s;
    ran_to(A->hit_hook);
    if (game_hooks) {                                 /* (Insane mode: guards taking two shots) */
        int h = game_hook(A->hit_hook);
        if (h == 1) goto out_hook;                    /* the shot flies on */
        if (h == 2) { jumped_to(A->hit_bra); branched(A->hit_bra, A->remove); goto remove; }
    }
    call(0x17170, A->hit_hook);                       /* the guard shot (or what the hook made it) */
    branched(A->hit_bra, A->remove);
remove:
    set_obj16(OBJ_ACTIVE, s, 0);
    set16(A->shots, (uint16_t)(ram16(A->shots) - 1));
    ran_to(A->remove + 0x10);
    jumped_to(A->rts);
out:
    ran_to(A->rts + 2);
    return;
out_hook:
    cyc_back(-16);                                    /* (the hook's rts) */
}
static void kind8_frame(uint16_t s) { shot_frame(0, s); }
static void kindC_frame(uint16_t s) { shot_frame(1, s); }

/* ---------------------------------------------------------------- the kinds, and their frame */

static const obj_fn obj_start[23] = {
    [0x00 / 4] = kind0_start, [0x04 / 4] = kind4_start, [0x08 / 4] = kind8_start, [0x0C / 4] = kindC_start,
};
static const obj_frame_fn obj_frame[23] = {
    [0x00 / 4] = kind0_frame, [0x04 / 4] = kind4_frame, [0x08 / 4] = kind8_frame, [0x0C / 4] = kindC_frame,
};

/* sub_187EA: every object's frame (its kind's frame routine, table 0x18824) */
int logic_objects_frame(void)
{
    if (!start()) return 0;
    begin(0x187EA);
    ran_to(0x187F2);
    uint16_t s = OBJS_FIRST;
    for (int n = 0xF;; n--, s = (uint16_t)(s + 2)) {
        REG_D[0] = (REG_D[0] & 0xFFFF0000u) | s;
        set_lo(1, (uint16_t)n);
        if (!obj16(OBJ_ACTIVE, s)) {
            branched(0x187FC, 0x1881C);
        } else {
            uint16_t kind = obj16(OBJ_KIND, s);
            uint32_t r = rom32(0x18824 + (int16_t)kind);
            obj_frame_fn c = kind < 23 * 4 ? obj_frame[kind / 4] : 0;
            {
                static char names[2][24][24];
                char *nm = names[!c][kind / 4 < 24 ? kind / 4 : 23];
                if (!nm[0]) snprintf(nm, 24, "object kind %02X%s", kind, c ? "" : " (68000)");
                count_named(nm);
            }
            uint32_t d0 = REG_D[0], d1 = REG_D[1];
            REG_A[6] = r;
            if (c && r == rom32(0x18824 + (int16_t)kind)) {
                ran_to(0x18818);
                jumped_to(r);
                c(s);
                jumped_to(0x18818);
            } else {
                call(r, 0x18816);
            }
            REG_D[0] = d0;
            REG_D[1] = d1;
            ran_to(0x1881C);
        }
        if (n == 0) { ran_to(0x18822); break; }
        ran_to(0x1881E);
        branched(0x1881E, 0x187F2);
    }
    return done();
}
