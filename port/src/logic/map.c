/* The stage's map: what is at a place, and walls (ram.h: MAP_*). */
#include "rt.h"
#include "map.h"

struct cell map_cell(uint16_t x, uint16_t y, int *floors_down)
{
    /* the floor: the building's floors are 80 pixels high (counted as the game does,
     * taking 80 off until below 0) */
    uint16_t floor4 = 0, rest = y;
    int n = 0;
    for (;;) {
        rest = (uint16_t)(rest - 0x50);
        if (rest & 0x8000) break;
        floor4 += 4;
        n++;
    }
    if (floors_down) *floors_down = n;
    uint32_t cells = ram32(MAP_FLOORS + floor4);
    uint16_t start = ram16(MAP_FLOOR_START + floor4 / 2);
    uint32_t at = cells + (int16_t)(uint16_t)((x >> 3) << 2) - (int16_t)start;
    struct cell c;
    c.walls = (uint8_t)RD8(at);
    c.walls_up = (uint8_t)RD8(at + 1);
    c.info = mem16(at + 2);
    c.at = at;
    c.rest = rest;
    c.start = start;
    return c;
}

uint8_t map_lane(uint16_t y)        { return (uint8_t)RD8(ram32(MAP_LANES) + (uint16_t)((y >> 3) << 1)); }
uint8_t map_lane_stairs(uint16_t y) { return (uint8_t)RD8(ram32(MAP_LANES) + (uint16_t)((y >> 3) << 1) + 1); }

void map_cell_time(int n, int with_rts)
{
    jumped_to(0x1016A);
    ran_to(0x1016E);
    for (int k = 0; k < n; k++) {
        ran_to(0x10176);
        branched(0x10176, 0x1016E);
    }
    branched(0x10170, 0x10178);
    ran_to(with_rts ? 0x1019E : 0x1019C);
}

/* sub_1016A: the cell at d0 = x, d1 = y: d0.b, d1.b its walls, d2 its word */
int logic_map_cell(void)
{
    if (!start()) return 0;
    int n;
    uint16_t x = lo(REG_D[0]), y = lo(REG_D[1]);
    struct cell c = map_cell(x, y, &n);
    begin(0x1016A);
    map_cell_time(n, 0);
    REG_D[7] = c.start;                               /* (for the callers) */
    REG_D[6] = 0x50;
    set_lo(0, (uint16_t)((((x >> 3) << 2) & 0xFF00) | c.walls));
    set_lo(1, (uint16_t)((c.rest & 0xFF00) | c.walls_up));
    set_lo(2, c.info);
    REG_A[6] = c.at + 4;
    REG_A[5] = MAP_FLOOR_START;
    flags_of16(c.info);
    return done();
}

/* the time of sub_101F2 / sub_10274 (its rts too) */
void wall_test_time(uint32_t routine, int up)
{
    jumped_to(routine);
    if (!up) { branched(routine + 0x26, routine + 0x2E); ran_to(routine + 0x32); }
    else ran_to(routine + 0x2E);
}

/* sub_101F2 / sub_10274: a wall in the way of object d4, the lane d3 pixels down from
 * it: of the walls d0.b (standing on the floor) or d1.b (standing on something: height 8
 * and up; sub_10274 the other way round) those of that lane. Z clear: a wall. */
static int wall_test(uint32_t routine, int swapped)
{
    if (!start()) return 0;
    uint16_t o = lo(REG_D[4]);
    uint16_t at = (uint16_t)(obj16(OBJ_Y, o) + lo(REG_D[3]));
    uint8_t lane = map_lane(at);
    int up = obj16(OBJ_HEIGHT, o) >= 8;
    int d = (up != swapped) ? 1 : 0;                  /* the register tested: d1 or d0 */
    uint8_t v = (uint8_t)(REG_D[d] & lane);
    REG_D[d] = (REG_D[d] & ~0xFFu) | v;
    REG_D[3] = (REG_D[3] & ~0xFFu) | lane;            /* (for the callers) */
    REG_D[7] = (REG_D[7] & 0xFFFF0000u) | (uint16_t)((at >> 3) << 1);
    REG_A[6] = OBJ_HEIGHT;
    FLAG_N = NFLAG_8(v); FLAG_Z = v; FLAG_V = VFLAG_CLEAR; FLAG_C = CFLAG_CLEAR;
    begin(routine);
    if (!up) branched(routine + 0x26, routine + 0x2E);
    ran_to(routine + (up ? 0x2C : 0x30));
    return done();
}

int logic_wall_test(void)  { return wall_test(0x101F2, 0); }
int logic_wall_test2(void) { return wall_test(0x10274, 1); }

int probe_wall(uint16_t o, uint32_t points, int16_t dx, int16_t dy, int16_t lane_dy, int swapped,
               int stairs, uint16_t *info)
{
    uint32_t pt = points + 2 * (int16_t)o;
    uint16_t x = (uint16_t)(obj16(OBJ_X, o) + dx + mem16(pt));
    uint16_t y = (uint16_t)(obj16(OBJ_Y, o) + dy + mem16(pt + 2));
    struct cell c = map_cell(x, y, 0);
    *info = c.info;
    if (stairs) {
        set_obj16(MAP_CELL_INFO, o, c.info);
        uint8_t st = map_lane_stairs(y);
        if (st && (c.info & 0x80) && obj16(OBJ_HEIGHT, o))
            return (c.info >> 8) & st;                /* on stairs: their bits */
    }
    uint8_t lane = map_lane((uint16_t)(obj16(OBJ_Y, o) + lane_dy));
    int up = obj16(OBJ_HEIGHT, o) >= 8;
    return ((up != swapped) ? c.walls_up : c.walls) & lane;
}

/* The probes around a guard (sub_176B8 and the others: d0 the object, d7 dx, d6 dy, d3
 * the lane's offset): Z clear: a wall; d2: the cell's word. Each: its points, which
 * walls first (sub_10274's order: swapped), whether stairs count, and where its parts
 * are (the time). */
struct probe_kind {
    uint32_t routine, points;
    uint8_t swapped, stairs;
    uint32_t to_tail;        /* stairs: the branch to the shared part (0: falls into it) */
    uint32_t tail;           /* stairs: the shared part (0x17850 / 0x178DA); plain: where map_cell is called */
};

/* a probe and its time (from its start; with_rts: its rts too); the cell's word in *info */
static int probe_run(const struct probe_kind *k, uint16_t o, int16_t dx, int16_t dy, int16_t lane_dy,
                     uint16_t *info_out, int with_rts)
{
    uint16_t info;
    int wall = probe_wall(o, k->points, dx, dy, lane_dy, k->swapped, k->stairs, &info);

    /* the time */
    uint32_t pt = k->points + 2 * (int16_t)o;
    uint16_t y = (uint16_t)(obj16(OBJ_Y, o) + dy + mem16(pt + 2));
    int n;
    map_cell((uint16_t)(obj16(OBJ_X, o) + dx + mem16(pt)), y, &n);
    int up = obj16(OBJ_HEIGHT, o) >= 8;
    uint32_t test = k->swapped ? 0x10274 : 0x101F2;
    jumped_to(k->routine);
    if (k->stairs) {
        uint32_t s = k->tail;
        if (k->routine == 0x17934) { ran_to(0x17946); branched(0x17946, 0x17832); }
        if (k->to_tail) { ran_to(k->to_tail); branched(k->to_tail, s); }
        else ran_to(s);
        ran_to(s + 0x0A);
        map_cell_time(n, 1);
        jumped_to(s + 0x0A);
        ran_to(s + 0x26);
        int normal = 1;
        if (map_lane_stairs(y)) {
            branched(s + 0x26, s + 0x36);
            if (!(info & 0x80)) branched(s + 0x3C, s + 0x2A);
            else if (!obj16(OBJ_HEIGHT, o)) branched(s + 0x48, s + 0x2A);
            else { ran_to(s + 0x58 + (with_rts ? 2 : 0)); normal = 0; }
        } else {
            ran_to(s + 0x2A);
        }
        if (normal) {
            ran_to(s + 0x30);
            wall_test_time(test, up);
            jumped_to(s + 0x30);
            ran_to(s + 0x34 + (with_rts ? 2 : 0));
        }
    } else {
        uint32_t m = k->tail;                         /* the jsr to sub_1016A */
        ran_to(m + 6);
        map_cell_time(n, 1);
        jumped_to(m + 6);
        ran_to(m + 12);
        wall_test_time(test, up);
        jumped_to(m + 12);
        ran_to(m + 16 + (with_rts ? 2 : 0));
    }
    if (info_out) *info_out = info;
    return wall;
}

/* what a probe leaves for its callers: d2 the cell's word, d4 the object, Z clear: a wall */
static void probe_result(uint16_t o, uint16_t info, int wall)
{
    set_lo(2, info);
    set_lo(4, o);
    FLAG_N = NFLAG_8(wall); FLAG_Z = (uint8_t)wall; FLAG_V = VFLAG_CLEAR; FLAG_C = CFLAG_CLEAR;
    scramble((1u << 3) | (1u << 5) | (0x60u << 8));   /* (d3, d5, a5, a6: nothing reads them) */
}

static int probe(const struct probe_kind *k)
{
    if (!start()) return 0;
    uint16_t o = lo(REG_D[0]), info;
    begin(k->routine);
    int wall = probe_run(k, o, (int16_t)lo(REG_D[7]), (int16_t)lo(REG_D[6]), (int16_t)lo(REG_D[3]), &info, 0);
    probe_result(o, info, wall);
    return done();
}

static const struct probe_kind P_176B8 = {0x176B8, OBJ_POINTS_A, 0, 0, 0, 0x176E6};
static const struct probe_kind P_176F8 = {0x176F8, OBJ_POINTS_A, 1, 0, 0, 0x17726};
static const struct probe_kind P_17738 = {0x17738, OBJ_POINTS_C, 0, 0, 0, 0x17766};
static const struct probe_kind P_17778 = {0x17778, OBJ_POINTS_C, 1, 0, 0, 0x177A6};
static const struct probe_kind P_1794A = {0x1794A, OBJ_POINTS_E, 1, 0, 0, 0x1797A};
static const struct probe_kind P_177B8 = {0x177B8, OBJ_POINTS_B, 0, 1, 0x177E8, 0x17850};
static const struct probe_kind P_177EC = {0x177EC, OBJ_POINTS_B, 1, 1, 0x1781C, 0x178DA};
static const struct probe_kind P_17820 = {0x17820, OBJ_POINTS_D, 0, 1, 0, 0x17850};
static const struct probe_kind P_178AA = {0x178AA, OBJ_POINTS_D, 1, 1, 0, 0x178DA};
static const struct probe_kind P_17934 = {0x17934, OBJ_POINTS_E, 0, 1, 0, 0x17850};

int logic_probe_176B8(void) { return probe(&P_176B8); }
int logic_probe_176F8(void) { return probe(&P_176F8); }
int logic_probe_17738(void) { return probe(&P_17738); }
int logic_probe_17778(void) { return probe(&P_17778); }
int logic_probe_1794A(void) { return probe(&P_1794A); }
int logic_probe_177B8(void) { return probe(&P_177B8); }
int logic_probe_177EC(void) { return probe(&P_177EC); }
int logic_probe_17820(void) { return probe(&P_17820); }
int logic_probe_178AA(void) { return probe(&P_178AA); }
int logic_probe_17934(void) { return probe(&P_17934); }

/* ================================================================ around an object */

/* how far below its y an object's feet are ($37B2) */
static int16_t feet(uint16_t o) { return (int16_t)obj16(0xFF37B2, o); }

/* a list of probes; the first wall found ends it: each probe's dx, kind, lane (pixels
 * down from the object), and where it is called (a bne to `end` after it, but for the
 * last). The probe that ended it in *last. Z clear: a wall. */
struct look_step { int16_t dx; const struct probe_kind *k; int16_t lane; uint32_t jsr_at; };

static int probes(uint16_t o, const struct look_step *st, int n, int16_t dy, uint32_t end,
                  uint16_t *info, int *last)
{
    int wall = 0;
    for (int i = 0; i < n; i++) {
        *last = i;
        ran_to(st[i].jsr_at + 6);
        wall = probe_run(st[i].k, o, st[i].dx, dy, st[i].lane, info, 1);
        jumped_to(st[i].jsr_at + 6);
        if (wall && i < n - 1) { branched(st[i].jsr_at + 6, end); return wall; }
    }
    ran_to(end);
    return wall;
}

/* sub_17A8A: a wall at either side of the guard or where it stands (the lane at its
 * feet) */
int logic_wall_near(void)
{
    if (!start()) return 0;
    uint16_t o = lo(REG_D[0]), info = 0;
    int16_t lane = (int16_t)(-feet(o) - 0x20);
    const struct look_step st[] = {
        {-3, &P_1794A, lane, 0x17A9E}, {3, &P_1794A, lane, 0x17ABC},
        {0, &P_176F8, lane, 0x17ADA}, {0, &P_17778, lane, 0x17AF4},
    };
    int last;
    begin(0x17A8A);
    int wall = probes(o, st, 4, 0, 0x17AFA, &info, &last);
    REG_D[6] = 0;
    REG_D[7] = (uint32_t)(int32_t)st[last].dx;        /* (moveq) */
    set_lo(3, (uint16_t)lane);
    probe_result(o, info, wall);
    return done();
}

/* sub_17AFC: a wall around the guard's feet (a little lower, at its sides, under it) */
int logic_wall_feet(void)
{
    if (!start()) return 0;
    uint16_t o = lo(REG_D[0]), info = 0;
    int16_t lane = (int16_t)(feet(o) - 0x20);
    const struct look_step st[] = {
        {0, &P_1794A, (int16_t)(feet(o) - 0x28), 0x17B0E}, {-3, &P_1794A, lane, 0x17B2A},
        {3, &P_1794A, lane, 0x17B46}, {0, &P_177EC, lane, 0x17B62}, {0, &P_178AA, lane, 0x17B7A},
    };
    int last;
    begin(0x17AFC);
    int wall = probes(o, st, 5, 0, 0x17B80, &info, &last);
    REG_D[6] = 0;
    if (last < 4) REG_D[7] = (uint32_t)(int32_t)st[last].dx;
    else REG_D[7] = 0;
    set_lo(3, (uint16_t)st[last].lane);
    probe_result(o, info, wall);
    return done();
}

/* sub_17C54: the floor under the guard, d6 pixels down (d7 across, then 4 to either
 * side, then under it): Z clear: there is floor */
int logic_floor_under(void)
{
    if (!start()) return 0;
    uint16_t o = lo(REG_D[0]), info = 0;
    int16_t dy = (int16_t)lo(REG_D[6]);
    int16_t lane = (int16_t)(feet(o) + dy - 0x20);
    const struct look_step st[] = {
        {(int16_t)lo(REG_D[7]), &P_17934, lane, 0x17C64}, {-4, &P_17934, lane, 0x17C82},
        {4, &P_17934, lane, 0x17CA0}, {0, &P_177B8, lane, 0x17CBC}, {0, &P_17820, lane, 0x17CD6},
    };
    int last;
    begin(0x17C54);
    int wall = probes(o, st, 5, dy, 0x17CDC, &info, &last);
    if (last == 1 || last == 2) set_lo(7, (uint16_t)st[last].dx);
    else if (last >= 3) REG_D[7] = 0;
    set_lo(3, (uint16_t)lane);
    probe_result(o, info, wall);
    return done();
}

/* sub_17B82: stairs just ahead of the guard ($3706: how far ahead its front is)? Z
 * clear: stairs */
int logic_stairs_ahead(void)
{
    if (!start()) return 0;
    uint16_t o = lo(REG_D[0]), info = 0;
    set_obj16(MAP_CELL_INFO, o, 0);
    int16_t ahead = (int16_t)(obj16(0xFF3706, o) + 8);
    begin(0x17B82);
    if (obj16(GUARD_FACING, o) == 3) branched(0x17BA8, 0x17BAE);
    else { ahead = (int16_t)-ahead; ran_to(0x17BAE); }
    int16_t lane = (int16_t)(feet(o) - 0x20);
    ran_to(0x17BC4);
    probe_run(&P_17934, o, ahead, 0, lane, &info, 1);
    jumped_to(0x17BC4);
    ran_to(0x17BD6);
    uint16_t st = obj16(MAP_CELL_INFO, o) & 0x80;
    REG_D[6] = 0;
    set_lo(2, info);
    set_lo(3, (uint16_t)lane);
    set_lo(4, o);
    FLAG_N = NFLAG_CLEAR; FLAG_Z = st; FLAG_V = VFLAG_CLEAR; FLAG_C = CFLAG_CLEAR;
    scramble((1u << 5) | (0x60u << 8));
    return done();
}

/* sub_17BD8: the guard (on the floor) is in contact with an object (a player) that is
 * not in the animation matching its own ($3E6A) and is in the state $9E9E: Z clear */
int logic_contact_free(void)
{
    if (!start()) return 0;
    uint16_t o = lo(REG_D[0]);
    begin(0x17BD8);
    if (obj16(OBJ_HEIGHT, o)) {
        ran_to(0x17BE6);
    } else {
        branched(0x17BE2, 0x17BEA);
        uint16_t c = obj16(TOUCH_PLAYER, o);
        set_lo(7, c);
        if (!c) {
            branched(0x17BF4, 0x17BE6);
        } else {
            uint16_t want = (uint16_t)(obj16(0xFF3E6A, o) * 2);
            set_lo(6, want);
            if (obj16(OBJ_ANIM, c) == want) {
                branched(0x17C0C, 0x17BE6);
            } else {
                uint16_t v = obj16(0xFF9E9E, c);
                REG_A[6] = 0xFF9E9E;
                flags_of16(v);
                ran_to(0x17C18);
                return done();
            }
        }
    }
    REG_D[7] = 0;                                     /* (moveq #0: Z set) */
    flags_of16(0);
    ran_to(0x17BE8);
    return done();
}

/* sub_17C1A: the guard (at height $10) is in contact with an object not in the
 * animation matching its own: d7 = 1, Z clear; else d7 = 0 */
int logic_contact_up(void)
{
    if (!start()) return 0;
    uint16_t o = lo(REG_D[0]);
    begin(0x17C1A);
    int yes = 0;
    if (obj16(OBJ_HEIGHT, o) != 0x10) {
        ran_to(0x17C2A);
    } else {
        branched(0x17C26, 0x17C2E);
        uint16_t c = obj16(TOUCH_PLAYER, o);
        uint16_t want = (uint16_t)(obj16(0xFF3E6A, o) * 2);
        set_lo(6, want);
        if (obj16(OBJ_ANIM, c) == want) branched(0x17C4E, 0x17C2A);
        else { yes = 1; ran_to(0x17C52); }
    }
    if (!yes) ran_to(0x17C2C);
    REG_D[7] = (uint32_t)yes;
    flags_of16((uint16_t)yes);
    return done();
}

/* sub_17A44: guard d2 is told of player d3 (unless it ignores that now: mask bit 7): it
 * has seen him, stops looking, and its script calls its "alerted" script ($6DF4) */
int logic_guard_told(void)
{
    if (!start()) return 0;
    uint16_t g = lo(REG_D[2]);
    begin(0x17A44);
    if (obj16(GUARD_EVENT_MASK, g) & 0x80) {
        branched(0x17A56, 0x17A84);
    } else {
        set_obj16(GUARD_SEEN, g, lo(REG_D[3]));
        set_obj16(GUARD_LOOKS, g, 0);
        set_obj32(0xFF6E8C, g, obj32(0xFF6DF4, g));
        ran_to(0x17A84);
    }
    ran_to(0x17A88);
    return done();
}

/* ================================================================ touching */

/* sub_1A95C / sub_1A9E8: do objects a (d0) and b (d1) touch? Neither lets shots pass
 * ($406E), (sub_1A95C: at the same height,) and closer across than their half widths
 * ($3706) together, and up and down the view than their depths ($37B2) together. The
 * side b is on: $A3AC (1: a is left of it... 2: right). d7 = 1: they touch. */
static int touching(uint16_t a, uint16_t b, int same_height, uint32_t routine, int *d6_out, int with_rts)
{
    uint32_t r = routine;
    int lvl = same_height ? 0 : -0x12;               /* (sub_1A9E8: no height test: 18 bytes shorter) */
    jumped_to(r);
    int hit = 0;
    int16_t d6 = (int16_t)lo(REG_D[6]);
    if (obj16(OBJ_PASS, a)) { branched(r + 0xC, r + 0x8A + lvl); goto out; }
    if (obj16(OBJ_PASS, b)) { branched(r + 0x14, r + 0x8A + lvl); goto out; }
    if (same_height) {
        d6 = (int16_t)obj16(OBJ_HEIGHT, a);
        if (obj16(OBJ_HEIGHT, a) != obj16(OBJ_HEIGHT, b)) { branched(0x1A982, 0x1A9E6); goto out; }
    }
    {
        int16_t dx = (int16_t)(obj16(OBJ_X, a) - obj16(OBJ_X, b));
        uint16_t side = 2;
        if (dx >= 0) branched(r + 0x3C + lvl, r + 0x46 + lvl);
        else { dx = (int16_t)-dx; side = 1; ran_to(r + 0x46 + lvl); }
        set16(0xFFA3AC, side);
        d6 = dx;
        uint16_t w = (uint16_t)(obj16(0xFF3706, a) + obj16(0xFF3706, b));
        if ((uint16_t)dx >= w) { branched(r + 0x5C + lvl, r + 0x8A + lvl); goto out; }
        int16_t dy = (int16_t)(obj16(OBJ_Y, a) - obj16(OBJ_Y, b));
        if (dy >= 0) branched(r + 0x6E + lvl, r + 0x74 + lvl);
        else { dy = (int16_t)-dy; ran_to(r + 0x74 + lvl); }
        d6 = dy;
        uint16_t h = (uint16_t)(obj16(0xFF37B2, a) + obj16(0xFF37B2, b));
        if ((uint16_t)dy >= h) { branched(r + 0x84 + lvl, r + 0x8A + lvl); goto out; }
        hit = 1;
        ran_to(r + 0x8A + lvl);
    }
out:
    if (with_rts) ran_to(r + 0x8C + lvl);
    if (d6_out) *d6_out = d6;
    return hit;
}

static int logic_touching_any(int same, uint32_t routine)
{
    if (!start()) return 0;
    int d6;
    begin(routine);
    int hit = touching(lo(REG_D[0]), lo(REG_D[1]), same, routine, &d6, 0);
    REG_D[7] = (uint32_t)hit;                         /* (for the callers) */
    set_lo(6, (uint16_t)d6);
    return done();
}

int logic_touching(void)  { return logic_touching_any(1, 0x1A95C); }
int logic_touching2(void) { return logic_touching_any(0, 0x1A9E8); }

/* sub_1A878: the players guard o touches (at its height): bit 0 player 1, bit 1 player
 * 2 (each in play, at state 1: on foot); the side each is on, $A3BC / $A3BE. Time from
 * the jsr at `jsr` to its rts. */
int players_touching(uint16_t o, uint32_t jsr)
{
    static const struct { uint32_t beq, bne, jsr, beq_none, end; } at[2] = {
        {0x1A886, 0x1A88E, 0x1A898, 0x1A8A4, 0x1A8B4}, {0x1A8BA, 0x1A8C2, 0x1A8CE, 0x1A8DA, 0x1A8EC},
    };
    ran_to(jsr + 6);
    jumped_to(0x1A878);
    uint16_t found = 0;
    uint32_t d7 = 0;
    ran_to(0x1A880);
    for (int p = 0; p < 2; p++) {
        uint16_t act = obj16(OBJ_ACTIVE, (uint16_t)(2 * p));
        if (act != 1) {
            branched(act ? at[p].bne : at[p].beq, at[p].end);
            continue;
        }
        ran_to(at[p].jsr + 6);
        int hit = touching(o, (uint16_t)(2 * p), 1, 0x1A95C, 0, 1);
        d7 = (uint32_t)hit;
        jumped_to(at[p].jsr + 6);
        if (!hit) { branched(at[p].beq_none, at[p].end); continue; }
        found |= (uint16_t)(p ? 2 : 1);
        set16(p ? P2_TOUCH_SIDE : P1_TOUCH_SIDE, ram16(0xFFA3AC));
        ran_to(at[p].end);
    }
    ran_to(0x1A8F2);                                  /* (its rts too) */
    jumped_to(jsr + 6);
    REG_D[7] = d7;                                    /* (as the game's code leaves them) */
    REG_D[6] = found;
    return found;
}

/* ================================================================ the players' lane walls */

/* sub_1019E / sub_10224 / sub_102A6 (player 2's: sub_101C8 / sub_1024C / sub_102E4):
 * player p's lane's walls, d3 pixels below his feet (MAP_LANES at (y + d3) / 8), kept
 * in the walls he meets on the floor (d0.b) or standing on something (d1.b): sub_1019E
 * by his height (below 8: the floor), sub_10224 the other way round (at 0: the ones on
 * something), sub_102A6 by his facing (into the screen: by height; 1: the floor; else
 * on something) */
static int lane_walls(int which, int p)
{
    static const uint32_t at[3] = {0x1019E, 0x10224, 0x102A6}, copy[3] = {0x2A, 0x28, 0x3E};
    uint32_t b = p ? copy[which] : 0;
    uint16_t o = (uint16_t)(2 * p);
    begin(at[which] + b);
    uint16_t i = (uint16_t)((uint16_t)((uint16_t)(obj16(OBJ_Y, o) + lo(REG_D[3])) >> 3) << 1);
    set_lo(7, i);
    REG_A[6] = ram32(MAP_LANES);
    REG_D[3] = (REG_D[3] & 0xFFFFFF00u) | (uint8_t)RD8(REG_A[6] + (uint32_t)(int32_t)(int16_t)i);
    int floor;
    uint16_t h = obj16(OBJ_HEIGHT, o);
    if (which == 0) {
        floor = h < 8;
        if (floor) { branched(0x101BC + b, 0x101C4 + b); ran_to(0x101C6 + b); } else ran_to(0x101C2 + b);
    } else if (which == 1) {
        floor = h != 0;
        if (!floor) { branched(0x10240 + b, 0x10248 + b); ran_to(0x1024A + b); } else ran_to(0x10246 + b);
    } else {
        uint8_t f = ram8(P1_FACING + 1 + o);
        REG_D[7] = (REG_D[7] & 0xFFFFFF00u) | f;
        if (!f) {
            branched(0x102C2 + b, 0x102D0 + b);
            floor = h < 8;
            if (floor) { branched(0x102D8 + b, 0x102E0 + b); ran_to(0x102E2 + b); } else ran_to(0x102DE + b);
        } else if (f == 1) { floor = 1; branched(0x102CA + b, 0x102E0 + b); ran_to(0x102E2 + b); }
        else { floor = 0; ran_to(0x102CE + b); branched(0x102CE + b, 0x102DC + b); ran_to(0x102DE + b); }
    }
    int r = floor ? 0 : 1;
    uint8_t v = (uint8_t)(REG_D[r] & REG_D[3]);
    REG_D[r] = (REG_D[r] & 0xFFFFFF00u) | v;
    FLAG_N = NFLAG_8(v); FLAG_Z = v; FLAG_V = VFLAG_CLEAR; FLAG_C = CFLAG_CLEAR;
    return done();
}
int logic_lane_walls(void)          { if (!start()) return 0; return lane_walls(0, 0); }
int logic_lane_walls_low(void)      { if (!start()) return 0; return lane_walls(1, 0); }
int logic_lane_walls_faced(void)    { if (!start()) return 0; return lane_walls(2, 0); }
int logic_lane_walls_p2(void)       { if (!start()) return 0; return lane_walls(0, 1); }
int logic_lane_walls_low_p2(void)   { if (!start()) return 0; return lane_walls(1, 1); }
int logic_lane_walls_faced_p2(void) { if (!start()) return 0; return lane_walls(2, 1); }

/* ================================================================ touching others */

/* (each object in view from `first`, 32 of them (d1, counted down in d2): one of `kind`
 * (any: -1) touching object d0 (sub_1A95C) - d6 it, d7 1; else d7 0) */
struct touch_at { uint32_t start, loop, act, onscr, kind, jsr, found, next, dbra, none; };

static int touching_any_of(const struct touch_at *at, uint16_t first, int kind, int pass_clear)
{
    begin(at->start);
    uint32_t d1 = REG_D[1];
    set_lo(1, first);
    set_lo(2, 0x1F);
    ran_to(at->loop);
    for (;;) {
        uint16_t g = lo(REG_D[1]);
        REG_A[6] = OBJ_ACTIVE;
        if (!obj16(OBJ_ACTIVE, g)) branched(at->act, at->next);
        else if (REG_A[6] = OBJ_ON_SCREEN, !obj16(OBJ_ON_SCREEN, g)) branched(at->onscr, at->next);
        else if (kind >= 0 && (REG_A[6] = ITEM_KIND, obj16(ITEM_KIND, g) != kind)) branched(at->kind, at->next);
        else {
            uint32_t d6 = 0;
            if (pass_clear) {
                /* (sub_1A764: shots pass through it no more, while $A3BA) */
                REG_A[6] = OBJ_PASS;
                set_lo(6, obj16(OBJ_PASS, g));
                if (!ram16(0xFFA3BA)) branched(0x1A79C, 0x1A7A4);
                else { set_obj16(OBJ_PASS, g, 0); ran_to(0x1A7A4); }
                d6 = REG_D[6];
            }
            call(0x1A95C, at->jsr);
            if (pass_clear) {
                REG_D[6] = d6;
                REG_A[6] = OBJ_PASS;
                set_obj16(OBJ_PASS, lo(REG_D[1]), lo(REG_D[6]));
            }
            if (lo(REG_D[7])) {
                branched(at->found, at->found + 0x12);
                set_lo(6, lo(REG_D[1]));
                REG_D[1] = d1;
                ran_to(at->found + 0x18);
                return done();
            }
            ran_to(at->next);
        }
        set_lo(1, (uint16_t)(lo(REG_D[1]) + 2));
        uint16_t c = lo(REG_D[2]);
        set_lo(2, (uint16_t)(c - 1));
        if (!c) { ran_to(at->dbra + 4); break; }
        branched(at->dbra, at->loop);
    }
    REG_D[7] = 0;
    REG_D[1] = d1;
    ran_to(at->none);
    return done();
}

/* sub_1A764: a guard in view touching object d0 (d6), at the same height */
int logic_guard_touching(void)
{
    static const struct touch_at at = {0x1A764, 0x1A770, 0x1A77A, 0x1A788, 0, 0x1A7A8,
                                       0x1A7BE, 0x1A7C2, 0x1A7C4, 0x1A7CE};
    if (!start()) return 0;
    return touching_any_of(&at, 0xC, -1, 1);
}

/* sub_1AD04: a door in view (item kind 4) touching object d0 (d6), at the same height */
int logic_door_touching(void)
{
    static const struct touch_at at = {0x1AD04, 0x1AD10, 0x1AD1A, 0x1AD28, 0x1AD38, 0x1AD3C,
                                       0x1AD44, 0x1AD48, 0x1AD4A, 0x1AD54};
    if (!start()) return 0;
    return touching_any_of(&at, 0x4C, 4, 0);
}
