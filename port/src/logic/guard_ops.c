/* The guards' script ops (tables 0x14D5A: an op starting, 0x14E5E: an op going on while
 * its wait lasts). A script is a list of words in ROM: an op number (times 4) and its
 * arguments. Each op returns where the guard's frame goes on: NEXT_OP (the next op
 * now, taking no time), AFTER_SCRIPT (the rest of its frame) or FRAME_OVER.
 *
 * o: the guard (object number * 2); at: where its arguments are (the start of an op),
 * or the game's a0 (an op going on: the work memory's object table, unused). */
#include "rt.h"
#include "guard_ops.h"

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
    call(0x16BE8, 0x1511E);
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

const guard_op guard_op_start[65] = {
    [0x00 / 4] = op_00_stop, [0x04 / 4] = op_04_wait, [0x08 / 4] = op_08_move, [0x0C / 4] = op_0C_walk,
    [0x10 / 4] = op_10_move_y, [0x14 / 4] = op_14_rise, [0x18 / 4] = op_18_look, [0x1C / 4] = op_1C_fire,
    [0x20 / 4] = op_20_fire, [0x24 / 4] = op_24_loop, [0x28 / 4] = op_28_event, [0x30 / 4] = op_30_bullet,
    [0x34 / 4] = op_34_pause, [0x38 / 4] = op_38_alarm,
};
const guard_op guard_op_going[65] = {
    [0x00 / 4] = go_00, [0x04 / 4] = go_04, [0x08 / 4] = go_08, [0x0C / 4] = go_0C, [0x10 / 4] = go_10,
    [0x14 / 4] = go_14, [0x18 / 4] = go_18, [0x1C / 4] = go_1C, [0x20 / 4] = go_20, [0x24 / 4] = go_24,
    [0x28 / 4] = go_28, [0x30 / 4] = go_30, [0x34 / 4] = go_34, [0x38 / 4] = go_38,
};
