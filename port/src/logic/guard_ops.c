/* The guards' script ops (tables 0x14D5A: an op starting, 0x14E5E: an op going on while
 * its wait lasts). A script is a list of words in ROM: an op number (times 4) and its
 * arguments. Each op returns where the guard's frame goes on: NEXT_OP (the next op
 * now, taking no time), AFTER_SCRIPT (the rest of its frame) or FRAME_OVER.
 *
 * o: the guard (object number * 2); at: where its arguments are (the start of an op),
 * or the game's a0 (an op going on: the work memory's object table, unused). */
#include "rt.h"
#include "guard_ops.h"
#include "guard_anim.h"
#include "guards.h"

/* the script goes on from here next time */
static void keep(uint16_t o, uint32_t at) { set_obj32(GUARD_SCRIPT, o, at); }

/* (each op's time: the instructions from its start up to the branch it ends with) */
static uint32_t to_after(uint32_t bra) { branched(bra, 0x144AE); return AFTER_SCRIPT; }
static uint32_t to_next(uint32_t bra) { branched(bra, 0x147C2); return NEXT_OP; }

static uint16_t wait_less(uint16_t o)
{
    uint16_t w = (uint16_t)(obj16(GUARD_WAIT, o) - 1);
    set_obj16(GUARD_WAIT, o, w);
    return w;
}

/* half a step a frame: odd steps alternate (the frame count's lowest bit) */
static int16_t half_step(int16_t step)
{
    return (int16_t)(int8_t)(uint8_t)((uint16_t)(step + (ram16(FRAME_COUNT) & 1)) >> 1);
}

/* 00: the script stops here this frame (a new place to go on from: at) */
static uint32_t op_00_stop(uint16_t o, uint32_t at)  { keep(o, at); return to_after(0x14F70); }
static uint32_t go_00(uint16_t o, uint32_t at)       { (void)o; (void)at; return to_after(0x14F70); }

/* 04: wait n frames */
static uint32_t go_04(uint16_t o, uint32_t at)       { (void)at; wait_less(o); return to_after(0x14F96); }
static uint32_t op_04_wait(uint16_t o, uint32_t at)
{
    set_obj16(GUARD_WAIT, o, mem16(at));
    keep(o, at + 2);
    ran_to(0x14F8C);
    return go_04(o, at);
}

/* 08: move by dx, dy a frame, for n frames */
static uint32_t go_08(uint16_t o, uint32_t at)
{
    (void)at;
    set_obj16(OBJ_X, o, (uint16_t)(obj16(OBJ_X, o) + obj16(OBJ_DX, o)));
    set_obj16(OBJ_Y, o, (uint16_t)(obj16(OBJ_Y, o) + obj16(OBJ_DY, o)));
    wait_less(o);
    return to_after(0x15000);
}
static uint32_t op_08_move(uint16_t o, uint32_t at)
{
    set_obj16(OBJ_DX, o, mem16(at));
    set_obj16(OBJ_DY, o, mem16(at + 2));
    set_obj16(GUARD_WAIT, o, mem16(at + 4));
    keep(o, at + 6);
    ran_to(0x14FC6);
    return go_08(o, at);
}

/* 0C: walk: dx half-pixels a frame, for n frames */
static uint32_t go_0C(uint16_t o, uint32_t at)
{
    (void)at;
    set_obj16(OBJ_X, o, (uint16_t)(obj16(OBJ_X, o) + half_step((int16_t)obj16(OBJ_DX, o))));
    wait_less(o);
    return to_after(0x15056);
}
static uint32_t op_0C_walk(uint16_t o, uint32_t at)
{
    set_obj16(OBJ_DX, o, mem16(at));
    set_obj16(GUARD_WAIT, o, mem16(at + 2));
    keep(o, at + 4);
    ran_to(0x15026);
    return go_0C(o, at);
}

/* 10: move dy a frame (up / down the view), for n frames */
static uint32_t go_10(uint16_t o, uint32_t at)
{
    (void)at;
    set_obj16(OBJ_Y, o, (uint16_t)(obj16(OBJ_Y, o) + obj16(OBJ_DY, o)));
    wait_less(o);
    return to_after(0x1509E);
}
static uint32_t op_10_move_y(uint16_t o, uint32_t at)
{
    set_obj16(OBJ_DY, o, mem16(at));
    set_obj16(GUARD_WAIT, o, mem16(at + 2));
    keep(o, at + 4);
    ran_to(0x1507C);
    return go_10(o, at);
}

/* 14: rise / sink: dz half-pixels a frame, for n frames */
static uint32_t go_14(uint16_t o, uint32_t at)
{
    (void)at;
    int16_t dz = (int16_t)obj16(0xFF39B6, o);
    uint16_t parity = ram16(FRAME_COUNT) & 1, step;
    if (dz >= 0) {
        step = (uint16_t)((uint16_t)(dz + parity) >> 1);
        branched(0x150E0, 0x150EA);
    } else {
        step = (uint16_t)-(int16_t)((uint16_t)(-dz + parity) >> 1);
        branched(0x150D8, 0x150E2);
    }
    set_obj16(OBJ_HEIGHT, o, (uint16_t)(obj16(OBJ_HEIGHT, o) + step));
    wait_less(o);
    return to_after(0x15102);
}
static uint32_t op_14_rise(uint16_t o, uint32_t at)
{
    set_obj16(0xFF39B6, o, mem16(at));
    set_obj16(GUARD_WAIT, o, mem16(at + 2));
    keep(o, at + 4);
    ran_to(0x150C4);
    return go_14(o, at);
}

/* 18: look for the players now (a player seen: remembered, $6CC4 set) */
static uint32_t look(uint16_t o)
{
    set_obj16(0xFF6CC4, o, 0);
    guard_look(o, 0x1511E);
    uint16_t found = ram16(0xFF6CC0);
    if (!found) return to_next(0x1512A);
    set_obj16(GUARD_SEEN, o, found);
    set_obj16(0xFF6CC4, o, 1);
    return to_next(0x15148);
}
static uint32_t op_18_look(uint16_t o, uint32_t at) { keep(o, at); ran_to(0x15114); return look(o); }
static uint32_t go_18(uint16_t o, uint32_t at)      { (void)at; return look(o); }

/* 1C / 20: fire (an object of kind 0 / 4: the guard's shot), and wait for it */
static uint32_t fire(uint16_t o, uint32_t at, uint16_t kind, uint32_t jsr_at)
{
    REG_D[7] = kind;
    call(0x18736, jsr_at);
    set_obj16(GUARD_WAIT, o, 0xFFFF);
    set_obj16(OBJ_ANIM_TIME, o, 0xFFFF);
    keep(o, at);
    return to_after(jsr_at + 0x2C);
}
static uint32_t op_1C_fire(uint16_t o, uint32_t at) { return fire(o, at, 0, 0x1514E); }
static uint32_t op_20_fire(uint16_t o, uint32_t at) { return fire(o, at, 4, 0x15180); }
static uint32_t go_1C(uint16_t o, uint32_t at)      { (void)o; (void)at; return to_after(0x1517A); }
static uint32_t go_20(uint16_t o, uint32_t at)      { (void)o; (void)at; return to_after(0x151AC); }

/* 24: count down (a word per guard, in the table given): not yet at 0, go to the place
 * given */
static uint32_t op_24_loop(uint16_t o, uint32_t at)
{
    uint32_t counts = mem32(at), to = mem32(at + 4);
    uint16_t n = (uint16_t)(obj16(counts, o) - 1);
    set_obj16(counts, o, n);
    if (n) { ran_to(0x151BE); keep(o, to); }
    else { branched(0x151B8, 0x151BE); keep(o, at + 8); }
    return to_after(0x151CC);
}
static uint32_t go_24(uint16_t o, uint32_t at)      { (void)o; (void)at; return to_after(0x151CC); }

/* 28: send itself event e (unless it ignores it now) */
static uint32_t op_28_event(uint16_t o, uint32_t at)
{
    uint16_t e = mem16(at);
    uint16_t bit = (uint16_t)((e >> 2) - 1);
    uint32_t mask = (REG_D[5] & 0xFFFF0000u) | obj16(GUARD_EVENT_MASK, o);
    if ((mask >> (bit & 31)) & 1) {
        branched(0x151E4, 0x151F2);
    } else {
        set_obj16(GUARD_EVENT, o, e);
        ran_to(0x151F2);
    }
    keep(o, at + 2);
    return to_after(0x15200);
}
static uint32_t go_28(uint16_t o, uint32_t at)      { keep(o, at); return to_after(0x15200); }

/* 30: fire the guard's bullet (an object of kind $10) */
static uint32_t op_30_bullet(uint16_t o, uint32_t at)
{
    REG_D[7] = 0x10;
    call(0x18736, 0x152DA);
    keep(o, at);
    return to_after(0x152EE);
}
static uint32_t go_30(uint16_t o, uint32_t at)      { (void)o; (void)at; return to_after(0x152EE); }

/* 34: nothing this frame */
static uint32_t op_34_pause(uint16_t o, uint32_t at) { keep(o, at); return to_after(0x15300); }
static uint32_t go_34(uint16_t o, uint32_t at)       { (void)o; (void)at; return to_after(0x15300); }

/* 38: raise the alarm: every other guard in play, in the view of the player this one
 * has seen and who has seen nobody yet, is told about him (sub_17A44) - unless this one
 * stands on something (height other than 0 or $10) */
static uint32_t op_38_alarm(uint16_t o, uint32_t at)
{
    keep(o, at);
    uint16_t seen = obj16(GUARD_SEEN, o);
    uint16_t z = obj16(OBJ_HEIGHT, o);
    ran_to(0x15332);
    for (uint16_t g = 0xC, n = 0x1F;; g += 2, n--) {
        if (!obj16(OBJ_ACTIVE, g)) branched(0x15336, 0x15376);
        else if (!(obj16(OBJ_ON_SCREEN, g) & (seen >> 1))) branched(0x15340, 0x15376);
        else if (obj16(GUARD_SEEN, g)) branched(0x15348, 0x15376);
        else if (z && z != 0x10) { ran_to(0x15360); branched(0x15360, 0x15376); }
        else {
            if (!z) branched(0x15356, 0x15364);
            else ran_to(0x15364);
            set_lo(2, g);
            set_lo(3, seen);
            set_lo(7, (uint16_t)(seen >> 1));
            call(0x17A44, 0x1536C);
            ran_to(0x15376);
        }
        if (n == 0) { ran_to(0x1537C); break; }
        branched(0x15378, 0x15332);
    }
    return to_after(0x1537C);
}
static uint32_t go_38(uint16_t o, uint32_t at)      { (void)o; (void)at; return to_after(0x1537C); }

/* 2C / 54 / 68: turn to a facing (2C: the one it wants, $92D0 - 8: none; 54: the one
 * given; 68: the one in a table given, by guard), each facing lasting `frames`, by the
 * turns of table 0 or 1. Facing it already: its animation for it, and the next op now;
 * else it waits while turning (its animation for each facing as it turns). The
 * instructions' places (the time): of 2C and of 54 / 68. */
struct turn_at { uint32_t tst, jsr0, bra0, jsr1, cmp_bne, bsr, bra_next, bra_after; };
static const struct turn_at TURN_2C = {0x1521A, 0x1521E, 0x15224, 0x15226, 0x15230, 0x15234, 0x15246, 0x15264};
static const struct turn_at TURN_54 = {0x15A42, 0x15A46, 0x15A4C, 0x15A4E, 0x15A58, 0x15A5C, 0x15A6E, 0x15A8C};

static uint32_t turn(uint16_t o, uint32_t at, uint16_t want, uint16_t frames, const struct turn_at *t)
{
    int alt = mem16(at) != 0;
    at += 2;
    uint16_t r;
    if (!alt) {
        ran_to(t->jsr0 + 6);
        r = guard_turn_begin(o, want, frames, 0);
        jumped_to(t->jsr0 + 6);
        branched(t->bra0, t->bra0 + 8);
    } else {
        branched(t->tst, t->jsr1);
        ran_to(t->jsr1 + 6);
        r = guard_turn_begin(o, want, frames, 1);
        jumped_to(t->jsr1 + 6);
    }
    set_lo(3, frames);
    if (r == 0xFFFF) {                                /* facing it already */
        REG_D[2] = 0xFFFFFFFFu;
        ran_to(t->bsr + 4);
        guard_face_anim(o);
        jumped_to(t->bsr + 4);
        keep(o, at);
        return to_next(t->bra_next);
    }
    set_lo(2, r);
    branched(t->cmp_bne, t->bra_next + 4);
    set_obj16(GUARD_WAIT, o, 0xFFFF);
    keep(o, at);
    return to_after(t->bra_after);
}

/* ... a frame of turning (its last: the wait over), the animation for its facing */
static uint32_t turning(uint16_t o, uint32_t jsr, uint32_t bne, uint32_t bsr, uint32_t bra)
{
    ran_to(jsr + 6);
    uint16_t r = guard_turn_step(o);
    jumped_to(jsr + 6);
    REG_D[2] = r;
    if (r == 0xFFFF) {
        set_obj16(GUARD_WAIT, o, 0);
        ran_to(bsr + 4);
    } else {
        branched(bne, bsr);
        ran_to(bsr + 4);
    }
    guard_face_anim(o);
    jumped_to(bsr + 4);
    return to_after(bra);
}

static uint32_t op_2C_turn(uint16_t o, uint32_t at)
{
    uint16_t frames = mem16(at);
    at += 2;
    uint16_t want = obj16(GUARD_WANT_FACING, o);
    if (want == 8) {
        set_lo(3, frames);
        set_lo(2, want);
        branched(0x15214, 0x15238);
        keep(o, at);
        return to_next(0x15246);
    }
    return turn(o, at, want, frames, &TURN_2C);
}
static uint32_t go_2C(uint16_t o, uint32_t at) { (void)at; return turning(o, 0x15268, 0x15272, 0x15280, 0x15284); }

static uint32_t op_54_turn(uint16_t o, uint32_t at)
{
    uint16_t frames = mem16(at), want = mem16(at + 2);
    return turn(o, at + 4, want, frames, &TURN_54);
}
static uint32_t go_54(uint16_t o, uint32_t at) { (void)at; return turning(o, 0x15A90, 0x15A9A, 0x15AA8, 0x15AAC); }

static uint32_t op_68_turn(uint16_t o, uint32_t at)
{
    uint16_t frames = mem16(at);
    uint32_t table = mem32(at + 2);
    uint16_t want = obj16(table, o);
    REG_A[6] = table;
    ran_to(0x15DF0);
    branched(0x15DF0, 0x15A40);
    return turn(o, at + 6, want, frames, &TURN_54);
}
static uint32_t go_68(uint16_t o, uint32_t at)
{
    branched(0x15E02, 0x15A90);
    (void)at;
    return turning(o, 0x15A90, 0x15A9A, 0x15AA8, 0x15AAC);
}

/* 3C: jump: a path (dx facing its way, dy, frames per step) from the table at 0x154F6,
 * followed frame by frame; walls stop it going across ($9744: no falling meanwhile) */
static uint32_t op_3C_jump(uint16_t o, uint32_t at)
{
    uint16_t which = mem16(at);
    uint32_t path = rom32(0x154F6 + (int16_t)(uint16_t)(which << 2));
    int16_t dx = (int16_t)mem16(path);
    if (obj16(GUARD_FACING, o) == 1) branched(0x153A0, 0x153A6);
    else { dx = (int16_t)-dx; ran_to(0x153A6); }
    set_obj16(OBJ_DX, o, (uint16_t)dx);
    set_obj16(OBJ_DY, o, mem16(path + 2));
    set_obj16(GUARD_PATH_LEFT, o, mem16(path + 4));
    set_obj32(GUARD_PATH, o, path + 6);
    set_obj16(GUARD_WAIT, o, 0xFFFF);
    keep(o, at + 2);
    return to_after(0x153E6);
}

/* a step along the path: the next one when this one's frames are over (0x8000: the end:
 * the wait over); dx facing its way unless `toward_view` (the dz paths: their own way) */
static uint32_t path_next(uint16_t o, uint32_t bne, uint32_t cmp_beq, uint32_t facing_b, uint32_t end_bra,
                          uint32_t stop, uint32_t step_table, int by_view)
{
    uint16_t left = (uint16_t)(obj16(GUARD_PATH_LEFT, o) - 1);
    set_obj16(GUARD_PATH_LEFT, o, left);
    if (left) { branched(bne, 0x144AE); return AFTER_SCRIPT; }
    uint32_t p = obj32(GUARD_PATH, o);
    uint16_t d = mem16(p);
    if (d == 0x8000) {
        branched(cmp_beq, stop);
        set_obj16(GUARD_WAIT, o, 0);
        return to_after(stop + 0xA);
    }
    uint16_t facing = obj16(GUARD_FACING, o);
    int keep_sign = by_view ? facing != 0 : facing == 1;
    if (keep_sign) branched(facing_b, facing_b + 6);
    else { d = (uint16_t)-(int16_t)d; ran_to(facing_b + 6); }
    set_obj16(step_table, o, d);
    set_obj16(OBJ_DY, o, mem16(p + 2));
    set_obj16(GUARD_PATH_LEFT, o, mem16(p + 4));
    set_obj32(GUARD_PATH, o, p + 6);
    return to_after(end_bra);
}

static uint32_t go_3C(uint16_t o, uint32_t at)
{
    (void)at;
    set_obj16(GUARD_STILL, o, 1);
    uint16_t c = obj16(TOUCH_PLAYER, o);
    int across = 1;
    if (c) {
        ran_to(0x15404);
        if (!obj16(0xFF9E9E, c)) { branched(0x1540E, 0x1546C); across = 0; }
        else ran_to(0x15412);
    } else {
        branched(0x15400, 0x15412);
    }
    if (across) {
        int16_t dx = (int16_t)obj16(OBJ_DX, o);
        REG_D[6] = 0;
        set_lo(7, (uint16_t)dx);
        static const uint32_t L[2] = {0x176B8, 0x177B8}, R[2] = {0x17738, 0x17820};
        const uint32_t *k = dx < 0 ? L : R;
        uint32_t base = dx < 0 ? 0x15422 : 0x15442;
        if (dx >= 0) branched(0x1541E, 0x15442);
        int blocked = 0;
        for (int i = 0; i < 2 && !blocked; i++) {
            set_lo(3, i ? 0xFFE8 : 0xFFD8);
            uint32_t jsr = base + 4 + i * 14;
            ran_to(jsr);
            call(k[i], jsr);
            if (FLAG_Z) { branched(jsr + 6, 0x1546C); blocked = 1; }   /* (a wall: Z clear) */
        }
        if (!blocked) {
            if (dx < 0) { ran_to(0x1543E); branched(0x1543E, 0x1545E); }
            else ran_to(0x1545E);
            set_obj16(OBJ_X, o, (uint16_t)(obj16(OBJ_X, o) + lo(REG_D[7])));
            ran_to(0x1546C);
        }
    }
    set_obj16(OBJ_Y, o, (uint16_t)(obj16(OBJ_Y, o) + obj16(OBJ_DY, o)));
    ran_to(0x1548E);
    return path_next(o, 0x1548E, 0x154AC, 0x154BC, 0x154E4, 0x154E8, OBJ_DX, 0);
}

/* 40: an object of kind $3C (sweat / dust) */
static uint32_t op_40_puff(uint16_t o, uint32_t at)
{
    REG_D[7] = 0x3C;
    call(0x18736, 0x15588);
    keep(o, at);
    return to_after(0x1559C);
}
static uint32_t go_40(uint16_t o, uint32_t at) { (void)o; (void)at; return to_after(0x1559C); }

/* 44 / 64: the way to the player it has seen: 1 left, 3 right (44: when near across, 0
 * up / 2 down the view by height, or 8: level with him) - into $92D0; the next op */
static uint32_t op_44_toward(uint16_t o, uint32_t at)
{
    uint16_t p = (uint16_t)(obj16(GUARD_SEEN, o) - 2);
    int16_t dx = (int16_t)(obj16(OBJ_X, p) - obj16(OBJ_X, o));
    uint16_t way = 3;
    if (dx >= 0) branched(0x155BC, 0x155C4);
    else { dx = (int16_t)-dx; way = 1; ran_to(0x155C4); }
    if ((uint16_t)dx >= 8) {
        branched(0x155C8, 0x15600);
    } else if (obj16(OBJ_HEIGHT, p) == obj16(OBJ_HEIGHT, o)) {
        branched(0x155DA, 0x15600);
    } else {
        int16_t dz = (int16_t)(obj16(OBJ_HEIGHT, p) - obj16(OBJ_HEIGHT, o));
        if (dz == 0) { way = 8; ran_to(0x155F6); branched(0x155F6, 0x15600); }
        else if (dz < 0) { way = 0; branched(0x155FA, 0x15600); }
        else { way = 2; ran_to(0x15600); }
    }
    set_obj16(GUARD_WANT_FACING, o, way);
    set_lo(6, way);
    set_lo(2, p);
    keep(o, at);
    return to_next(0x15618);
}
static uint32_t go_44(uint16_t o, uint32_t at)
{
    set_obj16(GUARD_WANT_FACING, o, lo(REG_D[6]));
    keep(o, at);
    return to_next(0x15618);
}

static uint32_t op_64_toward(uint16_t o, uint32_t at)
{
    uint16_t p = (uint16_t)(obj16(GUARD_SEEN, o) - 2);
    int16_t dx = (int16_t)(obj16(OBJ_X, p) - obj16(OBJ_X, o));
    uint16_t way = 3;
    if (dx >= 0) branched(0x15DC4, 0x15DCC);
    else { way = 1; ran_to(0x15DCC); }
    set_obj16(GUARD_WANT_FACING, o, way);
    set_lo(6, way);
    set_lo(2, p);
    keep(o, at);
    return to_next(0x15DE4);
}
static uint32_t go_64(uint16_t o, uint32_t at)
{
    set_obj16(GUARD_WANT_FACING, o, lo(REG_D[6]));
    keep(o, at);
    return to_next(0x15DE4);
}

/* 4C: the player touching it gets d7 (sub_09846 / sub_09894: shot, caught...) */
static uint32_t op_4C_hit_player(uint16_t o, uint32_t at)
{
    set_lo(7, mem16(at));
    if (obj16(TOUCH_WHO, o)) { branched(0x159D4, 0x159E0); call(0x09894, 0x159E0); }
    else { call(0x09846, 0x159D8); branched(0x159DE, 0x159E6); }
    keep(o, at + 2);
    return to_next(0x159F4);
}
static uint32_t go_4C(uint16_t o, uint32_t at) { keep(o, at); return to_next(0x159F4); }

/* 50: a long into one of the guards' tables (0x15A1C: by number) for this guard */
static uint32_t op_50_set(uint16_t o, uint32_t at)
{
    uint32_t table = rom32(0x15A1C + mem16(at));
    set_obj32(table, o, mem32(at + 2));
    keep(o, at + 6);
    return to_after(0x15A18);
}
static uint32_t go_50(uint16_t o, uint32_t at) { (void)o; (void)at; return to_after(0x15A18); }

/* 58: fall (a path of dy, frames from 0x15BF2) */
static uint32_t op_58_fall(uint16_t o, uint32_t at)
{
    set_obj16(OBJ_DY, o, rom16(0x15BF2));
    set_obj16(GUARD_PATH_LEFT, o, rom16(0x15BF4));
    set_obj32(GUARD_PATH, o, 0x15BF6);
    set_obj16(GUARD_WAIT, o, 0xFFFF);
    keep(o, at);
    return to_after(0x15AF2);
}

/* 60: hop up or down the view: a path (dz: its own way, toward the view when it faces
 * the view; dy; frames) from 0x15D7C */
static uint32_t op_60_hop(uint16_t o, uint32_t at)
{
    uint32_t p = 0x15D7C;
    int16_t dz = (int16_t)rom16(p);
    if (obj16(GUARD_FACING, o)) branched(0x15C86, 0x15C8C);
    else { dz = (int16_t)-dz; ran_to(0x15C8C); }
    set_obj16(0xFF39B6, o, (uint16_t)dz);
    set_obj16(OBJ_DY, o, rom16(p + 2));
    set_obj16(GUARD_PATH_LEFT, o, rom16(p + 4));
    set_obj32(GUARD_PATH, o, p + 6);
    set_obj16(GUARD_WAIT, o, 0xFFFF);
    keep(o, at);
    return to_after(0x15CCC);
}
static uint32_t go_60(uint16_t o, uint32_t at)
{
    (void)at;
    set_obj16(GUARD_STILL, o, 1);
    set_obj16(OBJ_HEIGHT, o, (uint16_t)(obj16(OBJ_HEIGHT, o) + obj16(0xFF39B6, o)));
    set_obj16(OBJ_Y, o, (uint16_t)(obj16(OBJ_Y, o) + obj16(OBJ_DY, o)));
    ran_to(0x15D16);
    return path_next(o, 0x15D16, 0x15D34, 0x15D42, 0x15D6A, 0x15D6E, 0xFF39B6, 1);
}

/* 6C: hold the player it touches (unless he is held already: the next op): he is held
 * by this guard ($9E9E, $9F2A), for 10 frames */
static uint32_t op_6C_hold(uint16_t o, uint32_t at)
{
    keep(o, at);
    uint16_t c = obj16(TOUCH_PLAYER, o);
    if (obj16(0xFF9E9E, c)) return to_next(0x15E28);
    set_obj16(0xFF9E9E, c, 1);
    set_obj16(0xFF9F2A, c, o);
    set_obj16(GUARD_WAIT, o, 10);
    return to_after(0x15E48);
}
static uint32_t go_6C(uint16_t o, uint32_t at) { (void)at; wait_less(o); return to_after(0x15E56); }

/* 70 / 74: up / down the stairs: a path of steps (0x15FEA facing left, 0x16014
 * right: dx, dy (up only), ending with $8000), frame by frame; at its end, more stairs
 * ahead going its way: the path again, at once (up: unless sub_1AD5E says no: then the
 * script goes back to the op that called it, $1E bytes back) */
static uint32_t stairs(uint16_t o, uint32_t at, uint16_t way, uint32_t base)
{
    if (way == 1) keep(o, at);
    set_obj16(GUARD_ON_STAIRS, o, way);
    uint32_t path = 0x15FEA;
    if (obj16(GUARD_FACING, o) == 1) branched(base + 0x2A, base + 0x32);
    else { path = 0x16014; ran_to(base + 0x32); }
    set_obj32(GUARD_PATH, o, path);
    set_obj16(GUARD_PATH_LEFT, o, 0);
    set_obj16(GUARD_WAIT, o, 0xFFFF);
    if (way == 2) keep(o, at);
    return to_after(way == 1 ? 0x15EB0 : 0x1609E);
}
static uint32_t op_70_up(uint16_t o, uint32_t at)   { return stairs(o, at, 1, 0x15E5A); }
static uint32_t op_74_down(uint16_t o, uint32_t at) { return stairs(o, at, 2, 0x1603A); }

/* the stairs ahead (a probe at its front, $3706 + 8 its way): the cell's word */
static uint16_t stairs_ahead(uint16_t o, int16_t dx, int16_t dy, uint32_t jsr)
{
    set_obj16(MAP_CELL_INFO, o, 0);
    set_lo(7, (uint16_t)dx);
    if (!dx) REG_D[7] = 0;
    set_lo(6, (uint16_t)dy);
    if (!dy) REG_D[6] = 0;
    set_lo(3, (uint16_t)(obj16(0xFF37B2, o) - 0x20));
    call(0x17934, jsr);
    return obj16(MAP_CELL_INFO, o);
}

static int16_t front(uint16_t o, uint32_t beq)
{
    int16_t d = (int16_t)(obj16(0xFF3706, o) + 8);
    if (obj16(GUARD_FACING, o) == 3) branched(beq, beq + 6);
    else { d = (int16_t)-d; ran_to(beq + 6); }
    return d;
}

static uint32_t go_70(uint16_t o, uint32_t at)
{
    (void)at;
    for (;;) {
        set_obj16(GUARD_STILL, o, 1);
        uint32_t p = obj32(GUARD_PATH, o);
        uint16_t i = obj16(GUARD_PATH_LEFT, o);
        uint16_t dx = mem16(p + (int16_t)i);
        if (dx != 0x8000) {
            set_obj16(OBJ_X, o, (uint16_t)(obj16(OBJ_X, o) + dx));
            set_obj16(OBJ_Y, o, (uint16_t)(obj16(OBJ_Y, o) + mem16(p + (int16_t)i + 2)));
            set_obj16(GUARD_PATH_LEFT, o, (uint16_t)(i + 4));
            return to_after(0x15F08);
        }
        branched(0x15EE0, 0x15F0C);
        uint16_t info = stairs_ahead(o, front(o, 0x15F2E), 0, 0x15F44);
        if (!(info & 0x80)) { branched(0x15F58, 0x15FD2); break; }
        uint16_t way = rom16(0x1603E + 2 * (info & 0xF));
        if (way != obj16(GUARD_FACING, o)) { branched(0x15F7E, 0x15FD2); break; }
        call(0x1AD5E, 0x15F82);
        if (lo(REG_D[7]) == 0) {                      /* more stairs: the path again */
            branched(0x15F8A, 0x15FC4);
            set_obj16(GUARD_PATH_LEFT, o, 0);
            ran_to(0x15FCE);
            branched(0x15FCE, 0x15EB4);
            continue;
        }
        set_obj16(0xFF9498, o, 1);
        uint32_t sp = obj32(GUARD_SCRIPT_SP, o);
        uint32_t back = mem32(sp);
        set_obj32(GUARD_SCRIPT_SP, o, sp + 4);
        keep(o, back - 0x1E);
        REG_A[0] = back - 0x1E;
        ran_to(0x15FC2);
        branched(0x15FC2, 0x15FD2);
        break;
    }
    set_obj16(GUARD_WAIT, o, 0);
    set_obj16(GUARD_ON_STAIRS, o, 0);
    return to_after(0x15FE6);
}

static uint32_t go_74(uint16_t o, uint32_t at)
{
    (void)at;
    for (;;) {
        uint32_t p = obj32(GUARD_PATH, o);
        uint16_t i = obj16(GUARD_PATH_LEFT, o);
        uint16_t dx = mem16(p + (int16_t)i);
        if (dx != 0x8000) {
            set_obj16(OBJ_X, o, (uint16_t)(obj16(OBJ_X, o) + dx));
            set_obj16(GUARD_PATH_LEFT, o, (uint16_t)(i + 4));
            return to_after(0x160D8);
        }
        branched(0x160C2, 0x160DC);
        front(o, 0x160FE);                            /* (worked out, then not used) */
        uint16_t info = stairs_ahead(o, 0, (int16_t)(obj16(0xFF37B2, o) + 8), 0x1611A);
        if (!(info & 0x80)) { branched(0x1612E, 0x16140); break; }
        set_obj16(GUARD_PATH_LEFT, o, 0);
        ran_to(0x1613C);
        branched(0x1613C, 0x160A2);
    }
    set_obj16(GUARD_WAIT, o, 0);
    set_obj16(GUARD_ON_STAIRS, o, 0);
    return to_after(0x16154);
}

/* 78 / 7C: an object of kind $4C / $14 (a thrown thing) */
static uint32_t make(uint16_t o, uint32_t at, uint16_t kind, uint32_t jsr)
{
    REG_D[7] = kind;
    call(0x18736, jsr);
    keep(o, at);
    return to_after(jsr + 0x14);
}
static uint32_t op_78_make(uint16_t o, uint32_t at) { return make(o, at, 0x4C, 0x1615A); }
static uint32_t op_7C_throw(uint16_t o, uint32_t at) { return make(o, at, 0x14, 0x16174); }

/* the script's control: jumps, calls, returns */
static uint32_t op_80_jump(uint16_t o, uint32_t at) { keep(o, mem32(at)); return to_next(0x1619C); }

/* 84 / 88 / 8C / 90: jump when the "equal" flag is clear / set, the "less" flag clear / set */
static uint32_t jump_if(uint16_t o, uint32_t at, uint32_t flag, int when_set, uint32_t b, uint32_t bra)
{
    uint32_t to = mem32(at);
    int set = obj16(flag, o) != 0;
    if (set == when_set) { ran_to(b + 6); keep(o, to); }
    else { branched(b, b + 6); keep(o, at + 4); }
    return to_next(bra);
}
static uint32_t op_84(uint16_t o, uint32_t at) { return jump_if(o, at, GUARD_FLAG_EQUAL, 0, 0x161AC, 0x161C0); }
static uint32_t op_88(uint16_t o, uint32_t at) { return jump_if(o, at, GUARD_FLAG_EQUAL, 1, 0x161D0, 0x161E4); }
static uint32_t op_8C(uint16_t o, uint32_t at) { return jump_if(o, at, GUARD_FLAG_LESS, 0, 0x161F4, 0x16208); }
static uint32_t op_90(uint16_t o, uint32_t at) { return jump_if(o, at, GUARD_FLAG_LESS, 1, 0x16218, 0x1622C); }

static uint32_t op_94_call(uint16_t o, uint32_t at)
{
    uint32_t sp = obj32(GUARD_SCRIPT_SP, o) - 4;
    set32(sp, at + 4);
    set_obj32(GUARD_SCRIPT_SP, o, sp);
    keep(o, mem32(at));
    return to_next(0x16256);
}
static uint32_t op_98_return(uint16_t o, uint32_t at)
{
    (void)at;
    uint32_t sp = obj32(GUARD_SCRIPT_SP, o);
    keep(o, mem32(sp));
    set_obj32(GUARD_SCRIPT_SP, o, sp + 4);
    return to_next(0x1627C);
}

/* 9C: a sound */
static uint32_t op_9C_sound(uint16_t o, uint32_t at)
{
    uint32_t d0 = REG_D[0];
    set_lo(7, mem16(at));
    set_lo(0, mem16(at));
    call(0x02D54, 0x16288);
    REG_D[0] = d0;
    keep(o, at + 2);
    return to_next(0x162A0);
}

/* the guards' own variables (word tables by guard): copy, set, add, subtract (a borrow
 * sets "less"), compare (equal / less flags) */
static uint32_t op_A0_copy(uint16_t o, uint32_t at)
{
    set_obj16(mem32(at + 4), o, obj16(mem32(at), o));
    keep(o, at + 8);
    return to_next(0x162BC);
}
static uint32_t op_A4_set(uint16_t o, uint32_t at)
{
    set_obj16(mem32(at), o, mem16(at + 4));
    keep(o, at + 6);
    return to_next(0x162D4);
}
static uint32_t op_A8_add(uint16_t o, uint32_t at)
{
    uint32_t a = mem32(at), b = mem32(at + 4);
    set_obj16(b, o, (uint16_t)(obj16(a, o) + obj16(b, o)));
    keep(o, at + 8);
    return to_next(0x162F6);
}
static uint32_t op_AC_sub(uint16_t o, uint32_t at)
{
    uint32_t a = mem32(at), b = mem32(at + 4);
    uint16_t x = obj16(b, o), y = obj16(a, o);
    int borrow = x < y;
    if (borrow) ran_to(0x1630E);
    else branched(0x16308, 0x1630E);
    set_obj16(b, o, (uint16_t)(x - y));
    set_obj16(GUARD_FLAG_LESS, o, (uint16_t)borrow);
    keep(o, at + 8);
    return to_next(0x1632A);
}
static uint32_t op_B0_add(uint16_t o, uint32_t at)
{
    uint32_t t = mem32(at + 2);
    set_obj16(t, o, (uint16_t)(mem16(at) + obj16(t, o)));
    keep(o, at + 6);
    return to_next(0x16348);
}
static uint32_t op_B4_sub(uint16_t o, uint32_t at)
{
    uint16_t n = mem16(at);
    uint32_t t = mem32(at + 2);
    uint16_t x = obj16(t, o);
    int borrow = x < n;
    if (borrow) ran_to(0x1635E);
    else branched(0x16358, 0x1635E);
    set_obj16(t, o, (uint16_t)(x - n));
    set_obj16(GUARD_FLAG_LESS, o, (uint16_t)borrow);
    keep(o, at + 6);
    return to_next(0x1637A);
}

/* B8: start animation n */
static uint32_t op_B8_anim(uint16_t o, uint32_t at)
{
    uint16_t anim = mem16(at);
    set_obj16(OBJ_ANIM, o, anim);
    set_obj32(OBJ_ANIM_STEPS, o, rom32(0x17CDE + (int16_t)(uint16_t)(anim << 2)));
    set_obj16(OBJ_ANIM_AT, o, 0);
    set_obj16(OBJ_ANIM_TIME, o, 1);
    keep(o, at + 2);
    return to_next(0x163C6);
}

/* BC / C0: compare a table's word with another's / with a number: equal, or less */
static uint32_t compared(uint16_t o, int16_t d, uint32_t beq, uint32_t equal_at, uint32_t to)
{
    if (d == 0) {
        branched(beq, equal_at);
        set_obj16(GUARD_FLAG_EQUAL, o, 1);
        ran_to(0x1640C);
    } else if (d > 0) {
        branched(beq + 4, 0x1640C);
    } else {
        ran_to(0x163F2);
        set_obj16(GUARD_FLAG_LESS, o, 1);
        ran_to(0x163FE);
        branched(0x163FE, 0x1640C);
    }
    (void)to;
    return 0;
}
static uint32_t op_BC_compare(uint16_t o, uint32_t at)
{
    set_obj16(GUARD_FLAG_EQUAL, o, 0);
    set_obj16(GUARD_FLAG_LESS, o, 0);
    int16_t d = (int16_t)(obj16(mem32(at + 4), o) - obj16(mem32(at), o));
    compared(o, d, 0x163EA, 0x16400, 0);
    keep(o, at + 8);
    return to_next(0x1641A);
}
static uint32_t op_C0_compare(uint16_t o, uint32_t at)
{
    set_obj16(GUARD_FLAG_EQUAL, o, 0);
    set_obj16(GUARD_FLAG_LESS, o, 0);
    int16_t d = (int16_t)(obj16(mem32(at + 2), o) - mem16(at));
    keep(o, at + 6);
    if (d > 0) { ran_to(0x16440); return to_next(0x1644E); }
    if (d == 0) { branched(0x1643C, 0x16400); set_obj16(GUARD_FLAG_EQUAL, o, 1); }
    else { branched(0x1643E, 0x163F2); set_obj16(GUARD_FLAG_LESS, o, 1); ran_to(0x163FE); branched(0x163FE, 0x1640C); }
    return to_next(0x1641A);
}

static uint32_t op_C4_nothing(uint16_t o, uint32_t at) { keep(o, at); return to_next(0x16460); }

/* (going on: only after a wait these ops never set; as the game's code goes) */
static uint32_t go_keep_next(uint16_t o, uint32_t at, uint32_t bra) { keep(o, at); return to_next(bra); }
static uint32_t go_78(uint16_t o, uint32_t at) { (void)o; (void)at; return to_after(0x1616E); }
static uint32_t go_7C(uint16_t o, uint32_t at) { (void)o; (void)at; return to_after(0x16188); }
static uint32_t go_80(uint16_t o, uint32_t at) { (void)o; (void)at; return to_next(0x1619C); }
static uint32_t go_84(uint16_t o, uint32_t at) { return go_keep_next(o, at, 0x161C0); }
static uint32_t go_88(uint16_t o, uint32_t at) { return go_keep_next(o, at, 0x161E4); }
static uint32_t go_8C(uint16_t o, uint32_t at) { return go_keep_next(o, at, 0x16208); }
static uint32_t go_90(uint16_t o, uint32_t at) { return go_keep_next(o, at, 0x1622C); }
static uint32_t go_94(uint16_t o, uint32_t at) { (void)o; (void)at; return to_next(0x16256); }
static uint32_t go_98(uint16_t o, uint32_t at) { (void)o; (void)at; return to_next(0x1627C); }
static uint32_t go_9C(uint16_t o, uint32_t at) { (void)o; (void)at; return to_next(0x162A0); }
static uint32_t go_A0(uint16_t o, uint32_t at) { (void)o; (void)at; return to_next(0x162BC); }
static uint32_t go_A4(uint16_t o, uint32_t at) { (void)o; (void)at; return to_next(0x162D4); }
static uint32_t go_A8(uint16_t o, uint32_t at) { (void)o; (void)at; return to_next(0x162F6); }
static uint32_t go_AC(uint16_t o, uint32_t at) { (void)o; (void)at; return to_next(0x1632A); }
static uint32_t go_B0(uint16_t o, uint32_t at) { (void)o; (void)at; return to_next(0x16348); }
static uint32_t go_B4(uint16_t o, uint32_t at) { (void)o; (void)at; return to_next(0x1637A); }
static uint32_t go_B8(uint16_t o, uint32_t at) { return go_keep_next(o, at, 0x163C6); }
static uint32_t go_BC(uint16_t o, uint32_t at) { return go_keep_next(o, at, 0x1641A); }
static uint32_t go_C0(uint16_t o, uint32_t at) { (void)o; (void)at; return to_next(0x1644E); }
static uint32_t go_C4(uint16_t o, uint32_t at) { (void)o; (void)at; return to_next(0x16460); }

/* ================================================================ running */

/* the checks around the guard (map.c; their time, with the jsr at `jsr`): Z clear: yes */
static int checked(uint32_t routine, uint32_t jsr) { call(routine, jsr); return FLAG_Z != 0; }
static int wall_near(uint32_t jsr)    { return checked(0x17A8A, jsr); }   /* beside or at it */
static int wall_feet(uint32_t jsr)    { return checked(0x17AFC, jsr); }   /* at its feet */
static int contact_free(uint32_t jsr) { return checked(0x17BD8, jsr); }   /* a player not held, in its way */

/* the facing a player in contact must have to be in its way ($3E6A: by the guard) */
static uint16_t contact_facing(uint16_t o) { return rom16(0x159C0 + 2 * obj16(0xFF3E6A, o)); }

/* Where the two running ops' code is (48: running along, 5C and 100 too; F0: changing
 * lane), for the time: the contact checks (head), the probes ahead and what follows them
 * (body: 48's places, F0's are $1084 on), and the ends. */
struct run_at { uint32_t head, body, stop, step; int f0; };
static const struct run_at RUN_48 = {0x15664, 0x156EC, 0x1596C, 0x1598E, 0};
static const struct run_at RUN_F0 = {0x166F6, 0x16770, 0x169E4, 0x16A06, 1};

/* the run over (the time from its start: r->stop) */
static uint32_t stop_clear(uint16_t o, const struct run_at *r)
{
    set_obj16(GUARD_WAIT, o, 0);
    set_obj16(OBJ_DX, o, 0);
    set_obj16(GUARD_DZ, o, 0);
    return to_after(r->stop + 0x1E);
}

/* ... stopped by something (why: GUARD_STOPPED), the branch to the end at bra */
static uint32_t run_stop(uint16_t o, const struct run_at *r, uint16_t why, uint32_t bra)
{
    set_obj16(GUARD_STOPPED, o, why);
    branched(bra, r->stop);
    return stop_clear(o, r);
}

static uint32_t run_step(uint16_t o, const struct run_at *r, uint32_t bra)
{
    if (bra) branched(bra, r->step);
    wait_less(o);
    return to_after(r->step + 0xA);
}

/* the body (a $1084 on for F0): b(48's place) */
#define B(x) ((x) - 0x156EC + r->body)

/* a wall in front: what stopped it */
static uint32_t run_blocked(uint16_t o, const struct run_at *r)
{
    jumped_to(B(0x15720));
    if (wall_near(B(0x15720)))
        return run_stop(o, r, 7, B(0x15736));
    branched(B(0x15726), B(0x1573A));
    if (wall_feet(B(0x1573A))) {
        branched(B(0x15740), B(0x15936));
        return run_stop(o, r, 4, B(0x15942));
    }
    if (contact_free(B(0x15744))) {
        branched(B(0x1574A), B(0x1591C));
        return run_stop(o, r, 3, B(0x15928));
    }
    return run_stop(o, r, 1, B(0x1575A));
}

/* moving across by dx: walls ahead (at head height, then at its feet), stairs */
static uint32_t run_across(uint16_t o, const struct run_at *r, int16_t dx)
{
    uint16_t facing = obj16(GUARD_FACING, o);
    int16_t f = (int16_t)obj16(0xFF37B2, o);
    jumped_to(B(0x156EC));
    REG_D[6] = 0;
    set_lo(7, (uint16_t)dx);
    set_lo(3, (uint16_t)(-f - 0x20));
    int wall;
    if (facing == 1) { wall = checked(0x176B8, B(0x1570E)); branched(B(0x15714), B(0x1571C)); }
    else { branched(B(0x1570A), B(0x15716)); wall = checked(0x17738, B(0x15716)); }
    if (wall) { ran_to(B(0x15720)); return run_blocked(o, r); }
    branched(B(0x1571C), B(0x1575E));

    /* at its feet */
    set_lo(3, (uint16_t)(f - 0x20));
    REG_D[6] = 0;
    int feet_wall;
    if (facing == 1) { feet_wall = checked(0x177B8, B(0x15784)); ran_to(B(0x1578E)); branched(B(0x1578E), B(0x1579E)); }
    else { branched(B(0x1577C), B(0x15790)); feet_wall = checked(0x17820, B(0x15794)); ran_to(B(0x1579E)); }
    uint16_t cell = lo(REG_D[2]);
    if (feet_wall) ran_to(B(0x157A4));
    else branched(B(0x1579E), B(0x157A4));

    int stairs = 0;
    if (!obj16(OBJ_HEIGHT, o)) {
        branched(B(0x157AE), B(0x158AE));
    } else if (cell & 0x80) {
        branched(B(0x157B8), B(0x15800));
        stairs = 1;
    } else if (feet_wall) {
        branched(B(0x157BE), B(0x158AE));
    } else {
        uint32_t d7 = REG_D[7];                       /* (kept on the stack meanwhile) */
        REG_D[6] = 0;
        REG_D[7] = 0;
        set_lo(3, (uint16_t)(f - 0x20));
        checked(0x17934, B(0x157D8));
        REG_D[7] = d7;
        cell = lo(REG_D[2]);
        if (!(cell & 0x80)) {
            branched(B(0x157E8), B(0x158AE));
        } else {
            call(0x1AD5E, B(0x157F0));
            int no = lo(REG_D[7]) != 0;
            REG_D[7] = d7;
            if (no) { branched(B(0x157FC), B(0x15720)); return run_blocked(o, r); }
            ran_to(B(0x15800));
            stairs = 1;
        }
    }
    if (stairs) {
        set_obj16(0xFF49D6, o, 0);
        uint16_t way = rom16(0x159C0 + 2 * (cell & 0xF));
        int up_stairs = 0;
        if (way != facing) {
            if ((cell & 0xF00) == 0x600) {
                branched(B(0x15832), B(0x158C4));
            } else {
                int16_t ahead = (int16_t)(obj16(0xFF3706, o) + 8);
                if (facing == 3) branched(B(0x15852), B(0x15858));
                else { ahead = (int16_t)-ahead; ran_to(B(0x15858)); }
                uint32_t d7 = REG_D[7];
                set_lo(7, (uint16_t)ahead);
                REG_D[6] = 0;
                set_lo(3, (uint16_t)(f - 0x1E));
                int w = checked(0x17934, B(0x15868));
                REG_D[7] = d7;
                if (w) branched(B(0x15872), B(0x158C4));
                else { set_obj16(0xFF49D6, o, 1); ran_to(B(0x15882)); branched(B(0x15882), B(0x1589E)); up_stairs = 1; }
            }
        } else {
            branched(B(0x15824), B(0x15886));
            if ((cell & 0xF00) == 0x400) branched(B(0x15890), B(0x158C4));
            else if (!checked(0x17B82, B(0x15894))) branched(B(0x1589A), B(0x158C4));
            else { ran_to(B(0x1589E)); up_stairs = 1; }
        }
        if (up_stairs) return run_stop(o, r, 6, B(0x158AA));
    } else {
        if (feet_wall) return run_stop(o, r, 2, B(0x158C0));
        branched(B(0x158B0), B(0x158C4));
    }
    /* the step across (none while falling) */
    if (obj16(GUARD_FALLING, o)) return run_step(o, r, B(0x158CE));
    set_obj16(OBJ_X, o, (uint16_t)(obj16(OBJ_X, o) + dx));
    return run_step(o, r, B(0x158E0));
}

/* moving up or down (dz; 48 stops on the floor, at 0 or $10) */
static uint32_t run_dz(uint16_t o, const struct run_at *r)
{
    jumped_to(B(0x158E4));
    int16_t dz = (int16_t)obj16(GUARD_DZ, o);
    if (!dz) return run_step(o, r, B(0x158EE));
    uint16_t z = obj16(OBJ_HEIGHT, o);
    if (z == 0 || z == 0x10) {
        if (z == 0) branched(B(0x158FC), B(0x15908));
        else ran_to(B(0x15908));
        if (contact_free(B(0x15908))) {
            branched(B(0x1590E), B(0x1591C));
            return run_stop(o, r, 3, B(0x15928));
        }
        if (wall_near(B(0x15912))) {
            ran_to(B(0x1591C));
            return run_stop(o, r, 3, B(0x15928));
        }
        branched(B(0x15918), B(0x1592C));
        if (wall_feet(B(0x1592C))) {
            ran_to(B(0x15936));
            return run_stop(o, r, 4, B(0x15942));
        }
        branched(B(0x15932), B(0x15946));
    } else {
        ran_to(B(0x15904));
        branched(B(0x15904), B(0x15946));
    }
    uint16_t nz = (uint16_t)(z + dz);
    set_obj16(OBJ_HEIGHT, o, nz);
    if (r->f0) { ran_to(0x169E2); return run_step(o, r, 0x169E2); }
    if (nz == 0) { branched(0x15960, 0x1596C); return stop_clear(o, r); }
    if (nz != 0x10) { branched(0x15968, 0x144AE); return AFTER_SCRIPT; }
    ran_to(0x1596C);
    return stop_clear(o, r);
}

/* the contact checks first: a player held and in contact above (height $10): blocked;
 * a player not held, facing its way: stop (5) */
static int run_contact(uint16_t o, const struct run_at *r, uint32_t *out, uint32_t head, uint32_t blocked_to)
{
    uint32_t h = head;
    jumped_to(h);
    uint16_t c = obj16(TOUCH_PLAYER, o);
    if (!c) { branched(h + 0xA, h + 0x5C); return 0; }
    if (obj16(0xFF9E9E, c)) {
        call(0x17C1A, h + 0x1C);
        if (z_clear()) { branched(h + 0x22, blocked_to); *out = run_blocked(o, r); return 1; }
        branched(h + 0x26, h + 0x5C);
        return 0;
    }
    branched(h + 0x18, h + 0x2A);
    if (contact_facing(o) == obj16(GUARD_FACING, o)) {
        *out = run_stop(o, r, 5, h + 0x58);
        return 1;
    }
    branched(h + 0x48, h + 0x5C);
    return 0;
}

/* a frame of running (48 / 5C going on) */
static uint32_t go_48(uint16_t o, uint32_t at)
{
    (void)at;
    const struct run_at *r = &RUN_48;
    uint32_t out;
    if (run_contact(o, r, &out, 0x15664, 0x15720)) return out;
    /* half a step a frame across (odd steps alternate) */
    hook(0x156C0);                                    /* (Insane mode: dogs run faster) */
    int16_t dx = (int16_t)obj16(OBJ_DX, o);
    if (!dx) { branched(0x156D4, 0x158E4); return run_dz(o, r); }
    uint16_t parity = ram16(FRAME_COUNT) & 1;
    if (dx > 0) { dx = (int16_t)((uint16_t)(dx + parity) >> 1); ran_to(0x156E0); branched(0x156E0, 0x156EC); }
    else { dx = (int16_t)-(int16_t)((uint16_t)(-dx + parity) >> 1); branched(0x156D8, 0x156E4); ran_to(0x156EC); }
    return run_across(o, r, dx);
}

/* a frame of changing lane (F0 going on) */
static uint32_t go_F0(uint16_t o, uint32_t at)
{
    (void)at;
    const struct run_at *r = &RUN_F0;
    uint32_t out;
    if (run_contact(o, r, &out, 0x166F6, 0x167A4)) return out;
    hook(0x16752);                                    /* (Insane mode: dogs run faster) */
    int16_t dx = (int16_t)obj16(OBJ_DX, o);
    if (!dx) { branched(0x1675C, 0x16968); return run_dz(o, r); }
    dx = (int16_t)(int8_t)(uint8_t)((uint16_t)(dx + (ram16(FRAME_COUNT) & 1)) >> 1);
    ran_to(0x16770);
    return run_across(o, r, dx);
}

/* a frame of op 100 (going on): a pixel a frame its way, but every 4th frame */
static uint32_t go_100(uint16_t o, uint32_t at)
{
    (void)at;
    const struct run_at *r = &RUN_48;
    uint32_t out;
    if (run_contact(o, r, &out, 0x16B38, 0x15720)) return out;
    int16_t dx = (ram16(FRAME_COUNT) & 3) ? 1 : 0;
    if (dx) branched(0x16BA0, 0x16BA6);
    else ran_to(0x16BA6);
    if (!dx) { branched(0x16BA8, 0x158E4); return run_dz(o, r); }
    int16_t d = (int16_t)obj16(OBJ_DX, o);
    if (!d) { branched(0x16BB6, 0x158E4); return run_dz(o, r); }
    if (d > 0) { branched(0x16BBA, 0x156EC); return run_across(o, r, 1); }
    ran_to(0x16BC0);
    branched(0x16BC0, 0x156EC);
    return run_across(o, r, -1);
}

/* 48 / F0 / 5C / 100: run its way (the way it wants, $92D0; 5C: a table's, by guard)
 * for n frames: dx, dz from a table by the way (48, F0: 0x1599C; 5C: 0x15C4A; 100:
 * 0x16BC4) */
static void run_begin(uint16_t o, uint16_t way, uint32_t table)
{
    uint32_t a = table + (int16_t)(uint16_t)(way << 2);
    set_obj16(OBJ_DX, o, rom16(a));
    set_obj16(GUARD_DZ, o, rom16(a + 2));
}
static uint32_t op_48_run(uint16_t o, uint32_t at)
{
    set_obj16(GUARD_WAIT, o, mem16(at));
    keep(o, at + 2);
    run_begin(o, obj16(GUARD_WANT_FACING, o), 0x1599C);
    ran_to(0x15664);
    return go_48(o, at);
}
static uint32_t op_F0_lane(uint16_t o, uint32_t at)
{
    set_obj16(GUARD_WAIT, o, mem16(at));
    keep(o, at + 2);
    run_begin(o, obj16(GUARD_WANT_FACING, o), 0x1599C);
    ran_to(0x166F6);
    return go_F0(o, at);
}
static uint32_t op_5C_run(uint16_t o, uint32_t at)
{
    set_obj16(GUARD_WAIT, o, mem16(at));
    uint32_t table = mem32(at + 2);
    keep(o, at + 6);
    uint16_t way = obj16(table, o);
    set_obj16(GUARD_FACING_SHOWN, o, way);
    ran_to(0x15C42);
    branched(0x15C42, 0x1564C);
    run_begin(o, way, 0x15C4A);
    ran_to(0x15664);
    return go_48(o, at);
}
static uint32_t go_5C(uint16_t o, uint32_t at) { branched(0x15C46, 0x15664); return go_48(o, at); }
static uint32_t op_100_run(uint16_t o, uint32_t at)
{
    set_obj16(GUARD_WAIT, o, mem16(at));
    keep(o, at + 2);
    uint16_t way = obj16(GUARD_WANT_FACING, o);
    ran_to(0x16B34);
    branched(0x16B34, 0x1564C);
    run_begin(o, way, 0x16BC4);
    ran_to(0x15664);
    return go_48(o, at);
}

/* 58 going on: falling: floor below (17C54's checks, dy down): landed (the wait over,
 * onto the floor); the path's next step when this one's frames are over */
static uint32_t go_58(uint16_t o, uint32_t at)
{
    (void)at;
    set_obj16(GUARD_STILL, o, 1);
    int16_t dy = (int16_t)obj16(OBJ_DY, o);
    set_lo(6, (uint16_t)dy);
    int16_t lane = (int16_t)(obj16(0xFF37B2, o) + dy - 0x20);
    static const struct { int16_t dx; uint32_t routine, jsr; } st[5] = {
        {0, 0x17934, 0x15B1E}, {-4, 0x17934, 0x15B3C}, {4, 0x17934, 0x15B5A}, {0, 0x177B8, 0x15B76}, {0, 0x17820, 0x15B90},
    };
    int floor = 0;
    REG_D[7] = 0;
    for (int i = 0; i < 5 && !floor; i++) {
        if (i == 1 || i == 2) set_lo(7, (uint16_t)st[i].dx);
        if (i == 3) REG_D[7] = 0;
        set_lo(3, (uint16_t)lane);
        floor = checked(st[i].routine, st[i].jsr);
        if (floor && i < 4) branched(st[i].jsr + 6, 0x15B9A);
    }
    if (!floor) {
        branched(0x15B96, 0x15BAC);
    } else {
        if (st[4].jsr && floor) ran_to(0x15B9A);
        set_obj16(GUARD_WAIT, o, 0);
        call(0x1798C, 0x15BA4);
        dy = (int16_t)lo(REG_D[7]);
        set_lo(6, (uint16_t)dy);
        ran_to(0x15BAC);
    }
    set_obj16(OBJ_Y, o, (uint16_t)(obj16(OBJ_Y, o) + dy));
    uint16_t left = (uint16_t)(obj16(GUARD_PATH_LEFT, o) - 1);
    set_obj16(GUARD_PATH_LEFT, o, left);
    if (left) { branched(0x15BC4, 0x144AE); return AFTER_SCRIPT; }
    uint32_t p = obj32(GUARD_PATH, o);
    set_obj16(OBJ_DY, o, mem16(p));
    set_obj16(GUARD_PATH_LEFT, o, mem16(p + 2));
    set_obj32(GUARD_PATH, o, p + 4);
    return to_after(0x15BEE);
}

/* ================================================================ the rest */

/* C8: its animation from step n (bytes in): that step's frame and time */
static uint32_t op_C8_anim_at(uint16_t o, uint32_t at)
{
    uint16_t n = mem16(at);
    uint32_t steps = rom32(0x17CDE + (int16_t)(uint16_t)(obj16(OBJ_ANIM, o) << 2));
    set_obj32(OBJ_ANIM_STEPS, o, steps);
    uint32_t st = steps + (int16_t)n;
    set_obj16(OBJ_FRAME, o, mem16(st));
    set_obj16(OBJ_ANIM_TIME, o, mem16(st + 2));
    set_obj16(OBJ_ANIM_AT, o, n);
    keep(o, at + 2);
    return to_next(0x164B6);
}

/* CC: back to its post's script, its script stack empty */
static uint32_t op_CC_to_post(uint16_t o, uint32_t at)
{
    (void)at;
    keep(o, obj32(GUARD_POST_SCRIPT, o));
    set_obj32(GUARD_SCRIPT_SP, o, GUARD_STACKS + (int16_t)(uint16_t)((o + 2) << 6));
    return to_after(0x164F4);
}
static uint32_t go_CC(uint16_t o, uint32_t at) { (void)o; (void)at; return to_after(0x164F4); }

/* D0: the object it carries ($8D78) goes */
static uint32_t op_D0_drop(uint16_t o, uint32_t at)
{
    uint16_t c = obj16(0xFF8D78, o);
    if (!c) branched(0x16502, 0x16514);
    else { set_obj16(0xFF8D78, o, 0); set_obj16(OBJ_ACTIVE, c, 0); ran_to(0x16514); }
    keep(o, at);
    return to_next(0x16522);
}
static uint32_t go_D0(uint16_t o, uint32_t at) { keep(o, at); return to_next(0x16522); }

/* D4: by why its last run stopped ($9498: 1-7), call the script in that place of the 7
 * after it; 0: go on after them */
static uint32_t op_D4_by_stop(uint16_t o, uint32_t at)
{
    uint16_t why = obj16(GUARD_STOPPED, o);
    if (!why) {
        branched(0x16530, 0x16542);
        keep(o, at + 0x1C);
        return to_next(0x16554);
    }
    uint32_t to = mem32(at + (int16_t)(uint16_t)(why << 2) - 4);
    ran_to(0x1653E);
    branched(0x1653E, 0x16232);                       /* (op 94's call) */
    uint32_t sp = obj32(GUARD_SCRIPT_SP, o) - 4;
    set32(sp, at + 0x1C);
    set_obj32(GUARD_SCRIPT_SP, o, sp);
    keep(o, to);
    return to_next(0x16256);
}
static uint32_t go_D4(uint16_t o, uint32_t at) { keep(o, at + 0x1C); return to_next(0x16554); }

/* D8: an animation by its facing: the word at $7644 + facing * 2 (as the game reads it) */
static uint32_t op_D8_anim_facing(uint16_t o, uint32_t at)
{
    uint16_t anim = ram16(GUARD_FACING + 2 * obj16(GUARD_FACING, o));
    ran_to(0x1656A);
    branched(0x1656A, 0x16380);                       /* (op B8 from its argument read) */
    set_obj16(OBJ_ANIM, o, anim);
    set_obj32(OBJ_ANIM_STEPS, o, rom32(0x17CDE + (int16_t)(uint16_t)(anim << 2)));
    set_obj16(OBJ_ANIM_AT, o, 0);
    set_obj16(OBJ_ANIM_TIME, o, 1);
    keep(o, at + 8);
    return to_next(0x163C6);
}
static uint32_t go_D8(uint16_t o, uint32_t at) { (void)o; (void)at; return to_next(0x1656E); }

/* DC: "equal" flag: the player it has seen stands at another height */
static uint32_t op_DC_other_height(uint16_t o, uint32_t at)
{
    keep(o, at);
    set_obj16(GUARD_FLAG_EQUAL, o, 0);
    uint16_t p = (uint16_t)(obj16(GUARD_SEEN, o) - 2);
    if (obj16(OBJ_HEIGHT, p) == obj16(OBJ_HEIGHT, o)) { branched(0x165A4, 0x147C2); return NEXT_OP; }
    set_obj16(GUARD_FLAG_EQUAL, o, 1);
    return to_next(0x165AE);
}
static uint32_t go_DC(uint16_t o, uint32_t at) { (void)o; (void)at; return to_next(0x165AE); }

/* E0: the way to its target ($94E4 x, $957C height) into the table given: 1 left, 3
 * right; level across: 0 up, 2 down, or there ("equal" set, nothing stored) */
static uint32_t op_E0_way_to(uint16_t o, uint32_t at)
{
    uint32_t table = mem32(at);
    at += 4;
    set_obj16(GUARD_FLAG_EQUAL, o, 0);
    int16_t dx = (int16_t)(obj16(0xFF94E4, o) - obj16(OBJ_X, o));
    uint16_t way = 3;
    if (dx > 0) { ran_to(0x165D8); branched(0x165D8, 0x16612); }
    else if (dx < 0) { ran_to(0x165DE); way = 1; branched(0x165DE, 0x16612); }
    else {
        branched(0x165D4, 0x165E2);
        int16_t dz = (int16_t)(obj16(0xFF957C, o) - obj16(OBJ_HEIGHT, o));
        way = 0;
        if (dz == 0) {
            set_obj16(GUARD_FLAG_EQUAL, o, 1);
            ran_to(0x16608);
            branched(0x16608, 0x16616);
            keep(o, at);
            return to_next(0x16624);
        }
        branched(0x165F8, 0x1660C);
        if (dz < 0) branched(0x1660C, 0x16612);
        else { way = 2; ran_to(0x16612); }
    }
    set_obj16(table, o, way);
    keep(o, at);
    return to_next(0x16624);
}
static uint32_t go_E0(uint16_t o, uint32_t at) { (void)o; (void)at; return to_next(0x16624); }

/* E4: return (as 98), and its frame goes on after the script */
static uint32_t op_E4_return(uint16_t o, uint32_t at)
{
    (void)at;
    uint32_t sp = obj32(GUARD_SCRIPT_SP, o);
    keep(o, mem32(sp));
    set_obj32(GUARD_SCRIPT_SP, o, sp + 4);
    return to_after(0x1664A);
}
static uint32_t go_E4(uint16_t o, uint32_t at) { (void)o; (void)at; return to_after(0x1664A); }

/* E8: back to as it was put at its post (sub_179B0) */
static uint32_t op_E8_reset(uint16_t o, uint32_t at)
{
    call(0x179B0, 0x1664E);
    keep(o, at);
    return to_after(0x16662);
}
static uint32_t go_E8(uint16_t o, uint32_t at) { (void)o; (void)at; return to_after(0x16662); }

/* EC: points (the long given) for the player who shot it (sub_11D6A), unless $566C */
static uint32_t op_EC_points(uint16_t o, uint32_t at)
{
    uint32_t pts = mem32(at);
    keep(o, at + 4);
    uint16_t who = obj16(0xFF9614, o) >> 1;
    if (who > 1) { ran_to(0x1668A); branched(0x1668A, 0x147C2); return NEXT_OP; }
    if (who == 0) branched(0x16682, 0x1668E);
    else ran_to(0x1668E);
    uint32_t d0 = REG_D[0], d1 = REG_D[1], a0 = REG_A[0];
    REG_D[6] = pts;
    set_lo(0, who);
    set_lo(1, (uint16_t)pts);
    if (ram16(0xFF566C)) branched(0x1669C, 0x166A6);
    else call(0x11D6A, 0x166A0);
    REG_D[0] = d0;
    REG_D[1] = d1;
    REG_A[0] = a0;
    set_lo(7, who);
    return to_next(0x166AA);
}
static uint32_t go_EC(uint16_t o, uint32_t at) { (void)o; (void)at; return to_next(0x166AA); }

/* F4 / F8: wait (its animation held at its first step's time) while the player it holds
 * ($A46C) is held / until he is held by 1 */
static uint32_t holding(uint16_t o, uint32_t base, int until_one)
{
    jumped_to(base);
    set_obj16(OBJ_ANIM_TIME, o, mem16(obj32(OBJ_ANIM_STEPS, o) + 2));
    uint16_t c = obj16(0xFFA46C, o);
    uint16_t h = obj16(0xFF9E9E, c);
    int go_on = until_one ? h != 1 : h != 0;
    uint32_t bne = base + (until_one ? 0x30 : 0x2E);  /* (F8's compare is a word longer) */
    if (go_on) { branched(bne, 0x144AE); return AFTER_SCRIPT; }
    set_obj16(GUARD_WAIT, o, 0);
    return to_after(bne + 0xE);
}
static uint32_t op_F4_hold(uint16_t o, uint32_t at)
{
    set_obj16(GUARD_WAIT, o, 0xFFFF);
    keep(o, at);
    ran_to(0x16A2E);
    return holding(o, 0x16A2E, 0);
}
static uint32_t go_F4(uint16_t o, uint32_t at) { (void)at; return holding(o, 0x16A2E, 0); }
static uint32_t op_F8_hold(uint16_t o, uint32_t at)
{
    set_obj16(GUARD_WAIT, o, 0xFFFF);
    keep(o, at);
    ran_to(0x16A88);
    return holding(o, 0x16A88, 1);
}
static uint32_t go_F8(uint16_t o, uint32_t at) { (void)at; return holding(o, 0x16A88, 1); }

/* FC: its script stack empty, and that its post's too */
static uint32_t op_FC_stack(uint16_t o, uint32_t at)
{
    uint32_t sp = GUARD_STACKS + (int16_t)(uint16_t)((o + 2) << 6);
    set_obj32(GUARD_SCRIPT_SP, o, sp);
    set_obj32(GUARD_POST_SP, o, sp);
    keep(o, at);
    return to_next(0x16B00);
}
static uint32_t go_FC(uint16_t o, uint32_t at) { (void)o; (void)at; return to_next(0x16B00); }

static uint32_t go_C8(uint16_t o, uint32_t at) { keep(o, at); return to_next(0x164B6); }

const guard_op guard_op_start[65] = {
    [0x00 / 4] = op_00_stop, [0x04 / 4] = op_04_wait, [0x08 / 4] = op_08_move, [0x0C / 4] = op_0C_walk,
    [0x10 / 4] = op_10_move_y, [0x14 / 4] = op_14_rise, [0x18 / 4] = op_18_look, [0x1C / 4] = op_1C_fire,
    [0x20 / 4] = op_20_fire, [0x24 / 4] = op_24_loop, [0x28 / 4] = op_28_event, [0x30 / 4] = op_30_bullet,
    [0x34 / 4] = op_34_pause, [0x38 / 4] = op_38_alarm, [0x2C / 4] = op_2C_turn, [0x3C / 4] = op_3C_jump,
    [0x40 / 4] = op_40_puff, [0x44 / 4] = op_44_toward, [0x4C / 4] = op_4C_hit_player, [0x50 / 4] = op_50_set,
    [0x54 / 4] = op_54_turn, [0x58 / 4] = op_58_fall, [0x60 / 4] = op_60_hop, [0x64 / 4] = op_64_toward,
    [0x68 / 4] = op_68_turn, [0x6C / 4] = op_6C_hold, [0x70 / 4] = op_70_up, [0x74 / 4] = op_74_down,
    [0x78 / 4] = op_78_make, [0x7C / 4] = op_7C_throw, [0x80 / 4] = op_80_jump, [0x84 / 4] = op_84,
    [0x88 / 4] = op_88, [0x8C / 4] = op_8C, [0x90 / 4] = op_90, [0x94 / 4] = op_94_call,
    [0x98 / 4] = op_98_return, [0x9C / 4] = op_9C_sound, [0xA0 / 4] = op_A0_copy, [0xA4 / 4] = op_A4_set,
    [0xA8 / 4] = op_A8_add, [0xAC / 4] = op_AC_sub, [0xB0 / 4] = op_B0_add, [0xB4 / 4] = op_B4_sub,
    [0xB8 / 4] = op_B8_anim, [0xBC / 4] = op_BC_compare, [0xC0 / 4] = op_C0_compare, [0xC4 / 4] = op_C4_nothing,
    [0x48 / 4] = op_48_run, [0xF0 / 4] = op_F0_lane, [0x5C / 4] = op_5C_run, [0x100 / 4] = op_100_run,
    [0xC8 / 4] = op_C8_anim_at, [0xCC / 4] = op_CC_to_post, [0xD0 / 4] = op_D0_drop, [0xD4 / 4] = op_D4_by_stop,
    [0xD8 / 4] = op_D8_anim_facing, [0xDC / 4] = op_DC_other_height, [0xE0 / 4] = op_E0_way_to,
    [0xE4 / 4] = op_E4_return, [0xE8 / 4] = op_E8_reset, [0xEC / 4] = op_EC_points, [0xF4 / 4] = op_F4_hold,
    [0xF8 / 4] = op_F8_hold, [0xFC / 4] = op_FC_stack,
};
const guard_op guard_op_going[65] = {
    [0x00 / 4] = go_00, [0x04 / 4] = go_04, [0x08 / 4] = go_08, [0x0C / 4] = go_0C, [0x10 / 4] = go_10,
    [0x14 / 4] = go_14, [0x18 / 4] = go_18, [0x1C / 4] = go_1C, [0x20 / 4] = go_20, [0x24 / 4] = go_24,
    [0x28 / 4] = go_28, [0x30 / 4] = go_30, [0x34 / 4] = go_34, [0x38 / 4] = go_38, [0x2C / 4] = go_2C,
    [0x3C / 4] = go_3C, [0x40 / 4] = go_40, [0x44 / 4] = go_44, [0x4C / 4] = go_4C, [0x50 / 4] = go_50,
    [0x54 / 4] = go_54, [0x60 / 4] = go_60, [0x64 / 4] = go_64, [0x68 / 4] = go_68, [0x6C / 4] = go_6C,
    [0x70 / 4] = go_70, [0x74 / 4] = go_74, [0x78 / 4] = go_78, [0x7C / 4] = go_7C, [0x80 / 4] = go_80,
    [0x84 / 4] = go_84, [0x88 / 4] = go_88, [0x8C / 4] = go_8C, [0x90 / 4] = go_90, [0x94 / 4] = go_94,
    [0x98 / 4] = go_98, [0x9C / 4] = go_9C, [0xA0 / 4] = go_A0, [0xA4 / 4] = go_A4, [0xA8 / 4] = go_A8,
    [0xAC / 4] = go_AC, [0xB0 / 4] = go_B0, [0xB4 / 4] = go_B4, [0xB8 / 4] = go_B8, [0xBC / 4] = go_BC,
    [0xC0 / 4] = go_C0, [0xC4 / 4] = go_C4, [0x48 / 4] = go_48, [0xF0 / 4] = go_F0, [0x5C / 4] = go_5C,
    [0x100 / 4] = go_100, [0x58 / 4] = go_58, [0xC8 / 4] = go_C8, [0xCC / 4] = go_CC, [0xD0 / 4] = go_D0,
    [0xD4 / 4] = go_D4, [0xD8 / 4] = go_D8, [0xDC / 4] = go_DC, [0xE0 / 4] = go_E0, [0xE4 / 4] = go_E4,
    [0xE8 / 4] = go_E8, [0xEC / 4] = go_EC, [0xF4 / 4] = go_F4, [0xF8 / 4] = go_F8, [0xFC / 4] = go_FC,
};
