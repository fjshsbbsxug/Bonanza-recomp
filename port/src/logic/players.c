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
static void state_set(uint32_t state, uint32_t table, uint32_t routine)
{
    uint16_t next = lo(REG_D[7]);
    set16(state + 6, ram16(state));                   /* the state before */
    set32(state + 8, ram32(state + 2));               /* ... and its routine */
    set16(state, next);
    set32(state + 2, rom32(table + (int16_t)(uint16_t)(next << 2)));

    set_lo(7, (uint16_t)(next << 2));                 /* (for the callers: lsl.w #2,d7) */
    FLAG_X = (uint)next >> 6;
    ran_to(routine + 0x28);
}
static int player_state(uint32_t state, uint32_t table, uint32_t routine)
{
    if (!start()) return 0;
    begin(routine);
    state_set(state, table, routine);
    return done();
}

int logic_player_state_p1(void) { return player_state(P1_STATE, 0x0B492, 0x0986A); }
int logic_player_state_p2(void) { return player_state(P2_STATE, 0x0D996, 0x098B8); }

/* sub_09846 / sub_09894: player p asks for state d7: only if table 0xA07A allows going
 * from his state to it ((state << 5) + new), and it is not his state already */
static int player_request(int p)
{
    if (!start()) return 0;
    uint32_t state = p ? P2_STATE : P1_STATE, r = p ? 0x09894 : 0x09846;
    begin(r);
    uint16_t cur = ram16(state), want = lo(REG_D[7]);
    uint16_t k = (uint16_t)(((uint16_t)(cur << 5) + want) & 0x7FFF);
    REG_A[6] = 0x0A07A;
    uint8_t ok = md.rom[0x0A07A + (int16_t)k];
    set_lo(6, (uint16_t)((k & 0xFF00) | ok));
    if (!ok) {
        FLAG_N = 0; FLAG_Z = 0; FLAG_V = VFLAG_CLEAR; FLAG_C = CFLAG_CLEAR;
        branched(r + 0x16, r + 0x4C);
        return done();
    }
    if (want == cur) {
        FLAG_N = 0; FLAG_Z = 0; FLAG_V = VFLAG_CLEAR; FLAG_C = CFLAG_CLEAR;
        branched(r + 0x20, r + 0x4C);
        return done();
    }
    ran_to(r + 0x24);
    state_set(state, p ? 0x0D996 : 0x0B492, r + 0x24);
    return done();
}

int logic_player_request_p1(void) { return player_request(0); }
int logic_player_request_p2(void) { return player_request(1); }

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

/* ================================================================ a player's frame */

/* sub_09026 / sub_091A8: player p's frame. Out of play: elsewhere (the game's end, or
 * waiting to come in). Its buttons: the jump button (C) counts as A or B, as the options
 * say; its protection runs out; a routine set to take over ($5682) takes over. In a state
 * the buttons may change (not one of the $8000 ones): A jumps ($8002); B, unless he is on
 * stairs, opens a door he faces ($800F) or fires (a shot, unless two are flying; $8004);
 * a direction walks ($8019) - up or down only where sub_0AFE8 lets him - or stands. Then
 * the state's routine runs, and what every frame does after it. */
static int player_frame(int p)
{
    if (!start()) return 0;
    uint16_t o = (uint16_t)(2 * p);
#define AT(a) P(p, a)
    begin(AT(0x09026));
    if (ram8(PR(p, 0xFF6C6F))) { ran_to(AT(0x09036)); return jump_to(AT(0x12F02)); }
    branched(AT(0x0902C), AT(0x09036));
    uint16_t active = obj16(OBJ_ACTIVE, o);
    set_lo(7, active);
    if (!active) { ran_to(AT(0x09046)); return jump_to(AT(0x137A6)); }
    branched(AT(0x0903C), AT(0x09046));
    if ((int16_t)active < 0) { set16(PR(p, P1_PAD), 0); ran_to(AT(0x09052)); }
    else branched(AT(0x09048), AT(0x09052));
    set16(PR(p, 0xFF56E2), 0);

    /* the jump button: C as A (bit 6) or B (bit 4) */
    uint8_t pressed = ram8(PR(p, P1_PAD_PRESSED));
    uint16_t d7 = (uint16_t)((active & 0xFF00) | pressed), d6 = d7;
    uint8_t c = pressed & 0x20;
    if (!ram16(BUTTONS_SWAPPED)) { c = (uint8_t)(c << 1); ran_to(AT(0x09070)); branched(AT(0x09070), AT(0x09074)); }
    else { branched(AT(0x0906A), AT(0x09072)); c >>= 1; ran_to(AT(0x09074)); }
    d7 = (uint16_t)((d7 & 0xFF00) | c);
    d6 |= d7;
    set8(PR(p, P1_PAD_PRESSED), (uint8_t)d6);
    set_lo(6, d6);

    /* protection running out */
    if (!ram16(PR(p, P1_PROTECT))) branched(AT(0x09082), AT(0x090A0));
    else {
        uint16_t t = (uint16_t)(ram16(PR(p, P1_PROTECT_TIME)) - 1);
        set16(PR(p, P1_PROTECT_TIME), t);
        if (t) branched(AT(0x0908C), AT(0x090A0));
        else {
            set16(PR(p, P1_PROTECT), 0);
            set_obj16(OBJ_FRAME, o, ram16(PR(p, P1_PROTECT_FRAME)));
            ran_to(AT(0x090A0));
        }
    }
    if (ram16(PR(p, P1_TAKEN_OVER))) {
        uint32_t r = ram32(PR(p, P1_TAKEN_OVER + 2));
        REG_A[0] = r;
        ran_to(AT(0x090B2));
        return jump_to(r);
    }
    branched(AT(0x090A6), AT(0x090B2));

    uint16_t d0 = ram16(PR(p, P1_STATE)) & 0x8000;
    set_lo(0, d0);
    if (d0) { branched(AT(0x090BC), AT(0x09188)); goto run; }
    d7 = (uint16_t)((d7 & 0xFF00) | (pressed = ram8(PR(p, P1_PAD_PRESSED))));
    set_lo(7, d7);
    REG_D[0] = ram8(PR(p, P1_ON_STAIRS)) & 3;
    if (REG_D[0]) { branched(AT(0x090D2), AT(0x09140)); goto walk; }
    if (!(pressed & 0x40)) branched(AT(0x090DA), AT(0x090E6));
    else { set_lo(0, 0x8002); branched(AT(0x090E2), AT(0x09180)); goto change; }
    if (!(pressed & 0x10)) { branched(AT(0x090EA), AT(0x09140)); goto walk; }
    {                                                 /* B: a door he faces, or a shot */
        uint16_t door = ram16(PR(p, 0xFF3D12));
        set_lo(7, door);
        if (!door) { branched(AT(0x090F4), AT(0x0911E)); goto fire; }
        REG_A[6] = 0xFF9E9E;
        if (obj16(0xFF9E9E, door)) { branched(AT(0x09102), AT(0x0911E)); goto fire; }
        uint16_t side = ram16(PR(p, 0xFF3E6A));
        set_lo(7, side);
        if ((uint8_t)side != ram8(PR(p, P1_FACING))) { branched(AT(0x09112), AT(0x0911E)); goto fire; }
        set_lo(0, 0x800F);
        branched(AT(0x0911A), AT(0x09180));
        goto change;
    }
fire:
    /* (two shots at once: the operand read from the ROM; Insane mode makes it 1) */
    if (ram16(PR(p, P1_SHOTS)) == rom16(AT(0x0911E) + 2)) { branched(AT(0x09126), AT(0x09140)); goto walk; }
    set_lo(0, o);
    set_lo(7, (uint16_t)(8 + 4 * p));                  /* (a shot: kind 8 / $C) */
    player_call(p, 0x18736, 0x09132);
    set_lo(0, 0x8004);
    branched(AT(0x0913C), AT(0x09180));
    goto change;
walk:
    REG_D[0] = 0;
    {
        uint8_t dir = ram8(PR(p, P1_PAD)) & 0xF;
        REG_D[7] = (REG_D[7] & ~0xFFu) | dir;
        if (!dir) { branched(AT(0x0914C), AT(0x09180)); goto change; }
        set_lo(0, 0x8019);
        REG_D[7] = (REG_D[7] & ~0xFFu) | (dir & 3);
        if (!(dir & 3)) { branched(AT(0x09158), AT(0x0917E)); REG_D[0] = 1; ran_to(AT(0x09180)); goto change; }
        uint32_t keep = REG_D[0];
        player_call(p, 0x0AFE8, 0x09160);
        REG_D[0] = keep;
        if (!z_clear()) { branched(AT(0x0916A), AT(0x09180)); goto change; }
        REG_D[0] = 0;
        dir = ram8(PR(p, P1_PAD)) & 0xC;
        REG_D[7] = (REG_D[7] & ~0xFFu) | dir;
        if (!dir) { branched(AT(0x0917A), AT(0x09180)); goto change; }
        REG_D[0] = 1;
        ran_to(AT(0x09180));
    }
change:
    set_lo(7, lo(REG_D[0]));
    player_call(p, 0x09846, 0x09182);
run:
    REG_A[0] = ram32(PR(p, P1_STATE_RUN));
    call(REG_A[0], AT(0x0918E));
    player_call(p, 0x09CBA, 0x09190);
    player_call(p, 0x09ADE, 0x09196);
    player_call(p, 0x094E6, 0x0919C);
    ran_to(AT(0x091A2) + 6);                        /* (its jmp) */
    return jump_to(p ? 0x09408 : 0x0932A);
#undef AT
}

int logic_player_frame_p1(void) { return player_frame(0); }
int logic_player_frame_p2(void) { return player_frame(1); }

/* sub_0932A / sub_09408: after player p's state: blinking while protected (his picture
 * every other frame); his animation's step (steps of 8 bytes after a count: picture,
 * frames, where what he holds goes (dx, dy bytes) and two more bytes; a picture of 0
 * stays hidden); the door he touches kept as last frame's */
static int player_after(int p)
{
    if (!start()) return 0;
    uint16_t o = (uint16_t)(2 * p);
#define AT(a) P(p, a)
    begin(AT(0x0932A));
    if (ram16(PR(p, P1_PROTECT)) != 1) branched(AT(0x09332), AT(0x09352));
    else {
        REG_D[6] = 0;
        REG_D[7] = (REG_D[7] & 0xFFFF0000u) | (ram16(FRAME_COUNT) & 1);
        if (lo(REG_D[7])) { REG_D[6] = ram16(PR(p, P1_PROTECT_PIC)); ran_to(AT(0x0934C)); }
        else branched(AT(0x09342), AT(0x0934C));
        set_obj16(OBJ_FRAME, o, lo(REG_D[6]));
    }
    if (ram16(ANIMS_HELD)) { branched(AT(0x09358), AT(0x093EA)); goto door; }
    set16(PR(p, P1_ANIM_DONE), 0);
    {
        uint16_t t = (uint16_t)(obj16(OBJ_ANIM_TIME, o) - 1);
        set_obj16(OBJ_ANIM_TIME, o, t);
        if (t) { branched(AT(0x09368), AT(0x093EA)); goto door; }
    }
    {
        uint16_t at = obj16(OBJ_ANIM_AT, o);
        uint32_t steps = obj32(OBJ_ANIM_STEPS, o), st = steps + (int16_t)(uint16_t)(at << 3);
        set_lo(7, (uint16_t)(at << 3));
        REG_A[6] = steps;
        uint16_t pic = mem16(st + 2);
        if (obj16(OBJ_FRAME, o)) { set_obj16(OBJ_FRAME, o, pic); ran_to(AT(0x0938E)); }
        else branched(AT(0x09384), AT(0x0938E));
        set16(PR(p, P1_PROTECT_PIC), pic);
        set_obj16(OBJ_ANIM_TIME, o, mem16(st + 4));
        set16(PR(p, P1_HELD_DX), (uint16_t)(int8_t)RD8(st + 6));
        set16(PR(p, P1_HELD_DY), (uint16_t)(int8_t)RD8(st + 7));
        set8(PR(p, P1_HELD_ACTIVE + 1), (uint8_t)RD8(st + 8));
        set8(PR(p, P1_HELD_BIG + 1), (uint8_t)RD8(st + 9));
        at++;
        set_obj16(OBJ_ANIM_AT, o, at);
        uint16_t count = mem16(steps);
        REG_D[6] = count;
        if (count != at) { branched(AT(0x093D8), AT(0x093EA)); goto door; }
        set_obj16(OBJ_ANIM_AT, o, 0);
        set16(PR(p, P1_ANIM_DONE), 3);
        ran_to(AT(0x093EA));
    }
door:
    set16(PR(p, P1_DOOR_BEFORE), ram16(PR(p, P1_DOOR)));
    set16(PR(p, P1_DOOR), 0);
    set16(PR(p, 0xFFA3AE), 0);
    set16(PR(p, 0xFFA3A8), 0);
    ran_to(AT(0x09406));
    return done();
#undef AT
}

int logic_player_after_p1(void) { return player_after(0); }
int logic_player_after_p2(void) { return player_after(1); }

/* sub_094E6 / sub_0966E: which ways player p's view should scroll ($1876's bits: 0 up,
 * 1 down, 2 left, 3 right). Not on stairs: across, by where he stands in the view (table
 * 0xA41A: by x in the view, the way it should go, or (bit 7) none, so it waits for him
 * facing; bit 4: only while he faces that way). Up and down: by how far he is from the
 * view's middle (32 pixels; 36 for player 2's lower view), when he is seen in his own
 * view and is far (40) or at an edge, or jumping, or in the air; not while lying shot. */
static int player_view(int p)
{
    if (!start()) return 0;
    uint16_t o = (uint16_t)(2 * p);
#define AT(a) P(p, a)
    begin(AT(0x094E6));
    REG_D[0] = 0;
    REG_D[7] = ram8(PR(p, P1_ON_STAIRS)) & 3;
    uint8_t facing = ram8(PR(p, P1_FACING));
    if (REG_D[7]) { branched(AT(0x094F4), AT(0x09560)); goto updown; }
    set16(PR(p, P1_VIEW_SPEED_X), (uint16_t)(ram16(PR(p, P1_VIEW_SPEED_X)) + 2));
    {
        uint16_t dx = (uint16_t)(obj16(OBJ_X, o) - ram16(PR(p, P1_VIEW_X)));
        REG_D[7] = dx;
        REG_A[6] = 0xA41A;
        uint8_t w = md.rom[0xA41A + (int16_t)dx];
        REG_D[6] = w;
        int wait = 0;
        if (w & 0x80) { ran_to(AT(0x09518)); wait = 1; }
        else {
            branched(AT(0x09514), AT(0x0952A));
            if (!(w & 0x10)) {
                branched(AT(0x0952E), AT(0x09558));
                if (w != facing) { branched(AT(0x0955E), AT(0x09518)); wait = 1; }
                else ran_to(AT(0x09560));
            } else {
                uint8_t way = w & 0xF;
                REG_D[5] = (REG_D[5] & ~0xFFu) | way;
                if (way != facing) { branched(AT(0x0953E), AT(0x09518)); wait = 1; }
                else if (facing == 1) { REG_D[0] |= 8; ran_to(AT(0x09550)); branched(AT(0x09550), AT(0x09560)); }
                else { branched(AT(0x09548), AT(0x09552)); REG_D[0] |= 4; branched(AT(0x09556), AT(0x09560)); }
            }
        }
        if (wait) {
            REG_D[6] = facing;
            set16(PR(p, P1_VIEW_LOOK), facing);
            branched(AT(0x09526), AT(0x09560));
        }
    }
updown:
    {
        uint32_t state_at = PR(p, P1_STATE);
        uint16_t state = ram16(state_at);
        REG_A[6] = state_at;
        int follow = 0;                               /* 0: no; 1: by how far */
        if (state == 0x8014) branched(AT(0x0956A), AT(0x095BE)), follow = 1;
        else if (!(REG_D[7] = obj16(OBJ_ON_SCREEN, o) & (1 << p)))
            branched(AT(0x09578), AT(0x095BE)), follow = 1;
        else {
            int16_t dy = (int16_t)(obj16(OBJ_Y, o) - ram16(PR(p, P1_VIEW_Y)));
            if (dy >= 0) branched(AT(0x09588), AT(0x0958E));
            else { dy = (int16_t)-dy; ran_to(AT(0x0958E)); }
            REG_D[7] = (uint16_t)dy;
            int far = (uint16_t)dy >= 0x28;
            if (far) branched(AT(0x09592), AT(0x095A0));
            else if (!ram16(PR(p, P1_EDGE))) { branched(AT(0x0959C), AT(0x095EC)); goto still; }
            else ran_to(AT(0x095A0));
            if (state == 0x8002) { branched(AT(0x095A4), AT(0x095EC)); goto still; }
            if (state != 0x800B) { branched(AT(0x095AC), AT(0x095BE)); follow = 1; }
            else {
                REG_D[7] = ram16(PR(p, P1_CELL)) & 0x80;
                if (!REG_D[7]) { branched(AT(0x095BA), AT(0x095EC)); goto still; }
                ran_to(AT(0x095BE));
                follow = 1;
            }
        }
        (void)follow;
        set16(PR(p, P1_VIEW_SPEED_Y), (uint16_t)(ram16(PR(p, P1_VIEW_SPEED_Y)) + 4));
        int16_t dy = (int16_t)(obj16(OBJ_Y, o) - ram16(PR(p, P1_VIEW_Y)));
        REG_D[7] = (uint16_t)dy;
        uint16_t middle = p ? 0x24 : 0x20;
        if (dy < 0) { ran_to(AT(0x095D4)); goto up; }
        branched(AT(0x095D0), AT(0x095DC));
        if ((uint16_t)dy == middle) { branched(AT(0x095E0), AT(0x095F0)); goto look; }
        if ((uint16_t)dy < middle) { branched(AT(0x095E4), AT(0x095D4)); goto up; }
        REG_D[0] |= 2;
        ran_to(AT(0x095EA));
        branched(AT(0x095EA), AT(0x095F0));
        goto look;
up:
        REG_D[0] |= 1;
        branched(AT(0x095D8), AT(0x095F0));
        goto look;
still:
        REG_D[0] &= ~3u;
        ran_to(AT(0x095F0));
    }
look:
    if (!ram16(PR(p, P1_VIEW_LOOK))) { branched(AT(0x095F6), AT(0x09666)); goto out; }
    {
        uint16_t dx = (uint16_t)(obj16(OBJ_X, o) - ram16(PR(p, P1_VIEW_X)));
        REG_D[7] = dx;
        REG_A[6] = 0xA41A;
        uint8_t w = md.rom[0xA41A + (int16_t)dx];
        REG_D[6] = w;
        if (w & 0x10) {
            uint16_t way = w & 0xF, look = ram16(PR(p, P1_VIEW_LOOK));
            REG_D[6] = way;
            set_lo(5, look);
            int done_ = 0;
            if (way == look) { branched(AT(0x09624), AT(0x09632)); done_ = 1; }
            else if ((uint8_t)way == facing) { ran_to(AT(0x09632)); done_ = 1; }
            else branched(AT(0x0962E), AT(0x0963C));
            if (done_) {
                set16(PR(p, P1_VIEW_LOOK), 0);
                branched(AT(0x09638), AT(0x09666));
                goto out;
            }
        } else {
            branched(AT(0x09614), AT(0x0963C));
        }
        uint8_t lr = ram8(PR(p, P1_PAD)) & 0xC;
        REG_D[7] = lr;
        if (lr) { set16(PR(p, P1_VIEW_SPEED_X), (uint16_t)(ram16(PR(p, P1_VIEW_SPEED_X)) + 2)); ran_to(AT(0x09650)); }
        else branched(AT(0x09646), AT(0x09650));
        REG_D[7] = 2;
        if (ram16(PR(p, P1_VIEW_LOOK)) == 2) branched(AT(0x0965A), AT(0x09660));
        else { REG_D[7] = 3; ran_to(AT(0x09660)); }
        REG_D[0] |= 1u << REG_D[7];
        branched(AT(0x09662), AT(0x09666));
    }
out:
    set16(PR(p, P1_VIEW_WANT), lo(REG_D[0]));
    ran_to(AT(0x0966C));
    return done();
#undef AT
}

int logic_player_view_p1(void) { return player_view(0); }
int logic_player_view_p2(void) { return player_view(1); }

/* sub_09ADE / sub_09B00: a little script moving player p (while $56B2): its next word
 * picks the command (table 0x9B22 / 0x9B2A), which goes on from there */
static int player_script(int p)
{
    if (!start()) return 0;
#define AT(a) P(p, a)
    begin(AT(0x09ADE));
    if (!ram16(PR(p, P1_SCRIPT_ON))) { branched(AT(0x09AE4), AT(0x09AFE)); return done(); }
    uint32_t at = ram32(PR(p, P1_SCRIPT));
    uint16_t cmd = (uint16_t)(mem16(at) << 2);
    REG_A[6] = at + 2;
    set_lo(7, cmd);
    REG_A[5] = rom32(PT(p, 0x09B22) + (int16_t)cmd);
    ran_to(AT(0x09AFE));                              /* (its jmp) */
    return jump_to(REG_A[5]);
#undef AT
}

int logic_player_script_p1(void) { return player_script(0); }
int logic_player_script_p2(void) { return player_script(1); }

/* sub_09CBA / sub_09DC6: what is under player p ($56BA: the map cell's bits, by
 * sub_0B8BC at his left, right and middle, when he is up on something: a cell with bit 7
 * where he stands, or one of kind $80 (an edge: $56BC too) under one side), whether his
 * view must follow (table 0x9F8A by y / 8; player 2's 0x9FDA by y / 4), and the way out:
 * standing on it ($F000) high up, he goes out ($8016) once all the loot is taken, else a
 * message says there is more to take (kind $28) */
static int player_ground(int p)
{
    if (!start()) return 0;
    uint16_t o = (uint16_t)(2 * p);
#define AT(a) P(p, a)
    begin(AT(0x09CBA));
    set16(PR(p, P1_CELL), 0);
    if (!obj16(OBJ_HEIGHT, o)) { branched(AT(0x09CC6), AT(0x09D46)); goto view; }
    {
        static const uint32_t jsr_at[3] = {0x09CD6, 0x09CF2, 0x09D0E};
        int edge = 0;
        for (int k = 0; k < 3; k++) {
            uint16_t x = obj16(OBJ_X, o);
            if (k == 0) x = (uint16_t)(x - 0x20);
            else {
                REG_A[6] = PT(p, 0x0B858);
                x = k == 1 ? (uint16_t)(x + rom16(REG_A[6] + 4) + 2) : (uint16_t)(x + rom16(REG_A[6] + 0xC) - 2);
            }
            set_lo(0, x);
            REG_D[7] = 0;
            player_call(p, 0x0B8BC, jsr_at[k]);
            if (z_clear()) { branched(AT(jsr_at[k] + 6), AT(0x09D2A)); edge = 1; break; }
        }
        uint16_t cell = lo(REG_D[2]);
        if (!edge) {
            set_lo(7, cell);
            REG_D[7] = (REG_D[7] & ~0xFFu) | (cell & 0x80);
            if (!(cell & 0x80)) { branched(AT(0x09D1E), AT(0x09D46)); goto view; }
            set16(PR(p, P1_CELL), cell);
            branched(AT(0x09D28), AT(0x09D46));
            goto view;
        }
        set_lo(7, cell & 0xF0);
        if ((cell & 0xF0) != 0x80) { branched(AT(0x09D34), AT(0x09D46)); goto view; }
        set16(PR(p, P1_CELL), cell);
        set16(PR(p, P1_EDGE), 1);
        ran_to(AT(0x09D46));
    }
view:
    {
        uint16_t row = (uint16_t)(obj16(OBJ_Y, o) >> (p ? 2 : 3));
        REG_A[6] = PT(p, 0x09F8A);
        uint8_t f = md.rom[REG_A[6] + (int16_t)row];
        REG_D[7] = (REG_D[7] & 0xFFFF0000u) | (uint16_t)((row & 0xFF00) | f);
        if (!f) branched(AT(0x09D56), AT(0x09D62));
        else { set16(PR(p, P1_EDGE), 1); ran_to(AT(0x09D62)); }
    }
    if (obj16(OBJ_HEIGHT, o) != 0x10) { branched(AT(0x09D6A), AT(0x09DC4)); return done(); }
    set_lo(7, ram16(PR(p, P1_CELL)) & 0xF000);
    if (lo(REG_D[7]) != 0xF000) { branched(AT(0x09D7C), AT(0x09DC4)); return done(); }
    if (ram16(LOOT_ALL)) {
        if (ram16(PR(p, P1_LEAVING))) { branched(AT(0x09D90), AT(0x09DC4)); return done(); }
        if (ram16(PR(p, P1_STATE)) == 0x800B) { branched(AT(0x09D9C), AT(0x09DC4)); return done(); }
        set_lo(7, 0x8016);
        player_call(p, 0x09846, 0x09DA4);
        branched(AT(0x09DAA), AT(0x09DC4));
        return done();
    }
    branched(AT(0x09D86), AT(0x09DAE));
    set_lo(7, ram16(PR(p, MSG_28_P1)));
    if (lo(REG_D[7])) { branched(AT(0x09DB4), AT(0x09DC4)); return done(); }
    REG_D[0] = o;
    set_lo(7, 0x28);
    player_call(p, 0x18736, 0x09DBE);
    ran_to(AT(0x09DC4));
    return done();
#undef AT
}

int logic_player_ground_p1(void) { return player_ground(0); }
int logic_player_ground_p2(void) { return player_ground(1); }
