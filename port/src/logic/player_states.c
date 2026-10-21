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
        set_obj16(OBJ_DX, O, rom16(REG_A[6] + (int16_t)lo(REG_D[7])));
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
        uint32_t pts = PT(p, 0x0B858) + (int16_t)lo(REG_D[0]);
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
    set_obj16(OBJ_DX, O, rom16(REG_A[6] + (int16_t)lo(REG_D[7])));
    REG_A[6] = PT(p, 0x0B84C);
    set_lo(7, rom16(REG_A[6] + (int16_t)lo(REG_D[7])));
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
/* ---------------------------------------------------------------- state 2: jumping */

#include "../video.h"

/* sub_0BEB2 / sub_0E334: player p's animation d7 (table 0x36E00 / 0x37720), if not the
 * one he has: its first picture, and where what he holds goes, at once (its step and
 * time stay) */
static int player_anim_set(int p)
{
    if (!start()) return 0;
    begin(AT(0x0BEB2));
    uint16_t anim = lo(REG_D[7]);
    if (anim == obj16(OBJ_ANIM, O)) { branched(AT(0x0BEB8), AT(0x0BF0E)); return done(); }
    set_obj16(OBJ_ANIM, O, anim);
    set_lo(7, (uint16_t)(anim << 2));
    uint32_t steps = rom32(PT(p, 0x36E00) + (int16_t)lo(REG_D[7]));
    set_obj32(OBJ_ANIM_STEPS, O, steps);
    REG_A[6] = steps;
    set_obj16(OBJ_FRAME, O, mem16(steps + 0xA));
    set16(PR(p, P1_HELD_DX), (uint16_t)(int8_t)RD8(steps + 0xE));
    REG_D[6] = (uint16_t)(int8_t)RD8(steps + 0xF);
    set16(PR(p, P1_HELD_DY), lo(REG_D[6]));
    set8(PR(p, P1_HELD_ACTIVE + 1), (uint8_t)RD8(steps + 0x10));
    set8(PR(p, P1_HELD_BIG + 1), (uint8_t)RD8(steps + 0x11));
    ran_to(AT(0x0BF0E));
    return done();
}
int logic_player_anim_set_p1(void) { return player_anim_set(0); }
int logic_player_anim_set_p2(void) { return player_anim_set(1); }

/* the path's next command (sub_0BA26 / sub_0DF10): table 0xBC4A by the word at P1_PATH
 * (0: a step, sub_0BC52; 1: another path, sub_0BE9A) */
static int path_next_cmd(int p)
{
    uint32_t a6 = ram32(PR(p, P1_PATH));
    uint16_t cmd = (uint16_t)(mem16(a6) << 2);
    REG_A[6] = a6 + 2;
    set_lo(7, cmd);
    REG_A[5] = rom32(PT(p, 0x0BC4A) + (int16_t)cmd);
    ran_to(AT(0x0BA3A) + 2);                          /* (its jmp (a5)) */
    return jump_to(REG_A[5]);
}
static int path_next(int p)
{
    if (!start()) return 0;
    begin(AT(0x0BA26));
    return path_next_cmd(p);
}
int logic_path_next_p1(void) { return path_next(0); }
int logic_path_next_p2(void) { return path_next(1); }

/* the jump's frame (sub_0B9C8 / sub_0DEB2): the buttons it takes (not up on the top
 * floor; not down on the ground floor, nor through a door he cannot go through: only
 * left and right then), then the path's next command */
static int jump_frame_go(int p)
{
    REG_D[7] = ram8(PR(p, P1_PAD)) & 0xF;
    if (obj16(OBJ_HEIGHT, O)) {
        branched(AT(0x0B9DA), AT(0x0BA10));
        if (obj16(OBJ_HEIGHT, O) == 0x10) { REG_D[7] &= ~1u; ran_to(AT(0x0BA20)); }
        else branched(AT(0x0BA18), AT(0x0BA20));
    } else {
        REG_D[7] &= ~2u;
        if (!ram16(PR(p, P1_TOUCHING))) branched(AT(0x0B9E8), AT(0x0BA20));
        else {
            uint16_t door = ram16(PR(p, P1_DOOR));
            set_lo(0, door);
            if (!door) branched(AT(0x0B9F2), AT(0x0BA20));
            else {
                uint32_t d7 = REG_D[7];
                CALL(0x0AB68, 0x0B9FA);
                uint16_t r = lo(REG_D[7]);
                REG_D[7] = d7;
                if (!r) branched(AT(0x0BA06), AT(0x0BA20));
                else { REG_D[7] &= 0xFFFFFF0Cu | ~0xFFu; ran_to(AT(0x0BA0E)); branched(AT(0x0BA0E), AT(0x0BA20)); }
            }
        }
    }
    set16(PR(p, P1_JUMP_PAD), lo(REG_D[7]));
    ran_to(AT(0x0BA26));
    return path_next_cmd(p);
}
static int jump_frame(int p)
{
    if (!start()) return 0;
    begin(AT(0x0B9C8));
    return jump_frame_go(p);
}
int logic_jump_frame_p1(void) { return jump_frame(0); }
int logic_jump_frame_p2(void) { return jump_frame(1); }

/* sub_0B970 / sub_0DE5A: after his crouch, the jump goes: its path (0xBF4E / 0xE3D0),
 * the path's commands and what follows each step (sub_0BA26, sub_0BA3C), his animation
 * from its start; the jump's frame from now on, and at once */
static int jump_go(int p)
{
    if (!start()) return 0;
    begin(AT(0x0B970));
    set32(PR(p, P1_PATH), p ? 0x0E3D0 : 0x0BF4E);
    set16(PR(p, 0xFF56A2), 0);
    set16(PR(p, P1_FALLING), 0);
    set16(PR(p, 0xFF56B0), 0);
    set16(PR(p, 0xFF56A6), 0);
    set32(PR(p, P1_PATH_NEXT), AT(0x0BA26));
    set32(PR(p, P1_PATH_AFTER), AT(0x0BA3C));
    set_obj16(OBJ_ANIM_AT, O, 0);
    set_obj16(OBJ_ANIM_TIME, O, 1);
    set32(PR(p, P1_STATE_RUN), AT(0x0B9C8));
    set32(PR(p, P1_STATE_RUN_BEFORE), AT(0x0B9C8));
    ran_to(AT(0x0B9C8));
    return jump_frame_go(p);
}
int logic_jump_go_p1(void) { return jump_go(0); }
int logic_jump_go_p2(void) { return jump_go(1); }

/* sub_0B934 / sub_0DE28: a jump starts (state 2): his standing pose, the jump's tiles
 * (player 1's: $A, $B, $59; player 2's: $28, $29), a crouch of 9 frames (sub_0B472),
 * then sub_0B970 */
static int state_jump(int p)
{
    if (!start()) return 0;
    begin(AT(0x0B934));
    CALL(0x099DE, 0x0B934);
    if (!p) {
        set_lo(7, 0xA);  CALL(0x01550, 0x0B93E);
        set_lo(7, 0xB);  CALL(0x01550, 0x0B948);
        set_lo(7, 0x59); CALL(0x01550, 0x0B952);
    } else {
        set_lo(7, 0x28); CALL(0x01550, 0x0B93E);
        set_lo(7, 0x29); CALL(0x01550, 0x0B948);
    }
    set16(PR(p, P1_WAIT), 9);
    set32(PR(p, P1_WAIT_THEN), AT(0x0B970));
    ran_to(AT(0x0B96A));
    return player_jump(p, 0x0B472, 0x0B96A);
}
int logic_state_jump_p1(void) { return state_jump(0); }
int logic_state_jump_p2(void) { return state_jump(1); }

/* a step of a path (sub_0BC52 / sub_0E13C; a6: the path): dy. Going up: his head (three
 * points: a wall above, sub_1019E) stops the rise, and he falls from there on (path
 * 0xBF86 / 0xE408). Falling: his feet (sub_0B8BC, three points) land him: on stairs,
 * state $801A; else he stands (from a jump: the view's little script) and player 1's
 * jump tiles are put back. Then y += dy, and what follows the step. */
static int path_step(int p)
{
    if (!start()) return 0;
    begin(AT(0x0BC52));
    uint32_t a6 = REG_A[6];
    set_lo(7, mem16(a6));
    a6 += 2;
    REG_A[6] = a6;
    set32(PR(p, P1_PATH), a6);
    uint16_t state = ram16(PR(p, P1_STATE));
    if (ram16(PR(p, P1_FALLING))) { branched(AT(0x0BC60), AT(0x0BD38)); goto falling; }
    {
        set_lo(0, (uint16_t)(obj16(OBJ_X, O) - 0x20));
        uint16_t d3 = 0xFFD8;
        if (obj16(OBJ_HEIGHT, O) == 0x10) { d3 = 0xFFD0; ran_to(AT(0x0BC88)); }
        else branched(AT(0x0BC80), AT(0x0BC88));
        set_lo(1, (uint16_t)(obj16(OBJ_Y, O) + d3 + lo(REG_D[7])));
        d3 = (uint16_t)(d3 + lo(REG_D[7]));
        set_lo(3, d3);
        uint32_t s3 = REG_D[3], s7 = REG_D[7];
        CALL(0x1016A, 0x0BC92);
        CALL(0x1019E, 0x0BC98);
        REG_D[3] = s3; REG_D[7] = s7;
        if (z_clear()) { branched(AT(0x0BCA2), AT(0x0BD1E)); goto head; }
        uint16_t dx = state == 0x8002 ? 0xFFD8 : 0xFFD9;
        if (state == 0x8002) branched(AT(0x0BCB8), AT(0x0BCC0)); else ran_to(AT(0x0BCC0));
        set_lo(0, (uint16_t)(obj16(OBJ_X, O) + dx));
        set_lo(1, (uint16_t)(obj16(OBJ_Y, O) + lo(REG_D[3])));
        CALL(0x1016A, 0x0BCCE);
        CALL(0x1019E, 0x0BCD4);
        REG_D[3] = s3; REG_D[7] = s7;
        if (z_clear()) { branched(AT(0x0BCDE), AT(0x0BD1E)); goto head; }
        dx = state == 0x8002 ? 0xFFE7 : 0xFFE6;
        if (state == 0x8002) branched(AT(0x0BCF4), AT(0x0BCFC)); else ran_to(AT(0x0BCFC));
        set_lo(0, (uint16_t)(obj16(OBJ_X, O) + dx));
        set_lo(1, (uint16_t)(obj16(OBJ_Y, O) + lo(REG_D[3])));
        CALL(0x1016A, 0x0BD0A);
        CALL(0x1019E, 0x0BD10);
        REG_D[7] = s7;
        if (!z_clear()) { branched(AT(0x0BD1A), AT(0x0BD98)); goto move; }
        ran_to(AT(0x0BD1E));
    }
head:
    set32(PR(p, P1_PATH), p ? 0x0E408 : 0x0BF86);
    set16(PR(p, P1_FALLING), 1);
    REG_A[6] = ram32(PR(p, P1_PATH_AFTER));
    ran_to(AT(0x0BD36) + 2);
    return jump_to(REG_A[6]);
falling:
    {
        static const uint16_t jsr_at[3] = {0xBD46, 0xBD68, 0xBD8A};
        uint32_t s7 = REG_D[7];
        for (int k = 0; k < 3; k++) {
            uint16_t fx = obj16(OBJ_X, O);
            if (k == 0) fx = (uint16_t)(fx - 0x20);
            else {
                REG_A[6] = PT(p, 0x0B858);
                fx = k == 1 ? (uint16_t)(fx + rom16(REG_A[6] + 4) + 2) : (uint16_t)(fx + rom16(REG_A[6] + 0xC) - 2);
            }
            set_lo(0, fx);
            CALL(0x0B8BC, jsr_at[k]);
            REG_D[7] = s7;
            if (z_clear()) { branched(AT(jsr_at[k] + 0xA), AT(0x0BDAC)); goto land; }
        }
        ran_to(AT(0x0BD98));
        goto move;
    }
land:
    CALL(0x0BF10, 0x0BDAC);
    if (obj16(OBJ_HEIGHT, O) && obj16(OBJ_HEIGHT, O) != 0x10) {   /* on stairs */
        ran_to(AT(0x0BDC8));
        uint32_t s7 = REG_D[7];
        set_lo(7, 0x801A);
        CALL(0x09846, 0x0BDD0);
        REG_D[7] = s7;
        set8(PR(p, P1_FACED + 1), ram8(PR(p, P1_ON_STAIRS)));
        set8(PR(p, P1_ON_STAIRS), ram8(PR(p, P1_STAIRS_WAY)));
        set8(PR(p, P1_STAIRS_WAY), 0);
        branched(AT(0x0BDF4), AT(0x0BD98));
        goto move;
    }
    if (!obj16(OBJ_HEIGHT, O)) branched(AT(0x0BDB8), AT(0x0BDF6));
    else branched(AT(0x0BDC4), AT(0x0BDF6));
    {
        uint32_t s7 = REG_D[7];
        if (ram16(PR(p, P1_STATE)) == 0x8002) {
            REG_A[6] = PT(p, 0x0B858);
            set16(PR(p, P1_LIFT), p ? 0xC : 0x10);
            CALL(0x09A62, 0x0BE12);
        } else branched(AT(0x0BE02), AT(0x0BE18));
        CALL(0x099D6, 0x0BE18);
        set8(PR(p, P1_STAIRS_WAY), 0);
        set8(PR(p, P1_ON_STAIRS), 0);
        if (!p) {                                     /* player 1's jump tiles put back */
            for (int k = 0; k < 4; k++) {
                REG_A[6] = 0x5A480 + 0x20 * k;
                video_at_long(0x64800000u + 0x200000u * k);
                REG_D[7] = (REG_D[7] & 0xFFFF0000u);
                call(0x0119C, 0x0BE3E + 0x1A * k);
            }
        }
        REG_D[7] = s7;
        ran_to(AT(0x0BE92) + 4);
        branched(AT(0x0BE96), AT(0x0BD98));
    }
move:
    {
        uint16_t y = (uint16_t)(obj16(OBJ_Y, O) + lo(REG_D[7]));
        set_lo(7, y);
        set_obj16(OBJ_Y, O, y);
        REG_A[6] = ram32(PR(p, P1_PATH_AFTER));
        ran_to(AT(0x0BDAA) + 2);
        return jump_to(REG_A[6]);
    }
}
int logic_path_step_p1(void) { return path_step(0); }
int logic_path_step_p2(void) { return path_step(1); }

/* another path (sub_0BE9A / sub_0E31C; a6: the command's words): whether he is falling,
 * and the path; then the path's next command */
static int path_switch(int p)
{
    if (!start()) return 0;
    begin(AT(0x0BE9A));
    uint32_t a6 = REG_A[6];
    set_lo(7, mem16(a6));
    set16(PR(p, P1_FALLING), lo(REG_D[7]));
    REG_A[5] = mem32(a6 + 2);
    REG_A[6] = a6 + 6;
    set32(PR(p, P1_PATH), REG_A[5]);
    REG_A[6] = ram32(PR(p, P1_PATH_NEXT));
    ran_to(AT(0x0BEB0) + 2);
    return jump_to(REG_A[6]);
}
int logic_path_switch_p1(void) { return path_switch(0); }
int logic_path_switch_p2(void) { return path_switch(1); }

/* after each step of the jump (sub_0BA3C / sub_0DF26): jumped onto stairs, he goes up or
 * down them while the way matches; else, rising, up or down pressed takes him onto
 * stairs there (sub_0D7E4 says whether he faces any: else his jumping picture); left or
 * right moves him (unless a wall, sub_0B0B8) and turns him; his animation follows (on
 * stairs: by the buttons; else 9 / $A by the way he faces); in those, B fires */
static void jump_stairs_anim(int p, uint32_t jsr)
{
    REG_D[7] = ram8(PR(p, P1_JUMP_PAD + 1));
    set_lo(7, rom16(PT(p, 0x0BF38) + (int16_t)(uint16_t)(lo(REG_D[7]) * 2)));
    CALL(0x0BEB2, jsr);
}
static int jump_after(int p)
{
    if (!start()) return 0;
    begin(AT(0x0BA3C));
    uint8_t way = ram8(PR(p, P1_STAIRS_WAY));
    if (way) {
        REG_D[7] = (REG_D[7] & ~0xFFu) | ram8(PR(p, P1_ON_STAIRS));
        if ((uint8_t)REG_D[7] != way) { branched(AT(0x0BA52), AT(0x0BC14)); goto fire; }
        if (way == 1) {
            if (obj16(OBJ_HEIGHT, O)) { branched(AT(0x0BA68), AT(0x0BB20)); goto down; }
            ran_to(AT(0x0BA6C));
            branched(AT(0x0BA6C), AT(0x0BA7A));
        } else {
            branched(AT(0x0BA5E), AT(0x0BA6E));
            if (obj16(OBJ_HEIGHT, O) != 0x10) { branched(AT(0x0BA76), AT(0x0BADE)); goto up; }
            ran_to(AT(0x0BA7A));
        }
        set8(PR(p, P1_ON_STAIRS), 0);
        branched(AT(0x0BA80), AT(0x0BC14));
        goto fire;
    }
    branched(AT(0x0BA42), AT(0x0BA84));
    if (ram16(PR(p, P1_FALLING))) { branched(AT(0x0BA8A), AT(0x0BB42)); goto side; }
    CALL(0x0D7E4, 0x0BA8E);
    if (z_clear()) {
        branched(AT(0x0BA94), AT(0x0BB28));
        set_lo(7, facing(p) == 1 ? 9 : 0xA);
        if (facing(p) == 1) branched(AT(0x0BB34), AT(0x0BB3C)); else ran_to(AT(0x0BB3C));
        CALL(0x0BEB2, 0x0BB3C);
        ran_to(AT(0x0BB42));
        goto side;
    }
    REG_D[7] = ram8(PR(p, P1_JUMP_PAD + 1));
    if (!REG_D[7]) { branched(AT(0x0BAA0), AT(0x0BB92)); goto anim; }
    if (REG_D[7] & 1) {                               /* up: onto stairs going up */
        REG_D[7] = 2;
        set8(PR(p, P1_FACED + 1), ram8(PR(p, P1_ON_STAIRS)));
        set8(PR(p, P1_ON_STAIRS), 2);
        set8(PR(p, P1_STAIRS_WAY), 2);
        jump_stairs_anim(p, 0x0BAD8);
        ran_to(AT(0x0BADE));
up:
        set_obj16(OBJ_HEIGHT, O, (uint16_t)(obj16(OBJ_HEIGHT, O) + 1));
        ran_to(AT(0x0BAE4));
        branched(AT(0x0BAE4), AT(0x0BB42));
        goto side;
    }
    branched(AT(0x0BAA8), AT(0x0BAE6));
    if (!(REG_D[7] & 2)) { branched(AT(0x0BAEA), AT(0x0BB42)); goto side; }
    REG_D[7] = 1;                                     /* down: onto stairs going down */
    set8(PR(p, P1_FACED + 1), ram8(PR(p, P1_ON_STAIRS)));
    set8(PR(p, P1_ON_STAIRS), 1);
    set8(PR(p, P1_STAIRS_WAY), 1);
    jump_stairs_anim(p, 0x0BB1A);
    ran_to(AT(0x0BB20));
down:
    set_obj16(OBJ_HEIGHT, O, (uint16_t)(obj16(OBJ_HEIGHT, O) - 1));
    ran_to(AT(0x0BB26));
    branched(AT(0x0BB26), AT(0x0BB42));
side:
    REG_D[7] = ram8(PR(p, P1_JUMP_PAD + 1));
    if (!REG_D[7]) { branched(AT(0x0BB4A), AT(0x0BB92)); goto anim; }
    REG_D[7] &= 0xC;
    if (!REG_D[7]) { branched(AT(0x0BB52), AT(0x0BC14)); goto fire; }
    CALL(0x0B0B8, 0x0BB56);
    if (z_clear()) { branched(AT(0x0BB5C), AT(0x0BB92)); goto anim; }
    {
        uint8_t w = (uint8_t)(ram8(PR(p, P1_JUMP_PAD + 1)) >> 2);
        set8(PR(p, P1_FACED), facing(p));
        set8(PR(p, P1_FACING), w);
        REG_D[7] = (uint16_t)((uint8_t)(w - 1) * 2);
        REG_A[6] = PT(p, 0x0B848);
        uint16_t x = (uint16_t)(obj16(OBJ_X, O) + rom16(REG_A[6] + (int16_t)lo(REG_D[7])));
        set_lo(6, x);
        set_obj16(OBJ_X, O, x);
        ran_to(AT(0x0BB92));
    }
anim:
    if (!ram8(PR(p, P1_STAIRS_WAY))) {
        branched(AT(0x0BB98), AT(0x0BBFA));
        set_lo(7, facing(p) == 1 ? 9 : 0xA);
        if (facing(p) == 1) branched(AT(0x0BC06), AT(0x0BC0E)); else ran_to(AT(0x0BC0E));
        CALL(0x0BEB2, 0x0BC0E);
        ran_to(AT(0x0BC14));
        goto fire;
    }
    {
        uint16_t a = obj16(OBJ_ANIM, O);
        static const uint16_t at[4] = {0xBBA4, 0xBBB0, 0xBBBC, 0xBBC8};
        int k;
        for (k = 0; k < 4 && a != 0xB + k; k++) ;
        if (k == 4) { branched(AT(0x0BBC8), AT(0x0BC14)); goto fire; }
        if (k < 3) branched(AT(at[k]), AT(0x0BBCC)); else ran_to(AT(0x0BBCC));
        REG_D[7] = ram8(PR(p, P1_JUMP_PAD + 1)) & 0xC;
        if (!REG_D[7]) { branched(AT(0x0BBD8), AT(0x0BC14)); goto fire; }
        jump_stairs_anim(p, 0x0BBF0);
        branched(AT(0x0BBF6), AT(0x0BC14));
    }
fire:
    {
        uint16_t a = obj16(OBJ_ANIM, O);
        if (a == 9) branched(AT(0x0BC1C), AT(0x0BC2C));
        else if (a != 0xA) { branched(AT(0x0BC28), AT(0x0BC48)); return done(); }
        else ran_to(AT(0x0BC2C));
        uint8_t pressed = ram8(PR(p, P1_PAD_PRESSED));
        REG_D[7] = (REG_D[7] & ~0xFFu) | pressed;
        if (!(pressed & 0x10)) { branched(AT(0x0BC36), AT(0x0BC48)); return done(); }
        set_lo(0, O);
        set_lo(7, (uint16_t)(8 + 4 * p));
        CALL(0x18736, 0x0BC42);
        ran_to(AT(0x0BC48));
        return done();
    }
}
int logic_jump_after_p1(void) { return jump_after(0); }
int logic_jump_after_p2(void) { return jump_after(1); }

/* ---------------------------------------------------------------- state 3: against a wall */

/* (an animation by the way he faces: left / right) */
static void anim_by_facing(int p, uint16_t left, uint16_t right, uint32_t beq, uint32_t jsr)
{
    set_lo(7, facing(p) == 1 ? left : right);
    if (facing(p) == 1) branched(AT(beq), AT(jsr)); else ran_to(AT(jsr));
    CALL(0x097F6, jsr);
}

/* the frame against a wall (sub_0C01A / sub_0E49C), by P1_STEP: on the ground at a
 * door: an open one, he stops; else B opens it ($800F). 0: when his animation ends, up or
 * down (as sub_0AFE8 lets him) take the stairs ($8019); a way pressed other than his
 * turns him away (step 3); A jumps; at a moment of the animation, sub_013E00 and step 1.
 * 1: turning away and jumping as in 0; else (if $6CAE lets him) he knocks (step 2).
 * 2: at the knock's end, a sound, $6CAE = 2, and back to the animation's third step
 * (step 0). 3: at the turn's end he stops. */
static int against_frame_go(int p)
{
    if (!obj16(OBJ_HEIGHT, O)) {
        uint16_t door = ram16(PR(p, P1_DOOR));
        set_lo(7, door);
        if (door) {
            REG_A[6] = 0xFF9E9E;
            if (obj16(0xFF9E9E, door)) { branched(AT(0x0C038), AT(0x0C1F4)); goto stop; }
            uint8_t b = ram8(PR(p, P1_PAD_PRESSED));
            REG_D[7] = (REG_D[7] & ~0xFFu) | b;
            if (b & 0x10) {
                set_lo(7, 0x800F);
                CALL(0x09846, 0x0C04E);
                branched(AT(0x0C054), AT(0x0C1FA));
                return done();
            }
            branched(AT(0x0C046), AT(0x0C058));
        } else branched(AT(0x0C02A), AT(0x0C058));
    } else branched(AT(0x0C020), AT(0x0C058));
    uint16_t step = ram16(PR(p, P1_STEP));
    set_lo(7, step);
    if (step) { branched(AT(0x0C05E), AT(0x0C114)); goto step1; }
    set_lo(7, obj16(OBJ_ANIM_TIME, O));
    if ((int16_t)lo(REG_D[7]) >= 0) { branched(AT(0x0C068), AT(0x0C1FA)); return done(); }
    set_lo(6, obj16(OBJ_HEIGHT, O) ? 2 : 1);
    if (obj16(OBJ_HEIGHT, O)) ran_to(AT(0x0C07E)); else branched(AT(0x0C076), AT(0x0C07E));
    REG_D[7] = (REG_D[7] & ~0xFFu) | ram8(PR(p, P1_PAD));
    set_lo(7, lo(REG_D[7]) & lo(REG_D[6]));
    if (lo(REG_D[7])) {
        CALL(0x0AFE8, 0x0C08A);
        if (!z_clear()) {
            set_lo(7, 0x8019);
            CALL(0x09846, 0x0C098);
            branched(AT(0x0C09E), AT(0x0C1FA));
            return done();
        }
        branched(AT(0x0C090), AT(0x0C0A2));
    } else branched(AT(0x0C086), AT(0x0C0A2));
    set_lo(7, (uint16_t)(((REG_D[7] & ~0xFFu) | ram8(PR(p, P1_PAD))) & 0xC) >> 2);
    if ((uint8_t)lo(REG_D[7]) != facing(p)) { branched(AT(0x0C0B4), AT(0x0C0D4)); goto turn; }
    {
        uint8_t b = ram8(PR(p, P1_PAD_PRESSED));
        REG_D[7] = (REG_D[7] & ~0xFFu) | b;
        if (b & 0x40) {
            set_lo(7, 0x8002);
            CALL(0x09846, 0x0C0CA);
            branched(AT(0x0C0D0), AT(0x0C1FA));
            return done();
        }
        branched(AT(0x0C0C2), AT(0x0C0F8));
    }
    if (obj16(OBJ_ANIM_TIME, O) != 0xFF87) { branched(AT(0x0C100), AT(0x0C1FA)); return done(); }
    CALL(0x13E00, 0x0C104);
    set16(PR(p, P1_STEP), 1);
    ran_to(AT(0x0C112));
    return done();
turn:
    anim_by_facing(p, 0x14, 0x17, 0x0C0E0, 0x0C0E8);
    set16(PR(p, P1_STEP), 3);
    ran_to(AT(0x0C0F6));
    return done();
step1:
    if (step == 1) {
        set_lo(7, (uint16_t)(((REG_D[7] & ~0xFFu) | ram8(PR(p, P1_PAD))) & 0xC) >> 2);
        if ((uint8_t)lo(REG_D[7]) != facing(p)) { branched(AT(0x0C132), AT(0x0C0D4)); goto turn; }
        uint8_t b = ram8(PR(p, P1_PAD_PRESSED));
        REG_D[7] = (REG_D[7] & ~0xFFu) | b;
        if (b & 0x40) {
            set_lo(7, 0x8002);
            CALL(0x09846, 0x0C146);
            branched(AT(0x0C14C), AT(0x0C1FA));
            return done();
        }
        branched(AT(0x0C13E), AT(0x0C150));
        if (!ram16(PR(p, P1_KNOCKED))) { branched(AT(0x0C156), AT(0x0C1FA)); return done(); }
        anim_by_facing(p, 0x15, 0x18, 0x0C166, 0x0C16E);
        set16(PR(p, P1_STEP), 2);
        ran_to(AT(0x0C17C));
        return done();
    }
    branched(AT(0x0C11C), AT(0x0C17E));
    if (step == 2) {
        set_lo(7, obj16(OBJ_ANIM_TIME, O));
        if ((int16_t)lo(REG_D[7]) >= 0) { branched(AT(0x0C190), AT(0x0C1FA)); return done(); }
        set_lo(0, 0xA7);
        CALL(0x02D54, 0x0C198);
        set16(PR(p, P1_KNOCKED), 2);
        anim_by_facing(p, 0x13, 0x16, 0x0C1B2, 0x0C1BA);
        REG_D[7] = 2;
        set_obj16(OBJ_ANIM_AT, O, 2);
        set_obj16(OBJ_ANIM_TIME, O, 0xFFFF);
        REG_D[7] = 2 << 3;
        REG_A[6] = obj32(OBJ_ANIM_STEPS, O);
        set_lo(6, mem16(REG_A[6] + 2 + 16));
        set_obj16(OBJ_FRAME, O, lo(REG_D[6]));
        set16(PR(p, P1_STEP), 0);
        ran_to(AT(0x0C1E8));
        return done();
    }
    branched(AT(0x0C186), AT(0x0C1EA));
    set_lo(7, obj16(OBJ_ANIM_TIME, O));
    if ((int16_t)lo(REG_D[7]) >= 0) { branched(AT(0x0C1F0), AT(0x0C1FA)); return done(); }
    ran_to(AT(0x0C1F4));
stop:
    CALL(0x099D6, 0x0C1F4);
    ran_to(AT(0x0C1FA));
    return done();
}
static int against_frame(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C01A));
    return against_frame_go(p);
}
int logic_against_frame_p1(void) { return against_frame(0); }
int logic_against_frame_p2(void) { return against_frame(1); }

/* sub_0BFE6 / sub_0E468: after his stop, he stands against the wall (animation $13 /
 * $16), step 0; its frame from now on, and at once */
static int against_go(int p)
{
    if (!start()) return 0;
    begin(AT(0x0BFE6));
    anim_by_facing(p, 0x13, 0x16, 0x0BFF2, 0x0BFFA);
    set16(PR(p, P1_STEP), 0);
    set32(PR(p, P1_STATE_RUN), AT(0x0C01A));
    set32(PR(p, P1_STATE_RUN_BEFORE), AT(0x0C01A));
    ran_to(AT(0x0C01A));
    return against_frame_go(p);
}
int logic_against_go_p1(void) { return against_go(0); }
int logic_against_go_p2(void) { return against_go(1); }

/* sub_0BFBE / sub_0E440: state 3, stopped by a wall or a door: his standing pose, tiles
 * $C / $2A, a wait of 4 / 7 frames, then sub_0BFE6 */
static int state_against(int p)
{
    if (!start()) return 0;
    begin(AT(0x0BFBE));
    CALL(0x099DE, 0x0BFBE);
    set_lo(7, p ? 0x2A : 0xC);
    CALL(0x01550, 0x0BFC8);
    set16(PR(p, P1_WAIT), p ? 7 : 4);
    set32(PR(p, P1_WAIT_THEN), AT(0x0BFE6));
    ran_to(AT(0x0BFE0));
    return player_jump(p, 0x0B472, 0x0BFE0);
}
int logic_state_against_p1(void) { return state_against(0); }
int logic_state_against_p2(void) { return state_against(1); }

/* ---------------------------------------------------------------- state 4: firing */

/* sub_0C1FE / sub_0E67E: state 4, firing (the frame's B made the shot): tiles 9 / $27, a
 * wait of 2 frames; then (sub_0C220) the firing animation ($F / $10), and when it ends
 * (sub_0C24E) he stops */
static int fire_frame_go(int p)
{
    set_lo(7, obj16(OBJ_ANIM_TIME, O));
    if ((int16_t)lo(REG_D[7]) >= 0) { branched(AT(0x0C254), AT(0x0C25E)); return done(); }
    CALL(0x099D6, 0x0C258);
    ran_to(AT(0x0C25E));
    return done();
}
static int fire_frame(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C24E));
    return fire_frame_go(p);
}
int logic_fire_frame_p1(void) { return fire_frame(0); }
int logic_fire_frame_p2(void) { return fire_frame(1); }

static int fire_go(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C220));
    anim_by_facing(p, 0xF, 0x10, 0x0C22C, 0x0C234);
    set32(PR(p, P1_STATE_RUN), AT(0x0C24E));
    set32(PR(p, P1_STATE_RUN_BEFORE), AT(0x0C24E));
    ran_to(AT(0x0C24E));
    return fire_frame_go(p);
}
int logic_fire_go_p1(void) { return fire_go(0); }
int logic_fire_go_p2(void) { return fire_go(1); }

static int state_fire(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C1FE));
    set_lo(7, p ? 0x27 : 9);
    CALL(0x01550, 0x0C202);
    set16(PR(p, P1_WAIT), 2);
    set32(PR(p, P1_WAIT_THEN), AT(0x0C220));
    ran_to(AT(0x0C21A));
    return player_jump(p, 0x0B472, 0x0C21A);
}
int logic_state_fire_p1(void) { return state_fire(0); }
int logic_state_fire_p2(void) { return state_fire(1); }

/* a routine's address the game sets with the move.l #a at instr (player 2's own) */
#define CONT(a, instr) player_imm32(p, a, instr)

/* the state's routine from now on (both longs) */
static void run_from(int p, uint32_t r)
{
    set32(PR(p, P1_STATE_RUN), r);
    set32(PR(p, P1_STATE_RUN_BEFORE), r);
}

/* (a wait of n frames, then the routine set at instr; its jmp to sub_0B472 at jmp) */
static int wait_then(int p, uint16_t n, uint32_t then, uint32_t instr, uint32_t jmp)
{
    set16(PR(p, P1_WAIT), n);
    set32(PR(p, P1_WAIT_THEN), CONT(then, instr));
    ran_to(AT(jmp));
    return player_jump(p, 0x0B472, jmp);
}

/* ---------------------------------------------------------------- state 5: tripped */

/* sub_0C260 / sub_0E6E0: state 5, tripped (on a can): his standing pose, tiles $17 and $D
 * ($35, $2B), 8 frames, then (sub_0C292) he flies along a path (0xC380, by the way he
 * faces: 0xCC62) as his animation ($29 / $2B) runs, off any stairs */
static int state_trip(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C260));
    CALL(0x099DE, 0x0C260);
    set_lo(7, p ? 0x35 : 0x17); CALL(0x01550, 0x0C26A);
    set_lo(7, p ? 0x2B : 0xD);  CALL(0x01550, 0x0C274);
    return wait_then(p, 8, 0x0C292, 0x0C282, 0x0C28C);
}
int logic_state_trip_p1(void) { return state_trip(0); }
int logic_state_trip_p2(void) { return state_trip(1); }

/* the trip's frame (sub_0C2D4 / sub_0E754): along the path (sub_0A56A); its animation
 * goes on ($2A / $2C); at the path's end he lies (animation $2D / $2E; protected while he
 * lies, sub_09F0E, 40 frames), lower down (14 pixels; player 2's 6), a sound, sub_0AF7E;
 * then (sub_0C366) once his protection ends he gets up again (player 2: 2 pixels lower
 * still, as the game does) and stands (Insane mode's hook there: the generated code's) */
static int trip_frame_go(int p)
{
    if (!p) set16(0xFF56F0, 0);
    CALL(0x0A56A, 0x0C2DA);
    if (obj16(OBJ_DX, O) == 0x8000) { branched(AT(0x0C2E8), AT(0x0C312)); goto lies; }
    set_lo(7, obj16(OBJ_ANIM_TIME, O));
    if ((int16_t)lo(REG_D[7]) >= 0) { branched(AT(0x0C2F2), AT(0x0C310)); return done(); }
    anim_by_facing(p, 0x2A, 0x2C, 0x0C302, 0x0C30A);
    ran_to(AT(0x0C310));
    return done();
lies:
    anim_by_facing(p, 0x2D, 0x2E, 0x0C31E, 0x0C326);
    set_lo(7, 0x28);
    ran_to(AT(0x0C330));
    if (game_hooks && game_hook(AT(0x0C330)) == 2)    /* (Insane mode: his own wait, not that) */
        jumped_to(AT(0x0C336));
    else
        CALL(0x09F0E, 0x0C330);
    set_obj16(OBJ_Y, O, (uint16_t)(obj16(OBJ_Y, O) + (p ? 6 : 0xE)));
    set_lo(0, 0xA9);
    CALL(0x02D54, 0x0C342);
    REG_D[7] = p ? 4 : 2;
    CALL(0x0AF7E, 0x0C34A);
    run_from(p, CONT(0x0C366, 0x0C350));
    ran_to(AT(0x0C364));
    return done();
}
static int trip_frame(int p)
{
    if (!start()) return 0;
    begin(CONT(0x0C2D4, 0x0C2C0));
    return trip_frame_go(p);
}
int logic_trip_frame_p1(void) { return trip_frame(0); }
int logic_trip_frame_p2(void) { return trip_frame(1); }

static int trip_go(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C292));
    anim_by_facing(p, 0x29, 0x2B, 0x0C29E, 0x0C2A6);
    REG_A[6] = PT(p, 0x0C380);
    REG_A[5] = PT(p, 0x0CC62);
    CALL(0x0A51A, 0x0C2B4);
    set8(PR(p, P1_ON_STAIRS), 0);
    run_from(p, CONT(0x0C2D4, 0x0C2C0));
    ran_to(CONT(0x0C2D4, 0x0C2C0));
    return trip_frame_go(p);
}
int logic_trip_go_p1(void) { return trip_go(0); }
int logic_trip_go_p2(void) { return trip_go(1); }

static int trip_up(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C366));
    if (ram16(PR(p, P1_PROTECT))) { branched(AT(0x0C36C), AT(0x0C37E)); return done(); }
    set_obj16(OBJ_Y, O, (uint16_t)(obj16(OBJ_Y, O) + (p ? 2 : -0xE)));
    CALL(0x099D6, 0x0C378);
    ran_to(AT(0x0C37E));
    return done();
}
int logic_trip_up_p1(void) { return trip_up(0); }
int logic_trip_up_p2(void) { return trip_up(1); }

/* ---------------------------------------------------------------- state 6 */

/* sub_0C3BA / sub_0E830: state 6: tiles $21 / $3F, 2 frames, then (sub_0C3DC) animation
 * $2F and sub_0AF7E (2 / 4); nothing more each frame */
static int state_6_go(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C3DC));
    set_lo(7, 0x2F);
    CALL(0x097F6, 0x0C3E0);
    REG_D[7] = p ? 4 : 2;
    CALL(0x0AF7E, 0x0C3E8);
    run_from(p, CONT(0x0C402, 0x0C3EE));
    ran_to(AT(0x0C402));
    return done();
}
int logic_state_6_go_p1(void) { return state_6_go(0); }
int logic_state_6_go_p2(void) { return state_6_go(1); }

static int state_6(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C3BA));
    set_lo(7, p ? 0x3F : 0x21);
    CALL(0x01550, 0x0C3BE);
    return wait_then(p, 2, 0x0C3DC, 0x0C3CC, 0x0C3D6);
}
int logic_state_6_p1(void) { return state_6(0); }
int logic_state_6_p2(void) { return state_6(1); }

/* ---------------------------------------------------------------- state $E: through a door */

/* sub_0C404 / sub_0E87A: state $E, going through the door he touches: protected for good
 * (sub_09F0E, -1), the guards forget him (sub_09F42), he goes to the door's y (6 / 10
 * pixels lower), up on the top floor (16; his feet 16 / 12), tiles $13 / $31, 5 frames;
 * then (sub_0C45C) what he holds goes (sub_0AE8A), sub_0ABF6, animation $1E from its first
 * picture, a sound; then (sub_0C49E) while the door stays open he waits; once it shuts,
 * at his animation's end, sub_0AEC2 */
static int state_door(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C404));
    set_lo(7, 0xFFFF);
    CALL(0x09F0E, 0x0C408);
    CALL(0x09F42, 0x0C40E);
    uint16_t door = ram16(PR(p, P1_DOOR));
    set_lo(7, door);
    REG_A[6] = OBJ_Y;
    set_lo(6, (uint16_t)(obj16(OBJ_Y, door) + (p ? 0xA : 6)));
    set_obj16(OBJ_Y, O, lo(REG_D[6]));
    set_obj16(OBJ_HEIGHT, O, 0x10);
    set16(PR(p, P1_LIFT), p ? 0xC : 0x10);
    set_lo(7, p ? 0x31 : 0x13);
    CALL(0x01550, 0x0C43E);
    return wait_then(p, 5, 0x0C45C, 0x0C44C, 0x0C456);
}
int logic_state_door_p1(void) { return state_door(0); }
int logic_state_door_p2(void) { return state_door(1); }

static int door_wait_go(int p)
{
    uint16_t door = ram16(PR(p, P1_DOOR));
    set_lo(7, door);
    if (door) {
        REG_A[6] = 0xFF9E9E;
        if (obj16(0xFF9E9E, door)) { branched(AT(0x0C4B2), AT(0x0C4DE)); return done(); }
        ran_to(AT(0x0C4B6));
    } else branched(AT(0x0C4A4), AT(0x0C4B6));
    run_from(p, CONT(0x0C4D4, 0x0C4B6));
    set_obj16(OBJ_ANIM_TIME, O, 1);
    ran_to(AT(0x0C4D2));
    return done();
}
static int door_wait(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C49E));
    return door_wait_go(p);
}
int logic_door_wait_p1(void) { return door_wait(0); }
int logic_door_wait_p2(void) { return door_wait(1); }

static int door_go(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C45C));
    CALL(0x0AE8A, 0x0C45C);
    CALL(0x0ABF6, 0x0C462);
    set_lo(7, 0x1E);
    CALL(0x097F6, 0x0C46C);
    REG_A[6] = obj32(OBJ_ANIM_STEPS, O);
    set_obj16(OBJ_FRAME, O, mem16(REG_A[6] + 2));
    set_lo(0, 0xB6);
    CALL(0x02D54, 0x0C484);
    run_from(p, CONT(0x0C49E, 0x0C48A));
    ran_to(AT(0x0C49E));
    return door_wait_go(p);
}
int logic_door_go_p1(void) { return door_go(0); }
int logic_door_go_p2(void) { return door_go(1); }

static int door_end(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C4D4));
    set_lo(7, obj16(OBJ_ANIM_TIME, O));
    if ((int16_t)lo(REG_D[7]) >= 0) { ran_to(AT(0x0C4DE)); return done(); }
    branched(AT(0x0C4DA), AT(0x0C4E0));
    CALL(0x0AEC2, 0x0C4E0);
    ran_to(AT(0x0C4E6));
    return done();
}
int logic_door_end_p1(void) { return door_end(0); }
int logic_door_end_p2(void) { return door_end(1); }

/* ---------------------------------------------------------------- state 7: loot taken */

/* sub_0C4E8 / sub_0E960: state 7, he takes loot: what he holds up (kind $18 / $1C),
 * protected for 60 frames, tiles $1B / $39, 5 frames; then (sub_0C522) animation $1A /
 * $1B and a sound (sub_02D98, $E5); at its end (sub_0C55A) he stops */
static int state_loot(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C4E8));
    set_lo(0, O);
    set_lo(7, p ? 0x1C : 0x18);
    CALL(0x18736, 0x0C4F0);
    set_lo(7, 0x3C);
    CALL(0x09EDA, 0x0C4FA);
    set_lo(7, p ? 0x39 : 0x1B);
    CALL(0x01550, 0x0C504);
    return wait_then(p, 5, 0x0C522, 0x0C512, 0x0C51C);
}
int logic_state_loot_p1(void) { return state_loot(0); }
int logic_state_loot_p2(void) { return state_loot(1); }

static int loot_frame_go(int p)
{
    set_lo(7, obj16(OBJ_ANIM_TIME, O));
    if ((int16_t)lo(REG_D[7]) >= 0) { branched(AT(0x0C560), AT(0x0C56A)); return done(); }
    CALL(0x099D6, 0x0C564);
    ran_to(AT(0x0C56A));
    return done();
}
static int loot_frame(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C55A));
    return loot_frame_go(p);
}
int logic_loot_frame_p1(void) { return loot_frame(0); }
int logic_loot_frame_p2(void) { return loot_frame(1); }

static int loot_go(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C522));
    anim_by_facing(p, 0x1A, 0x1B, 0x0C52E, 0x0C536);
    set_lo(0, 0xE5);
    CALL(0x02D98, 0x0C540);
    run_from(p, CONT(0x0C55A, 0x0C546));
    ran_to(AT(0x0C55A));
    return loot_frame_go(p);
}
int logic_loot_go_p1(void) { return loot_go(0); }
int logic_loot_go_p2(void) { return loot_go(1); }

/* ---------------------------------------------------------------- states 8, 9: falling */

/* sub_0C56C / sub_0E9E4: state 8, he falls down a hole: protected for a moment (sub_09F0E,
 * 1), animation $30 from its first picture, along a path (0xC63A), a sound; each frame
 * (sub_0C5BA) the view scrolls on, the buttons do nothing (only bits $8080 kept), a step
 * (sub_0A8EA) with its sound (table 0xC83C by the path's step); at the path's end: state
 * $8009 and five puffs of dust (sub_1A108, 0 to $10) */
static int fall_frame_go(int p)
{
    set16(PR(p, P1_VIEW_SPEED_X), (uint16_t)(ram16(PR(p, P1_VIEW_SPEED_X)) + 2));
    set_lo(7, ram16(PR(p, P1_PAD)) & 0x8080);
    set16(PR(p, P1_PAD), lo(REG_D[7]));
    CALL(0x0A8EA, 0x0C5D0);
    if (obj16(OBJ_DX, O) != 0x8000) {
        branched(AT(0x0C5DE), AT(0x0C61C));
        set_lo(7, (uint16_t)(ram16(PR(p, 0xFF56A2)) - 1));
        REG_D[0] = 0;
        REG_A[6] = 0x0C83C;
        uint8_t snd = md.rom[0x0C83C + (int16_t)lo(REG_D[7])];
        REG_D[0] = snd;
        if (!snd) { branched(AT(0x0C62E), AT(0x0C638)); return done(); }
        CALL(0x02D54, 0x0C632);
        ran_to(AT(0x0C638));
        return done();
    }
    set_lo(7, 0x8009);
    CALL(0x09846, 0x0C5E6);
    REG_D[0] = O;
    static const uint16_t jsr_at[5] = {0xC5F0, 0xC5F8, 0xC600, 0xC608, 0xC610};
    for (int k = 0; k < 5; k++) {
        REG_D[1] = (uint32_t)(4 * k);
        CALL(0x1A108, jsr_at[k]);
    }
    ran_to(AT(0x0C61A));
    return done();
}
static int fall_frame(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C5BA));
    return fall_frame_go(p);
}
int logic_fall_frame_p1(void) { return fall_frame(0); }
int logic_fall_frame_p2(void) { return fall_frame(1); }

static int state_fall(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C56C));
    set_lo(7, 1);
    CALL(0x09F0E, 0x0C570);
    set_lo(7, 0x30);
    CALL(0x097F6, 0x0C57A);
    REG_A[6] = obj32(OBJ_ANIM_STEPS, 0);              /* (player 1's: player 2's copy too) */
    set_obj16(OBJ_FRAME, 0, mem16(REG_A[6] + 2));
    REG_A[6] = PT(p, 0x0C63A);
    REG_A[5] = PT(p, 0x0CC62);
    CALL(0x0A51A, 0x0C596);
    set_lo(0, 0xA4);
    CALL(0x02D54, 0x0C5A0);
    run_from(p, CONT(0x0C5BA, 0x0C5A6));
    ran_to(AT(0x0C5BA));
    return fall_frame_go(p);
}
int logic_state_fall_p1(void) { return state_fall(0); }
int logic_state_fall_p2(void) { return state_fall(1); }

/* sub_0C87C / sub_0ECB4: state 9, down to the ground floor: animation $31, a path
 * (0xC8E6), a sound; at the path's end (sub_0C8B4) he is on the ground floor (y $188 /
 * $18C), stands, protected for 180 frames, and $A5C6 cleared */
static int land_frame(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C8B4));
    CALL(0x0A8EA, 0x0C8B4);
    if (obj16(OBJ_DX, O) != 0x8000) { branched(AT(0x0C8C2), AT(0x0C8E4)); return done(); }
    set_obj16(OBJ_Y, O, p ? 0x18C : 0x188);
    CALL(0x099D6, 0x0C8CE);
    set_lo(7, 0xB4);
    CALL(0x09EDA, 0x0C8D8);
    set16(0xFFA5C6, 0);
    ran_to(AT(0x0C8E4));
    return done();
}
int logic_land_frame_p1(void) { return land_frame(0); }
int logic_land_frame_p2(void) { return land_frame(1); }

static int state_land(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C87C));
    set_lo(7, 0x31);
    CALL(0x097F6, 0x0C880);
    REG_A[6] = PT(p, 0x0C8E6);
    REG_A[5] = PT(p, 0x0CC62);
    CALL(0x0A51A, 0x0C88E);
    set_lo(0, 0x99);
    CALL(0x02D54, 0x0C898);
    run_from(p, CONT(0x0C8B4, 0x0C89E));
    ran_to(AT(0x0C8B2));
    return done();
}
int logic_state_land_p1(void) { return state_land(0); }
int logic_state_land_p2(void) { return state_land(1); }

/* ---------------------------------------------------------------- state $A, and sub_0C952 */

/* sub_0C910 / sub_0ED40: state $A: tiles $1E / $3C, a frame, then (sub_0C932) animation
 * $23; nothing more each frame */
static int state_a(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C910));
    set_lo(7, p ? 0x3C : 0x1E);
    CALL(0x01550, 0x0C914);
    return wait_then(p, 1, 0x0C932, 0x0C922, 0x0C92C);
}
int logic_state_a_p1(void) { return state_a(0); }
int logic_state_a_p2(void) { return state_a(1); }

static int state_a_go(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C932));
    set_lo(7, 0x23);
    CALL(0x097F6, 0x0C936);
    run_from(p, CONT(0x0C950, 0x0C93C));
    ran_to(AT(0x0C950));
    return done();
}
int logic_state_a_go_p1(void) { return state_a_go(0); }
int logic_state_a_go_p2(void) { return state_a_go(1); }

/* sub_0C952 / sub_0ED82 (set as his state's routine by sub_1BEB4): his standing pose,
 * tiles $1A / $38, 9 frames; then (sub_0C97A) animation $24, a sound, a path (0xC9DA);
 * each frame (sub_0C9B2) the view follows him ($56BC) along it; at its end, player 1 two
 * pixels higher (player 2's copy: none), and he stands */
static int climb(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C952));
    CALL(0x099DE, 0x0C952);
    set_lo(7, p ? 0x38 : 0x1A);
    CALL(0x01550, 0x0C95C);
    return wait_then(p, 9, 0x0C97A, 0x0C96A, 0x0C974);
}
int logic_climb_p1(void) { return climb(0); }
int logic_climb_p2(void) { return climb(1); }

static int climb_go(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C97A));
    set_lo(7, 0x24);
    CALL(0x097F6, 0x0C97E);
    set_lo(0, 0x99);
    CALL(0x02D54, 0x0C988);
    REG_A[6] = PT(p, 0x0C9DA);
    REG_A[5] = PT(p, 0x0CC62);
    CALL(0x0A51A, 0x0C996);
    run_from(p, CONT(0x0C9B2, 0x0C99C));
    ran_to(AT(0x0C9B0));
    return done();
}
int logic_climb_go_p1(void) { return climb_go(0); }
int logic_climb_go_p2(void) { return climb_go(1); }

static int climb_frame(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C9B2));
    set16(PR(p, P1_EDGE), 1);
    CALL(0x0A8EA, 0x0C9BA);
    if (obj16(OBJ_DX, O) != 0x8000) { branched(AT(0x0C9C8), AT(0x0C9D8)); return done(); }
    if (!p) set_obj16(OBJ_Y, O, (uint16_t)(obj16(OBJ_Y, O) - 2));
    CALL(0x099D6, 0x0C9D2);
    ran_to(AT(0x0C9D8));
    return done();
}
int logic_climb_frame_p1(void) { return climb_frame(0); }
int logic_climb_frame_p2(void) { return climb_frame(1); }

/* (the buttons do nothing: only bits $8080 kept) */
static void pad_off(int p)
{
    set_lo(7, ram16(PR(p, P1_PAD)) & 0x8080);
    set16(PR(p, P1_PAD), lo(REG_D[7]));
}

/* ---------------------------------------------------------------- state $B: shot */

/* sub_0C9F4 / sub_0EE26: state $B, shot by a guard: his standing pose, what he holds goes
 * (sub_0AE8A), off the stairs (sub_0CFA4); his size kept (16 wide while down); he turns
 * to the way he is pushed, off any stairs, on the ground or the top floor; tiles $D / $2B,
 * 8 frames; then (sub_0CA82) protected for good, the guards forget him, animation $20 /
 * $1F, knocked back along a path (0xCC44 / 0xCC2A), sub_0B33E, sub_0ABF6, a sound
 * (sub_02D98, $E4); $56F0: at an edge, or shot in a jump */
static int state_hit(int p)
{
    if (!start()) return 0;
    begin(AT(0x0C9F4));
    CALL(0x099DE, 0x0C9F4);
    CALL(0x0AE8A, 0x0C9FA);
    CALL(0x0CFA4, 0x0CA00);
    set16(PR(p, P1_SIZE_KEPT), obj16(0xFF3706, O));
    set16(PR(p, P1_SIZE_KEPT + 2), obj16(0xFF37B2, O));
    set_obj16(0xFF3706, O, 0x10);
    set16(PR(p, P1_FACED), ram16(PR(p, P1_FACING)));
    REG_D[7] = 1;
    set_lo(6, ram16(PR(p, P1_PUSH)));
    if ((int16_t)lo(REG_D[6]) < 0) branched(AT(0x0CA34), AT(0x0CA3A));
    else { REG_D[7] = 2; ran_to(AT(0x0CA3A)); }
    set8(PR(p, P1_FACING), (uint8_t)REG_D[7]);
    set8(PR(p, P1_ON_STAIRS), 0);
    set_lo(7, 0);
    if (obj16(OBJ_HEIGHT, O) < 8) branched(AT(0x0CA52), AT(0x0CA5A));
    else { set_lo(7, 0x10); ran_to(AT(0x0CA5A)); }
    set_obj16(OBJ_HEIGHT, O, lo(REG_D[7]));
    set_lo(7, p ? 0x2B : 0xD);
    CALL(0x01550, 0x0CA64);
    return wait_then(p, 8, 0x0CA82, 0x0CA72, 0x0CA7C);
}
int logic_state_hit_p1(void) { return state_hit(0); }
int logic_state_hit_p2(void) { return state_hit(1); }

/* the knock-back's frame (sub_0CB1C / sub_0EF4E): along the path facing back (table
 * 0xCC5E), his facing put back after; at its end, his feet looked for at 0 (player 2:
 * 12) unless he lies shot, and the view's script (with 0xCC72 / 0xDD4C); then
 * (sub_0CB94) while the script runs, from its step $4A his feet at 16 / 12; after it, 180
 * frames (sub_0CBE2), then his picture and size back (player 1: 16 pixels higher) and
 * sub_0AEC2 */
static int hit_frame_go(int p)
{
    pad_off(p);
    REG_D[7] = facing(p);
    REG_A[6] = PT(p, 0x0CC5E);
    set8(PR(p, P1_FACING), md.rom[REG_A[6] + (int16_t)lo(REG_D[7])]);
    uint32_t d7 = REG_D[7];
    CALL(0x0A56A, 0x0CB44);
    REG_D[7] = d7;
    set8(PR(p, P1_FACING), (uint8_t)d7);
    if (obj16(OBJ_DX, O) != 0x8000) { branched(AT(0x0CB5C), AT(0x0CC28)); return done(); }
    if (ram16(PR(p, P1_STATE_BEFORE)) == 0x8014) branched(AT(0x0CB68), AT(0x0CB74));
    else { set16(PR(p, P1_LIFT), p ? 0xC : 0); ran_to(AT(0x0CB74)); }
    REG_A[6] = p ? 0x0DD4C : 0x0CC72;
    CALL(0x09A62, 0x0CB78);
    run_from(p, CONT(0x0CB94, 0x0CB7E));
    ran_to(AT(0x0CB92));
    return done();
}
static int hit_frame(int p)
{
    if (!start()) return 0;
    begin(AT(0x0CB1C));
    return hit_frame_go(p);
}
int logic_hit_frame_p1(void) { return hit_frame(0); }
int logic_hit_frame_p2(void) { return hit_frame(1); }

static int hit_go(int p)
{
    if (!start()) return 0;
    begin(AT(0x0CA82));
    set_lo(7, 0xFFFF);
    CALL(0x09EDA, 0x0CA86);
    CALL(0x09F42, 0x0CA8C);
    anim_by_facing(p, 0x20, 0x1F, 0x0CA9E, 0x0CAA6);
    if (facing(p) == 1) { REG_A[6] = PT(p, 0x0CC44); branched(AT(0x0CAB8), AT(0x0CAC0)); }
    else { REG_A[6] = PT(p, 0x0CC2A); ran_to(AT(0x0CAC0)); }
    REG_A[5] = PT(p, 0x0CC62);
    CALL(0x0A51A, 0x0CAC4);
    CALL(0x0B33E, 0x0CACA);
    CALL(0x0ABF6, 0x0CAD0);
    set_lo(0, 0xE4);
    CALL(0x02D98, 0x0CADA);
    if (!p) set16(0xFF56F0, 0);
    set_lo(7, ram16(PR(p, P1_CELL)) & 0x80);
    if (lo(REG_D[7])) branched(AT(0x0CAF0), AT(0x0CB00));
    else {
        if (p) set16(PR(p, P1_FROM_JUMP), 0);
        if (ram16(PR(p, P1_STATE_BEFORE)) != 0x8002) { branched(AT(0x0CAFC), AT(0x0CB08)); goto run; }
        ran_to(AT(0x0CB00));
    }
    set16(PR(p, P1_FROM_JUMP), 1);
    ran_to(AT(0x0CB08));
run:
    run_from(p, CONT(0x0CB1C, 0x0CB08));
    ran_to(AT(0x0CB1C));
    return hit_frame_go(p);
}
int logic_hit_go_p1(void) { return hit_go(0); }
int logic_hit_go_p2(void) { return hit_go(1); }

static int hit_script(int p)
{
    if (!start()) return 0;
    begin(AT(0x0CB94));
    pad_off(p);
    if (ram16(PR(p, P1_SCRIPT_ON))) {
        if (ram16(PR(p, P1_SCRIPT_STEP)) < 0x4A) { branched(AT(0x0CBB6), AT(0x0CC28)); return done(); }
        set16(PR(p, P1_LIFT), p ? 0xC : 0x10);
        ran_to(AT(0x0CBC2));
        return done();
    }
    branched(AT(0x0CBAA), AT(0x0CBC4));
    set16(PR(p, P1_STEP), 0xB4);
    run_from(p, CONT(0x0CBE2, 0x0CBCC));
    ran_to(AT(0x0CBE0));
    return done();
}
int logic_hit_script_p1(void) { return hit_script(0); }
int logic_hit_script_p2(void) { return hit_script(1); }

static int hit_down(int p)
{
    if (!start()) return 0;
    begin(AT(0x0CBE2));
    pad_off(p);
    uint16_t n = (uint16_t)(ram16(PR(p, P1_STEP)) - 1);
    set16(PR(p, P1_STEP), n);
    if (n) { branched(AT(0x0CBF8), AT(0x0CC28)); return done(); }
    set_obj16(OBJ_FRAME, O, ram16(PR(p, P1_PROTECT_PIC)));
    if (!p) set_obj16(OBJ_Y, O, (uint16_t)(obj16(OBJ_Y, O) - 0x10));
    set_obj16(0xFF3706, O, ram16(PR(p, P1_SIZE_KEPT)));
    set_obj16(0xFF37B2, O, ram16(PR(p, P1_SIZE_KEPT + 2)));
    CALL(0x0AEC2, 0x0CC22);
    ran_to(AT(0x0CC28));
    return done();
}
int logic_hit_down_p1(void) { return hit_down(0); }
int logic_hit_down_p2(void) { return hit_down(1); }

/* ---------------------------------------------------------------- state $D: hiding */

/* sub_0CD62 / sub_0F17C: player p comes out of the object he hides in: its picture back,
 * 60 frames before it may be used again, and it is free */
static int hide_out(int p)
{
    if (!start()) return 0;
    begin(AT(0x0CD62));
    uint16_t o = ram16(PR(p, P1_HIDES_IN));
    set_lo(7, o);
    set_obj16(OBJ_FRAME, o, obj16(OBJ_PICTURE, o));
    set_obj16(OBJ_DELAY, o, 0x3C);
    set_obj16(0xFF9E9E, o, 1);
    set16(PR(p, P1_HIDES_IN), 0);
    REG_A[5] = OBJ_PICTURE;
    REG_A[6] = 0xFF9E9E;
    ran_to(AT(0x0CD98));
    return done();
}
int logic_hide_out_p1(void) { return hide_out(0); }
int logic_hide_out_p2(void) { return hide_out(1); }

/* sub_0CC86 / sub_0F0A0: state $D, he hides in an object (P1_HIDES_IN): the guards forget
 * him, his standing pose, tiles $18 / $36, 16 frames; then (sub_0CCB4) animation $32 from
 * its first picture, the object's picture gone, protected for good while he hides; after
 * 60 frames (sub_0CD0E) a way pressed brings him out (animation $33; at its end,
 * sub_0CD46: out of the object, unprotected, and he stands) */
static int state_hide(int p)
{
    if (!start()) return 0;
    begin(AT(0x0CC86));
    CALL(0x09F42, 0x0CC86);
    CALL(0x099DE, 0x0CC8C);
    set_lo(7, p ? 0x36 : 0x18);
    CALL(0x01550, 0x0CC96);
    return wait_then(p, 0x10, 0x0CCB4, 0x0CCA4, 0x0CCAE);
}
int logic_state_hide_p1(void) { return state_hide(0); }
int logic_state_hide_p2(void) { return state_hide(1); }

static int hide_wait_go(int p)
{
    uint16_t d = (uint16_t)(obj16(OBJ_DELAY, O) - 1);
    set_obj16(OBJ_DELAY, O, d);
    if ((int16_t)d >= 0) { branched(AT(0x0CD14), AT(0x0CD60)); return done(); }
    uint8_t dir = ram8(PR(p, P1_PAD)) & 0xF;
    REG_D[7] = (REG_D[7] & ~0xFFu) | dir;
    if (!dir) { branched(AT(0x0CD22), AT(0x0CD60)); return done(); }
    set_lo(7, 0x33);
    CALL(0x097F6, 0x0CD2A);
    run_from(p, CONT(0x0CD46, 0x0CD30));
    ran_to(AT(0x0CD44));
    return done();
}
static int hide_wait(int p)
{
    if (!start()) return 0;
    begin(AT(0x0CD0E));
    return hide_wait_go(p);
}
int logic_hide_wait_p1(void) { return hide_wait(0); }
int logic_hide_wait_p2(void) { return hide_wait(1); }

static int hide_go(int p)
{
    if (!start()) return 0;
    begin(AT(0x0CCB4));
    set_lo(7, 0x32);
    CALL(0x097F6, 0x0CCB8);
    REG_A[6] = obj32(OBJ_ANIM_STEPS, O);
    set_obj16(OBJ_FRAME, O, mem16(REG_A[6] + 2));
    uint16_t o = ram16(PR(p, P1_HIDES_IN));
    set_lo(7, o);
    set_obj16(OBJ_FRAME, o, 0);
    set_obj16(0xFF9E9E, o, 2);
    set_lo(7, 0xFFFF);
    CALL(0x09F0E, 0x0CCEC);
    set_obj16(OBJ_DELAY, O, 0x3C);
    run_from(p, CONT(0x0CD0E, 0x0CCFA));
    ran_to(AT(0x0CD0E));
    return hide_wait_go(p);
}
int logic_hide_go_p1(void) { return hide_go(0); }
int logic_hide_go_p2(void) { return hide_go(1); }

static int hide_end(int p)
{
    if (!start()) return 0;
    begin(AT(0x0CD46));
    set_lo(7, obj16(OBJ_ANIM_TIME, O));
    if ((int16_t)lo(REG_D[7]) >= 0) { branched(AT(0x0CD4C), AT(0x0CD60)); return done(); }
    CALL(0x0CD62, 0x0CD50);
    set16(PR(p, P1_PROTECT), 0);
    CALL(0x099D6, 0x0CD5A);
    ran_to(AT(0x0CD60));
    return done();
}
int logic_hide_end_p1(void) { return hide_end(0); }
int logic_hide_end_p2(void) { return hide_end(1); }

/* ---------------------------------------------------------------- state $F: opening a door */

/* sub_0CD9A / sub_0F1B4: state $F, he opens the door he faces: his standing pose, tiles
 * $12 / $30, 5 frames; then (sub_0CDC2) animation $1C / $1D, the door open, opened by him
 * ($9F2A: 0 / 2); when the animation ends (sub_0CE0E) he stands */
static int state_open(int p)
{
    if (!start()) return 0;
    begin(AT(0x0CD9A));
    CALL(0x099DE, 0x0CD9A);
    set_lo(7, p ? 0x30 : 0x12);
    CALL(0x01550, 0x0CDA4);
    return wait_then(p, 5, 0x0CDC2, 0x0CDB2, 0x0CDBC);
}
int logic_state_open_p1(void) { return state_open(0); }
int logic_state_open_p2(void) { return state_open(1); }

static int open_frame_go(int p)
{
    set_lo(7, obj16(OBJ_ANIM_TIME, O));
    if ((int16_t)lo(REG_D[7]) >= 0) { branched(AT(0x0CE14), AT(0x0CE1E)); return done(); }
    CALL(0x099D6, 0x0CE18);
    ran_to(AT(0x0CE1E));
    return done();
}
static int open_frame(int p)
{
    if (!start()) return 0;
    begin(AT(0x0CE0E));
    return open_frame_go(p);
}
int logic_open_frame_p1(void) { return open_frame(0); }
int logic_open_frame_p2(void) { return open_frame(1); }

static int open_go(int p)
{
    if (!start()) return 0;
    begin(AT(0x0CDC2));
    anim_by_facing(p, 0x1C, 0x1D, 0x0CDCE, 0x0CDD6);
    uint16_t door = ram16(PR(p, P1_DOOR));
    set_lo(7, door);
    set_obj16(0xFF9E9E, door, 1);
    set_obj16(0xFF9F2A, door, O);
    REG_A[6] = 0xFF9F2A;
    run_from(p, CONT(0x0CE0E, 0x0CDFA));
    ran_to(AT(0x0CE0E));
    return open_frame_go(p);
}
int logic_open_go_p1(void) { return open_go(0); }
int logic_open_go_p2(void) { return open_go(1); }

/* ---------------------------------------------------------------- state $10: knocked flat */

/* sub_0CE20 / sub_0F23A: state $10, knocked flat: protected while he lies ($56BE = 2),
 * the guards forget him, tiles $15 / $33, 2 frames; then (sub_0CE50) animation $19 from
 * its first picture, a sound; after 180 frames (sub_0CE8E): out of a wall behind him (two
 * pixels left), and, shot before, sub_0AEC2; else he stands, protected for 180 frames */
static int state_flat(int p)
{
    if (!start()) return 0;
    begin(AT(0x0CE20));
    set16(PR(p, P1_PROTECT), 2);
    CALL(0x09F42, 0x0CE28);
    set_lo(7, p ? 0x33 : 0x15);
    CALL(0x01550, 0x0CE32);
    return wait_then(p, 2, 0x0CE50, 0x0CE40, 0x0CE4A);
}
int logic_state_flat_p1(void) { return state_flat(0); }
int logic_state_flat_p2(void) { return state_flat(1); }

static int flat_frame_go(int p)
{
    uint16_t n = (uint16_t)(ram16(PR(p, P1_WAIT)) - 1);
    set16(PR(p, P1_WAIT), n);
    if (n) { branched(AT(0x0CE94), AT(0x0CEEE)); return done(); }
    set_lo(0, (uint16_t)(obj16(OBJ_X, O) - 0x18));
    set_lo(1, (uint16_t)(obj16(OBJ_Y, O) - 0x20));
    CALL(0x1016A, 0x0CEAC);
    REG_D[3] = 0xFFFFFFE0u;
    CALL(0x1019E, 0x0CEB4);
    if (z_clear()) { set_obj16(OBJ_X, O, (uint16_t)(obj16(OBJ_X, O) - 2)); ran_to(AT(0x0CEC4)); }
    else branched(AT(0x0CEBA), AT(0x0CEC4));
    if (ram16(PR(p, P1_STATE_BEFORE)) == 0x800B) {
        ran_to(AT(0x0CED0));
        return player_jump(p, 0x0AEC2, 0x0CED0);
    }
    branched(AT(0x0CECC), AT(0x0CED6));
    CALL(0x099D6, 0x0CED6);
    set_lo(7, 0xB4);
    CALL(0x09EDA, 0x0CEE0);
    set16(PR(p, 0xFF56E2), 1);
    ran_to(AT(0x0CEEE));
    return done();
}
static int flat_frame(int p)
{
    if (!start()) return 0;
    begin(AT(0x0CE8E));
    return flat_frame_go(p);
}
int logic_flat_frame_p1(void) { return flat_frame(0); }
int logic_flat_frame_p2(void) { return flat_frame(1); }

static int flat_go(int p)
{
    if (!start()) return 0;
    begin(AT(0x0CE50));
    set_lo(7, 0x19);
    CALL(0x097F6, 0x0CE54);
    REG_A[6] = obj32(OBJ_ANIM_STEPS, O);
    set_obj16(OBJ_FRAME, O, mem16(REG_A[6] + 2));
    set_lo(0, 0x99);
    CALL(0x02D54, 0x0CE6C);
    run_from(p, CONT(0x0CE8E, 0x0CE72));
    set16(PR(p, P1_WAIT), 0xB4);
    ran_to(AT(0x0CE8E));
    return flat_frame_go(p);
}
int logic_flat_go_p1(void) { return flat_go(0); }
int logic_flat_go_p2(void) { return flat_go(1); }

/* ---------------------------------------------------------------- state $12: carrying */

/* sub_0CFA4 / sub_0F3BE: player p puts down what he carries (P1_CARRIES): it is free
 * again, with its picture */
static int carry_drop(int p)
{
    if (!start()) return 0;
    begin(AT(0x0CFA4));
    uint16_t o = ram16(PR(p, P1_CARRIES));
    set_lo(7, o);
    if (!o) { branched(AT(0x0CFAA), AT(0x0CFD2)); return done(); }
    set_obj16(0xFF9E9E, o, 1);
    set_obj16(OBJ_FRAME, o, obj16(OBJ_PICTURE, o));
    set16(PR(p, P1_CARRIES), 0);
    REG_A[5] = OBJ_PICTURE;
    REG_A[6] = OBJ_FRAME;
    ran_to(AT(0x0CFD2));
    return done();
}
int logic_carry_drop_p1(void) { return carry_drop(0); }
int logic_carry_drop_p2(void) { return carry_drop(1); }

/* sub_0CEF0 / sub_0F30A: state $12, he carries something: tiles $14 / $32, 6 frames;
 * then (sub_0CF12) animation $26 or $25 (by P1_CARRY_KIND); each frame (sub_0CF40) B uses
 * it (the object's next in the table busy, a sound; then, sub_0CFD4, back when the
 * animation ends); else sub_0AF32, and out of this state, it is put down */
static int carry_frame_go(int p)
{
    uint8_t b = ram8(PR(p, P1_PAD_PRESSED));
    REG_D[7] = b;
    if (b & 0x10) {
        set_obj16(OBJ_ANIM_TIME, O, 1);
        run_from(p, CONT(0x0CFD4, 0x0CF58));
        uint16_t o = (uint16_t)(ram16(PR(p, P1_CARRIES)) + 2);
        set_lo(7, o);
        REG_A[6] = 0xFF9E9E;
        set_obj16(0xFF9E9E, o, 2);
        set_lo(0, 0x9D);
        CALL(0x02D54, 0x0CF84);
        ran_to(AT(0x0CF8A));
        return done();
    }
    branched(AT(0x0CF4C), AT(0x0CF8C));
    CALL(0x0AF32, 0x0CF8C);
    if (ram16(PR(p, P1_STATE)) == 0x8012) { branched(AT(0x0CF9A), AT(0x0CFA2)); return done(); }
    CALL(0x0CFA4, 0x0CF9E);
    ran_to(AT(0x0CFA2));
    return done();
}
static int carry_frame(int p)
{
    if (!start()) return 0;
    begin(AT(0x0CF40));
    return carry_frame_go(p);
}
int logic_carry_frame_p1(void) { return carry_frame(0); }
int logic_carry_frame_p2(void) { return carry_frame(1); }

static int carry_go(int p)
{
    if (!start()) return 0;
    begin(AT(0x0CF12));
    int other = ram16(PR(p, P1_CARRY_KIND)) == 1;
    set_lo(7, other ? 0x26 : 0x25);
    if (other) branched(AT(0x0CF1E), AT(0x0CF26)); else ran_to(AT(0x0CF26));
    CALL(0x097F6, 0x0CF26);
    run_from(p, CONT(0x0CF40, 0x0CF2C));
    ran_to(AT(0x0CF40));
    return carry_frame_go(p);
}
int logic_carry_go_p1(void) { return carry_go(0); }
int logic_carry_go_p2(void) { return carry_go(1); }

static int carry_used(int p)
{
    if (!start()) return 0;
    begin(AT(0x0CFD4));
    set_lo(7, obj16(OBJ_ANIM_TIME, O));
    if ((int16_t)lo(REG_D[7]) >= 0) { branched(AT(0x0CFDA), AT(0x0CFF2)); return done(); }
    run_from(p, CONT(0x0CF40, 0x0CFDE));
    ran_to(AT(0x0CFF2));
    return done();
}
int logic_carry_used_p1(void) { return carry_used(0); }
int logic_carry_used_p2(void) { return carry_used(1); }

static int state_carry(int p)
{
    if (!start()) return 0;
    begin(AT(0x0CEF0));
    set_lo(7, p ? 0x32 : 0x14);
    CALL(0x01550, 0x0CEF4);
    return wait_then(p, 6, 0x0CF12, 0x0CF02, 0x0CF0C);
}
int logic_state_carry_p1(void) { return state_carry(0); }
int logic_state_carry_p2(void) { return state_carry(1); }

/* ---------------------------------------------------------------- state $14: dropped */

/* sub_0CFF4 / sub_0F40E: state $14: tiles $1F / $3D, a frame; then (sub_0D016) animation
 * $21 / $22 and a path (0xD152 / 0xF56C) run as the jump's are (its commands: sub_0D07A;
 * after each step, sub_0D090: left or right turns him and moves him a pixel unless a wall
 * (sub_102A6, two points, table 0xB870); B fires) */
static int drop_next_cmd(int p)
{
    uint32_t a6 = ram32(PR(p, P1_PATH));
    uint16_t cmd = (uint16_t)(mem16(a6) << 2);
    REG_A[6] = a6 + 2;
    set_lo(7, cmd);
    REG_A[5] = rom32(PT(p, 0x0BC4A) + (int16_t)cmd);
    ran_to(AT(0x0D08E) + 2);
    return jump_to(REG_A[5]);
}
static int drop_next(int p)
{
    if (!start()) return 0;
    begin(AT(0x0D07A));
    return drop_next_cmd(p);
}
int logic_drop_next_p1(void) { return drop_next(0); }
int logic_drop_next_p2(void) { return drop_next(1); }

static int drop_go(int p)
{
    if (!start()) return 0;
    begin(AT(0x0D016));
    anim_by_facing(p, 0x21, 0x22, 0x0D022, 0x0D02A);
    set32(PR(p, P1_PATH), CONT(0x0D152, 0x0D030));
    set16(PR(p, 0xFF56A2), 0);
    set16(PR(p, P1_FALLING), 0);
    set16(PR(p, P1_STEP), 0);
    set16(PR(p, 0xFF56A6), 0);
    set32(PR(p, P1_PATH_NEXT), CONT(0x0D07A, 0x0D052));
    set32(PR(p, P1_PATH_AFTER), CONT(0x0D090, 0x0D05C));
    run_from(p, CONT(0x0D07A, 0x0D066));
    ran_to(AT(0x0D07A));
    return drop_next_cmd(p);
}
int logic_drop_go_p1(void) { return drop_go(0); }
int logic_drop_go_p2(void) { return drop_go(1); }

static int state_drop(int p)
{
    if (!start()) return 0;
    begin(AT(0x0CFF4));
    set_lo(7, p ? 0x3D : 0x1F);
    CALL(0x01550, 0x0CFF8);
    return wait_then(p, 1, 0x0D016, 0x0D006, 0x0D010);
}
int logic_state_drop_p1(void) { return state_drop(0); }
int logic_state_drop_p2(void) { return state_drop(1); }

static int drop_after(int p)
{
    if (!start()) return 0;
    begin(AT(0x0D090));
    uint8_t w = ram8(PR(p, P1_PAD)) & 0xC;
    REG_D[5] = w;
    if (!w) { branched(AT(0x0D09C), AT(0x0D134)); goto fire; }
    w >>= 2;
    REG_D[5] = w;
    set8(PR(p, P1_FACING), w);
    set_lo(7, w == 1 ? 0x21 : 0x22);
    set_lo(6, w == 1 ? 0xFFFF : 1);
    if (w == 1) branched(AT(0x0D0B8), AT(0x0D0C4)); else ran_to(AT(0x0D0C4));
    CALL(0x097F6, 0x0D0C4);
    {
        set_lo(5, (uint16_t)((uint16_t)(w - 1) << 3));
        uint32_t pts = PT(p, 0x0B870) + (int16_t)lo(REG_D[5]);
        set_lo(0, (uint16_t)(obj16(OBJ_X, O) + rom16(pts)));
        set_lo(3, rom16(pts + 2));
        set_lo(1, (uint16_t)(obj16(OBJ_Y, O) + lo(REG_D[3])));
        REG_A[6] = pts + 4;
        uint32_t d6 = REG_D[6], a6 = REG_A[6];
        CALL(0x1016A, 0x0D0EA);
        CALL(0x102A6, 0x0D0F0);
        REG_D[6] = d6; REG_A[6] = a6;
        if (z_clear()) { branched(AT(0x0D0FA), AT(0x0D134)); goto fire; }
        set_lo(0, (uint16_t)(obj16(OBJ_X, O) + rom16(REG_A[6])));
        REG_A[6] += 2;
        set_lo(3, rom16(REG_A[6]));
        set_lo(1, (uint16_t)(obj16(OBJ_Y, O) + lo(REG_D[3])));
        CALL(0x1016A, 0x0D114);
        CALL(0x102A6, 0x0D11A);
        REG_D[6] = d6;
        if (z_clear()) { branched(AT(0x0D124), AT(0x0D134)); goto fire; }
        set_lo(6, (uint16_t)(lo(REG_D[6]) + obj16(OBJ_X, O)));
        set_obj16(OBJ_X, O, lo(REG_D[6]));
        ran_to(AT(0x0D134));
    }
fire:
    {
        uint8_t b = ram8(PR(p, P1_PAD_PRESSED));
        REG_D[7] = (REG_D[7] & ~0xFFu) | b;
        if (!(b & 0x10)) { branched(AT(0x0D13E), AT(0x0D150)); return done(); }
        set_lo(0, O);
        set_lo(7, (uint16_t)(8 + 4 * p));
        CALL(0x18736, 0x0D14A);
        ran_to(AT(0x0D150));
        return done();
    }
}
int logic_drop_after_p1(void) { return drop_after(0); }
int logic_drop_after_p2(void) { return drop_after(1); }

/* ---------------------------------------------------------------- state $15: off an edge */

/* sub_0D246 / sub_0F660: state $15, he walks off a floor's edge: his standing pose, tiles
 * $16 / $34, 2 frames; then (sub_0D26E) his fall's moves (table 0xD2EE left, 0xD318
 * right: a count, then dx, dy pairs) with animation $11 / $12; each frame (sub_0D2B4) a
 * move, and after the last he stands */
static int state_off_edge(int p)
{
    if (!start()) return 0;
    begin(AT(0x0D246));
    CALL(0x099DE, 0x0D246);
    set_lo(7, p ? 0x34 : 0x16);
    CALL(0x01550, 0x0D250);
    return wait_then(p, 2, 0x0D26E, 0x0D25E, 0x0D268);
}
int logic_state_off_edge_p1(void) { return state_off_edge(0); }
int logic_state_off_edge_p2(void) { return state_off_edge(1); }

static int off_edge_frame_go(int p)
{
    uint32_t a6 = ram32(PR(p, P1_PATH));
    set_lo(7, (uint16_t)(mem16(a6) + obj16(OBJ_X, O)));
    set_obj16(OBJ_X, O, lo(REG_D[7]));
    set_lo(7, (uint16_t)(mem16(a6 + 2) + obj16(OBJ_Y, O)));
    set_obj16(OBJ_Y, O, lo(REG_D[7]));
    REG_A[6] = a6 + 4;
    set32(PR(p, P1_PATH), a6 + 4);
    uint16_t n = (uint16_t)(ram16(PR(p, P1_STEPS_LEFT)) - 1);
    set16(PR(p, P1_STEPS_LEFT), n);
    if (n) { branched(AT(0x0D2E2), AT(0x0D2EC)); return done(); }
    CALL(0x099D6, 0x0D2E6);
    ran_to(AT(0x0D2EC));
    return done();
}
static int off_edge_frame(int p)
{
    if (!start()) return 0;
    begin(AT(0x0D2B4));
    return off_edge_frame_go(p);
}
int logic_off_edge_frame_p1(void) { return off_edge_frame(0); }
int logic_off_edge_frame_p2(void) { return off_edge_frame(1); }

static int off_edge_go(int p)
{
    if (!start()) return 0;
    begin(AT(0x0D26E));
    uint32_t moves = PT(p, 0x0D2EE);
    set_lo(7, 0x11);
    if (facing(p) == 1) branched(AT(0x0D280), AT(0x0D28E));
    else { moves = PT(p, 0x0D318); set_lo(7, 0x12); ran_to(AT(0x0D28E)); }
    set16(PR(p, P1_STEPS_LEFT), rom16(moves));
    REG_A[6] = moves + 2;
    set32(PR(p, P1_PATH), moves + 2);
    CALL(0x097F6, 0x0D29A);
    run_from(p, CONT(0x0D2B4, 0x0D2A0));
    ran_to(AT(0x0D2B4));
    return off_edge_frame_go(p);
}
int logic_off_edge_go_p1(void) { return off_edge_go(0); }
int logic_off_edge_go_p2(void) { return off_edge_go(1); }

/* ---------------------------------------------------------------- state $16: out */

/* sub_0D342 / sub_0F75C: state $16, he goes out: to the way out (on the roof), protected
 * for good, off any stairs; the stage's leaving script (table 0x376B8 by stage, through
 * sub_0A99A); he stands, out ($56E4) */
static int state_out(int p)
{
    if (!start()) return 0;
    begin(AT(0x0D342));
    set_obj16(OBJ_X, O, ram16(WAY_OUT_X));
    set_obj16(OBJ_Y, O, ram16(WAY_OUT_Y));
    set_obj16(OBJ_HEIGHT, O, 0x10);
    set_lo(7, 0xFFFF);
    CALL(0x09F0E, 0x0D362);
    set8(PR(p, P1_ON_STAIRS), 0);
    set16(LEAVING, 1);
    set_lo(7, (uint16_t)((uint16_t)(ram16(0xFFC05A) - 1) << 2));
    REG_A[6] = rom32(0x376B8 + (int16_t)lo(REG_D[7]));
    CALL(0x0A99A, 0x0D38A);
    CALL(0x099D6, 0x0D390);
    set16(PR(p, P1_LEAVING), 1);
    ran_to(AT(0x0D39E));
    return done();
}
int logic_state_out_p1(void) { return state_out(0); }
int logic_state_out_p2(void) { return state_out(1); }

/* ---------------------------------------------------------------- state $17: at the way out */

/* sub_0D3A0 / sub_0F7BA: state $17, at the way out with all the loot: his standing pose,
 * tiles $11 / $2F, 16 frames; then (sub_0D3C8) animation $38 / $39; each frame (sub_0D3FC)
 * once the other player is out of play or here too, the way out comes (kind $30; unless
 * it is here, or $554E); while the other is still coming, he waves (sub_0D43A) */
static int exit_frame_go(int p);
static int exit_wait_go(int p)
{
    uint16_t step = ram16(PR(p, P1_STEP));
    if (!step) {
        set_lo(7, obj16(OBJ_ANIM_TIME, O));
        if ((int16_t)lo(REG_D[7]) >= 0) { branched(AT(0x0D44A), AT(0x0D522)); return done(); }
        set16(PR(p, P1_STEP), 1);
        CALL(0x099DE, 0x0D456);
        set_lo(7, p ? 0x37 : 0x19);
        CALL(0x01550, 0x0D460);
        return wait_then(p, 0x10, 0x0D47E, 0x0D46E, 0x0D478);
    }
    branched(AT(0x0D440), AT(0x0D4AE));
    if (step != 1) { branched(AT(0x0D4B6), AT(0x0D522)); return done(); }
    set_lo(7, obj16(OBJ_ANIM_TIME, O));
    if ((int16_t)lo(REG_D[7]) >= 0) { branched(AT(0x0D4C0), AT(0x0D522)); return done(); }
    set16(PR(p, P1_STEP), 0);
    CALL(0x099DE, 0x0D4CC);
    set_lo(7, p ? 0x2F : 0x11);
    CALL(0x01550, 0x0D4D6);
    return wait_then(p, 0x10, 0x0D4F4, 0x0D4E4, 0x0D4EE);
}
static int exit_frame_go(int p)
{
    uint16_t other = (uint16_t)(2 - 2 * p);
    if (obj16(OBJ_ACTIVE, other) && ram16(p ? P1_STATE : P2_STATE) != 0x8017) {
        branched(AT(0x0D40E), AT(0x0D43A));
        return exit_wait_go(p);
    }
    if (obj16(OBJ_ACTIVE, other)) ran_to(AT(0x0D412));
    else branched(AT(0x0D402), AT(0x0D412));
    if (ram16(ESCAPE)) { branched(AT(0x0D418), AT(0x0D438)); return done(); }
    if (ram16(0xFF554E)) { branched(AT(0x0D422), AT(0x0D438)); return done(); }
    CALL(0x187CE, 0x0D426);
    REG_D[0] = O;
    set_lo(7, 0x30);
    CALL(0x18736, 0x0D432);
    ran_to(AT(0x0D438));
    return done();
}
static int exit_frame(int p)
{
    if (!start()) return 0;
    begin(AT(0x0D3FC));
    return exit_frame_go(p);
}
int logic_exit_frame_p1(void) { return exit_frame(0); }
int logic_exit_frame_p2(void) { return exit_frame(1); }

/* (the animation after a wait, then the frame: sub_0D3C8, sub_0D47E, sub_0D4F4) */
static int exit_anim(int p, uint32_t r, uint16_t left, uint16_t right)
{
    if (!start()) return 0;
    begin(AT(r));
    anim_by_facing(p, left, right, r + 0xC, r + 0x14);
    if (r == 0x0D3C8) {
        set16(PR(p, P1_STEP), 0);
        run_from(p, CONT(0x0D3FC, 0x0D3E8));
        ran_to(AT(0x0D3FC));
        return exit_frame_go(p);
    }
    run_from(p, CONT(0x0D3FC, r + 0x1A));
    ran_to(AT(r + 0x2E));
    return done();
}
int logic_exit_go_p1(void) { return exit_anim(0, 0x0D3C8, 0x38, 0x39); }
int logic_exit_go_p2(void) { return exit_anim(1, 0x0D3C8, 0x38, 0x39); }
int logic_exit_wave_p1(void) { return exit_anim(0, 0x0D47E, 0x3A, 0x3B); }
int logic_exit_wave_p2(void) { return exit_anim(1, 0x0D47E, 0x3A, 0x3B); }
int logic_exit_back_p1(void) { return exit_anim(0, 0x0D4F4, 0x38, 0x39); }
int logic_exit_back_p2(void) { return exit_anim(1, 0x0D4F4, 0x38, 0x39); }

static int state_exit(int p)
{
    if (!start()) return 0;
    begin(AT(0x0D3A0));
    CALL(0x099DE, 0x0D3A0);
    set_lo(7, p ? 0x2F : 0x11);
    CALL(0x01550, 0x0D3AA);
    return wait_then(p, 0x10, 0x0D3C8, 0x0D3B8, 0x0D3C2);
}
int logic_state_exit_p1(void) { return state_exit(0); }
int logic_state_exit_p2(void) { return state_exit(1); }

/* ---------------------------------------------------------------- state $18: caught */

/* sub_0D524 / sub_0F93E: state $18: what he holds goes, sub_0ABF6, his standing pose,
 * tiles $1C / $3A, 2 frames; then (sub_0D558) animation $27 / $28 from its first picture,
 * lying for 120 frames (sub_09F0E); once that ends (sub_0D59E), sub_0AEC2 */
static int state_18(int p)
{
    if (!start()) return 0;
    begin(AT(0x0D524));
    CALL(0x0AE8A, 0x0D524);
    CALL(0x0ABF6, 0x0D52A);
    CALL(0x099DE, 0x0D530);
    set_lo(7, p ? 0x3A : 0x1C);
    CALL(0x01550, 0x0D53A);
    return wait_then(p, 2, 0x0D558, 0x0D548, 0x0D552);
}
int logic_state_18_p1(void) { return state_18(0); }
int logic_state_18_p2(void) { return state_18(1); }

static int state_18_frame_go(int p)
{
    if (ram16(PR(p, P1_PROTECT))) { branched(AT(0x0D5A4), AT(0x0D5AE)); return done(); }
    CALL(0x0AEC2, 0x0D5A8);
    ran_to(AT(0x0D5AE));
    return done();
}
static int state_18_frame(int p)
{
    if (!start()) return 0;
    begin(AT(0x0D59E));
    return state_18_frame_go(p);
}
int logic_state_18_frame_p1(void) { return state_18_frame(0); }
int logic_state_18_frame_p2(void) { return state_18_frame(1); }

static int state_18_go(int p)
{
    if (!start()) return 0;
    begin(AT(0x0D558));
    anim_by_facing(p, 0x27, 0x28, 0x0D564, 0x0D56C);
    REG_A[6] = obj32(OBJ_ANIM_STEPS, O);
    set_obj16(OBJ_FRAME, O, mem16(REG_A[6] + 2));
    set_lo(7, 0x78);
    CALL(0x09F0E, 0x0D584);
    run_from(p, CONT(0x0D59E, 0x0D58A));
    ran_to(AT(0x0D59E));
    return state_18_frame_go(p);
}
int logic_state_18_go_p1(void) { return state_18_go(0); }
int logic_state_18_go_p2(void) { return state_18_go(1); }

/* ---------------------------------------------------------------- state $19: stairs */

/* sub_0D654 / sub_0FA6E: on the stairs, a frame: at their end (the ground or the top
 * floor, the way he goes) he steps off (state 0, his pose back, sub_0996E). Else up or
 * down by OBJ_DZ (and a pixel on every other frame), held to 0..16; left or right (unless
 * a wall, sub_0B0B8) turns him and moves him (OBJ_DX keeps the way: bits 2-3, and 0-1 up
 * or down); his animation for that (table 0xD7CE: its steps, its frame left as it is);
 * at the top or bottom, if none of his feet is on the floor, the view's little script */
static int stairs_frame_go(int p)
{
    uint16_t h = obj16(OBJ_HEIGHT, O);
    set_lo(6, h);
    REG_D[7] = ram8(PR(p, P1_ON_STAIRS));
    int off;
    if (REG_D[7] == 1) {
        if (!h) { branched(AT(0x0D66C), AT(0x0D67A)); off = 1; }
        else { ran_to(AT(0x0D670)); branched(AT(0x0D670), AT(0x0D690)); off = 0; }
    } else {
        branched(AT(0x0D666), AT(0x0D672));
        if (h < 0x10) { branched(AT(0x0D676), AT(0x0D690)); off = 0; }
        else { ran_to(AT(0x0D67A)); off = 1; }
    }
    if (off) {
        set8(PR(p, P1_ON_STAIRS), 0);
        REG_D[7] = 0;
        CALL(0x09846, 0x0D682);
        CALL(0x0996E, 0x0D688);
        ran_to(AT(0x0D68E));
        return done();
    }
    set_lo(7, (uint16_t)((ram16(FRAME_COUNT) & 1) + obj16(OBJ_DZ, O)));
    int16_t nh = (int16_t)(obj16(OBJ_HEIGHT, O) + lo(REG_D[7]));
    if (nh < 0) { nh = 0; REG_D[6] = 0; ran_to(AT(0x0D6AE)); branched(AT(0x0D6AE), AT(0x0D6BC)); }
    else {
        branched(AT(0x0D6A8), AT(0x0D6B0));
        if ((uint16_t)nh <= 0x10) branched(AT(0x0D6B4), AT(0x0D6BC));
        else { nh = 0x10; ran_to(AT(0x0D6BC)); }
        set_lo(6, (uint16_t)nh);
    }
    set_obj16(OBJ_HEIGHT, O, (uint16_t)nh);
    set_obj16(OBJ_DX, O, obj16(OBJ_DX, O) & 3);
    REG_D[7] = (REG_D[7] & ~0xFFu) | (ram8(PR(p, P1_PAD)) & 0xC);
    if (!(REG_D[7] & 0xFF)) { branched(AT(0x0D6D4), AT(0x0D730)); goto anim; }
    CALL(0x0B0B8, 0x0D6D8);
    if (z_clear()) { branched(AT(0x0D6DE), AT(0x0D730)); goto anim; }
    {
        uint8_t w = (uint8_t)((ram8(PR(p, P1_PAD)) & 0xF) >> 2);
        set8(PR(p, P1_FACED), facing(p));
        set8(PR(p, P1_FACING), w);
        REG_D[7] = (uint16_t)((uint8_t)(w - 1) * 2);
        REG_A[6] = PT(p, 0x0B848);
        set_lo(6, (uint16_t)(obj16(OBJ_X, O) + rom16(REG_A[6] + (int16_t)lo(REG_D[7]))));
        set_obj16(OBJ_X, O, lo(REG_D[6]));
        REG_D[7] = (uint16_t)((ram8(PR(p, P1_PAD)) & 0xC) | obj16(OBJ_DX, O));
        set_obj16(OBJ_DX, O, lo(REG_D[7]));
        ran_to(AT(0x0D730));
    }
anim:
    {
        uint16_t k = (uint16_t)(obj16(OBJ_DX, O) * 2);
        REG_A[6] = PT(p, 0x0D7CE);
        uint16_t a = rom16(REG_A[6] + (int16_t)k);
        set_obj16(OBJ_ANIM, O, a);
        set_lo(7, (uint16_t)(a << 2));
        REG_A[6] = PT(p, 0x36E00);
        set_obj32(OBJ_ANIM_STEPS, O, rom32(REG_A[6] + (int16_t)lo(REG_D[7])));
    }
    h = obj16(OBJ_HEIGHT, O);
    if (h && h != 0x10) {
        ran_to(AT(0x0D768));
        branched(AT(0x0D768), AT(0x0D7CC));
        return done();
    }
    if (!h) branched(AT(0x0D75C), AT(0x0D76C)); else ran_to(AT(0x0D76C));
    {
        static const uint16_t jsr_at[3] = {0xD778, 0xD794, 0xD7B0};
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
            if (z_clear()) { branched(AT(jsr_at[k] + 6), AT(0x0D7CC)); return done(); }
        }
        REG_A[6] = PT(p, 0x0B858);
        set16(PR(p, P1_LIFT), p ? 0xC : 0x10);
        CALL(0x09A62, 0x0D7C6);
        ran_to(AT(0x0D7CC));
        return done();
    }
}
static int stairs_frame(int p)
{
    if (!start()) return 0;
    begin(AT(0x0D654));
    return stairs_frame_go(p);
}
int logic_stairs_frame_p1(void) { return stairs_frame(0); }
int logic_stairs_frame_p2(void) { return stairs_frame(1); }

/* sub_0D5B4 / sub_0F9CE: state $19, onto the stairs (down if down is held, else up;
 * state $1A, sub_0D5B0: the way he is on them): his standing pose, tiles $20 and $22
 * ($3E, $40), 3 frames; then (sub_0D5FE) the stairs' heights per frame and dx (table
 * 0xD96A by the way), his pose kept (sub_098E2), his animation by the buttons (table
 * 0xD7CE); the stairs' frame from then on */
static int stairs_on(int p)
{
    CALL(0x099DE, 0x0D5CC);
    set_lo(7, p ? 0x3E : 0x20); CALL(0x01550, 0x0D5D6);
    set_lo(7, p ? 0x40 : 0x22); CALL(0x01550, 0x0D5E0);
    return wait_then(p, 3, 0x0D5FE, 0x0D5EE, 0x0D5F8);
}
static int state_stairs(int p)
{
    if (!start()) return 0;
    begin(AT(0x0D5B4));
    REG_D[0] = 1;
    uint8_t pad = ram8(PR(p, P1_PAD));
    REG_D[7] = (REG_D[7] & ~0xFFu) | pad;
    if (pad & 2) branched(AT(0x0D5C0), AT(0x0D5C6));
    else { REG_D[0] = 2; ran_to(AT(0x0D5C6)); }
    set8(PR(p, P1_ON_STAIRS), (uint8_t)REG_D[0]);
    ran_to(AT(0x0D5CC));
    return stairs_on(p);
}
int logic_state_stairs_p1(void) { return state_stairs(0); }
int logic_state_stairs_p2(void) { return state_stairs(1); }

static int state_stairs_on(int p)
{
    if (!start()) return 0;
    begin(AT(0x0D5B0));
    branched(AT(0x0D5B0), AT(0x0D5CC));
    return stairs_on(p);
}
int logic_state_stairs_on_p1(void) { return state_stairs_on(0); }
int logic_state_stairs_on_p2(void) { return state_stairs_on(1); }

static int stairs_go(int p)
{
    if (!start()) return 0;
    begin(AT(0x0D5FE));
    REG_D[0] = (uint32_t)((ram8(PR(p, P1_ON_STAIRS)) - 1) << 2) & 0xFFFF;
    REG_A[6] = PT(p, 0x0D96A);
    set_obj16(OBJ_DZ, O, rom16(REG_A[6] + (int16_t)lo(REG_D[0])));
    set_obj16(OBJ_DX, O, rom16(REG_A[6] + (int16_t)lo(REG_D[0]) + 2));
    CALL(0x098E2, 0x0D61E);
    REG_D[7] = (uint16_t)((ram8(PR(p, P1_PAD)) & 0xF) * 2);
    REG_A[6] = PT(p, 0x0D7CE);
    set_lo(7, rom16(REG_A[6] + (int16_t)lo(REG_D[7])));
    CALL(0x097F6, 0x0D63A);
    run_from(p, CONT(0x0D654, 0x0D640));
    ran_to(AT(0x0D654));
    return stairs_frame_go(p);
}
int logic_stairs_go_p1(void) { return stairs_go(0); }
int logic_stairs_go_p2(void) { return stairs_go(1); }
