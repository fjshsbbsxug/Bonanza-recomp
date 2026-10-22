/* Guards (objects 6..37): events, scripts, what they have seen. */
#include "rt.h"
#include <stdio.h>
#include <stdlib.h>
#include "guard_ops.h"
#include "guards.h"
#include "map.h"

/* sub_09F42 / sub_09F66: every guard after player 1 (2) / player 2 (4) forgets him */
static int guards_forget(uint16_t player, uint32_t routine)
{
    if (!start()) return 0;
    begin(routine);
    ran_to(routine + 0x0E);
    for (uint16_t o = 0xC; o < 0xC + 64; o += 2) {
        if (obj16(GUARD_SEEN, o) == player) {
            set_obj16(GUARD_SEEN, o, 0);
            ran_to(routine + 0x1E);
        } else {
            branched(routine + 0x14, routine + 0x1C);
        }
        if (o < 0xC + 62) branched(routine + 0x1E, routine + 0x0E);
        else ran_to(routine + 0x22);
    }

    REG_A[6] = GUARD_SEEN;                            /* (for the callers) */
    set_lo(7, 0x4C);
    set_lo(6, 0xFFFF);
    FLAG_X = 0;                                       /* (addq: no carry) */
    return done();
}

int logic_guards_forget_p1(void) { return guards_forget(2, 0x09F42); }
int logic_guards_forget_p2(void) { return guards_forget(4, 0x09F66); }

/* sub_171B6: the script guard d6 of kind d7 runs for the event waiting for it: d7 =
 * the word for that event in the kind's row of the table at 0x171D6 (16 words a kind,
 * by event / 4); 0 when no event waits */
int logic_guard_event_script(void)
{
    if (!start()) return 0;
    uint16_t kind = lo(REG_D[7]), g = lo(REG_D[6]);
    uint32_t row = 0x171D6 + (int16_t)(uint16_t)(kind << 5);
    uint16_t event = obj16(GUARD_EVENT, g);
    uint16_t script = event ? rom16(row + (int16_t)(uint16_t)((event >> 2) * 2)) : 0;

    set_lo(7, script);                                /* (for the callers) */
    REG_A[6] = row;
    REG_A[5] = GUARD_EVENT;
    flags_of16(script);
    begin(0x171B6);
    if (!event) branched(0x171C8, 0x171D4);
    ran_to(0x171D4);
    return done();
}

/* sub_17170: send guard d6 event d7: 4 shot, 8 touched, $C a player seen... (each a
 * script the guard runs next). Unless the guard ignores the event now, its script's
 * stack starts over and the event waits for it. */
int logic_guard_event(void)
{
    if (!start()) return 0;
    uint16_t g = lo(REG_D[6]), event = lo(REG_D[7]);
    uint16_t mask = obj16(GUARD_EVENT_MASK, g);
    uint16_t bit = (uint16_t)((event >> 2) - 1);
    int ignored = (((REG_D[5] & 0xFFFF0000u) | mask) >> (bit & 31)) & 1;
    if (!ignored) {
        set_obj32(GUARD_SCRIPT_SP, g, GUARD_STACKS + (int16_t)(uint16_t)((g + 2) << 6));
        set_obj16(GUARD_EVENT, g, event);
    }

    set_lo(4, bit);                                   /* (for the callers) */
    set_lo(5, ignored ? mask : (uint16_t)(g * 2));
    begin(0x17170);
    if (ignored) branched(0x17186, 0x171B0);
    ran_to(0x171B4);
    return done();
}

/* ================================================================ the guards' frame */

/* the fall a guard starts when there is no floor under it: steps (dy, frames) */
#define FALL_STEPS 0x1478A

/* a guard touches player p (0 / 1), on the side the touch test found: the player is
 * pushed away from it and, unless he is being shot (state $8002) or already caught,
 * caught. The time: of the instructions from `at` (player 1's code; player 2's is the
 * same, 0x80 further on) up to the end of the catching. */
static void touch(uint16_t o, int p, uint16_t facing)
{
    uint32_t at = p ? 0x80 : 0;
    uint16_t side = ram16(p ? P2_TOUCH_SIDE : P1_TOUCH_SIDE);
    set_obj16(TOUCHED_BY, o, p ? 4 : 2);
    set_obj16(TOUCH_SIDE, o, side);
    set_obj16(TOUCH_WHO, o, p ? 2 : 0);
    if (facing == 1) branched(0x1465E + at, 0x14666 + at);
    else ran_to(0x14666 + at);
    set16(p ? P2_PUSH : P1_PUSH, facing == 1 ? 0xFFFF : 1);
    if (ram16(p ? P2_CAUGHT : P1_CAUGHT)) {
        branched(0x14672 + at, 0x14714);
    } else if (ram16(p ? P2_STATE : P1_STATE) == 0x8002) {
        branched(0x1467E + at, 0x14714);
    } else {
        set16(p ? P2_CAUGHT : P1_CAUGHT, 1);
        set16(p ? P2_CAUGHT_SIDE : P1_CAUGHT_SIDE, side);
        if (p) ran_to(0x14714);
        else branched(0x14694, 0x14714);
    }
}

/* the part of a guard's frame after its script (0x144AE..0x14782): its animation steps,
 * it falls when there is no floor under it, it touches the players; a guard that has
 * seen a player and touches one catches him (event $1C) */
static void after_script(uint16_t o)
{
    /* the animation: the next step when this one's time is up (a negative frame: the
     * steps start over) */
    jumped_to(0x144AE);
    uint16_t time = (uint16_t)(obj16(OBJ_ANIM_TIME, o) - 1);
    set_obj16(OBJ_ANIM_TIME, o, time);
    if (time) {
        branched(0x144B8, 0x14502);
    } else {
        uint16_t at = obj16(OBJ_ANIM_AT, o);
        ran_to(0x144C6);
        for (;;) {
            uint32_t step = obj32(OBJ_ANIM_STEPS, o) + (int16_t)at;
            uint16_t frame = mem16(step);
            if (!(frame & 0x8000)) {
                set_obj16(OBJ_FRAME, o, frame);
                set_obj16(OBJ_ANIM_TIME, o, mem16(step + 2));
                set_obj16(OBJ_ANIM_AT, o, (uint16_t)(at + 4));
                branched(0x144E0, 0x144E8);
                ran_to(0x14502);
                break;
            }
            at = 0;
            ran_to(0x144E6);
            branched(0x144E6, 0x144C6);
        }
    }

    /* falling */
    set_obj16(GUARD_99A4, o, 0);
    if (obj16(GUARD_STILL, o)) {
        branched(0x14516, 0x145F0);
    } else if (!obj16(GUARD_FALLING, o)) {
        ran_to(0x14524);
        if ((ram16(FRAME_COUNT) & 1) != ((o >> 1) & 1)) {   /* (every other frame) */
            branched(0x1453C, 0x145F0);
        } else {
            REG_D[7] = 0;
            REG_D[6] = 2;
            call(0x17C54, 0x14544);                          /* the floor under it */
            if (FLAG_Z) {
                branched(0x1454A, 0x145F0);
            } else {
                set_obj16(GUARD_FALL_DY, o, rom16(FALL_STEPS));
                set_obj16(GUARD_FALL_LEFT, o, rom16(FALL_STEPS + 2));
                set_obj32(GUARD_FALL_NEXT, o, FALL_STEPS + 4);
                set_obj16(GUARD_FALLING, o, 1);
                ran_to(0x14582);
                branched(0x14582, 0x145F0);
            }
        }
    } else {
        branched(0x14524, 0x14586);
        uint16_t dy = obj16(GUARD_FALL_DY, o);
        set_lo(6, dy);
        REG_D[7] = 0;
        call(0x17C54, 0x14592);                              /* the floor, dy below */
        if (!FLAG_Z) {
            branched(0x14598, 0x145AE);
        } else {
            set_obj16(GUARD_FALLING, o, 0);                  /* landed: onto the floor */
            call(0x1798C, 0x145A6);
            dy = lo(REG_D[7]);
            set_lo(6, dy);
            ran_to(0x145AE);
        }
        set_obj16(OBJ_Y, o, (uint16_t)(obj16(OBJ_Y, o) + dy));
        uint16_t left = (uint16_t)(obj16(GUARD_FALL_LEFT, o) - 1);
        set_obj16(GUARD_FALL_LEFT, o, left);
        if (left) {
            branched(0x145C6, 0x145F0);
        } else {
            uint32_t next = obj32(GUARD_FALL_NEXT, o);
            set_obj16(GUARD_FALL_DY, o, mem16(next));
            set_obj16(GUARD_FALL_LEFT, o, mem16(next + 2));
            set_obj32(GUARD_FALL_NEXT, o, next + 4);
            ran_to(0x145F0);
        }
    }

    /* touching the players (player 1 only by a guard facing the view or sideways) */
    set_obj16(TOUCHED_BY, o, 0);
    uint16_t touching = players_touching(o, 0x145FA);
    uint16_t facing = obj16(GUARD_FACING, o);
    int p1 = 0;
    if (!(touching & 1)) branched(0x14604, 0x14696);
    else if (ram16(P1_PROTECT)) branched(0x1460E, 0x14696);
    else if (!facing) { branched(0x1461C, 0x14628); p1 = 1; }
    else if (facing & 4) branched(0x14624, 0x14696);
    else { ran_to(0x14628); p1 = 1; }
    if (p1) {
        touch(o, 0, facing);
    } else if (!(touching & 2)) {
        branched(0x1469A, 0x14778);
        goto end;
    } else if (ram16(P2_PROTECT)) {
        branched(0x146A4, 0x14778);
        goto end;
    } else {
        touch(o, 1, facing);
    }

    /* a guard that has seen a player catches the one it touches (standing on the
     * floor, or at height $10) */
    if (obj16(GUARD_SEEN, o)) {
        uint16_t z = obj16(OBJ_HEIGHT, o);
        if (obj16(GUARD_FALLING, o)) {
            branched(0x1472C, 0x14778);
        } else if (z && z != 0x10) {
            if (z) ran_to(0x14742);
            branched(0x14742, 0x14778);
        } else {
            if (!z) branched(0x1473A, 0x14746);
            else ran_to(0x14746);
            set_lo(6, o);
            set_lo(7, 0x1C);
            call(0x17170, 0x1474C);                          /* (logic_guard_event) */
            branched(0x14752, 0x14778);
        }
    } else {
        branched(0x1471E, 0x14754);
        if (rom16(0x147BA + 2 * facing) != obj16(TOUCH_SIDE, o)) branched(0x14772, 0x14778);
        else ran_to(0x14778);
    }
end:
    set_obj16(TOUCH_PLAYER, o, 0);
    ran_to(0x14782);
    /* (the game's code leaves d5-d7 and a4-a6 as its steps left them: nothing reads them) */
    scramble(0xE0 | (0x70 << 8) | SCRAMBLE_FLAGS);
}

/* the part of a guard's frame before its script (0x1423A..0x1449A). A guard out of
 * view goes back to its post (unless its post is in view), once; a guard in view
 * forgets a player no longer in its view, looks for players (a player seen: event $C,
 * unless an event waiting has its own script), and an event waiting starts its script
 * (the guard's kind's script for it), or a script call asked for ($6E8C) is made.
 * 1: its script runs now; 0: its frame is over. */
static int before_script(uint16_t o)
{
    jumped_to(0x1423A);
    REG_A[0] = OBJ_ACTIVE;                            /* (the script's ops find it there) */
    if (!obj16(OBJ_ACTIVE, o)) {
        branched(0x14244, 0x14782);
        return 0;
    }
    if (!obj16(OBJ_ON_SCREEN, o)) {
        ran_to(0x14256);
        if (obj16(GUARD_PLACED, o)) {
            branched(0x14260, 0x14782);
            return 0;
        }
        /* would it be in view at its post? */
        uint16_t x = obj16(OBJ_X, o), y = obj16(OBJ_Y, o), z = obj16(OBJ_HEIGHT, o);
        set_obj16(OBJ_X, o, obj16(GUARD_POST_X, o));
        set_obj16(OBJ_Y, o, obj16(GUARD_POST_Y, o));
        set_obj16(OBJ_HEIGHT, o, obj16(GUARD_POST_HEIGHT, o));
        call(0x058B4, 0x142AA);
        if (obj16(OBJ_ON_SCREEN, o)) {                /* yes: it stays where it was */
            set_obj16(OBJ_ON_SCREEN, o, 0);
            set_obj16(OBJ_X, o, x);
            set_obj16(OBJ_Y, o, y);
            set_obj16(OBJ_HEIGHT, o, z);
            ran_to(0x142E4);
            branched(0x142E4, 0x14782);
            return 0;
        }
        branched(0x142BE, 0x142E8);
        set_obj16(OBJ_X, o, obj16(GUARD_POST_X, o));
        set_obj16(OBJ_Y, o, obj16(GUARD_POST_Y, o));
        set_obj16(OBJ_HEIGHT, o, obj16(GUARD_POST_HEIGHT, o));
        set_obj16(GUARD_FACING, o, obj16(GUARD_POST_FACING, o));
        set_obj32(GUARD_SCRIPT, o, obj32(GUARD_POST_SCRIPT, o));
        set_obj32(GUARD_SCRIPT_SP, o, obj32(GUARD_POST_SP, o));
        set_obj16(GUARD_WAIT, o, 0);
        set_obj16(OBJ_PASS, o, 0);
        set_obj16(0xFF32FE, o, 0);
        call(0x179B0, 0x14376);                       /* its standing animation */
        set_obj16(GUARD_PLACED, o, 1);
        ran_to(0x14382);
        branched(0x14382, 0x14782);
        return 0;
    }
    branched(0x14252, 0x14386);
    set_obj16(GUARD_STILL, o, 0);
    set_obj16(GUARD_PLACED, o, 0);

    /* a player it has seen and who is out of its view: forgotten */
    uint16_t seen = obj16(GUARD_SEEN, o);
    if (!seen) {
        branched(0x143A4, 0x143D4);
    } else {
        uint16_t view = obj16(OBJ_ON_SCREEN, o);
        int in_view = seen == 2 ? (view & 1) : (view & 2);
        if (seen == 2) {
            if (in_view) branched(0x143BC, 0x143D4);
            else { ran_to(0x143C0); set_obj16(GUARD_SEEN, o, 0); branched(0x143CA, 0x143D4); }
        } else {
            branched(0x143B4, 0x143CE);
            if (in_view) ran_to(0x143D4);
            else { branched(0x143D2, 0x143C0); set_obj16(GUARD_SEEN, o, 0); branched(0x143CA, 0x143D4); }
        }
    }

    /* looking for the players */
    if (!obj16(GUARD_LOOKS, o)) {
        branched(0x143DE, 0x14416);
    } else {
        guard_look(o, 0x143E2);
        uint16_t found = ram16(0xFF6CC0);
        set_obj16(GUARD_SEEN, o, found);
        if (!found) {
            branched(0x143F6, 0x14416);
        } else {
            set_lo(7, 3);
            set_lo(6, o);
            call(0x171B6, 0x14400);                   /* (logic_guard_event_script) */
            if (FLAG_Z) {
                branched(0x14406, 0x14416);           /* an event waiting has its own script */
            } else {
                set_lo(7, 0xC);
                set_lo(6, o);
                call(0x17170, 0x14410);               /* (logic_guard_event): a player seen */
            }
        }
    }

    /* an event starts its script; or a script call asked for */
    uint16_t event = obj16(GUARD_EVENT, o);
    if (event) {
        set_obj16(GUARD_EVENT, o, 0);
        set_obj16(GUARD_WAIT, o, 0);
        uint32_t scripts = rom32(0x1479E + (uint16_t)(event - 4));   /* the kinds' scripts for it */
        set_obj32(GUARD_SCRIPT, o, ram32(scripts + 2 * o));
        ran_to(0x1445A);
        branched(0x1445A, 0x1449A);
    } else {
        branched(0x14420, 0x1445C);
        uint32_t call_to = obj32(0xFF6E8C, o);
        if (!call_to) {
            branched(0x1446A, 0x1449A);
        } else {
            uint32_t sp = obj32(GUARD_SCRIPT_SP, o) - 4;
            set32(sp, obj32(GUARD_SCRIPT, o));
            set_obj32(GUARD_SCRIPT_SP, o, sp);
            set_obj32(GUARD_SCRIPT, o, call_to);
            set_obj32(0xFF6E8C, o, 0);
            set_obj16(GUARD_WAIT, o, 0);
            ran_to(0x1449A);
        }
    }
    scramble(0xE0 | (0x78 << 8) | SCRAMBLE_FLAGS);   /* (d5-d7, a3-a6: nothing reads them) */
    return 1;
}

/* the guard's script: an op waiting (its wait not over) goes on, else the next op
 * starts; ops that take no time go straight on to the next (0x147C2). The ops are still
 * the game's code. The end: its frame goes on after the script (0x144AE), or is over. */
/* (checks: LOGIC_OPS_OFF=48,F0,... runs those ops as the game's code) */
static int op_off(uint16_t op)
{
    static int init;
    static uint8_t off[65];
    if (!init) {
        init = 1;
        const char *e = getenv("LOGIC_OPS_OFF");
        while (e && *e) {
            char *end;
            unsigned long v = strtoul(e, &end, 16);
            if (end == e) break;
            if (v / 4 < 65) off[v / 4] = 1;
            e = *end ? end + 1 : end;
        }
    }
    return off[op / 4];
}

static uint32_t run_script(uint16_t o)
{
    static const uint32_t stops[] = {AFTER_SCRIPT, NEXT_OP, FRAME_OVER};
    uint32_t handler, next;
    uint16_t op;
    guard_op c_op;
    if (obj16(GUARD_WAIT, o)) {
        branched(0x144A4, 0x147E6);
        op = obj16(0xFF780C, o);
        set_lo(7, op);
        handler = rom32(0x14E5E + op);
        c_op = op < 65 * 4 && !op_off(op) ? guard_op_going[op / 4] : 0;
        ran_to(0x147FA);
    } else {
        ran_to(0x144AE);                              /* (to the jmp) */
        goto fetch;
    }
    for (;;) {
        jumped_to(handler);
        {
            static char names[2][2][65][24];
            int going = c_op ? c_op != guard_op_start[op / 4] : handler == rom32(0x14E5E + op);
            char *nm = names[!c_op][going][op / 4];
            if (!nm[0])
                snprintf(nm, 24, "guard op %02X%s%s", op, going ? " going" : "", c_op ? "" : " (68000)");
            count_named(nm);
        }
        if (c_op)
            next = c_op(o, REG_A[0]);
        else
            next = run_part(handler, stops, 3);
        if (next != NEXT_OP)
            return next;
    fetch:
        jumped_to(0x147C2);
        {
            uint32_t at = obj32(GUARD_SCRIPT, o);
            op = mem16(at);
            set_obj16(0xFF780C, o, op);
            REG_A[0] = at + 2;
            set_lo(7, op);
            handler = rom32(0x14D5A + op);
            REG_A[6] = handler;
            c_op = op < 65 * 4 && !op_off(op) ? guard_op_start[op / 4] : 0;
            ran_to(0x147E6);
        }
    }
}

/* sub_14232: every guard's frame: guards out of view put back at their posts, events
 * (shot, touched, a player seen) start their scripts, scripts run, animations step,
 * guards fall, touch the players */
int logic_guards_update(void)
{
    begin_long(0x14232);                              /* (long: in the ending a frame can
                                                       *  end inside it) */
    REG_D[0] = 0xC;                                   /* o: the first guard */
    set_lo(1, ram16(GUARD_COUNT));
    ran_to(0x1423A);
    for (;;) {
        uint16_t o = lo(REG_D[0]);
        if (before_script(o) && run_script(o) == 0x144AE)
            after_script(o);
        jumped_to(0x14782);
        set_lo(0, (uint16_t)(o + 2));
        FLAG_N = NFLAG_16(o + 2); FLAG_Z = (uint16_t)(o + 2); FLAG_V = VFLAG_ADD_16(2, o, o + 2u);
        FLAG_X = FLAG_C = CFLAG_16(o + 2u);
        uint16_t left = (uint16_t)(lo(REG_D[1]) - 1);
        set_lo(1, left);
        if (left == 0xFFFF) break;
        branched(0x14784, 0x1423A);
    }
    ran_to(0x14788);
    return done_long();
}

/* ================================================================ looking for the players */

/* sub_16C68 / sub_16E8C: does guard o see player p (0 / 1)? Not if the player is out of
 * play, protected, hidden ($56BA & $200), being caught ($8014), or the guard faces away
 * from the view, or he is not in the guard's view, or not within 31 pixels up or down
 * the view. Facing him, he is seen within a range by cells (tables at 0x170B0 / 0x170F0:
 * the guard on the floor / on something; by cell, and +32 when he stands on something);
 * facing the view from height $10, only near (0x17130). And nothing in between: walls
 * (bit $10 of the cells, the floor's or the upper walls by his height) and closed doors
 * (cells $9x / $Ax: door object $4C + x not open); a guard standing on something looking
 * down at him far off: no walls in the 4 cells ahead. Seen: LOOK_FOUND = 2 / 4. */
static void look_at(uint16_t o, int p)
{
    uint32_t at = p ? 0x224 : 0;                      /* (player 2's code: the same, further on) */
    uint16_t P = (uint16_t)(2 * p);
    jumped_to(0x16C68 + at);
    uint32_t prot = p ? P2_PROTECT : P1_PROTECT;
    uint32_t hidden = p ? 0xFF5764 : 0xFF56BA;
    uint32_t state = p ? P2_STATE : P1_STATE;
    uint32_t end = 0x16E8A + at;
    if (!obj16(OBJ_ACTIVE, P)) { branched(0x16C6E + at, end); goto out; }
    if (ram16(prot)) { branched(0x16C78 + at, end); goto out; }
    if ((ram16(hidden) & 0x200) == 0x200) { branched(0x16C8A + at, end); goto out; }
    if (ram16(state) == 0x8014) { branched(0x16C96 + at, end); goto out; }
    uint16_t facing = obj16(GUARD_FACING, o);
    if (facing == 2) { branched(0x16CA6 + at, end); goto out; }
    if (!(obj16(OBJ_ON_SCREEN, o) & (p ? 2 : 1))) { branched(0x16CB8 + at, end); goto out; }
    int16_t dy = (int16_t)(obj16(OBJ_Y, o) - obj16(OBJ_Y, P));
    if (dy >= 0) branched(0x16CCA + at, 0x16CD0 + at);
    else { dy = (int16_t)-dy; ran_to(0x16CD0 + at); }
    if ((uint16_t)dy >= 0x1F) { branched(0x16CD4 + at, end); goto out; }

    /* which way he is; facing him or not */
    uint16_t gx = obj16(OBJ_X, o), px = obj16(OBJ_X, P);
    uint16_t dx = (uint16_t)(gx - px);
    uint16_t toward = 1;
    int16_t step = -4;
    if (gx >= px) branched(0x16CF4 + at, 0x16CFE + at);
    else { dx = (uint16_t)-(int16_t)dx; toward = 3; step = 4; ran_to(0x16CFE + at); }
    uint32_t range;
    uint16_t cells = dx >> 3;
    if (facing != toward) {
        if (facing) { ran_to(0x16D06 + at); branched(0x16D06 + at, end); goto out; }
        if (obj16(OBJ_HEIGHT, o) != 0x10) { ran_to(0x16D16 + at); branched(0x16D16 + at, end); goto out; }
        range = 0x17130;
        set16(LOOK_CELLS, cells);
        ran_to(0x16D26 + at);
        branched(0x16D26 + at, 0x16D46 + at);
    } else {
        branched(0x16D00 + at, 0x16D28 + at);
        set16(LOOK_CELLS, cells);
        range = 0x170B0;
        if (!obj16(OBJ_HEIGHT, o)) branched(0x16D3E + at, 0x16D46 + at);
        else { range = 0x170F0; ran_to(0x16D46 + at); }
    }
    uint16_t pz = obj16(OBJ_HEIGHT, P);
    uint16_t up = pz >= 8 ? 0x20 : 0;
    if (pz >= 8) ran_to(0x16D54 + at);
    else branched(0x16D4E + at, 0x16D54 + at);
    if (!md.rom[range + (int16_t)(uint16_t)(cells + up)]) { branched(0x16D5A + at, end); goto out; }

    /* the guard's cell (its floor, 32 pixels left of it) */
    uint16_t y = obj16(OBJ_Y, o), rest = y;
    uint16_t floor4 = 0;
    ran_to(0x16D6A + at);
    for (;;) {
        rest = (uint16_t)(rest - 0x50);
        if (rest & 0x8000) break;
        floor4 += 4;
        ran_to(0x16D74 + at);
        branched(0x16D74 + at, 0x16D6A + at);
    }
    branched(0x16D6E + at, 0x16D76 + at);
    uint32_t cell = ram32(MAP_FLOORS + floor4) + (int16_t)(uint16_t)((((uint16_t)(gx - 0x20)) >> 3) << 2)
                    - (int16_t)ram16(MAP_FLOOR_START + floor4 / 2);
    uint16_t gz = obj16(OBJ_HEIGHT, o);
    if (!gz) branched(0x16DAC + at, 0x16E22 + at);
    else if (pz) branched(0x16DB4 + at, 0x16E22 + at);
    else if (ram16(LOOK_CELLS) < 4) branched(0x16DC0 + at, 0x16E22 + at);
    else if (!facing) branched(0x16DCE + at, 0x16E22 + at);
    else {
        int16_t d = -4;
        if (facing == 1) branched(0x16DE0 + at, 0x16DEC + at);
        else { d = 4; ran_to(0x16DEC + at); }
        int16_t k = d;
        for (int i = 0; i < 4; i++) {
            uint8_t b = (uint8_t)RD8(cell + (int16_t)k + 1);
            uint32_t bne = 0x16DF4 + at + 0xE * i;
            if (b & 0x10) { ran_to(bne); branched(bne, end); goto out; }
            k = (int16_t)(k + d);
            ran_to(i < 3 ? bne + 6 : 0x16E22 + at);
        }
    }

    /* the cells to him, one by one */
    {
        int up_wall = obj16(OBJ_HEIGHT, P) != 0;      /* (his height: the walls up there) */
        if (up_wall) ran_to(0x16E30 + at);
        else branched(0x16E2A + at, 0x16E30 + at);
        uint16_t n = ram16(LOOK_CELLS);
        ran_to(0x16E3A + at);
        for (;;) {
            uint8_t b = (uint8_t)RD8(cell + up_wall);
            if (b & 0x10) { ran_to(0x16E40 + at); branched(0x16E40 + at, end); goto out; }
            if (up_wall) {
                branched(0x16E46 + at, 0x16E7C + at);
            } else {
                uint16_t info = mem16(cell + 2), kind = info & 0xF0;
                if (kind == 0x90 || kind == 0xA0) {
                    if (kind == 0x90) branched(0x16E58 + at, 0x16E64 + at);
                    else ran_to(0x16E64 + at);
                    uint16_t door = (uint16_t)(info - 0x90 + 0x4C);
                    if (obj16(0xFF9E9E, door) != 2) { ran_to(0x16E78 + at); branched(0x16E78 + at, end); goto out; }
                    ran_to(0x16E7C + at);
                } else {
                    ran_to(0x16E60 + at);
                    branched(0x16E60 + at, 0x16E7C + at);
                }
            }
            cell += (int16_t)step;
            if (n == 0) { ran_to(0x16E82 + at); break; }
            n--;
            ran_to(0x16E7E + at);
            branched(0x16E7E + at, 0x16E3A + at);
        }
        set16(LOOK_FOUND, p ? 4 : 2);
        ran_to(end);
    }
out:
    ran_to(end + 2);                                  /* (its rts) */
}

static void look_from(uint16_t o, uint32_t jsr);

/* sub_16BE8: guard o looks for the players (LOOK_FOUND: who it found, or 0). Having seen
 * one already: it keeps him in sight, looking again every 4th frame. Else every 4th
 * frame it looks at players 1 and 2 in turn. Time from the jsr at `jsr` (with its rts). */
void guard_look(uint16_t o, uint32_t jsr)
{
    look_from(o, jsr);
    jumped_to(jsr + 6);
}

static void look_from(uint16_t o, uint32_t jsr)
{
    ran_to(jsr + 6);
    jumped_to(0x16BE8);
    uint16_t seen = obj16(GUARD_SEEN, o);
    if (seen) {
        uint16_t n = (uint16_t)((obj16(GUARD_LOOK_COUNT, o) + 1) & 3);
        set_obj16(GUARD_LOOK_COUNT, o, n);
        if (n) { ran_to(0x16C12); set16(LOOK_FOUND, seen); jumped_to(0x16C12); return; }
        branched(0x16C06, 0x16C12);
        set16(LOOK_FOUND, 0);
        if (seen == 2) { ran_to(0x16C24); look_at(o, 0); jumped_to(0x16C24); ran_to(0x16C26); }
        else { branched(0x16C1A, 0x16C26); ran_to(0x16C2C); look_at(o, 1); jumped_to(0x16C2C); ran_to(0x16C2E); }
        return;
    }
    branched(0x16BF2, 0x16C2E);
    if (game_hooks) {                                 /* (the port's changes: Insane mode) */
        uint32_t pc = REG_PC;
        int h = game_hook(0x16C2E);
        if (h == 2) {                                 /* it looks with the routine it names, now */
            uint32_t to = REG_PC;
            REG_PC = pc;
            look_at(o, to == 0x16E8C);
            return;
        }
    }
    set16(LOOK_FOUND, 0);
    uint16_t n = (uint16_t)((obj16(GUARD_LOOK_COUNT, o) + 1) & 3);
    set_obj16(GUARD_LOOK_COUNT, o, n);
    if (n) { branched(0x16C44, 0x16C66); ran_to(0x16C68); return; }
    uint16_t which = obj16(GUARD_LOOK_WHICH, o) ^ 1;
    set_obj16(GUARD_LOOK_WHICH, o, which);
    if (!which) { ran_to(0x16C5E); look_at(o, 0); jumped_to(0x16C5E); ran_to(0x16C60); }
    else { branched(0x16C54, 0x16C60); ran_to(0x16C66); look_at(o, 1); jumped_to(0x16C66); ran_to(0x16C68); }
}

/* sub_179B0: guard d0's state cleared: what he carries out of play, his script's place
 * back to its start, his counters, waits and marks cleared */
int logic_guard_clear(void)
{
    if (!start()) return 0;
    begin(0x179B0);
    int16_t o = (int16_t)lo(REG_D[0]);
    REG_A[6] = 0xFF8D78;
    set_lo(7, ram16(0xFF8D78 + o));
    if (!lo(REG_D[7])) branched(0x179BA, 0x179CC);
    else {
        set16(0xFF8D78 + o, 0);
        REG_A[6] = OBJ_ACTIVE;
        set16(OBJ_ACTIVE + (int16_t)lo(REG_D[7]), 0);
        ran_to(0x179CC);
    }
    REG_A[5] = 0xFF7560;
    set16(0xFF7560 + o, ram16(0xFF7514 + o));
    static const uint32_t cleared[] = {
        0xFF931C, 0xFF9498, 0xFF9368, 0xFF9744, 0xFF734C, 0xFF7398, 0xFF73E4, 0xFF7430,
        0xFF747C, 0xFF74C8,
    };
    for (unsigned i = 0; i < sizeof cleared / sizeof *cleared; i++) set16(cleared[i] + o, 0);
    REG_A[6] = 0xFF74C8;
    ran_to(0x17A42);
    return done();
}

/* sub_141F8: the stage's guards set up: their count (table 0x14216 by the stage entry),
 * then sub_147FA, which runs past a frame's end: the game's code calls it (its rts at
 * 0x14214 returns for this routine) */
int logic_guards_setup(void)
{
    if (!start()) return 0;
    begin(0x141F8);
    set_lo(0, (uint16_t)((ram16(STAGE_ENTRY) - 1) * 2));
    REG_A[0] = 0x14216;
    set16(GUARD_COUNT, mem16(0x14216 + (int16_t)lo(REG_D[0])));
    ran_to(0x14214);
    REG_A[7] -= 4; set32(REG_A[7], 0x14214);
    return jump_to(0x147FA);
}

/* sub_147FA (long: see begin_long): each of the stage's guards (objects 6 on, GUARD_COUNT
 * + 1 of them) set up from its kind (table 0x14AB8 by stage, then 0x1CFDE by kind): his
 * counters cleared; his script's places and his kind's values (size, speed, ...); where
 * he starts kept as his post; his work space ($40 bytes each from $7D88); the boxes he
 * looks through around him (by his size: up-left, down-left, up-right, down-right,
 * ahead); his turn in the frame (by his number, in 4) and his half (in 2) */
int logic_guards_place(void)
{
    begin_long(0x147FA);
    set_lo(0, (uint16_t)((ram16(STAGE_ENTRY) - 1) << 2));
    REG_A[0] = mem32(0x14AB8 + (int16_t)lo(REG_D[0]));
    REG_D[0] = 0xC;
    set_lo(1, ram16(GUARD_COUNT));
    static const uint32_t cleared[] = {
        0xFF6CC4, 0xFF6D10, 0xFF6D5C, 0xFF734C, 0xFF7398, 0xFF73E4, 0xFF7430, 0xFF9284,
        0xFF92D0, 0xFF931C, 0xFF8DC4,
    };
    static const uint32_t script[] = {     /* (longs) */
        0xFF6F24, 0xFF6FBC, 0xFF7054, 0xFF70EC, 0xFF7184, 0xFF721C, 0xFF72B4, 0xFF6DF4,
        0xFF8EA8, 0xFF990C,
    };
    for (;;) {
        uint16_t o = lo(REG_D[0]);
        REG_D[7] = 0;
        for (unsigned k = 0; k < sizeof cleared / sizeof *cleared; k++) set16(cleared[k] + o, 0);
        set_lo(7, (uint16_t)(mem16(REG_A[0]) << 2));
        REG_A[0] += 2;
        REG_A[6] = mem32(0x1CFDE + (int16_t)lo(REG_D[7]));
        uint16_t o2 = (uint16_t)(o * 2);
        for (unsigned k = 0; k < sizeof script / sizeof *script; k++) {
            set32(script[k] + o2, mem32(REG_A[6]));
            REG_A[6] += 4;
        }
        set_lo(7, mem16(REG_A[6])); REG_A[6] += 2;
        set16(0xFF7514 + o, lo(REG_D[7]));
        set16(0xFF7560 + o, lo(REG_D[7]));
        static const uint32_t values[] = { 0xFF75AC, 0xFF75F8, 0xFF406E, OBJ_SIZE_X, OBJ_SIZE_Y };
        for (unsigned k = 0; k < 5; k++) { set16(values[k] + o, mem16(REG_A[6])); REG_A[6] += 2; }
        set16(0xFF7644 + o, mem16(REG_A[6]));
        set16(0xFF91EC + o, mem16(REG_A[6])); REG_A[6] += 2;
        set16(0xFF7728 + o, mem16(REG_A[6])); REG_A[6] += 2;
        set_lo(7, o2);
        set32(0xFF7774 + o2, mem32(REG_A[6]));
        set32(0xFF8FD8 + o2, mem32(REG_A[6]));
        set16(0xFF9108 + o, ram16(OBJ_X + o));        /* (his post) */
        set16(0xFF9154 + o, ram16(OBJ_Y + o));
        set16(0xFF91A0 + o, ram16(OBJ_HEIGHT + o));
        set16(0xFF9238 + o, 1);
        REG_D[6] = (uint16_t)((uint16_t)(o + 2) << 6);
        REG_A[6] = 0xFF7D48 + (uint32_t)(int16_t)lo(REG_D[6]);
        set_lo(6, o2);
        set32(0xFF7CB0 + o2, REG_A[6]);
        set32(0xFF9070 + o2, REG_A[6]);
        uint16_t sx = ram16(OBJ_SIZE_X + o), sy = ram16(OBJ_SIZE_Y + o);
        static const struct { uint32_t box; int8_t fx, fy; } box[5] = {
            {0xFF411A, -1, -1}, {0xFF4272, -1, 1}, {0xFF43CA, 1, -1}, {0xFF4522, 1, 1}, {0xFF467A, 0, 1},
        };
        for (int k = 0; k < 5; k++) {
            uint16_t x = box[k].fx < 0 ? (uint16_t)-sx : box[k].fx > 0 ? sx : 0;
            uint16_t y = box[k].fy < 0 ? (uint16_t)-sy : sy;
            set16(box[k].box + o2, (uint16_t)(x - 0x20));
            set16(box[k].box + o2 + 2, (uint16_t)(y - 0x20));
            set_lo(7, (uint16_t)(y - 0x20));
        }
        REG_A[6] = 0xFF9400; REG_A[5] = OBJ_SIZE_Y; REG_A[4] = 0xFF467A;
        set16(0xFF93B4 + o, (uint16_t)((o - mem16(0x000C)) & 3));
        set_lo(7, o & 1);
        set16(0xFF9400 + o, lo(REG_D[7]));
        set_lo(0, (uint16_t)(o + 2));
        ran_to(0x14AB2);
        uint16_t c = lo(REG_D[1]);
        set_lo(1, (uint16_t)(c - 1));
        if (!c) { ran_to(0x14AB6); break; }
        branched(0x14AB2, 0x14814);
    }
    return done_long();
}
