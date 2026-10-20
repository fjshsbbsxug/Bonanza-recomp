/* Objects: taking them away, placing them. */
#include "rt.h"

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
