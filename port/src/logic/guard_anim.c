/* Guards turning and their animations. A guard turns from one facing to another
 * through the facings between (sequences in ROM, 0x174AC, picked from tables at 0x1742C
 * / 0x1746C by the facing it has and the one it wants); each facing lasts a few frames.
 * Its animation follows its facing (its own table of animations by facing, $8EA8). */
#include "rt.h"
#include "guard_anim.h"

/* sub_1767C: start guard o's animation (its number in OBJ_ANIM; steps from 0x17CDE) */
void guard_anim_start(uint16_t o)
{
    jumped_to(0x1767C);
    uint16_t anim = obj16(OBJ_ANIM, o);
    set_obj32(OBJ_ANIM_STEPS, o, rom32(0x17CDE + (int16_t)(uint16_t)(anim << 2)));
    set_obj16(OBJ_ANIM_AT, o, 0);
    set_obj16(OBJ_ANIM_TIME, o, 1);
    ran_to(0x176B8);
}

int logic_guard_anim_start(void)
{
    if (!start()) return 0;
    uint16_t o = lo(REG_D[0]);
    begin(0x1767C);
    guard_anim_start(o);
    cyc_back(16);                                     /* (its rts: the caller's) */
    set_lo(6, (uint16_t)(o * 2));                     /* (for the callers) */
    return done();
}

/* sub_15288: guard o's animation for the way it faces (and its extra word, unless
 * negative: $32FE), started */
void guard_face_anim(uint16_t o)
{
    jumped_to(0x15288);
    uint16_t facing = obj16(GUARD_FACING, o);
    uint32_t a = obj32(GUARD_FACING_ANIMS, o) + (int16_t)(uint16_t)(facing << 2);
    set_obj16(OBJ_ANIM, o, mem16(a));
    uint16_t extra = mem16(a + 2);
    if (extra & 0x8000) branched(0x152B6, 0x152BE);
    else { set_obj16(0xFF32FE, o, extra); ran_to(0x152BE); }
    ran_to(0x152C4);
    guard_anim_start(o);
    jumped_to(0x152C4);
    set_obj16(GUARD_FACING_SHOWN, o, facing);
    ran_to(0x152D8);
}

/* sub_173D6 / sub_173DC: guard o starts turning to facing `want`, each facing lasting
 * `frames`; 0xFFFF: it faces that way already */
uint16_t guard_turn_begin(uint16_t o, uint16_t want, uint16_t frames, int alt)
{
    uint32_t table = alt ? 0x1746C : 0x1742C;
    jumped_to(alt ? 0x173DC : 0x173D6);
    if (alt) ran_to(0x173E0);
    else { ran_to(0x173DA); branched(0x173DA, 0x173E0); }
    uint16_t facing = obj16(GUARD_FACING, o);
    if (facing == want) {
        ran_to(0x173F4 - 2 + 2);
        return 0xFFFF;
    }
    branched(0x173EC, 0x173F4);
    uint16_t seq = rom16(table + (int16_t)(uint16_t)(facing << 3) + (int16_t)(uint16_t)(want * 2));
    set_obj32(GUARD_TURN_SEQ, o, rom32(0x174AC + (int16_t)(uint16_t)(seq << 2)));
    set_obj16(GUARD_TURN_TIME, o, frames);
    set_obj16(GUARD_TURN_STEP, o, frames);
    ran_to(0x1742C);
    return (uint16_t)(want * 2);
}

/* sub_1763C: a frame of guard o's turning: its next facing when this one's time is up;
 * 0xFFFF: done */
uint16_t guard_turn_step(uint16_t o)
{
    jumped_to(0x1763C);
    uint16_t next = 0;
    uint16_t t = (uint16_t)(obj16(GUARD_TURN_TIME, o) - 1);
    set_obj16(GUARD_TURN_TIME, o, t);
    if (t) {
        branched(0x17648, 0x1767A);
    } else {
        set_obj16(GUARD_TURN_TIME, o, obj16(GUARD_TURN_STEP, o));
        uint32_t seq = obj32(GUARD_TURN_SEQ, o);
        next = mem16(seq);
        if (next & 0x8000) branched(0x17668, 0x17676);
        else { set_obj16(GUARD_FACING, o, next); ran_to(0x17676); }
        set_obj32(GUARD_TURN_SEQ, o, seq + 2);
    }
    ran_to(0x1767C);
    return next;
}

int logic_guard_turn_begin(void)
{
    if (!start()) return 0;
    uint16_t o = lo(REG_D[0]);
    begin(0x173D6);
    uint16_t r = guard_turn_begin(o, lo(REG_D[2]), lo(REG_D[3]), 0);
    if (r == 0xFFFF) REG_D[2] = 0xFFFFFFFFu;          /* (moveq #-1) */
    else set_lo(2, r);
    cyc_back(16);
    return done();
}

int logic_guard_turn_begin2(void)
{
    if (!start()) return 0;
    uint16_t o = lo(REG_D[0]);
    begin(0x173DC);
    uint16_t r = guard_turn_begin(o, lo(REG_D[2]), lo(REG_D[3]), 1);
    if (r == 0xFFFF) REG_D[2] = 0xFFFFFFFFu;
    else set_lo(2, r);
    cyc_back(16);
    return done();
}

int logic_guard_turn_step(void)
{
    if (!start()) return 0;
    uint16_t o = lo(REG_D[0]);
    begin(0x1763C);
    uint16_t r = guard_turn_step(o);
    REG_D[2] = r;                                     /* (moveq #0, then the word read) */
    cyc_back(16);
    return done();
}
