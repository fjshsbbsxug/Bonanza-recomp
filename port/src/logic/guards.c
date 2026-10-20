/* Guards (objects 6..37): events, scripts, what they have seen. */
#include "rt.h"
#include "guard_ops.h"

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
    call(0x1A878, 0x145FA);                                  /* d6: bit 0 player 1, bit 1 player 2 */
    uint16_t touching = lo(REG_D[6]);
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
        call(0x16BE8, 0x143E2);
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
        c_op = op < 65 * 4 ? guard_op_going[op / 4] : 0;
        ran_to(0x147FA);
    } else {
        ran_to(0x144AE);                              /* (to the jmp) */
        goto fetch;
    }
    for (;;) {
        jumped_to(handler);
        if (c_op) {
            count_named(c_op == guard_op_start[op / 4] ? "guard op (start)" : "guard op (going on)");
            next = c_op(o, REG_A[0]);
        } else {
            count_named("guard op in 68000 code");
            next = run_part(handler, stops, 3);
        }
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
            c_op = op < 65 * 4 ? guard_op_start[op / 4] : 0;
            ran_to(0x147E6);
        }
    }
}

/* sub_14232: every guard's frame: guards out of view put back at their posts, events
 * (shot, touched, a player seen) start their scripts, scripts run, animations step,
 * guards fall, touch the players */
int logic_guards_update(void)
{
    if (!start()) return 0;
    begin(0x14232);
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
    return done();
}
