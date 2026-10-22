/* Objects: taking them away, placing them. */
#include "rt.h"
#include "objects.h"
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
static void place_above_do(uint16_t o, int up, uint32_t routine)
{
    uint16_t from = ram16(ESCAPE);
    set_obj16(OBJ_X, o, obj16(OBJ_X, from));
    uint16_t y = (uint16_t)(obj16(OBJ_Y, from) - up);
    set_obj16(OBJ_Y, o, y);

    set_lo(2, from);                                  /* (for the callers) */
    set_lo(7, y);
    REG_A[6] = OBJ_Y;
    ran_to(routine + 0x24);                           /* (up to its rts) */
}
static int place_above(int up, uint32_t routine)
{
    if (!start()) return 0;
    begin(routine);
    place_above_do(lo(REG_D[0]), up, routine);
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

/* ---------------------------------------------------------------- the guards' bullets */

/* kind $10 (sub_18F10): a guard's bullet, from where he stands, 2 pixels a frame his way */
static void kind10_start(uint16_t maker, uint16_t s)
{
    set_obj16(OBJ_X, s, obj16(OBJ_X, maker));
    set_obj16(OBJ_Y, s, obj16(OBJ_Y, maker));
    set_obj16(OBJ_HEIGHT, s, obj16(OBJ_HEIGHT, maker));
    set_obj16(OBJ_FRAME, s, 0x165);
    set_obj16(OBJ_FLICKER, s, ram16(SHOT_TOGGLE));
    set16(SHOT_TOGGLE, ram16(SHOT_TOGGLE) ^ 1);
    set_obj16(OBJ_ACTIVE, s, 1);
    set_obj16(OBJ_ON_SCREEN, s, 3);
    int left = obj16(GUARD_FACING, maker) == 1;
    if (left) branched(0x18F84, 0x18F8C);
    else ran_to(0x18F8C);
    set_obj16(OBJ_DX, s, left ? 0xFFFE : 2);
    set_obj16(0xFF3706, s, 0);
    set_obj16(0xFF37B2, s, 0);
    ran_to(0x18FAC);                                  /* (its rts too) */
}

/* the players it pushes over (sub_1A878): a player it touches, not protected, is shot
 * (state $800B, pushed the bullet's way) unless already lying shot ($8014); the object
 * goes. Returns 1 if so (the time counted up to where it goes), else 0. Used by the
 * bullet and by thrown things, which have the same code (at = its jsr to sub_1A878). */
static int hits_player(uint16_t s, uint32_t at, int p2_protected)
{
    uint16_t found = (uint16_t)players_touching(s, at);
    REG_D[6] = found;
    REG_D[7] = 0;                                     /* (sub_1A95C's answer, the last asked) */
    if (obj16(OBJ_ACTIVE, 2) == 1) REG_D[7] = found >> 1 & 1;
    else if (obj16(OBJ_ACTIVE, 0) == 1) REG_D[7] = found & 1;
    for (int p = 0; p < 2; p++) {
        /* (player 2's test 0x3A bytes on; without its protection test, 0xA shorter) */
        int skip = p && !p2_protected ? 0xA : 0;
        uint32_t b = at + 6 + 0x3A * p - skip, next = at + 0x40 + (0x3A - skip) * p;
        if (!skip && ram16(p ? P2_PROTECT : P1_PROTECT)) { branched(b + 6, next); continue; }
        if (!(found & (1 << p))) { branched(b + 0xE, next); continue; }
        if (ram16(p ? P2_STATE : P1_STATE) == 0x8014) { branched(b + 0x1A, 0); return 1; }
        set16(p ? P2_PUSH : P1_PUSH, obj16(OBJ_DX, s));
        set_lo(7, 0x800B);
        call(p ? 0x9894 : 0x9846, b + 0x30);
        branched(b + 0x36, 0);
        return 1;
    }
    return 0;
}

/* the bullet's frame (sub_18FAC): out of view: gone. Else 2 pixels on; on every other
 * frame (flicker) it stops at walls and hits players, or something else (sub_1AD04). */
static void kind10_frame(uint16_t s)
{
    set_obj16(OBJ_FLICKER, s, obj16(OBJ_FLICKER, s) ^ 1);
    if (!obj16(OBJ_ON_SCREEN, s)) { ran_to(0x18FC6); goto remove; }
    branched(0x18FC2, 0x18FD2);
    uint16_t x = (uint16_t)(obj16(OBJ_X, s) + obj16(OBJ_DX, s));
    if (obj16(OBJ_FLICKER, s)) {
        branched(0x18FF0, 0x19024);
    } else {
        int n;
        struct cell c = map_cell((uint16_t)(x - 0x20), (uint16_t)(obj16(OBJ_Y, s) - 0x20), &n);
        int up = obj16(OBJ_HEIGHT, s) >= 8;
        int wall = (up ? c.walls_up : c.walls) & map_lane((uint16_t)(obj16(OBJ_Y, s) - 0x20));
        ran_to(0x1900E + 6);
        map_cell_time(n, 1);
        jumped_to(0x1900E + 6);
        ran_to(0x19018 + 6);
        wall_test_time(0x101F2, up);
        jumped_to(0x19018 + 6);
        if (wall) { branched(0x19022, 0x18FC6); goto remove; }
        ran_to(0x19024);
    }
    set_obj16(OBJ_X, s, x);
    if (obj16(OBJ_FLICKER, s)) { branched(0x19032, 0x190CA); goto out; }
    REG_D[0] = s;
    if (hits_player(s, 0x19036, 1)) { jumped_to(0x18FC6); goto remove; }
    call(0x1AD04, 0x190B0);
    if (!lo(REG_D[7])) { branched(0x190B8, 0x190CA); goto out; }
    if (!obj16(0xFF9E9E, lo(REG_D[6]))) { branched(0x190C6, 0x18FC6); goto remove; }
    ran_to(0x190CA);
out:
    ran_to(0x190CC);
    return;
remove:
    set_obj16(OBJ_ACTIVE, s, 0);
    ran_to(0x18FD2);
}

/* ---------------------------------------------------------------- animation and paths */

/* (these small routines, as called by the jsr at `jsr`: its time and the routine's) */
void into(uint32_t jsr, uint32_t routine) { ran_to(jsr + 6); jumped_to(routine); }

/* sub_1B088: object o's animation anim starts (its steps from table 0x1CCBC) */
void anim_start(uint16_t o, uint16_t anim)
{
    set_obj16(OBJ_ANIM, o, anim);
    set_obj32(OBJ_ANIM_STEPS, o, rom32(0x1CCBC + (int16_t)(uint16_t)(anim << 2)));
    set_obj16(OBJ_ANIM_AT, o, 0);
    set_obj16(OBJ_ANIM_TIME, o, 1);
    set_lo(7, (uint16_t)(anim << 2));
    ran_to(0x1B0CC);
}

/* sub_1B032: object o's animation on a frame: on the step's last frame, the next step
 * (a negative frame: back to the first) */
void anim_step(uint16_t o)
{
    uint16_t t = (uint16_t)(obj16(OBJ_ANIM_TIME, o) - 1);
    set_obj16(OBJ_ANIM_TIME, o, t);
    REG_A[6] = OBJ_ANIM_TIME;
    if (t) { branched(0x1B03C, 0x1B086); ran_to(0x1B088); return; }
    ran_to(0x1B04A);
    uint16_t at = obj16(OBJ_ANIM_AT, o);
    uint32_t steps = obj32(OBJ_ANIM_STEPS, o);
    set_lo(5, (uint16_t)(o * 2));
    uint16_t f;
    while ((int16_t)(f = mem16(steps + (int16_t)at)) < 0) {
        ran_to(0x1B06A);
        REG_D[7] = 0;
        at = 0;
        branched(0x1B06A, 0x1B04A);
    }
    branched(0x1B064, 0x1B06C);
    set_obj16(OBJ_FRAME, o, f);
    set_obj16(OBJ_ANIM_TIME, o, mem16(steps + (int16_t)at + 2));
    set_lo(7, f);
    set_lo(6, (uint16_t)(at + 4));
    set_obj16(OBJ_ANIM_AT, o, (uint16_t)(at + 4));
    REG_A[5] = OBJ_ANIM_TIME;
    REG_A[6] = OBJ_ANIM_AT;
    ran_to(0x1B088);
}

/* sub_016EC: object o starts moving along the path at `path` */
static void move_start(uint16_t o, uint32_t path)
{
    set_obj32(OBJ_MOVE, o, path);
    set_obj16(OBJ_DX, o, mem16(path));
    set_obj16(OBJ_DY, o, mem16(path + 2));
    set_obj16(OBJ_MOVE_LEFT, o, mem16(path + 4));
    set_obj16(OBJ_MOVE_AT, o, 8);
    REG_A[6] = path + 4;
    ran_to(0x0172E);
}

/* sub_0172E: object o moves a step on its path; at the step's end, the next step's */
static void move_step(uint16_t o)
{
    set_obj16(OBJ_X, o, (uint16_t)(obj16(OBJ_X, o) + obj16(OBJ_DX, o)));
    set_obj16(OBJ_Y, o, (uint16_t)(obj16(OBJ_Y, o) + obj16(OBJ_DY, o)));
    uint16_t left = (uint16_t)(obj16(OBJ_MOVE_LEFT, o) - 1);
    set_obj16(OBJ_MOVE_LEFT, o, left);
    if (left) { branched(0x0176C, 0x017A6); ran_to(0x017AC); return; }
    uint16_t at = obj16(OBJ_MOVE_AT, o);
    uint32_t p = obj32(OBJ_MOVE, o) + (int16_t)at;
    set_obj16(OBJ_DX, o, mem16(p));
    set_obj16(OBJ_DY, o, mem16(p + 2));
    set_obj16(OBJ_MOVE_LEFT, o, mem16(p + 4));
    set_obj16(OBJ_MOVE_AT, o, (uint16_t)(at + 8));
    ran_to(0x017AC);
}

/* ---------------------------------------------------------------- thrown things */

/* sub_191E6: a thrown thing s starts on its path: the one for its way and the bounces
 * it has left (table 0x19212) */
static void thrown_path(uint16_t s, uint32_t jsr)
{
    into(jsr, 0x191E6);
    uint32_t paths = rom32(0x19212 + (int16_t)(uint16_t)(obj16(THROWN_WAY, s) << 2));
    uint32_t path = mem32(paths + (int16_t)(uint16_t)(obj16(THROWN_BOUNCES, s) << 2));
    REG_A[5] = paths;
    set_lo(7, (uint16_t)(obj16(THROWN_BOUNCES, s) << 2));
    into(0x1920A, 0x016EC);
    move_start(s, path);
    jumped_to(0x19210);
    ran_to(0x19212);
    jumped_to(jsr + 6);
}

/* kind $14 (sub_190CC): something a guard throws, from 16 pixels above him, his way;
 * it bounces 4 times */
static void kind14_start(uint16_t maker, uint16_t s)
{
    set_obj16(OBJ_X, s, obj16(OBJ_X, maker));
    set_obj16(OBJ_Y, s, (uint16_t)(obj16(OBJ_Y, maker) - 0x10));
    set_obj16(OBJ_HEIGHT, s, obj16(OBJ_HEIGHT, maker));
    set_obj16(OBJ_FLICKER, s, ram16(SHOT_TOGGLE));
    set16(SHOT_TOGGLE, ram16(SHOT_TOGGLE) ^ 1);
    set_obj16(OBJ_ACTIVE, s, 1);
    set_obj16(OBJ_ON_SCREEN, s, 3);
    into(0x19132, 0x1B088);
    anim_start(s, 0x1B);
    jumped_to(0x19138);
    set_obj16(OBJ_STATE, s, 0);
    set_obj16(THROWN_BOUNCES, s, 4);
    int left = obj16(GUARD_FACING, maker) == 1;
    if (left) branched(0x19160, 0x19166);
    else ran_to(0x19166);
    set_obj16(THROWN_WAY, s, left ? 0 : 2);
    thrown_path(s, 0x19176);
    set_obj16(0xFF3706, s, 4);
    set_obj16(0xFF37B2, s, 4);
    set_obj32(OBJ_POINTS_A, s, 0xFFDCFFDC);
    set_obj32(OBJ_POINTS_B, s, 0xFFE0FFE4);
    set_obj32(OBJ_POINTS_C, s, 0xFFE4FFDC);
    set_obj32(OBJ_POINTS_D, s, 0xFFE0FFE4);
    set_lo(7, (uint16_t)(s * 2));
    ran_to(0x191E6);                                  /* (its rts too) */
}

/* the thrown thing's frame (sub_1946A): out of view: gone. Flying, it knocks over a
 * player it touches (and breaks), or breaks on something else (sub_1AD04); a floor or
 * ceiling turns it (table 0x1968A), a wall the other way round, and at its path's end
 * it goes on the next; each costs a bounce, the last breaks it. Breaking: a crash, and
 * gone when the animation ends. */
static void bounce(uint16_t s, uint16_t way, uint32_t at)
{
    set_obj16(THROWN_WAY, s, way);
    thrown_path(s, at);
}
static void kind14_frame(uint16_t s)
{
    if (!obj16(OBJ_ON_SCREEN, s)) {
        set_obj16(OBJ_ACTIVE, s, 0);
        ran_to(0x19484);
        return;
    }
    branched(0x19474, 0x19484);
    uint16_t state = obj16(OBJ_STATE, s);
    set_lo(7, state);
    if (state) {
        branched(0x1948E, 0x19636);
        if (state == 1) {                             /* breaking */
            ran_to(0x19642 + 6);
            jumped_to(0x1B088);
            anim_start(s, 0x1C);
            jumped_to(0x19648);
            uint32_t d0 = REG_D[0];
            set_lo(0, 0xB3);
            call(0x02D54, 0x19650);
            REG_D[0] = d0;
            set_obj16(OBJ_STATE, s, 2);
            branched(0x19666, 0x19682);
        } else {
            branched(0x1963A, 0x1966A);
            set_lo(7, obj16(OBJ_ANIM_TIME, s));
            if ((int16_t)lo(REG_D[7]) >= 0) branched(0x19674, 0x19682);
            else { set_obj16(OBJ_ACTIVE, s, 0); ran_to(0x19682); }
        }
        goto anim;
    }
    ran_to(0x19492);
    REG_D[0] = s;
    if (hits_player(s, 0x19492, 1)) { jumped_to(0x19576); goto broken; }
    call(0x1AD04, 0x1950C);
    if (lo(REG_D[7])) {
        if (!obj16(0xFF9E9E, lo(REG_D[6]))) { branched(0x19522, 0x195A6); goto wall; }
        ran_to(0x19526);
    } else {
        branched(0x19514, 0x19526);
    }
    /* a floor or ceiling? */
    REG_D[7] = 0;
    set_lo(6, obj16(OBJ_DY, s));
    set_lo(5, (uint16_t)(s * 2));
    set_lo(3, (uint16_t)(ram16(OBJ_POINTS_B + 2 * (int16_t)s + 2) + obj16(OBJ_DY, s)));
    call(0x177B8, 0x19542);
    if (z_clear()) {
        ran_to(0x1954C);
        uint16_t way = rom16(0x1968A + (int16_t)(uint16_t)(obj16(THROWN_WAY, s) * 2));
        set_lo(7, (uint16_t)(obj16(THROWN_WAY, s) * 2));
        REG_A[5] = 0x1968A;
        bounce(s, way, 0x19562);
        goto bounced;
    }
    branched(0x19548, 0x19586);
    /* a wall? */
    REG_D[6] = 0;
    set_lo(7, obj16(OBJ_DX, s));
    set_lo(3, 0xFFE0);
    if ((int16_t)obj16(OBJ_DX, s) < 0) {
        call(0x176B8, 0x1959C);
        if (!z_clear()) { branched(0x195A2, 0x195E8); goto move; }
        ran_to(0x195A6);
    } else {
        branched(0x19598, 0x195E0);
        call(0x17738, 0x195E0);
        if (!z_clear()) { ran_to(0x195E8); goto move; }
        branched(0x195E6, 0x195A6);
    }
wall: {
        int16_t way = (int16_t)(obj16(THROWN_WAY, s) - 1);
        if (way >= 0) branched(0x195B2, 0x195B8);
        else { way = 3; ran_to(0x195B8); }
        set_lo(7, (uint16_t)way);
        bounce(s, (uint16_t)way, 0x195BC);
    }
    {
        uint16_t n = (uint16_t)(obj16(THROWN_BOUNCES, s) - 1);
        set_obj16(THROWN_BOUNCES, s, n);
        if (n) { branched(0x195CC, 0x195E8); goto move; }
        set_obj16(OBJ_STATE, s, 1);
        ran_to(0x195DC);
        branched(0x195DC, 0x195E8);
        goto move;
    }
bounced: {
        uint16_t n = (uint16_t)(obj16(THROWN_BOUNCES, s) - 1);
        set_obj16(THROWN_BOUNCES, s, n);
        if (n) { branched(0x19572, 0x195E8); goto move; }
        ran_to(0x19576);
    }
broken:
    set_obj16(OBJ_STATE, s, 1);
    ran_to(0x19582);
    branched(0x19582, 0x195E8);
move:
    into(0x195E8, 0x0172E);
    move_step(s);
    jumped_to(0x195EE);
    if (obj16(OBJ_DX, s) != 0x8000) { branched(0x195FA, 0x19682); goto anim; }
    {                                                 /* its path's end */
        uint16_t n = (uint16_t)(obj16(THROWN_BOUNCES, s) - 1);
        set_obj16(THROWN_BOUNCES, s, n);
        if (!n) {
            set_obj16(OBJ_STATE, s, 1);
            ran_to(0x19618);
            branched(0x19618, 0x19682);
            goto anim;
        }
        branched(0x19608, 0x1961C);
        bounce(s, (uint16_t)((obj16(THROWN_WAY, s) + 1) & 3), 0x1962C);
        branched(0x19632, 0x19682);
    }
anim:
    into(0x19682, 0x1B032);
    anim_step(s);
    jumped_to(0x19688);
    ran_to(0x1968A);
}

/* ---------------------------------------------------------------- kinds $18 to $38 */

/* kinds $18 / $1C (sub_19692 / sub_1971E): what player 1 / 2 holds up (one at a time:
 * P1_TAKE_AWAY / P2_TAKE_AWAY notes it, to be taken away) */
static void held_start(uint16_t s, uint32_t slot, uint32_t r)
{
    if (ram16(slot)) { branched(r + 6, r + 0x28); ran_to(r + 0x2A); return; }
    set_obj16(OBJ_FRAME, s, 0x38);
    set_obj16(OBJ_ACTIVE, s, 1);
    set16(slot, s);
    ran_to(r + 0x2A);
}
static void kind18_start(uint16_t maker, uint16_t s) { (void)maker; held_start(s, P1_TAKE_AWAY, 0x19692); }
static void kind1C_start(uint16_t maker, uint16_t s) { (void)maker; held_start(s, P2_TAKE_AWAY, 0x1971E); }

/* its frame (sub_196BC / sub_19748): where the player says, frame $38 or $3E (none
 * while the player has no picture) */
static void held_frame(uint16_t s, int p, uint32_t r)
{
    /* (where its branches are: player 2's code reads 2(a6), longer) */
    static const uint8_t at_[2][5] = {{0x46, 0x4E, 0x56, 0x5C, 0x62}, {0x4C, 0x54, 0x5E, 0x64, 0x6A}};
    const uint8_t *b = at_[p];
    uint32_t at = P1_HELD_DX + (p ? 0xAA : 0);
    uint16_t pl = (uint16_t)(2 * p);
    set_obj16(OBJ_X, s, (uint16_t)(ram16(at) + obj16(OBJ_X, pl)));
    set_obj16(OBJ_Y, s, (uint16_t)(ram16(at + 2) + obj16(OBJ_Y, pl)));
    set_obj16(OBJ_HEIGHT, s, obj16(OBJ_HEIGHT, pl));
    set_obj16(OBJ_ACTIVE, s, ram16(at + 4));
    uint16_t f = 0x38;
    if (!ram16(at + 6)) branched(r + b[0], r + b[1]);
    else { f = 0x3E; ran_to(r + b[1]); }
    set_lo(7, f);
    if (!obj16(OBJ_FRAME, pl)) { REG_D[7] = 0; f = 0; ran_to(r + b[3]); }
    else branched(r + b[2], r + b[3]);
    set_obj16(OBJ_FRAME, s, f);
    REG_A[6] = OBJ_FRAME;
    ran_to(r + b[4]);
}
static void kind18_frame(uint16_t s) { held_frame(s, 0, 0x196BC); }
static void kind1C_frame(uint16_t s) { held_frame(s, 1, 0x19748); }

/* the messages: kinds $20, $24, $28 (sub_197B2, sub_19906, sub_19A34): a big picture,
 * its overseas version if the console says so (port $A10001 bit 7); its tiles loaded
 * (sub_01550) */
static void msg_picture(uint16_t s, uint16_t id, uint16_t picture, uint32_t at)
{
    uint8_t overseas = (uint8_t)(RD8(0xA10001) & 0x80);
    REG_D[5] = (REG_D[5] & ~0xFFu) | overseas;
    if (overseas) { id++; picture++; ran_to(at + 0x1E); }
    else branched(at + 0x12, at + 0x1E);
    set_obj16(OBJ_FRAME, s, 0);
    set_obj16(OBJ_PICTURE, s, picture);
    set_lo(6, picture);
    set_lo(7, id);
    REG_A[6] = OBJ_PICTURE;
    call(0x01550, at + 0x32);
}

/* sub_198EA / sub_19A18 / sub_19B8A / sub_19BA6: a message object taken away */
static void msg_remove(uint32_t slot, uint32_t r)
{
    uint16_t o = ram16(slot);
    set_lo(7, o);
    if (!o) { branched(r + 6, r + 0x1A); ran_to(r + 0x1C); return; }
    set_obj16(OBJ_ACTIVE, o, 0);
    set16(slot, 0);
    REG_A[6] = OBJ_ACTIVE;
    ran_to(r + 0x1C);
}
static void msg_remove_at(uint32_t slot, uint32_t r, uint32_t jsr)
{
    into(jsr, r);
    msg_remove(slot, r);
    jumped_to(jsr + 6);
}

/* (the rest of a message's start: its place, 50 frames' delay, how long it shows) */
static void msg_place(uint16_t s, uint16_t y_or_0, uint16_t left)
{
    set_obj16(OBJ_X, s, 0x100);
    if (y_or_0) set_obj16(OBJ_Y, s, y_or_0);
    set_obj16(OBJ_HEIGHT, s, 0);
    set_obj16(OBJ_ACTIVE, s, 1);
    set_obj16(OBJ_DELAY, s, 0x32);
    set_obj16(OBJ_LEFT, s, left);
}

static void kind20_start(uint16_t maker, uint16_t s)
{
    (void)maker;
    static const uint32_t slots[4] = {MSG_24, MSG_28_P1, MSG_28_P2, MSG_20};
    for (int i = 0; i < 4; i++)
        if (ram16(slots[i])) { branched(0x197B8 + 0xA * i, 0x19852); ran_to(0x19854); return; }
    msg_picture(s, 0x4A, 0x16A, 0x197DA);
    msg_place(s, 0, 0xF0);
    set16(MSG_20, s);
    ran_to(0x19854);
}
static void kind24_start(uint16_t maker, uint16_t s)
{
    (void)maker;
    if (ram16(MSG_NONE)) { branched(0x1990C, 0x19998); ran_to(0x1999A); return; }
    if (ram16(MSG_24)) { branched(0x19916, 0x19998); ran_to(0x1999A); return; }
    msg_remove_at(MSG_20, 0x198EA, 0x1991A);
    msg_picture(s, 0x4C, 0x16C, 0x19920);
    msg_place(s, 0, 0xF0);
    set16(MSG_24, s);
    ran_to(0x1999A);
}
static void kind28_start(uint16_t maker, uint16_t s)
{
    if (ram16(MSG_NONE)) { branched(0x19A3A, 0x19ADA); ran_to(0x19ADC); return; }
    msg_remove_at(MSG_20, 0x198EA, 0x19A3E);
    msg_picture(s, 0x4E, 0x16E, 0x19A44);
    set_obj16(OBJ_X, s, 0x100);
    uint32_t slot = MSG_28_P1;
    uint16_t y = 0x120;
    if (!maker) branched(0x19A94, 0x19AA2);
    else { slot = MSG_28_P2; y = 0xA0; ran_to(0x19AA2); }
    REG_A[5] = slot;
    set_lo(7, y);
    set_obj16(OBJ_Y, s, y);
    set16(slot, s);
    msg_place(s, y, maker);
    ran_to(0x19ADC);
}

/* (a message drawn over each player's view still playing: sub_05DB0 draws it) */
static void msg_draw(uint16_t s, uint16_t y, uint32_t jsr)
{
    if (y) set_obj16(OBJ_Y, s, y);
    set_lo(7, obj16(OBJ_PICTURE, s));
    call(0x05DB0, jsr);
}

/* kinds $20 / $24's frame (sub_19854 / sub_1999A): after its delay, drawn on 3 frames
 * of 4 over each view (kind $20: not a player out or caught); kind $24 goes after its
 * time */
static void kind20_frame(uint16_t s)
{
    uint16_t d = (uint16_t)(obj16(OBJ_DELAY, s) - 1);
    set_obj16(OBJ_DELAY, s, d);
    if ((int16_t)d >= 0) { branched(0x1985E, 0x198E8); goto out; }
    set_lo(7, ram16(FRAME_COUNT) & 3);
    if (!lo(REG_D[7])) { branched(0x1986C, 0x198E8); goto out; }
    if (!obj16(OBJ_ACTIVE, 0)) branched(0x19876, 0x198AC);
    else if (ram16(0xFF5682)) branched(0x19880, 0x198AC);
    else if (ram16(P1_STATE) == 0x8017) branched(0x1988C, 0x198AC);
    else { ran_to(0x198A6); msg_draw(s, 0x120, 0x198A6); }
    if (!obj16(OBJ_ACTIVE, 2)) { branched(0x198B2, 0x198E8); goto out; }
    if (ram16(0xFF572C)) { branched(0x198BC, 0x198E8); goto out; }
    if (ram16(P2_STATE) == 0x8017) { branched(0x198C8, 0x198E8); goto out; }
    msg_draw(s, 0xA0, 0x198E2);
out:
    ran_to(0x198EA);
}
static void kind24_frame(uint16_t s)
{
    uint16_t d = (uint16_t)(obj16(OBJ_DELAY, s) - 1);
    set_obj16(OBJ_DELAY, s, d);
    if ((int16_t)d >= 0) { branched(0x199A4, 0x19A16); goto out; }
    set_lo(7, ram16(FRAME_COUNT) & 3);
    if (!lo(REG_D[7])) { branched(0x199B2, 0x19A16); goto out; }
    if (!obj16(OBJ_ACTIVE, 0)) branched(0x199BC, 0x199DC);
    else msg_draw(s, 0x120, 0x199D6);
    if (!obj16(OBJ_ACTIVE, 2)) branched(0x199E2, 0x19A02);
    else msg_draw(s, 0xA0, 0x199FC);
    {
        uint16_t left = (uint16_t)(obj16(OBJ_LEFT, s) - 1);
        set_obj16(OBJ_LEFT, s, left);
        if (left) { branched(0x19A0C, 0x19A16); goto out; }
        msg_remove_at(MSG_24, 0x19A18, 0x19A10);
    }
out:
    ran_to(0x19A18);
}

/* kind $28's frame (sub_19ADC): over its player's view after its delay, while he is
 * still in play with all four of $F000 bits set (and no $1818); else gone (player 2's,
 * once he is out, clears player 1's note: as the game does) */
static void kind28_frame(uint16_t s)
{
    uint16_t d = (uint16_t)(obj16(OBJ_DELAY, s) - 1);
    set_obj16(OBJ_DELAY, s, d);
    if ((int16_t)d >= 0) { branched(0x19AE6, 0x19B88); ran_to(0x19B8A); return; }
    uint16_t p = obj16(OBJ_LEFT, s);
    set_lo(7, p);
    if (!p) {
        if (!obj16(OBJ_ACTIVE, 0)) { branched(0x19AFE, 0x19B30); goto gone1; }
        if (ram16(0xFF1818)) { branched(0x19B08, 0x19B30); goto gone1; }
        set_lo(7, ram16(0xFF56BA) & 0xF000);
        if (lo(REG_D[7]) != 0xF000) { branched(0x19B1A, 0x19B30); goto gone1; }
        msg_draw(s, 0, 0x19B28);
        ran_to(0x19B30);
        return;
    }
    branched(0x19AF4, 0x19B42);
    if (!obj16(OBJ_ACTIVE, 2)) { branched(0x19B48, 0x19B30); goto gone1; }
    if (ram16(0xFF1818)) { branched(0x19B50, 0x19B78); goto gone2; }
    set_lo(7, ram16(0xFF5764) & 0xF000);
    if (lo(REG_D[7]) != 0xF000) { branched(0x19B62, 0x19B78); goto gone2; }
    msg_draw(s, 0, 0x19B70);
    ran_to(0x19B78);
    return;
gone1:
    set_obj16(OBJ_ACTIVE, s, 0);
    set16(MSG_28_P1, 0);
    REG_A[6] = OBJ_ACTIVE;
    ran_to(0x19B42);
    return;
gone2:
    set_obj16(OBJ_ACTIVE, s, 0);
    set16(MSG_28_P2, 0);
    REG_A[6] = OBJ_ACTIVE;
    ran_to(0x19B8A);
}

/* kinds $2C / $38 (sub_19BC2 / sub_19BF2): a picture ($34B / $351), placed by its frame
 * 32 / 64 pixels above object $9E02 (sub_19C00 / sub_19C26) */
static void kind2C_start(uint16_t maker, uint16_t s)
{
    (void)maker;
    set_obj16(OBJ_FRAME, s, 0x34B);
    set_obj16(0xFF37B2, s, 0x10);
    set_obj16(OBJ_HEIGHT, s, 0);
    set_obj16(OBJ_ACTIVE, s, 2);
    REG_A[6] = OBJ_ACTIVE;
    ran_to(0x19BF2);
}
static void kind38_start(uint16_t maker, uint16_t s)
{
    (void)maker;
    set_obj16(OBJ_FRAME, s, 0x351);
    branched(0x19BFE, 0x19BCE);
    set_obj16(0xFF37B2, s, 0x10);
    set_obj16(OBJ_HEIGHT, s, 0);
    set_obj16(OBJ_ACTIVE, s, 2);
    REG_A[6] = OBJ_ACTIVE;
    ran_to(0x19BF2);
}
static void kind2C_frame(uint16_t s) { place_above_do(s, 0x20, 0x19C00); ran_to(0x19C26); }
static void kind38_frame(uint16_t s) { place_above_do(s, 0x40, 0x19C26); ran_to(0x19C4C); }

/* ---------------------------------------------------------------- kinds $3C to $48 */

/* kind $3C (sub_19F30): animation 8 over its maker's head (the maker carries it) */
static void kind3C_start(uint16_t maker, uint16_t s)
{
    into(0x19F3A, 0x1B088);
    anim_start(s, 8);
    jumped_to(0x19F40);
    set_obj16(GUARD_CARRIES, maker, s);
    set_obj16(OBJ_ACTIVE, s, 1);
    ran_to(0x19F5C);
}
/* its frame (sub_19F5C): 6 pixels over its maker's head, wherever he is */
static void kind3C_frame(uint16_t s)
{
    uint16_t o = obj16(OBJ_OWNER, s);
    set_obj16(OBJ_X, s, obj16(OBJ_X, o));
    uint16_t up = (uint16_t)-obj16(0xFF37B2, o);
    uint16_t y = (uint16_t)(obj16(OBJ_Y, o) + up - 6);
    set_obj16(OBJ_Y, s, y);
    set_obj16(OBJ_HEIGHT, s, obj16(OBJ_HEIGHT, o));
    set_lo(7, o);
    set_lo(6, y);
    set_lo(5, up);
    into(0x19F9E, 0x1B032);
    anim_step(s);
    jumped_to(0x19FA4);
    ran_to(0x19FA6);
}

/* kinds $40 / $44 (sub_19FA6 / sub_1A00C): frame $368, 24 / 48 pixels above its maker;
 * it goes when it reaches the maker's y again (its frame, sub_1A072, keeps it that far
 * from the maker as it changes) */
static void rises_start(uint16_t maker, uint16_t s, uint16_t dy, uint32_t r)
{
    set_obj16(OBJ_FRAME, s, 0x368);
    set_obj16(OBJ_X, s, obj16(OBJ_X, maker));
    set_obj16(OBJ_DY, s, dy);
    uint16_t y = (uint16_t)(obj16(OBJ_Y, maker) + dy);
    set_obj16(OBJ_Y, s, y);
    set_obj16(OBJ_END_Y, s, y);
    set_obj16(OBJ_HEIGHT, s, 0x10);
    set_obj16(OBJ_ACTIVE, s, 1);
    set_obj16(0xFF37B2, s, 0x10);
    set_lo(6, dy);
    set_lo(7, y);
    ran_to(r + 0x66);
}
static void kind40_start(uint16_t maker, uint16_t s) { rises_start(maker, s, 0xFFE8, 0x19FA6); }
static void kind44_start(uint16_t maker, uint16_t s) { rises_start(maker, s, 0xFFD0, 0x1A00C); }
static void rises_frame(uint16_t s)
{
    uint16_t o = obj16(OBJ_OWNER, s);
    uint16_t y = (uint16_t)(obj16(OBJ_Y, o) + obj16(OBJ_DY, s));
    set_obj16(OBJ_Y, s, y);
    set_lo(7, o);
    set_lo(6, y);
    if (y != obj16(OBJ_END_Y, s)) { branched(0x1A09E, 0x1A0AC); ran_to(0x1A0AE); return; }
    set_obj16(OBJ_ACTIVE, s, 0);
    ran_to(0x1A0AE);
}

/* kind $48 (sub_1A0AE): a puff where sub_1A108 said (place, animation, path), along
 * its path; gone at the path's end (its frame, sub_1A23A) */
static void kind48_start(uint16_t maker, uint16_t s)
{
    (void)maker;
    set_obj16(OBJ_X, s, ram16(PUFF_X));
    set_obj16(OBJ_Y, s, ram16(PUFF_Y));
    set_obj16(OBJ_HEIGHT, s, ram16(PUFF_HEIGHT));
    into(0x1A0E4, 0x1B088);
    anim_start(s, ram16(PUFF_ANIM));
    jumped_to(0x1A0EA);
    into(0x1A0F0, 0x016EC);
    move_start(s, ram32(PUFF_PATH));
    jumped_to(0x1A0F6);
    set_obj16(OBJ_ACTIVE, s, 2);
    ran_to(0x1A108);
}
static void kind48_frame(uint16_t s)
{
    into(0x1A23A, 0x0172E);
    move_step(s);
    jumped_to(0x1A240);
    if (obj16(OBJ_DX, s) != 0x8000) branched(0x1A24C, 0x1A25A);
    else { set_obj16(OBJ_ACTIVE, s, 0); ran_to(0x1A25A); }
    into(0x1A25A, 0x1B032);
    anim_step(s);
    jumped_to(0x1A260);
    ran_to(0x1A262);
}

/* ---------------------------------------------------------------- the way out */

/* kind $30 (sub_19C4C): the way out at the stage's end (frame $34C), over the player who
 * reached it; its music, its pictures' tiles; each player still in play stops
 * (sub_099D6 / sub_09A1C) */
static void kind30_start(uint16_t maker, uint16_t s)
{
    uint32_t d0 = REG_D[0];
    set_lo(0, 0xE0);
    call(0x02D54, 0x19C54);
    REG_D[0] = d0;
    set16(0xFF180A, 1);
    msg_remove_at(MSG_24, 0x19A18, 0x19C66);
    set_lo(7, 0x50);
    call(0x01550, 0x19C70);
    set_lo(7, 0x51);
    call(0x01550, 0x19C7A);
    set_obj16(OBJ_X, s, obj16(OBJ_X, maker));
    uint16_t up = 0x4A;
    if (!maker) branched(0x19C9E, 0x19CA6);
    else { up = 0x4E; ran_to(0x19CA6); }
    uint16_t y = (uint16_t)(obj16(OBJ_Y, maker) - up);
    set_lo(6, up);
    set_lo(7, y);
    set_obj16(OBJ_Y, s, y);
    set_obj16(OBJ_HEIGHT, s, 0);
    set_obj16(0xFF37B2, s, 0x10);
    set_obj16(OBJ_FRAME, s, 0x34C);
    set_obj16(OBJ_DELAY, s, 0x20);
    set_obj16(ESCAPE_PARTS, s, 0x40);
    set_obj16(OBJ_DY, s, rom16(0x19EAE));
    set_obj16(OBJ_LEFT, s, rom16(0x19EB0));
    REG_A[5] = 0x19EB0;
    set_obj16(ESCAPE_STEP, s, 4);
    set_obj16(ESCAPE_WAIT, s, 0x32);
    set_obj16(OBJ_ACTIVE, s, 2);
    set_obj16(OBJ_STATE, s, 0);
    set16(ESCAPE, s);
    REG_A[6] = OBJ_STATE;
    if (!obj16(OBJ_ACTIVE, 0)) branched(0x19D38, 0x19D42);
    else call(0x099D6, 0x19D3C);
    if (!obj16(OBJ_ACTIVE, 2)) branched(0x19D48, 0x19D52);
    else call(0x09A1C, 0x19D4C);
    set16(0xFF1826, 1);
    ran_to(0x19D5C);
}

/* its frame (sub_19D5C): waits 50 frames, then (tiles $56, a sound) 32 more, then (a
 * sound) moves: its parts come ($38 after 64 frames, $2C and $34 after 32), and it
 * goes along its moves (table 0x19EAE: dy, frames), taking the players up with it; at
 * the moves' end, the stage is done ($1828) */
static void kind30_frame(uint16_t s)
{
    uint16_t state = obj16(OBJ_STATE, s);
    set_lo(7, state);
    REG_A[6] = OBJ_STATE;
    if (state < 2) {
        if (state) { branched(0x19D66, 0x19DA4); ran_to(0x19DAC); }
        REG_A[5] = ESCAPE_WAIT;
        uint16_t w = (uint16_t)(obj16(ESCAPE_WAIT, s) - 1);
        set_obj16(ESCAPE_WAIT, s, w);
        if (w) { branched(state ? 0x19DB6 : 0x19D74, 0x19E90); goto out; }
        uint32_t d0 = REG_D[0];
        if (!state) {
            set_obj16(OBJ_STATE, s, 1);
            set_obj16(ESCAPE_WAIT, s, 0x20);
            set_lo(7, 0x56);
            call(0x01550, 0x19D88);
            set_lo(0, 0x88);
            call(0x02D54, 0x19D96);
            REG_D[0] = d0;
            branched(0x19DA0, 0x19E90);
        } else {
            set_obj16(OBJ_STATE, s, 2);
            set_lo(0, 0xAC);
            call(0x02D54, 0x19DC8);
            REG_D[0] = d0;
            branched(0x19DD2, 0x19E90);
        }
        goto out;
    }
    branched(0x19D66, 0x19DA4);
    branched(0x19DA8, 0x19DD6);
    {
        int16_t t = (int16_t)obj16(ESCAPE_PARTS, s);
        set_lo(7, (uint16_t)t);
        if (t < 0) branched(0x19DE0, 0x19DF8);
        else {
            set_obj16(ESCAPE_PARTS, s, (uint16_t)--t);
            set_lo(7, (uint16_t)t);
            if (t >= 0) branched(0x19DEA, 0x19DF8);
            else { set_lo(7, 0x38); call(0x18736, 0x19DF2); }
        }
    }
    {
        int16_t t = (int16_t)obj16(OBJ_DELAY, s);
        set_lo(7, (uint16_t)t);
        if (t < 0) branched(0x19E02, 0x19E24);
        else {
            set_obj16(OBJ_DELAY, s, (uint16_t)--t);
            set_lo(7, (uint16_t)t);
            if (t >= 0) branched(0x19E0C, 0x19E24);
            else {
                set_lo(7, 0x2C);
                call(0x18736, 0x19E14);
                set_lo(7, 0x34);
                call(0x18736, 0x19E1E);
            }
        }
    }
    {
        uint16_t dy = obj16(OBJ_DY, s);
        REG_A[5] = OBJ_DY;
        if ((int16_t)dy >= 0) branched(0x19E34, 0x19E48);
        else {                                        /* going up: the players too */
            set_obj16(OBJ_Y, 0, (uint16_t)(obj16(OBJ_Y, 0) + dy));
            uint16_t y2 = (uint16_t)(obj16(OBJ_Y, 2) + dy);
            set_obj16(OBJ_Y, 2, y2);
            set_lo(6, y2);
            ran_to(0x19E48);
        }
        set_obj16(OBJ_Y, s, (uint16_t)(obj16(OBJ_Y, s) + dy));
        set_lo(7, (uint16_t)(obj16(OBJ_Y, s)));
    }
    REG_A[6] = OBJ_LEFT;
    {
        uint16_t left = (uint16_t)(obj16(OBJ_LEFT, s) - 1);
        set_obj16(OBJ_LEFT, s, left);
        if (left) { branched(0x19E5A, 0x19E90); goto out; }
    }
    {
        uint16_t at = obj16(ESCAPE_STEP, s);
        set_obj16(ESCAPE_STEP, s, (uint16_t)(at + 4));
        uint32_t a = 0x19EAE + (int16_t)at;
        uint16_t dy = rom16(a);
        set_lo(7, dy);
        REG_A[5] = a + 2;
        if (dy == 0x8000) {                           /* the moves' end */
            branched(0x19E78, 0x19E92);
            uint32_t d0 = REG_D[0];
            set_lo(0, 0xE1);
            call(0x02D54, 0x19E9A);
            REG_D[0] = d0;
            set16(0xFF1828, 1);
            ran_to(0x19EAE);
            return;
        }
        set_obj16(OBJ_DY, s, dy);
        set_obj16(OBJ_LEFT, s, rom16(a + 2));
        REG_A[6] = OBJ_LEFT;
        ran_to(0x19E90);
    }
out:
    ran_to(0x19E92);
}

/* kind $34 (sub_19EC0): a part of the way out (frame $34D, animation $1A), placed 32
 * pixels above it (its frame, sub_19F04) */
static void kind34_start(uint16_t maker, uint16_t s)
{
    (void)maker;
    set_obj16(OBJ_FRAME, s, 0x34D);
    set_obj16(0xFF37B2, s, 0x10);
    set_obj16(OBJ_HEIGHT, s, 0);
    set_obj16(OBJ_ACTIVE, s, 2);
    into(0x19EF8, 0x1B088);
    anim_start(s, 0x1A);
    jumped_to(0x19EFE);
    ran_to(0x19F04);
}
static void kind34_frame(uint16_t s)
{
    place_above_do(s, 0x20, 0x19F04);
    into(0x19F28, 0x1B032);
    anim_step(s);
    jumped_to(0x19F2E);
    ran_to(0x19F30);
}

/* ---------------------------------------------------------------- kinds $4C to $58 */

/* kind $4C (sub_1A262): a guard's bomb: as a bullet, with an animation ($18), and it
 * bursts (animation $19) on what it hits */
static void kind4C_start(uint16_t maker, uint16_t s)
{
    set_obj16(OBJ_X, s, obj16(OBJ_X, maker));
    set_obj16(OBJ_Y, s, obj16(OBJ_Y, maker));
    set_obj16(OBJ_HEIGHT, s, obj16(OBJ_HEIGHT, maker));
    set_obj16(0xFF3706, s, 0);
    set_obj16(0xFF37B2, s, 0);
    into(0x1A2AA, 0x1B088);
    anim_start(s, 0x18);
    jumped_to(0x1A2B0);
    set_obj16(OBJ_FLICKER, s, ram16(SHOT_TOGGLE));
    set16(SHOT_TOGGLE, ram16(SHOT_TOGGLE) ^ 1);
    set_obj16(OBJ_ACTIVE, s, 1);
    set_obj16(OBJ_ON_SCREEN, s, 3);
    set_obj16(OBJ_STATE, s, 0);
    int left = obj16(GUARD_FACING, maker) == 1;
    if (left) branched(0x1A2FC, 0x1A304);
    else ran_to(0x1A304);
    set_lo(7, left ? 0xFFFE : 2);
    set_obj16(OBJ_DX, s, left ? 0xFFFE : 2);
    REG_A[6] = OBJ_DX;
    ran_to(0x1A310);
}
static void kind4C_frame(uint16_t s)
{
    set_obj16(OBJ_FLICKER, s, obj16(OBJ_FLICKER, s) ^ 1);
    if (obj16(OBJ_STATE, s)) {                        /* bursting: gone at its end */
        branched(0x1A326, 0x1A44E);
        set_lo(7, obj16(OBJ_ANIM_TIME, s));
        if ((int16_t)lo(REG_D[7]) < 0) { branched(0x1A458, 0x1A338); goto remove; }
        ran_to(0x1A45C);
        goto anim;
    }
    if (!obj16(OBJ_ON_SCREEN, s)) { ran_to(0x1A338); goto remove; }
    branched(0x1A334, 0x1A344);
    uint16_t x = (uint16_t)(obj16(OBJ_X, s) + obj16(OBJ_DX, s));
    if (obj16(OBJ_FLICKER, s)) {
        branched(0x1A362, 0x1A398);
    } else {
        int n;
        struct cell c = map_cell((uint16_t)(x - 0x20), (uint16_t)(obj16(OBJ_Y, s) - 0x20), &n);
        int up = obj16(OBJ_HEIGHT, s) >= 8;
        int wall = (up ? c.walls_up : c.walls) & map_lane((uint16_t)(obj16(OBJ_Y, s) - 0x20));
        ran_to(0x1A380 + 6);
        map_cell_time(n, 1);
        jumped_to(0x1A380 + 6);
        ran_to(0x1A38A + 6);
        wall_test_time(0x101F2, up);
        jumped_to(0x1A38A + 6);
        if (wall) { branched(0x1A394, 0x1A434); goto burst; }
        ran_to(0x1A398);
    }
    set_obj16(OBJ_X, s, x);
    if (obj16(OBJ_FLICKER, s)) { branched(0x1A3A6, 0x1A45C); goto anim; }
    REG_D[0] = s;
    if (hits_player(s, 0x1A3AA, 0)) { jumped_to(0x1A434); goto burst; }
    call(0x1AD04, 0x1A41A);
    if (!lo(REG_D[7])) { branched(0x1A422, 0x1A45C); goto anim; }
    if (obj16(0xFF9E9E, lo(REG_D[6]))) { branched(0x1A430, 0x1A45C); goto anim; }
    ran_to(0x1A434);
burst:
    into(0x1A438, 0x1B088);
    anim_start(s, 0x19);
    jumped_to(0x1A43E);
    set_obj16(OBJ_STATE, s, 1);
    REG_A[6] = OBJ_STATE;
    branched(0x1A44A, 0x1A45C);
anim:
    into(0x1A45C, 0x1B032);
    anim_step(s);
    jumped_to(0x1A462);
    ran_to(0x1A464);
    return;
remove:
    set_obj16(OBJ_ACTIVE, s, 0);
    REG_A[6] = OBJ_ACTIVE;
    ran_to(0x1A344);
}

/* kind $50 (sub_1A464): "time up": the other messages go; it shows after 50 frames
 * (its frame, sub_1A4F2) */
static void kind50_start(uint16_t maker, uint16_t s)
{
    (void)maker;
    set16(0xFF180A, 1);
    msg_remove_at(MSG_24, 0x19A18, 0x1A46C);
    msg_remove_at(MSG_28_P1, 0x19B8A, 0x1A472);
    msg_remove_at(MSG_28_P2, 0x19BA6, 0x1A478);
    msg_remove_at(MSG_20, 0x198EA, 0x1A47E);
    msg_picture(s, 0x48, 0x1AE, 0x1A484);
    set_obj16(OBJ_X, s, 0x100);
    set_obj16(OBJ_HEIGHT, s, 0);
    set_obj16(OBJ_ACTIVE, s, 1);
    set_obj16(OBJ_DELAY, s, 0x32);
    set16(MSG_NONE, s);
    REG_A[6] = OBJ_DELAY;
    ran_to(0x1A4F2);
}

/* (time up for a player still playing: he stops (OBJ_ACTIVE $8000, $6C6A); one going
 * up or down stairs, or carrying (states $8012, $800D), finishes (player routines);
 * one holding loot ($800A) puts it back where it was) */
static void time_up(int p, uint32_t r)
{
    set_obj16(OBJ_ACTIVE, (uint16_t)(2 * p), 0x8000);
    set16(0xFF6C6A + 2 * p, 1);
    uint16_t st = ram16(p ? P2_STATE : P1_STATE);
    REG_A[6] = p ? P2_STATE : P1_STATE;
    if (st == 0x8012) {
        call(p ? 0x0F3BE : 0x0CFA4, r + 0x1E);
        branched(r + 0x24, r + 0x82);
    } else if (branched(r + 0x1A, r + 0x28), st == 0x800D) {
        call(p ? 0x0F17C : 0x0CD62, r + 0x30);
        branched(r + 0x36, r + 0x82);
    } else if (branched(r + 0x2C, r + 0x3A), st != 0x800A) {
        branched(r + 0x3E, r + 0x82);
    } else {
        uint16_t o = ram16(0xFFA398 + 2 * p);
        set_lo(7, o);
        if (!o) branched(r + 0x48, r + 0x82);
        else {
            set_obj16(OBJ_X, o, obj16(0xFF508E, o));
            set_obj16(OBJ_Y, o, obj16(OBJ_END_Y, o));
            set_obj16(0xFF9E9E, o, 1);
            set16(0xFFA398 + 2 * p, 0);
            ran_to(r + 0x82);
        }
    }
}
static void kind50_frame(uint16_t s)
{
    uint16_t d = (uint16_t)(obj16(OBJ_DELAY, s) - 1);
    set_obj16(OBJ_DELAY, s, d);
    if ((int16_t)d >= 0) { branched(0x1A4FC, 0x1A67C); goto out; }
    if (!obj16(OBJ_ACTIVE, 0)) branched(0x1A506, 0x1A5BE);
    else if (ram16(P1_STATE) == 0x8017) branched(0x1A512, 0x1A5BE);
    else if (ram16(0xFF5682)) branched(0x1A51C, 0x1A5BE);
    else {
        time_up(0, 0x1A520);
        msg_draw(s, 0x130, 0x1A5B8);
    }
    if (!obj16(OBJ_ACTIVE, 2)) { branched(0x1A5C4, 0x1A67C); goto out; }
    if (ram16(0xFF572C)) { branched(0x1A5CE, 0x1A67C); goto out; }
    if (ram16(P2_STATE) == 0x8017) { branched(0x1A5DA, 0x1A67C); goto out; }
    time_up(1, 0x1A5DE);
    msg_draw(s, 0xB0, 0x1A676);
out:
    ran_to(0x1A67E);
}

/* kinds $54 / $58 (sub_1A6D6 / sub_1A71A): pictures $396 / $397 at the screen's
 * middle ($58's frame: sub_05D88) */
static void still_start(uint16_t s, uint16_t x, uint16_t frame, uint32_t slot, uint32_t end)
{
    set_obj16(OBJ_X, s, x);
    set_obj16(OBJ_Y, s, 0xEF);
    set_obj16(OBJ_HEIGHT, s, 0);
    set_obj16(OBJ_FRAME, s, frame);
    set_obj16(OBJ_ACTIVE, s, 0xFFFF);
    set16(slot, s);
    REG_A[6] = OBJ_ACTIVE;
    ran_to(end);
}
static void kind54_start(uint16_t maker, uint16_t s) { (void)maker; still_start(s, 0x104, 0x396, 0xFF9E08, 0x1A718); }
static void kind58_start(uint16_t maker, uint16_t s) { (void)maker; still_start(s, 0x100, 0x397, 0xFF9E0A, 0x1A75C); }
static void kind54_frame(uint16_t s) { (void)s; ran_to(0x1A71A); }
static void kind58_frame(uint16_t s) { (void)s; call(0x05D88, 0x1A75C); ran_to(0x1A764); }

/* ---------------------------------------------------------------- the kinds, and their frame */

/* (every kind: table 0x18772's start routines, 0x18824's frame routines) */
static const obj_fn obj_start[23] = {
    kind0_start, kind4_start, kind8_start, kindC_start,
    kind10_start, kind14_start, kind18_start, kind1C_start,
    kind20_start, kind24_start, kind28_start, kind2C_start,
    kind30_start, kind34_start, kind38_start, kind3C_start,
    kind40_start, kind44_start, kind48_start, kind4C_start,
    kind50_start, kind54_start, kind58_start,
};
static const obj_frame_fn obj_frame[23] = {
    kind0_frame, kind4_frame, kind8_frame, kindC_frame,
    kind10_frame, kind14_frame, kind18_frame, kind1C_frame,
    kind20_frame, kind24_frame, kind28_frame, kind2C_frame,
    kind30_frame, kind34_frame, kind38_frame, kind3C_frame,
    rises_frame, rises_frame, kind48_frame, kind4C_frame,
    kind50_frame, kind54_frame, kind58_frame,
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

/* sub_198EA / sub_19A18: the object kept at $FF181A / $FF9E06 (if any) taken out of play */
static int object_out(uint32_t r, uint32_t kept, uint32_t beq, uint32_t rts)
{
    if (!start()) return 0;
    begin(r);
    set_lo(7, ram16(kept));
    if (!lo(REG_D[7])) { branched(beq, rts); return done(); }
    REG_A[6] = OBJ_ACTIVE;
    set16(OBJ_ACTIVE + (int16_t)lo(REG_D[7]), 0);
    set16(kept, 0);
    ran_to(rts);
    return done();
}
int logic_object_out(void)  { return object_out(0x198EA, 0xFF181A, 0x198F0, 0x19904); }
int logic_object_out2(void) { return object_out(0x19A18, 0xFF9E06, 0x19A1E, 0x19A32); }
