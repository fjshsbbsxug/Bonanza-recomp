/* The players: their states, animations, poses, protection and paths. Player 1's
 * words; player 2's are at +2 in the per-object tables, and at their own addresses
 * for the rest (ram.h). */
#include "rt.h"

/* A player who has just been hurt, has tripped or has landed is protected for a while:
 * guards and their bullets leave him alone. sub_09EDA / sub_09EF4: protect player 1 /
 * 2 for d7 frames (blinking); sub_09F0E / sub_09F28: the same, while he lies or gets up. */
static int protect(int p, uint16_t kind, uint32_t routine)
{
    if (!start()) return 0;
    uint32_t base = p ? P2_PROTECT : P1_PROTECT;
    set16(base + 2, lo(REG_D[7]));                    /* how long */
    set16(base, kind);                                /* what kind */
    set16(base + 4, obj16(OBJ_FRAME, 2 * p));         /* the picture he had */
    begin(routine);
    ran_to(routine + 0x18);
    return done();
}

int logic_protect_p1(void)  { return protect(0, 1, 0x09EDA); }
int logic_protect_p2(void)  { return protect(1, 1, 0x09EF4); }
int logic_protect2_p1(void) { return protect(0, 2, 0x09F0E); }
int logic_protect2_p2(void) { return protect(1, 2, 0x09F28); }

/* sub_097F6 / sub_0981E: start player 1's / 2's animation d7 (its steps from the table
 * at 0x36E00 / 0x37720), from its first step, at once */
static int player_anim(int p, uint32_t table, uint32_t routine)
{
    if (!start()) return 0;
    uint16_t anim = lo(REG_D[7]);
    uint32_t steps = rom32(table + (int16_t)(uint16_t)(anim << 2));
    set_obj16(OBJ_ANIM, 2 * p, anim);
    set_obj16(OBJ_ANIM_AT, 2 * p, 0);
    set_obj16(OBJ_ANIM_TIME, 2 * p, 1);
    set_obj32(OBJ_ANIM_STEPS, 2 * p, steps);

    set_lo(7, (uint16_t)(anim << 2));                 /* (for the callers: lsl.w #2,d7) */
    FLAG_X = (uint)anim >> 6;
    REG_A[6] = steps;
    begin(routine);
    ran_to(routine + 0x26);
    return done();
}

int logic_player_anim_p1(void) { return player_anim(0, 0x36E00, 0x097F6); }
int logic_player_anim_p2(void) { return player_anim(1, 0x37720, 0x0981E); }

/* sub_0986A / sub_098B8: player 1 / 2 goes into state d7 (standing, walking, shot,
 * tripped...): the state before and its routine kept, the new state's routine (run
 * every frame) from the table at 0xB492 / 0xD996 */
static int player_state(uint32_t state, uint32_t table, uint32_t routine)
{
    if (!start()) return 0;
    uint16_t next = lo(REG_D[7]);
    set16(state + 6, ram16(state));                   /* the state before */
    set32(state + 8, ram32(state + 2));               /* ... and its routine */
    set16(state, next);
    set32(state + 2, rom32(table + (int16_t)(uint16_t)(next << 2)));

    set_lo(7, (uint16_t)(next << 2));                 /* (for the callers: lsl.w #2,d7) */
    FLAG_X = (uint)next >> 6;
    begin(routine);
    ran_to(routine + 0x28);
    return done();
}

int logic_player_state_p1(void) { return player_state(P1_STATE, 0x0B492, 0x0986A); }
int logic_player_state_p2(void) { return player_state(P2_STATE, 0x0D996, 0x098B8); }

/* sub_098E2 / sub_09928: keep player 1's / 2's pose (animation, its steps, step, time
 * and picture - the picture he had when his protection began, if protected); sub_0996E /
 * sub_099A2 put it back */
static int pose_keep(int p, uint32_t routine)
{
    if (!start()) return 0;
    uint32_t keep = p ? P2_POSE_KEPT : P1_POSE_KEPT;
    uint32_t protect = p ? P2_PROTECT : P1_PROTECT;
    uint16_t o = (uint16_t)(2 * p);
    set16(keep, obj16(OBJ_ANIM, o));
    set32(keep + 2, obj32(OBJ_ANIM_STEPS, o));
    set16(keep + 6, obj16(OBJ_ANIM_AT, o));
    set16(keep + 8, obj16(OBJ_ANIM_TIME, o));
    int protected_now = ram16(protect) != 0;
    set16(keep + 10, protected_now ? ram16(protect + 4) : obj16(OBJ_FRAME, o));
    begin(routine);
    if (!protected_now) branched(routine + 0x34, routine + 0x3E);
    ran_to(routine + 0x44);
    return done();
}

static int pose_back(int p, uint32_t routine)
{
    if (!start()) return 0;
    uint32_t keep = p ? P2_POSE_KEPT : P1_POSE_KEPT;
    uint16_t o = (uint16_t)(2 * p);
    set_obj16(OBJ_ANIM, o, ram16(keep));
    set_obj32(OBJ_ANIM_STEPS, o, ram32(keep + 2));
    set_obj16(OBJ_ANIM_AT, o, ram16(keep + 6));
    set_obj16(OBJ_ANIM_TIME, o, ram16(keep + 8));
    set_obj16(OBJ_FRAME, o, ram16(keep + 10));
    begin(routine);
    ran_to(routine + 0x32);
    return done();
}

int logic_pose_keep_p1(void) { return pose_keep(0, 0x098E2); }
int logic_pose_keep_p2(void) { return pose_keep(1, 0x09928); }
int logic_pose_back_p1(void) { return pose_back(0, 0x0996E); }
int logic_pose_back_p2(void) { return pose_back(1, 0x099A2); }

/* sub_0A51A / sub_0A542: player 1 / 2 starts moving along the path a6 (its first step:
 * x and y per frame, and a third word); a5 kept in $6C12 / $6C16 */
static int path_start(int p, uint32_t routine)
{
    if (!start()) return 0;
    uint32_t path = REG_A[6];
    uint16_t o = (uint16_t)(2 * p);
    set32(p ? P2_PATH : P1_PATH, path);
    set_obj16(OBJ_DX, o, mem16(path));
    set_obj16(OBJ_DY, o, mem16(path + 2));
    set16(p ? 0xFF575A : 0xFF56B0, mem16(path + 4));
    set16(p ? 0xFF574C : 0xFF56A2, 1);
    set32(p ? 0xFF6C16 : 0xFF6C12, REG_A[5]);

    REG_A[6] = path + 4;                              /* (for the callers) */
    begin(routine);
    ran_to(routine + 0x26);
    return done();
}

int logic_path_start_p1(void) { return path_start(0, 0x0A51A); }
int logic_path_start_p2(void) { return path_start(1, 0x0A542); }

/* sub_0BF10 / sub_0E392: d7 = the word for player 1's / 2's height within its 8
 * pixels (y & 7), from the table at 0xBF28 / 0xE3AA */
static int height_step(int p, uint32_t table, uint32_t routine)
{
    if (!start()) return 0;
    uint16_t sub = obj16(OBJ_Y, (uint16_t)(2 * p)) & 7;
    set_lo(7, rom16(table + 2 * sub));                /* (for the callers) */
    REG_A[6] = table;
    FLAG_X = 0;                                       /* (add.w d7,d7: no carry) */
    begin(routine);
    ran_to(routine + 0x16);
    return done();
}

int logic_height_step_p1(void) { return height_step(0, 0x0BF28, 0x0BF10); }
int logic_height_step_p2(void) { return height_step(1, 0x0E3AA, 0x0E392); }
