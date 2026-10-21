/* The players' states (tables 0xB492 / 0xD996: by state, its routine; a state's routine
 * may set the one run on the next frames, P1_STATE_RUN). Written once with player 1's
 * code and work memory; P(), PR() and PT() give player 2's (rt.h). */
#include "rt.h"

#define AT(a) P(p, a)
#define O ((uint16_t)(2 * p))
/* a routine called by the jsr at jsr (player 2's: what his copy's jsr calls; rt.h) */
#define CALL(a, jsr) player_call(p, a, jsr)

/* ---------------------------------------------------------------- small routines */

/* (the facing byte: 1 left, 2 right; 3A62 per player) */
static uint8_t facing(int p) { return ram8(PR(p, P1_FACING)); }

/* sub_099DE / sub_09A24: player p's standing pose: his standing animation for the way he
 * faces (1 left, 2 right) and its first picture; his size (6 wide, 14 / 10 high) and how
 * high his feet are looked for (16 / 12) */
static void stand_pose(int p)
{
    uint8_t f = facing(p);
    REG_D[7] = 1;
    if (f == 1) branched(AT(0x099E8), AT(0x099EE));
    else { REG_D[7] = 2; ran_to(AT(0x099EE)); }
    CALL(0x097F6, 0x099EE);
    REG_A[6] = obj32(OBJ_ANIM_STEPS, O);
    set_obj16(OBJ_FRAME, O, mem16(REG_A[6] + 2));
    set16(PR(p, P1_LIFT), p ? 0xC : 0x10);
    set_obj16(0xFF3706, O, 6);
    set_obj16(0xFF37B2, O, p ? 0xA : 0xE);
    ran_to(AT(0x09A1A));
}
static int player_stand_pose(int p)
{
    if (!start()) return 0;
    begin(AT(0x099DE));
    stand_pose(p);
    return done();
}
int logic_player_stand_pose_p1(void) { return player_stand_pose(0); }
int logic_player_stand_pose_p2(void) { return player_stand_pose(1); }

/* sub_099D6 / sub_09A1C: player p stops: state 0 (standing), and his standing pose */
static int player_stop(int p)
{
    if (!start()) return 0;
    begin(AT(0x099D6));
    REG_D[7] = 0;
    CALL(0x09846, 0x099D8);
    stand_pose(p);
    return done();
}
int logic_player_stop_p1(void) { return player_stop(0); }
int logic_player_stop_p2(void) { return player_stop(1); }

/* sub_09A62 / sub_09A8C: player p's little script starts (sub_09AB6, the same for both;
 * a6 kept), unless one runs */
static int player_script_start(int p)
{
    if (!start()) return 0;
    begin(AT(0x09A62));
    if (ram16(PR(p, P1_SCRIPT_ON))) { branched(AT(0x09A68), AT(0x09A8A)); return done(); }
    set16(PR(p, P1_SCRIPT_ON), 1);
    set32(PR(p, P1_SCRIPT), 0x09AB6);
    set16(PR(p, P1_SCRIPT_STEP), 0);
    set32(PR(p, P1_SCRIPT_AT), REG_A[6]);
    ran_to(AT(0x09A8A));
    return done();
}
int logic_player_script_start_p1(void) { return player_script_start(0); }
int logic_player_script_start_p2(void) { return player_script_start(1); }

/* sub_0AAE2 / sub_0AB24: can player p go through door d0? d7 = 1 if it is at his height
 * and closed, or open the way he goes (its animation, table 0xABEE by the side) */
static int player_door_ok(int p)
{
    if (!start()) return 0;
    begin(AT(0x0AAE2));
    uint16_t d = lo(REG_D[0]);
    REG_D[7] = 0;
    uint16_t h = obj16(OBJ_HEIGHT, d);
    set_lo(6, h);
    REG_A[6] = OBJ_HEIGHT;
    if (h != obj16(OBJ_HEIGHT, O)) { branched(AT(0x0AAF0), AT(0x0AB22)); return done(); }
    REG_A[6] = 0xFF9E9E;
    if (!obj16(0xFF9E9E, d)) { branched(AT(0x0AAFE), AT(0x0AB20)); REG_D[7] = 1; ran_to(AT(0x0AB22)); return done(); }
    uint16_t side = (uint16_t)(ram16(PR(p, P1_CAUGHT_SIDE)) * 2);
    uint16_t anim = rom16(0x0ABEE + (int16_t)side);
    set_lo(6, anim);
    REG_A[6] = OBJ_ANIM;
    if (anim != obj16(OBJ_ANIM, d)) { branched(AT(0x0AB1C), AT(0x0AB22)); return done(); }
    REG_D[7] = 1;
    ran_to(AT(0x0AB22));
    return done();
}
int logic_player_door_ok_p1(void) { return player_door_ok(0); }
int logic_player_door_ok_p2(void) { return player_door_ok(1); }

/* ---------------------------------------------------------------- state 0: standing */

/* sub_0B56A / sub_0DA6E: standing: the standing animation; nothing more each frame (the
 * frame's buttons change the state); player 1's size set again (6 by 14) */
static int state_stand(int p)
{
    if (!start()) return 0;
    begin(AT(0x0B56A));
    REG_D[7] = 1;
    if (facing(p) == 1) branched(AT(0x0B574), AT(0x0B57A));
    else { REG_D[7] = 2; ran_to(AT(0x0B57A)); }
    CALL(0x097F6, 0x0B57A);
    if (!p) {
        set_obj16(0xFF3706, O, 6);
        set_obj16(0xFF37B2, O, 0xE);
    }
    set32(PR(p, P1_STATE_RUN), AT(0x0B5A4));
    set32(PR(p, P1_STATE_RUN_BEFORE), AT(0x0B5A4));
    ran_to(AT(0x0B5A4));
    return done();
}
int logic_state_stand_p1(void) { return state_stand(0); }
int logic_state_stand_p2(void) { return state_stand(1); }

/* ---------------------------------------------------------------- state 1: walking */

/* the walk's frame (sub_0B5FC / sub_0DAF0, also run at once when he starts): he turns
 * to the way pressed (its speed, table 0xB848; its animation, table 0xB84C). Touching a
 * door on the side he goes: through it if it lets him (sub_0AAE2), else he stops (state
 * $8003). Else ahead of him (table 0xB858 by facing: two points) a wall stops him; at a
 * floor's edge the way he goes he falls ($8015; not on the way out before all the loot
 * is taken); else he moves on, and if none of his feet (sub_0B8BC, three points) is on
 * the floor, he stops and the view's script starts */
static void walk(int p)
{
    uint8_t want = (ram8(PR(p, P1_PAD)) & 4) ? 1 : 2;
    REG_D[7] = want;
    if (want == 1) { ran_to(AT(0x0B60C)); branched(AT(0x0B60C), AT(0x0B612)); }
    else { branched(AT(0x0B606), AT(0x0B610)); ran_to(AT(0x0B612)); }
    if (want != facing(p)) {
        set8(PR(p, P1_FACED), facing(p));
        set8(PR(p, P1_FACING), want);
        REG_D[7] = (uint16_t)((want - 1) * 2);
        REG_A[6] = PT(p, 0x0B848);
        set_obj16(OBJ_DX, O, rom16(REG_A[6] + REG_D[7]));
        ran_to(AT(0x0B63C));
    } else branched(AT(0x0B618), AT(0x0B63C));
    {
        uint16_t k = (uint16_t)((facing(p) - 1) * 2);
        REG_A[6] = PT(p, 0x0B84C);
        uint16_t anim = rom16(REG_A[6] + (int16_t)k);
        REG_D[7] = anim;
        if (anim == obj16(OBJ_ANIM, O)) branched(AT(0x0B656), AT(0x0B660));
        else CALL(0x097F6, 0x0B65A);
    }
    /* a door on the side he goes */
    if (!ram16(PR(p, P1_TOUCHING))) { branched(AT(0x0B666), AT(0x0B6A8)); goto ahead; }
    set_lo(0, ram16(PR(p, P1_CAUGHT_SIDE)));
    if ((uint8_t)lo(REG_D[0]) != facing(p)) { branched(AT(0x0B676), AT(0x0B6A8)); goto ahead; }
    {
        uint16_t door = ram16(PR(p, P1_DOOR));
        set_lo(0, door);
        if (!door) { branched(AT(0x0B680), AT(0x0B7E6)); return; }
        CALL(0x0AAE2, 0x0B684);
        if (!lo(REG_D[7])) { branched(AT(0x0B68C), AT(0x0B6A8)); goto ahead; }
        set_lo(7, door);
        REG_A[6] = 0xFF9E9E;
        if (obj16(0xFF9E9E, door)) { branched(AT(0x0B6A0), AT(0x0B7E6)); return; }
        branched(AT(0x0B6A4), AT(0x0B836));
        goto stop;
    }
ahead:
    {
        REG_D[0] = (uint16_t)((facing(p) - 1) << 3);
        uint32_t pts = PT(p, 0x0B858) + lo(REG_D[0]);
        set_lo(0, (uint16_t)(obj16(OBJ_X, O) + rom16(pts)));
        uint16_t dy = rom16(pts + 2);
        set_lo(3, dy);
        set_lo(1, (uint16_t)(obj16(OBJ_Y, O) + dy));
        REG_A[6] = pts + 4;
        int skip = 0;
        set_lo(7, ram16(PR(p, P1_CELL)) & 0x80);
        if (!lo(REG_D[7])) branched(AT(0x0B6D6), AT(0x0B6EE));
        else {
            uint16_t row = (uint16_t)(obj16(OBJ_Y, O) >> 3);
            set_lo(7, row);
            REG_A[5] = PT(p, 0x0B880);
            if (md.rom[REG_A[5] + (int16_t)row]) { branched(AT(0x0B6EA), AT(0x0B706)); skip = 1; }
            else ran_to(AT(0x0B6EE));
        }
        if (!skip) {
            uint32_t a6 = REG_A[6];
            CALL(0x1016A, 0x0B6F2);
            CALL(0x1019E, 0x0B6F8);
            REG_A[6] = a6;
            if (z_clear()) { branched(AT(0x0B702), AT(0x0B836)); goto stop; }
            ran_to(AT(0x0B706));
        }
        set_lo(0, (uint16_t)(obj16(OBJ_X, O) + rom16(REG_A[6])));
        REG_A[6] += 2;
        dy = rom16(REG_A[6]);
        set_lo(3, dy);
        set_lo(1, (uint16_t)(obj16(OBJ_Y, O) + dy));
        CALL(0x1016A, 0x0B718);
        uint32_t d3 = REG_D[3];
        CALL(0x1019E, 0x0B722);
        REG_D[3] = d3;
        if (z_clear()) { branched(AT(0x0B72C), AT(0x0B7E8)); goto edge; }
        uint16_t cell = lo(REG_D[2]);
        set_lo(7, cell & 0x80);
        if (!(cell & 0x80)) { branched(AT(0x0B736), AT(0x0B76E)); goto move; }
        if (!obj16(OBJ_HEIGHT, O)) { branched(AT(0x0B740), AT(0x0B76E)); goto move; }
        set_lo(7, cell & 0xF);
        if ((uint8_t)(cell & 0xF) != facing(p)) { branched(AT(0x0B750), AT(0x0B76E)); goto move; }
        {
            uint16_t lane = (uint16_t)(((uint16_t)(obj16(OBJ_Y, O) + lo(REG_D[3])) >> 3) * 2);
            REG_A[6] = ram32(MAP_LANES);
            uint8_t b = (uint8_t)RD8(REG_A[6] + 1 + (int16_t)lane);
            REG_D[7] = (REG_D[7] & 0xFFFF0000u) | (uint16_t)((lane & 0xFF00) | b);
            if (b) { branched(AT(0x0B76A), AT(0x0B80E)); goto out_way; }
            ran_to(AT(0x0B76E));
        }
    }
move:
    {
        uint16_t x = (uint16_t)(obj16(OBJ_X, O) + obj16(OBJ_DX, O));
        set_lo(7, x);
        set_obj16(OBJ_X, O, x);
        static const uint16_t jsr_at[3] = {0xB78C, 0xB7A8, 0xB7C4};
        for (int k = 0; k < 3; k++) {
            uint16_t fx = obj16(OBJ_X, O);
            if (k == 0) fx = (uint16_t)(fx - 0x20);
            else {
                REG_A[6] = PT(p, 0x0B858);
                fx = k == 1 ? (uint16_t)(fx + rom16(REG_A[6] + 4) + 2) : (uint16_t)(fx + rom16(REG_A[6] + 0xC) - 2);
            }
            set_lo(0, fx);
            REG_D[7] = 2;
            CALL(0x0B8BC, jsr_at[k]);
            if (z_clear()) { branched(AT(jsr_at[k] + 6), AT(0x0B7E6)); return; }
        }
        REG_A[6] = PT(p, 0x0B858);
        set16(PR(p, P1_LIFT), p ? 0xC : 0x10);
        CALL(0x09A62, 0x0B7DA);
        CALL(0x099D6, 0x0B7E0);
        ran_to(AT(0x0B7E6));
        return;
    }
edge:
    if (!obj16(OBJ_HEIGHT, O)) { branched(AT(0x0B7EE), AT(0x0B846)); return; }
    set_lo(7, lo(REG_D[2]) & 0xF0);
    if (lo(REG_D[7]) != 0x80) { branched(AT(0x0B7FC), AT(0x0B846)); return; }
    set_lo(2, lo(REG_D[2]) & 0xF);
    if ((uint8_t)lo(REG_D[2]) != facing(p)) { branched(AT(0x0B80A), AT(0x0B846)); return; }
    ran_to(AT(0x0B80E));
out_way:
    set_lo(7, ram16(PR(p, P1_CELL)) & 0xF000);
    if (lo(REG_D[7]) != 0xF000) branched(AT(0x0B81C), AT(0x0B82A));
    else if (!ram16(LOOT_ALL)) { branched(AT(0x0B826), AT(0x0B834)); return; }
    else ran_to(AT(0x0B82A));
    set_lo(7, 0x8015);
    CALL(0x09846, 0x0B82E);
    ran_to(AT(0x0B834));
    return;
stop:
    CALL(0x099DE, 0x0B836);
    set_lo(7, 0x8003);
    CALL(0x09846, 0x0B840);
    ran_to(AT(0x0B846));
}

static int state_walk_frame(int p)
{
    if (!start()) return 0;
    begin(AT(0x0B5FC));
    walk(p);
    return done();
}
int logic_state_walk_frame_p1(void) { return state_walk_frame(0); }
int logic_state_walk_frame_p2(void) { return state_walk_frame(1); }

/* sub_0B5A6 / sub_0DA9A: walking starts (state 1; also states $0C, $11, $13): while his
 * little script runs, he stops instead. His tiles ($54 / $55), his speed and animation
 * for the way he faces; the walk's frame from now on, and at once */
static int state_walk(int p)
{
    if (!start()) return 0;
    begin(AT(0x0B5A6));
    if (ram16(PR(p, P1_SCRIPT_ON))) {
        CALL(0x099D6, 0x0B5B0);
        ran_to(AT(0x0B5B6));
        return done();
    }
    branched(AT(0x0B5AC), AT(0x0B5B8));
    set_lo(7, p ? 0x55 : 0x54);
    CALL(0x01550, 0x0B5BC);
    REG_D[7] = (uint16_t)((facing(p) - 1) * 2);
    REG_A[6] = PT(p, 0x0B848);
    set_obj16(OBJ_DX, O, rom16(REG_A[6] + REG_D[7]));
    REG_A[6] = PT(p, 0x0B84C);
    set_lo(7, rom16(REG_A[6] + REG_D[7]));
    CALL(0x097F6, 0x0B5E2);
    set32(PR(p, P1_STATE_RUN), AT(0x0B5FC));
    set32(PR(p, P1_STATE_RUN_BEFORE), AT(0x0B5FC));
    ran_to(AT(0x0B5FC));
    walk(p);
    return done();
}
int logic_state_walk_p1(void) { return state_walk(0); }
int logic_state_walk_p2(void) { return state_walk(1); }

/* ---------------------------------------------------------------- waiting, feet */

/* sub_0B472 / sub_0D976: player p waits P1_WAIT frames (his state's routine, sub_0B47C,
 * counts them), then the routine at P1_WAIT_THEN runs his state */
static int player_wait(int p, int from_start)
{
    if (!start()) return 0;
    if (from_start) {
        begin(AT(0x0B472));
        set32(PR(p, P1_STATE_RUN), AT(0x0B47C));
        ran_to(AT(0x0B47C));
    } else begin(AT(0x0B47C));
    uint16_t n = (uint16_t)(ram16(PR(p, P1_WAIT)) - 1);
    set16(PR(p, P1_WAIT), n);
    if (n) { branched(AT(0x0B482), AT(0x0B490)); return done(); }
    set32(PR(p, P1_STATE_RUN), ram32(PR(p, P1_WAIT_THEN)));
    ran_to(AT(0x0B490));
    return done();
}
int logic_player_wait_p1(void) { return player_wait(0, 1); }
int logic_player_wait_p2(void) { return player_wait(1, 1); }
int logic_player_waiting_p1(void) { return player_wait(0, 0); }
int logic_player_waiting_p2(void) { return player_wait(1, 0); }

/* sub_0B8BC / sub_0DDB0: what is under player p's foot at x d0, d7 below his feet (the
 * map cell there, sub_1016A; its lane, by depth, from the lanes table): the walls of
 * that lane (low: d0; high, from 8 pixels up: d1) left with the cell's bits, the flags
 * set by that (nonzero: something there). On a cell of the floor's edge (bit 7) while
 * up, the lane's other byte, and d1 the cell's high byte. */
static int player_foot(int p)
{
    if (!start()) return 0;
    begin(AT(0x0B8BC));
    uint16_t y = (uint16_t)(obj16(OBJ_Y, O) - 0x20 + ram16(PR(p, P1_LIFT)) + lo(REG_D[7]));
    set_lo(1, y);
    set_lo(7, y);
    uint32_t d7 = REG_D[7];
    CALL(0x1016A, 0x0B8D4);
    REG_D[7] = d7;
    uint16_t lane = (uint16_t)((y >> 3) * 2);
    set_lo(7, lane);
    REG_A[6] = ram32(MAP_LANES);
    int up = obj16(OBJ_HEIGHT, O) >= 8;
    uint8_t b;
    if (RD8(REG_A[6] + 1 + (int16_t)lane)) {
        branched(AT(0x0B8EC), AT(0x0B908));
        uint16_t cell = lo(REG_D[2]);
        set_lo(3, cell & 0x80);
        if (!(cell & 0x80)) { branched(AT(0x0B90E), AT(0x0B8F0)); goto low; }
        if (!obj16(OBJ_HEIGHT, O)) { branched(AT(0x0B916), AT(0x0B8F0)); goto low; }
        set_lo(1, (uint16_t)(cell >> 8));
        b = (uint8_t)RD8(REG_A[6] + 1 + (int16_t)lane);
        REG_D[7] = (REG_D[7] & ~0xFFu) | b;
        if (!up) { branched(AT(0x0B928), AT(0x0B930)); goto in_d0; }
        ran_to(AT(0x0B92C));
        goto in_d1;
    }
    ran_to(AT(0x0B8F0));
low:
    b = (uint8_t)RD8(REG_A[6] + (int16_t)lane);
    REG_D[7] = (REG_D[7] & ~0xFFu) | b;
    if (!up) { branched(AT(0x0B8FC), AT(0x0B904)); goto in_d0; }
    ran_to(AT(0x0B900));
in_d1:
    {
        uint8_t r = (uint8_t)(REG_D[1] & b);
        REG_D[1] = (REG_D[1] & ~0xFFu) | r;
        FLAG_N = NFLAG_8(r); FLAG_Z = r; FLAG_V = VFLAG_CLEAR; FLAG_C = CFLAG_CLEAR;
        ran_to(pc_now() + 2);
        return done();
    }
in_d0:
    {
        uint8_t r = (uint8_t)(REG_D[0] & b);
        REG_D[0] = (REG_D[0] & ~0xFFu) | r;
        FLAG_N = NFLAG_8(r); FLAG_Z = r; FLAG_V = VFLAG_CLEAR; FLAG_C = CFLAG_CLEAR;
        ran_to(pc_now() + 2);
        return done();
    }
}
int logic_player_foot_p1(void) { return player_foot(0); }
int logic_player_foot_p2(void) { return player_foot(1); }
