/* The players' views each frame: scrolling them (sub_08576: player 1's view, the wait
 * for the frame's end, player 2's view) and the drawing they ask for, a piece a frame
 * (sub_080FA: the map's columns and rows, the HUD's split, tiles).
 *
 * Both views have the same code, player 2's further on (by VIEW_CODE, TASK_CODE), and
 * their own work memory, player 2's VIEW_RAM further on. */
#include "rt.h"
#include "../video.h"

#define VIEW_RAM     0xA44       /* player 2's view's work memory, from player 1's */
#define VIEW_CODE    0x222       /* sub_08576's code for player 2's view, from player 1's */
#define TASK_CODE    0x23E       /* sub_080FA's code (and its tasks) for player 2's view */

/* player 1's view (player 2's: + VIEW_RAM) */
#define VIEW_Y       0xFF1844    /* the map's row at the top, in pixels */
#define VIEW_X       0xFF1846    /* how far across it has scrolled (from its column) */
#define VIEW_TASKS   0xFF1858    /* (8 words) the drawing asked for: each a task, in turn */
#define VIEW_TASK_AT 0xFF1868    /*   the task looked at first */
#define VIEW_SPLIT   0xFF186A    /* the HUD at the other side */
#define VIEW_LEFT    0xFF186C    /* (the column being drawn is to the left) */
#define VIEW_DX      0xFF186E    /* speeds, twice per frame: across, */
#define VIEW_DY      0xFF1870    /*   down */
#define VIEW_MOVE    0xFF1876    /* the moves asked for: up 1, down 2, left 4, right 8 */
#define VIEW_HELD    0xFF1878    /* the view not moving */
#define VIEW_ROW     0xFF187A
#define VIEW_COLUMN  0xFF187C    /* the map's column at the left, and the one on the right */
#define VIEW_COLUMN2 0xFF1880
#define MAP_ROWS     0xFF2CCE    /* the map's size */
#define MAP_COLUMNS  0xFF2CD0
#define MAP_WIDTH    0xFF2CCC

static uint32_t view_ram(int v, uint32_t a) { return a + (v ? VIEW_RAM : 0); }

/* ---------------------------------------------------------------- the drawing tasks */

#define VIEW_COLUMN_AT  0xFF187E  /* the column's place in the plane, for the left one and */
#define VIEW_COLUMN2_AT 0xFF1882  /*   the right one ($80 wide, 4 bytes a column) */
#define VIEW_TILES   0xFF1884    /* (2 x $500 bytes) the column's tiles for the two planes */
#define MAP_AT       0xFF1838    /* (long) the stage's map, and its blocks' tiles (8 bytes */
#define BLOCKS       0xFF183C    /*   a block: 2 for each plane, 2 rows) */

/* sub_284B0 (player 2's sub_287A0): the map's column at a0 (15 blocks down) into the tiles
 * to draw, each block's two columns for each plane */
static void column_tiles(int v)
{
    uint32_t b = v ? 0x2F0 : 0;
    REG_A[4] = view_ram(v, VIEW_TILES);
    REG_A[3] = view_ram(v, VIEW_TILES + 0x500);
    set_lo(7, 0xE);
    set_lo(6, (uint16_t)(ram16(MAP_WIDTH) * 2));
    ran_to(0x284C6 + b);
    for (;;) {
        set_lo(5, (uint16_t)(mem16(REG_A[0]) << 3));
        uint32_t t = REG_A[1] + (uint32_t)(int16_t)lo(REG_D[5]);
        set16(REG_A[4], mem16(t));     set16(REG_A[3], mem16(t + 2));
        set16(REG_A[4] + 2, mem16(t + 4)); set16(REG_A[3] + 2, mem16(t + 6));
        REG_A[4] += 4; REG_A[3] += 4;
        REG_A[0] += (uint32_t)(int16_t)lo(REG_D[6]);
        uint16_t c = lo(REG_D[7]);
        set_lo(7, (uint16_t)(c - 1));
        if (!c) { ran_to(0x284E0 + b); break; }
        branched(0x284DC + b, 0x284C6 + b);
    }
}
static uint16_t step4(uint16_t at, int dir)          /* (the place in the plane's row) */
{
    return (uint16_t)((at & 0xFF80) + ((at + 4 * dir) & 0x7F));
}

/* sub_28624 / sub_286E2 (player 2's sub_28914 / sub_289D2): the map's column at the right
 * or the left edge into the tiles to draw (the view's row, a block up or down while it
 * moves), handed to the line routine; the edges a column on */
static void column_edge(int v, int left)
{
    uint32_t b = (v ? 0x2F0 : 0) + (left ? 0xBE : 0);
    int dir = left ? -1 : 1;
    set_lo(6, ram16(view_ram(v, VIEW_ROW)));
    if (!ram16(view_ram(v, VIEW_HELD))) branched(0x28630 + b, 0x28648 + b);
    else {
        set_lo(7, 5);
        if (!ram16(view_ram(v, VIEW_SPLIT))) branched(0x2863E + b, 0x28646 + b);
        else { ran_to(0x28642 + b); set_lo(7, 0xFFFB); }
        set_lo(6, (uint16_t)(lo(REG_D[6]) + lo(REG_D[7])));
    }
    set_lo(3, 0);
    set_lo(5, (uint16_t)(ram16(MAP_WIDTH) * 2));
    if ((int16_t)lo(REG_D[6]) >= 0) branched(0x28654 + b, 0x2865C + b);
    else {
        set_lo(6, (uint16_t)-lo(REG_D[6]));
        set_lo(5, (uint16_t)-lo(REG_D[5]));
        ran_to(0x2865C + b);
    }
    for (;;) {                                         /* (the row's place in the map) */
        set_lo(6, (uint16_t)(lo(REG_D[6]) - 1));
        if ((int16_t)lo(REG_D[6]) < 0) { branched(0x2865E + b, 0x28666 + b); break; }
        set_lo(3, (uint16_t)(lo(REG_D[3]) + lo(REG_D[5])));
        ran_to(0x28664 + b);
        branched(0x28664 + b, 0x2865C + b);
    }
    uint32_t col = view_ram(v, left ? VIEW_COLUMN2 : VIEW_COLUMN);
    set_lo(7, (uint16_t)(ram16(col) * 2 + lo(REG_D[3])));
    REG_A[0] = ram32(MAP_AT) + (uint32_t)(int16_t)lo(REG_D[7]);
    REG_A[1] = ram32(BLOCKS);
    ran_to(0x28684 + b);                               /* (jsr sub_284B0, inline) */
    jumped_to(0x284B0 + (v ? 0x2F0 : 0));
    column_tiles(v);
    ran_to(0x284E2 + (v ? 0x2F0 : 0));
    jumped_to(0x28684 + b);
    set32(0xFFC10C + (v ? 4 : 0), v ? 0x8C1A : 0x89C0);
    set16(0xFFC114 + (v ? 4 : 0), ram16(view_ram(v, left ? VIEW_COLUMN2_AT : VIEW_COLUMN_AT)));
    uint32_t c1 = view_ram(v, VIEW_COLUMN), c2 = view_ram(v, VIEW_COLUMN2);
    set16(c1, (uint16_t)(ram16(c1) + dir));
    set16(c1 + 2, step4(ram16(c1 + 2), dir));
    set16(c2, (uint16_t)(ram16(c2) + dir));
    set_lo(7, step4(ram16(c2 + 2), dir));
    set_lo(6, lo(REG_D[7]) & 0x7F);
    set16(c2 + 2, lo(REG_D[7]));
    ran_to(0x286E0 + b);
}
static int column_edge_logic(int v, int left)
{
    if (!start()) return 0;
    begin(0x28624 + (v ? 0x2F0 : 0) + (left ? 0xBE : 0));
    column_edge(v, left);
    return done();
}
int logic_column_right(void)    { return column_edge_logic(0, 0); }
int logic_column_left(void)     { return column_edge_logic(0, 1); }
int logic_column_right_p2(void) { return column_edge_logic(1, 0); }
int logic_column_left_p2(void)  { return column_edge_logic(1, 1); }

/* (task 0: the map's column at the view's edge drawn: sub_28624 / sub_286E2, player 2's
 * sub_28914 / sub_289D2) */
static void task_column(int v, uint32_t b)
{
    if (!ram16(view_ram(v, VIEW_LEFT))) {
        ran_to(0x08166 + b);                          /* (jsr, inline) */
        jumped_to(0x28624 + (v ? 0x2F0 : 0));
        column_edge(v, 0);
        ran_to(0x286E2 + (v ? 0x2F0 : 0));
        jumped_to(0x08166 + b);
        branched(0x08166 + b, 0x0816E + b);
    } else {
        branched(0x0815C + b, 0x08168 + b);
        ran_to(0x0816E + b);
        jumped_to(0x286E2 + (v ? 0x2F0 : 0));
        column_edge(v, 1);
        ran_to(0x287A0 + (v ? 0x2F0 : 0));
        jumped_to(0x0816E + b);
    }
    REG_A[0] = view_ram(v, VIEW_TASKS);
    set16(view_ram(v, VIEW_TASKS), 0);
    ran_to(0x0817A + b);
}

/* (tasks 1 and 2: the HUD's split, its two words at $C114 / $C118 and the line routine
 * at $C10C / $C110, for one half then the other) */
static void task_split(int v, uint32_t b, int second)
{
    uint32_t c = second ? 0x3E : 0;                   /* (task 2's code, from task 1's) */
    static const uint16_t W[2][2][2][2] = {           /* [view][second][split][word] */
        {{{0xC500, 0xCA00}, {0xC500, 0xC000}}, {{0xC000, 0xC500}, {0xCA00, 0xC500}}},
        {{{0xD600, 0xDB00}, {0xD600, 0xD100}}, {{0xD100, 0xD600}, {0xDB00, 0xD600}}},
    };
    static const uint32_t LINE[2][2] = {{0x8B08, 0x8B28}, {0x8D62, 0x8D82}};
    uint32_t a0 = 0xFFC114 + 4 * v;
    int split = ram16(view_ram(v, VIEW_SPLIT)) != 0;
    if (!split) { ran_to(0x08192 + b + c); branched(0x08192 + b + c, 0x0819C + b + c); }
    else { branched(0x08186 + b + c, 0x08194 + b + c); ran_to(0x0819C + b + c); }
    set16(a0, W[v][second][split][0]);
    set16(a0 + 2, W[v][second][split][1]);
    set32(0xFFC10C + 4 * v, LINE[v][second]);
    uint32_t t = view_ram(v, VIEW_TASKS);
    REG_A[0] = t;
    set16(t + 2 + 2 * second, 0);
    set16(t + 4 + 2 * second, 1);
    ran_to(0x081B8 + b + c);
}

/* (tasks 3 and 4: a row of the map drawn (sub_284E2 / sub_287D2) from the column at
 * the right (wrapping round the map's width; task 4: 10 on, past the map's width from
 * its start) and the row (10 down with the HUD on that side), 40 lines down with task 4) */
static void task_row(int v, uint32_t b, int second)
{
    /* (task 4's code has its x test longer: its places past it, from task 3's) */
    uint32_t c = second ? 0x5E : 0, e = second ? 0x6E : 0;
    REG_A[0] = 0xFF2CD4;
    uint16_t x = ram16(view_ram(v, VIEW_COLUMN2));
    set_lo(7, x);
    if (!second) {
        if (!(x & 0x8000)) branched(0x08202 + b, 0x0820C + b);
        else { x = (uint16_t)(x + ram16(MAP_WIDTH)); ran_to(0x0820C + b); }
    } else {
        x = (uint16_t)(x + 0xA);
        if (x < ram16(MAP_WIDTH)) branched(0x08264 + b, 0x0826A + b);
        else { x = 0; REG_D[7] = 0; ran_to(0x0826A + b); }
    }
    set_lo(7, x);
    set16(0xFF2CD4, x);
    uint16_t y = ram16(view_ram(v, VIEW_ROW));
    if (!ram16(view_ram(v, VIEW_SPLIT))) branched(0x0821C + b + c, 0x08224 + b + c);
    else { y = (uint16_t)(y + 0xA); ran_to(0x08224 + b + c); }
    set16(0xFF2CD6, y);
    uint16_t line = ram16(view_ram(v, 0xFF1882)) & 0x7F;
    if (second) {
        line = (uint16_t)(line + 0x28);
        if (line < 0x80) branched(0x08298 + b, 0x082A0 + b);
        else { line = (uint16_t)(line - 0x80); ran_to(0x082A0 + b); }
    }
    set16(0xFF2CD8, line);
    set_lo(7, line);
    call(v ? 0x287D2 : 0x284E2, 0x08236 + b + e);
    uint32_t t = view_ram(v, VIEW_TASKS);
    REG_A[0] = t;
    set16(t + 6 + 2 * second, 0);
    set16(t + 8 + 2 * second, 1);
    ran_to(0x0824E + b + e);
}

/* (task 5: the line routine for the split's end) */
static void task_split_end(int v, uint32_t b)
{
    set32(0xFFC10C + 4 * v, v ? 0x8DAA : 0x8B50);
    uint32_t t = view_ram(v, VIEW_TASKS);
    REG_A[0] = t;
    set16(t + 0xA, 0);
    ran_to(0x082D2 + b);
}

/* (task 6: the tiles the player's code left at $5712 / $57BC - where from, the video
 * command, 32 bytes times $571A + 1 - into video memory; then one of up to 4 more
 * waiting ($5724) taken) */
static void task_tiles(int v, uint32_t b)
{
    uint32_t p = v ? 0xAA : 0;
    uint32_t a6 = ram32(0xFF5712 + p);
    video_at_long(ram32(0xFF5716 + p));
    set_lo(7, ram16(0xFF571A + p));
    REG_A[0] = 0xC00000;
    ran_to(0x082EE + b);
    for (;;) {
        for (int k = 0; k < 8; k++, a6 += 4) {
            uint32_t w = mem32(a6);
            video_put((uint16_t)(w >> 16));
            video_put((uint16_t)w);
        }
        uint16_t c = lo(REG_D[7]);
        set_lo(7, (uint16_t)(c - 1));
        if (!c) { ran_to(0x08302 + b); break; }
        branched(0x082FE + b, 0x082EE + b);
    }
    REG_A[6] = a6;
    uint32_t t = view_ram(v, VIEW_TASKS);
    REG_A[0] = t;
    set16(t + 0xC, 0);
    if (!ram16(0xFF5724 + p)) { branched(0x08312 + b, 0x08336 + b); goto out; }
    REG_A[6] = 0xFF571C + p;
    set_lo(6, 3);
    for (;;) {
        set_lo(7, ram16(REG_A[6]));
        REG_A[6] += 2;
        if (lo(REG_D[7])) { branched(0x08322 + b, 0x0832A + b); break; }
        uint16_t c = lo(REG_D[6]);
        set_lo(6, (uint16_t)(c - 1));
        if (!c) { ran_to(0x0832A + b); break; }
        branched(0x08326 + b, 0x08320 + b);
    }
    set16(0xFF5724 + p, (uint16_t)(ram16(0xFF5724 + p) - 1));
    REG_A[5] = view_ram(v, 0xFF1864);
    ran_to(0x08336 + b);
out:
    ran_to(0x08338 + b);                              /* (its rts) */
}

/* sub_080FA: on even frames player 1's view, on odd ones player 2's: the first drawing
 * task asked for, from the one after the last (each a piece of the drawing the
 * scrolling asks for, in order: column, the split, rows, tiles) */
int logic_view_tasks(void)
{
    if (!start()) return 0;
    begin(0x080FA);
    int v = ram16(0xFF17A2) & 1;
    set_lo(0, (uint16_t)v);
    uint32_t b = v ? TASK_CODE : 0;
    if (v) { branched(0x08104, 0x08338); set_lo(0, 1); }
    ran_to(0x08108 + b);
    uint32_t tasks = view_ram(v, VIEW_TASKS), at = view_ram(v, VIEW_TASK_AT);
    REG_A[0] = tasks;
    REG_A[1] = at;
    set_lo(0, 7);
    for (;;) {
        uint16_t k = ram16(at);
        set_lo(1, (uint16_t)(k * 2));
        if (ram16(tasks + (int16_t)(uint16_t)(k * 2))) {
            set_lo(1, (uint16_t)(k * 4));
            REG_A[0] = rom32(0x0813A + b + (int16_t)(uint16_t)(k * 4));
            uint32_t jsr = 0x0812A + b, task = REG_A[0] - b;
            if (task < 0x08156 || task > 0x082D2) call(REG_A[0], jsr);   /* (not a task) */
            else {
                ran_to(jsr + 2);
                jumped_to(REG_A[0]);
                switch (task) {
                case 0x08156: task_column(v, b); break;
                case 0x0817A: task_split(v, b, 0); break;
                case 0x081B8: task_split(v, b, 1); break;
                case 0x081F6: task_row(v, b, 0); break;
                case 0x0824E: task_row(v, b, 1); break;
                case 0x082BC: task_split_end(v, b); break;
                default:      task_tiles(v, b); break;
                }
                jumped_to(jsr + 2);
            }
            branched(0x0812C + b, 0x08138 + b);
            break;
        }
        branched(0x08120 + b, 0x0812E + b);
        set16(at, (uint16_t)((k + 1) & 7));
        uint16_t c = lo(REG_D[0]);
        set_lo(0, (uint16_t)(c - 1));
        if (!c) { ran_to(0x08138 + b); break; }
        branched(0x08134 + b, 0x08118 + b);
    }
    return done();
}

/* ---------------------------------------------------------------- scrolling */

/* (view v scrolls by the moves asked for: up and down a pixel per half speed, the
 * map's row redrawn (sub_285A6 / sub_28896) unless at the map's top or bottom; across
 * likewise, a column at a time drawn as each 16 pixels go by, stopping at the map's
 * sides. Its code at b from player 1's.) */
static void view_scroll(int v)
{
    uint32_t b = v ? VIEW_CODE : 0;
    uint16_t top = v ? 0x159 : 0xFFC9;               /* (the top the view stops at) */
#define R(a) view_ram(v, a)
    if (ram16(R(VIEW_HELD))) { branched(0x08588 + b, 0x08796 + b); goto end; }
    uint16_t d1 = (uint16_t)((uint16_t)((ram16(0xFF17A2) & 1) + ram16(R(VIEW_DY))) >> 1);
    set_lo(1, d1);
    set16(R(0xFF1874), d1);
    uint16_t move = ram16(R(VIEW_MOVE));
    set_lo(0, move);
    int dy = 0;
    if (move & 1) {
        if (ram16(R(VIEW_ROW)) != 0xFFFB) branched(0x085BA + b, 0x085CA + b);
        else if (ram16(R(VIEW_Y)) == top) { branched(0x085C6 + b, 0x08636 + b); goto across; }
        else ran_to(0x085CA + b);
        dy = -1;
    } else {
        branched(0x085AE + b, 0x085EE + b);
        if (!(move & 2)) { branched(0x085F2 + b, 0x08636 + b); goto across; }
        set_lo(7, (uint16_t)(ram16(MAP_ROWS) - 0xA));
        if (lo(REG_D[7]) != ram16(R(VIEW_ROW))) branched(0x08606 + b, 0x08616 + b);
        else if (ram16(R(VIEW_Y)) == top) { branched(0x08612 + b, 0x08636 + b); goto across; }
        else ran_to(0x08616 + b);
        dy = 1;
    }
    {
        uint16_t d = (uint16_t)(dy * d1);
        set16(R(VIEW_Y), (uint16_t)(ram16(R(VIEW_Y)) + d));
        set16(R(0xFF1850), (uint16_t)(ram16(R(0xFF1850)) + d));
        set16(R(0xFF1854), (uint16_t)(ram16(R(0xFF1854)) + d));
        uint32_t s1 = REG_D[1];
        call(v ? 0x28896 : 0x285A6, (dy < 0 ? 0x085E0 : 0x0862C) + b);
        REG_D[1] = s1;
        if (dy < 0) branched(0x085EA + b, 0x08636 + b); else ran_to(0x08636 + b);
    }
across:
    if (ram16(R(VIEW_X)) & 1) {
        REG_D[1] = 1;
        d1 = 1;
        branched(0x08646 + b, 0x08660 + b);
    } else {
        branched(0x08640 + b, 0x08648 + b);
        d1 = (uint16_t)((uint16_t)((ram16(0xFF17A2) & 1) + ram16(R(VIEW_DX))) >> 1);
        set_lo(1, d1);
        set16(R(0xFF1872), d1);
        ran_to(0x08660 + b);
    }
    move = ram16(R(VIEW_MOVE));
    set_lo(0, move);
    if (!move) { branched(0x08666 + b, 0x08796 + b); goto end; }
    {
        int left = (move & 4) != 0;
        if (!left && !(move & 8)) { branched(0x0866E + b, 0x08706 + b); branched(0x0870A + b, 0x08796 + b); goto end; }
        uint32_t c = left ? 0 : 0x9C;                 /* (the move right's code, from the left's) */
        if (!left) branched(0x0866E + b, 0x08706 + b);
        uint16_t col = ram16(R(left ? VIEW_COLUMN : VIEW_COLUMN2));
        REG_A[0] = R(VIEW_TASKS);
        /* (a column being drawn counts as there already) */
        if (!ram16(R(VIEW_TASKS))) branched(0x08682 + b + c, 0x0869A + b + c);
        else {
            int16_t step = ram16(R(VIEW_LEFT)) ? -1 : 1;
            if (!ram16(R(VIEW_LEFT))) branched(0x08690 + b + c, 0x08698 + b + c);
            else ran_to(0x08698 + b + c);
            col = (uint16_t)(col + step);
            set_lo(left ? 6 : 7, (uint16_t)step);
        }
        set_lo(left ? 7 : 6, col);
        uint16_t edge = left ? ram16(MAP_COLUMNS) : 0xFFFE;
        if (left) set_lo(6, edge);
        if (col != edge) branched((left ? 0x086A2 : 0x0873A) + b, (left ? 0x086B4 : 0x0874C) + b);
        else {
            set_lo(7, ram16(R(VIEW_X)) & 0xF);
            if (!lo(REG_D[7])) { branched((left ? 0x086B0 : 0x08748) + b, 0x08796 + b); goto end; }
            ran_to((left ? 0x086B4 : 0x0874C) + b);
        }
        uint16_t d = left ? (uint16_t)-d1 : d1;
        set16(R(VIEW_X), (uint16_t)(ram16(R(VIEW_X)) + d));
        set16(R(0xFF1852), (uint16_t)(ram16(R(0xFF1852)) - d));
        set16(R(0xFF1856), (uint16_t)(ram16(R(0xFF1856)) - d));
        uint16_t x = ram16(R(VIEW_X));
        if (!(x & 0x8000)) branched((left ? 0x086CC : 0x08764) + b, (left ? 0x086D2 : 0x0876A) + b);
        else { x = (uint16_t)-x; ran_to((left ? 0x086D2 : 0x0876A) + b); }
        x = (uint16_t)(x >> 4);
        x = left ? (uint16_t)(x + 0x11) : (uint16_t)(x - 2);
        set_lo(7, x);
        if (x == (left ? ram16(R(VIEW_COLUMN)) : col)) { branched((left ? 0x086DE : 0x08770) + b, 0x08796 + b); goto end; }
        set_lo(7, ram16(R(VIEW_X)) & 0xF);
        if (lo(REG_D[7])) { branched((left ? 0x086EC : 0x0877E) + b, 0x08796 + b); goto end; }
        /* a whole column scrolled by: the next one asked for */
        REG_A[0] = R(VIEW_TASKS);
        set16(R(VIEW_TASKS), 1);
        set16(R(VIEW_LEFT), left ? 0 : 1);
        if (left) branched(0x08702 + b, 0x08796 + b); else ran_to(0x08796 + b);
    }
end:
    set16(R(VIEW_MOVE), 0);
#undef R
}

/* sub_08576, to the wait for the frame's end: the frame counted, the drawing asked for
 * (sub_080FA), player 1's view scrolled; then the game's own wait loop */
int logic_views_frame(void)
{
    if (!start()) return 0;
    begin(0x08576);
    set16(0xFF17A2, (uint16_t)(ram16(0xFF17A2) + 1));
    call(0x080FA, 0x0857C);
    view_scroll(0);
    ran_to(0x0879C);
    return jump_to(0x0879C);
}

/* sub_08576 after the wait: player 2's view scrolled */
int logic_views_frame2(void)
{
    if (!start()) return 0;
    begin(0x087A4);
    view_scroll(1);
    ran_to(0x089BE);
    return done();
}

/* ---------------------------------------------------------------- map columns */

#define COLUMN_JOBS  0xFF2CF4    /* (8 of $40 bytes) map columns waiting to be drawn: in use,
                                    ... +$E how many more times */
#define COLUMN_BUSY  0xFF2EF4    /* a column drawn this frame */

/* sub_017D8 (in the frame's interrupt): the first column job waiting drawn (sub_01836:
 * two rows of its tiles); done when its count runs out */
int logic_column_jobs(void)
{
    if (!start()) return 0;
    begin(0x017D8);
    set16(COLUMN_BUSY, 0);
    uint32_t a0 = COLUMN_JOBS;
    set_lo(0, 7);
    ran_to(0x017E8);
    for (;;) {
        if (ram16(a0)) {
            set16(COLUMN_BUSY, 1);
            REG_A[0] = a0;
            call(0x01836, 0x017FA);
            REG_A[0] = a0;
            uint16_t left = (uint16_t)(ram16(a0 + 0xE) - 1);
            set16(a0 + 0xE, left);
            if (left) branched(0x01808, 0x01818);
            else { set16(a0, 0); branched(0x0180E, 0x01818); }
            break;
        }
        branched(0x017EA, 0x01810);
        a0 += 0x40;
        REG_A[0] = a0;
        uint16_t c = lo(REG_D[0]);
        set_lo(0, (uint16_t)(c - 1));
        if (!c) { ran_to(0x01818); break; }
        branched(0x01814, 0x017E8);
    }
    return done();
}

/* sub_017AC: a map column job (from a6: where from, the tiles' table, the video address,
 * the count, how many times; d7 the step) into the first free slot (none: nothing) */
int logic_column_job_add(void)
{
    if (!start()) return 0;
    begin(0x017AC);
    uint32_t a0 = COLUMN_JOBS;
    set_lo(0, 7);
    ran_to(0x017B6);
    for (;;) {
        if (!ram16(a0)) break;
        a0 += 0x40;
        uint16_t c = lo(REG_D[0]);
        set_lo(0, (uint16_t)(c - 1));
        if (!c) { REG_A[0] = a0; ran_to(0x017C4); return done(); }
        ran_to(0x017C0);
        branched(0x017C0, 0x017B6);
    }
    branched(0x017B8, 0x017C6);
    uint32_t a6 = REG_A[6];
    set16(a0, 1);
    set32(a0 + 2, mem32(a6));
    set32(a0 + 6, mem32(a6 + 4));
    set16(a0 + 0xA, mem16(a6 + 8));
    set16(a0 + 0xC, mem16(a6 + 10));
    set16(a0 + 0xE, mem16(a6 + 12));
    set16(a0 + 0x10, lo(REG_D[7]));
    REG_A[0] = a0 + 0x10;
    REG_A[6] = a6 + 14;
    ran_to(0x017D6);
    return done();
}

/* The views when a player goes out (each stage's pair of routines, table 0x4566 / 0x459A
 * by stage; sub_0493C / sub_049D0 for the 5th): the first columns asked for again
 * (sub_017AC; each job's data 14 bytes, player 2's after player 1's), and where the
 * view is put back on the map; the kept place set the same, or copied (`copy`). Player
 * 2's map words are player 1's + $1100. */
static const struct view_reset {
    uint32_t p1, p2, jobs;
    uint8_t njobs, copy;
    uint16_t column, column_word, column2, row, row_word, left, top, cx, cy, x;
} VIEW_RESET[10] = {
    {0x04610, 0x04690, 0x0565E, 1, 0, 0x11, 0xC044, 0xFFFE, 0x05, 0xC078, 0x020, 0x0C0, 0x0A0, 0x0C8, 0x0000},
    {0x0471E, 0x0479E, 0x0567A, 1, 0, 0x11, 0xC044, 0xFFFE, 0x05, 0xC078, 0x020, 0x0C0, 0x0A0, 0x0C8, 0x0000},
    {0x0482C, 0x048AC, 0x05696, 1, 0, 0x11, 0xC044, 0xFFFE, 0x0F, 0xC078, 0x020, 0x160, 0x0A0, 0x168, 0x0000},
    {0x0493C, 0x049D0, 0x056B2, 2, 1, 0x2B, 0xC02C, 0x0018, 0x0F, 0xC060, 0x1CC, 0x160, 0x24C, 0x168, 0xFE54},
    {0x04A72, 0x04AF2, 0x056EA, 1, 0, 0x11, 0xC044, 0xFFFE, 0x00, 0xC078, 0x020, 0x070, 0x0A0, 0x078, 0x0000},
    {0x04B80, 0x04C00, 0x05706, 1, 0, 0x11, 0xC044, 0xFFFE, 0x0A, 0xC078, 0x020, 0x110, 0x0A0, 0x118, 0x0000},
    {0x04C90, 0x04D10, 0x05722, 1, 0, 0x11, 0xC044, 0xFFFE, 0x0F, 0xC078, 0x020, 0x160, 0x0A0, 0x168, 0x0000},
    {0x04D9E, 0x04E22, 0x0573E, 1, 1, 0x54, 0xC050, 0x0041, 0xFFFB, 0xC004, 0x450, 0x070, 0x4D0, 0x078, 0xFBD0},
    {0x04EB4, 0x04F34, 0x0575A, 1, 0, 0x11, 0xC044, 0xFFFE, 0x0F, 0xC078, 0x020, 0x160, 0x0A0, 0x168, 0x0000},
    {0x04FC4, 0x05044, 0x05776, 1, 0, 0x11, 0xC044, 0xFFFE, 0x0F, 0xC078, 0x020, 0x160, 0x0A0, 0x168, 0x0000},
};

static int view_reset(int i, int v)
{
    const struct view_reset *t = &VIEW_RESET[i];
    uint32_t r = v ? t->p2 : t->p1;
    uint16_t hi = v ? 0x1100 : 0;
#define R(a) view_ram(v, a)
    begin(r);
    for (int k = 0; k < t->njobs; k++) {
        set_lo(7, ram16(MAP_WIDTH));
        REG_A[6] = t->jobs + 0xE * (v ? t->njobs : 0) + 0xE * k;
        call(0x017AC, r + 0xA + 0x10 * k);
    }
    set16(R(VIEW_COLUMN), t->column);
    set16(R(0xFF187E), (uint16_t)(t->column_word + hi));
    set16(R(VIEW_COLUMN2), t->column2);
    set16(R(VIEW_ROW), t->row);
    set16(R(0xFF1882), (uint16_t)(t->row_word + hi));
    set16(R(0xFF1852), t->left);
    set16(R(0xFF1850), t->top);
    set16(R(0xFF1856), t->cx);
    set16(R(0xFF1854), t->cy);
    set16(R(VIEW_Y), v ? 0x159 : 0xFFC9);
    set16(R(VIEW_X), t->x);
    set16(R(0xFF1848), ram16(R(VIEW_Y)));
    set16(R(0xFF184A), ram16(R(VIEW_X)));
    set16(R(0xFF2284), 0);
    flags_of16(0);
    ran_to(r + (t->njobs == 2 ? 0x92 : t->copy ? 0x82 : 0x7E));
#undef R
    return done();
}
#define VIEW_RESET_FN(i) \
    int logic_view_reset##i##_p1(void) { if (!start()) return 0; return view_reset(i, 0); } \
    int logic_view_reset##i##_p2(void) { if (!start()) return 0; return view_reset(i, 1); }
VIEW_RESET_FN(0) VIEW_RESET_FN(1) VIEW_RESET_FN(2) VIEW_RESET_FN(3) VIEW_RESET_FN(4)
VIEW_RESET_FN(5) VIEW_RESET_FN(6) VIEW_RESET_FN(7) VIEW_RESET_FN(8) VIEW_RESET_FN(9)
