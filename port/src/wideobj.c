/* Widescreen: the game's objects beside the 4:3 picture.
 *
 * The game only turns objects into sprites while they are within 144 pixels (plus
 * the object's half width) of a view's centre (sub_058B4). In widescreen the side
 * areas would show the level without its props, levers, presses or trap doors,
 * so this module builds the missing sprites itself from the game's object tables,
 * exactly like sub_059C6 does. It only reads RAM: the emulated game is unchanged.
 *
 * Patrol return (option): a guard that leaves the 4:3 picture is put back at its
 * post by the game at once (sub_14232, if the post is off screen), in its neutral
 * state, and is not drawn until it is near the picture again. In widescreen that
 * would look like guards vanishing in plain view. With the option on, a "ghost"
 * of the guard is shown instead, walking back to its post with the walk animation
 * the guard was last seen using, and standing there once it arrives - which is
 * where (and how) the game really has it.
 *
 * Object tables (word per object, index = object * 2):
 *   $2EF6 state (0 = none)     $304E x      $30FA y      $31A6 height above floor
 *   $2FA2 seen by view 1/2     $3252 frame  $32FE mirrored
 *   $3706 half width   $37B2 half height   $3C66 attribute bits
 *   guards (objects 6..6+$6CC2): $9108/$9154/$91A0 post x/y/height
 * Views: centre x $1856 / $229A, centre y $1854 / $2298, camera $1852,$1850 / $2296,$2294.
 * Frames: tbl_24B80 (ROM) -> count-1 (high byte), then per piece dy, dx, size, attr. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "md.h"
#include "wideobj.h"

#define NOBJ 0x56
#define FRAMES_TBL 0x24B80
#define FIRST_GUARD 6

static int16_t r16(uint32_t a) { return (int16_t)((md.ram[a & 0xFFFF] << 8) | md.ram[(a + 1) & 0xFFFF]); }
static uint16_t u16(uint32_t a) { return (uint16_t)r16(a); }

typedef struct {
    uint16_t st, vis, frame, flip, attr;
    int16_t x, y, z, w, h;
} obj_t;

typedef struct {
    int valid;
    obj_t o[NOBJ];
    int16_t cam[2][4];      /* per view: camera y, camera x, centre y, centre x */
    uint16_t active[2];     /* view in use ($2EF6 / $2EF8) */
    int nguards;
    int16_t post[NOBJ][3];  /* guards: post x, y, height */
} snap_t;

static snap_t snaps[3];
static int cur;             /* snaps[cur] = start of this frame, then the two before */

/* ---- what we learn about each guard while the game shows it ---- */
#define SEQ 48
typedef struct {
    uint16_t walk[SEQ]; int nwalk; int walk_dir; uint16_t walk_flip; int speed16;
    uint16_t idle[SEQ]; int nidle;
} anim_t;

typedef struct {
    int seen;               /* was drawn by the game in the previous snapshot */
    int16_t lx, ly, lz;     /* last position drawn by the game */
    uint16_t lframe, lflip;
    int still;              /* frames without moving */
    anim_t a;
    /* ghost */
    int ghost;              /* 1 walking to its post, 2 standing at it, 3 not met yet (at its post) */
    int gx16, gy, gz, gview;
    int gstep;
} guard_t;

static guard_t guards[NOBJ];
static uint32_t level_id;   /* map pointer: new level -> forget everything */

static void take_snapshot(snap_t *s)
{
    s->valid = 1;
    for (int i = 0; i < NOBJ; i++) {
        uint32_t o = i * 2;
        obj_t *b = &s->o[i];
        b->st = u16(0x2EF6 + o);
        b->vis = u16(0x2FA2 + o);
        b->frame = u16(0x3252 + o);
        b->flip = u16(0x32FE + o);
        b->attr = u16(0x3C66 + o);
        b->x = r16(0x304E + o);
        b->y = r16(0x30FA + o);
        b->z = r16(0x31A6 + o);
        b->w = r16(0x3706 + o);
        b->h = r16(0x37B2 + o);
        s->post[i][0] = r16(0x9108 + o);
        s->post[i][1] = r16(0x9154 + o);
        s->post[i][2] = r16(0x91A0 + o);
    }
    s->cam[0][0] = r16(0x1850); s->cam[0][1] = r16(0x1852); s->cam[0][2] = r16(0x1854); s->cam[0][3] = r16(0x1856);
    s->cam[1][0] = r16(0x2294); s->cam[1][1] = r16(0x2296); s->cam[1][2] = r16(0x2298); s->cam[1][3] = r16(0x229A);
    s->active[0] = u16(0x2EF6);
    s->active[1] = u16(0x2EF8);
    s->nguards = r16(0x6CC2) + 1;
    if (s->nguards < 0 || FIRST_GUARD + s->nguards > NOBJ) s->nguards = 0;
}

static int view_on(const snap_t *s, int v)
{
    return s->active[v] != 0 && s->active[v] != 0x8000;
}

/* the game's visibility test (sub_058B4), with extra width */
static int in_range(const snap_t *s, int v, int x, int y, int z, int w, int h, int extra)
{
    int dx = abs(s->cam[v][3] - x);
    if (dx > (uint16_t)(w + 0x90 + extra)) return 0;
    int dy = abs(s->cam[v][2] + 0x10 + z - y);
    return dy <= (uint16_t)(h + 0x28);
}

static int is_guard(const snap_t *s, int i)
{
    return i >= FIRST_GUARD && i < FIRST_GUARD + s->nguards;
}

/* ---- sprite output ---- */
static wobj_sprite *out_list;
static int out_n, out_max;

static void emit_frame(const snap_t *s, int v, uint16_t frame, int x, int y, int z, int flip,
                       uint16_t attr_or, int only_outside, int width)
{
    if (!frame) return;
    uint32_t p = FRAMES_TBL + frame * 4u;
    if (p + 4 > md.rom_size) return;
    p = ((uint32_t)md.rom[p] << 24 | md.rom[p + 1] << 16 | md.rom[p + 2] << 8 | md.rom[p + 3]) & 0xFFFFFF;
    if (p + 2 > md.rom_size) return;
    int n = (md.rom[p] >> 0) + 1;    /* high byte of the first word */
    p += 2;
    int camy = s->cam[v][0], camx = s->cam[v][1];
    int top = v ? 7 : 135;          /* the view's lines */
    for (int k = 0; k < n && out_n < out_max; k++, p += 6) {
        if (p + 6 > md.rom_size) return;
        int dy = (int8_t)md.rom[p], dx = (int8_t)md.rom[p + 1];
        uint16_t size = (uint16_t)(md.rom[p + 2] << 8 | md.rom[p + 3]);
        uint16_t attr = (uint16_t)(md.rom[p + 4] << 8 | md.rom[p + 5]) | attr_or;
        if (z < 0xD) attr |= 0x8000;
        int sy = dy + y - camy + (v ? 0x85 : 0x105) - z + 0x14;
        int sx;
        if (!flip) {
            sx = dx + x - camx + 0x80;
        } else {
            int wd = ((size >> 7) & 0x18) + 8;
            sx = x - (dx + wd) - camx + 0x80;
            attr ^= 0x800;
        }
        wobj_sprite *e = &out_list[out_n];
        e->y = sy - 128;
        e->x = sx - 128;
        e->hsz = ((size >> 10) & 3) + 1;
        e->vsz = ((size >> 8) & 3) + 1;
        e->attr = attr;
        e->ymin = top;
        e->ymax = top + 80;
        e->only_outside = only_outside;
        (void)width;
        out_n++;
    }
}

/* ---- guards: learning and ghosts ---- */
static void push_seq(uint16_t *seq, int *n, uint16_t f)
{
    if (*n < SEQ) { seq[(*n)++] = f; return; }
    memmove(seq, seq + 1, (SEQ - 1) * sizeof(*seq));
    seq[SEQ - 1] = f;
}

static void learn(const snap_t *s, const snap_t *prev)
{
    for (int i = FIRST_GUARD; i < FIRST_GUARD + s->nguards; i++) {
        const obj_t *b = &s->o[i];
        guard_t *g = &guards[i];
        int shown = b->st && (b->vis & 3) && b->frame;
        if (shown) {
            const obj_t *p = &prev->o[i];
            int moved = g->seen && p->y == b->y ? b->x - g->lx : 0;
            if (g->seen && moved && abs(moved) <= 3 && p->frame) {
                if (g->a.walk_dir != (moved > 0 ? 1 : -1)) g->a.nwalk = 0;
                g->a.walk_dir = moved > 0 ? 1 : -1;
                g->a.walk_flip = b->flip;
                push_seq(g->a.walk, &g->a.nwalk, b->frame);
                g->a.speed16 = (g->a.speed16 * 7 + abs(moved) * 16) / 8;
                g->still = 0;
            } else if (g->seen && !moved) {
                /* standing at its post, not yet alarmed */
                if (++g->still > 4 && b->x == s->post[i][0] && b->y == s->post[i][1])
                    push_seq(g->a.idle, &g->a.nidle, b->frame);
            }
            g->seen = 1;
            g->lx = b->x; g->ly = b->y; g->lz = b->z;
            g->lframe = b->frame; g->lflip = b->flip;
            g->ghost = 0;
        } else {
            g->seen = 0;
        }
    }
}

/* a guard of the same size that was seen walking / standing, for guards never seen doing so */
static const anim_t *anim_for(const snap_t *s, int i, int walking)
{
    const anim_t *a = &guards[i].a;
    if (walking ? a->nwalk >= 8 : a->nidle >= 8) return a;
    for (int j = FIRST_GUARD; j < FIRST_GUARD + s->nguards; j++) {
        const anim_t *b = &guards[j].a;
        if (s->o[j].w == s->o[i].w && s->o[j].h == s->o[i].h && (walking ? b->nwalk >= 8 : b->nidle >= 8))
            return b;
    }
    return NULL;
}

static void ghosts(const snap_t *s, const snap_t *prev, int extra, int width)
{
    for (int i = FIRST_GUARD; i < FIRST_GUARD + s->nguards; i++) {
        const obj_t *b = &s->o[i];
        guard_t *g = &guards[i];
        if (!b->st || (b->vis & 3)) { g->ghost = 0; continue; }
        if (!g->ghost && !b->frame && b->x == s->post[i][0] && b->y == s->post[i][1]) {
            /* not met yet: the game keeps it at its post without a picture until it
             * comes near the 4:3 picture - show it standing there */
            g->ghost = 3;
            g->gx16 = b->x * 16; g->gy = b->y; g->gz = b->z;
            g->gview = view_on(s, 0) ? 0 : 1;
            g->gstep = i * 7;
        }
        if (g->ghost == 3 && (b->frame || b->x != s->post[i][0])) g->ghost = 0;
        if (!g->ghost) {
            /* just left the picture: the game has put it back at its post */
            const obj_t *p = &prev->o[i];
            if (!(p->vis & 3) || !p->frame) continue;
            int v = (p->vis & 1) ? 0 : 1;
            /* only if the game really moved it to its post (otherwise it just waits where
             * it is, and is drawn there), and the post is on the same floor */
            if (b->x != s->post[i][0] || b->y != s->post[i][1] || b->x == g->lx) continue;
            if (abs(s->post[i][1] - g->ly) >= 40) continue;
            g->ghost = 1;
            g->gx16 = g->lx * 16;
            g->gy = g->ly;
            g->gz = g->lz;
            g->gview = v;
            g->gstep = 0;
        }
        int v = g->gview;
        if (g->ghost == 3 && view_on(s, 1) && !in_range(s, 0, b->x, b->y, b->z, b->w, b->h, extra))
            v = 1;   /* not met yet: whichever player's view it is in */
        if (!view_on(s, v)) { if (g->ghost != 3) g->ghost = 0; continue; }
        int post = s->post[i][0];
        int gx = g->gx16 / 16;
        const anim_t *walk = anim_for(s, i, 1), *idle = anim_for(s, i, 0);
        if (g->gy != s->post[i][1])     /* change lane on the way */
            g->gy += s->post[i][1] > g->gy ? 1 : -1;
        if (g->ghost == 1) {
            int dir = post > gx ? 1 : -1;
            int sp = walk && walk->speed16 > 0 ? walk->speed16 : 16;
            if (abs(post * 16 - g->gx16) <= sp) {
                g->gx16 = post * 16;
                g->ghost = 2;
            } else {
                g->gx16 += dir * sp;
            }
        }
        gx = g->gx16 / 16;
        /* out of the wide picture: it is at its post now, as far as anyone can tell */
        if (!in_range(s, v, gx, g->gy, g->gz, b->w, b->h, extra)) {
            if (g->ghost == 1) { g->gx16 = post * 16; g->ghost = 2; }
            continue;
        }
        if (g->ghost == 3 && !anim_for(s, i, 0)) {
            continue;   /* no guard like it seen standing yet: nothing to show */
        }
        /* inside the 4:3 picture the game would draw it itself */
        if (in_range(s, v, gx, g->gy, g->gz, b->w, b->h, 0))
            continue;
        uint16_t frame;
        int flip;
        g->gstep++;
        if (g->ghost == 1) {
            int dir = post * 16 > g->gx16 ? 1 : -1;
            if (walk) {
                frame = walk->walk[g->gstep % walk->nwalk];
                flip = dir == walk->walk_dir ? walk->walk_flip : !walk->walk_flip;
            } else {
                frame = g->lframe;
                flip = g->lflip;
            }
        } else {
            if (idle) frame = idle->idle[g->gstep % idle->nidle];
            else if (walk) frame = walk->walk[0];
            else frame = g->lframe;
            flip = 0;
        }
        emit_frame(s, v, frame, gx, g->gy, g->gz, flip, b->attr, 1, width);
    }
}

int wobj_build(int margin, int width, int patrol_return, wobj_sprite *list, int max)
{
    uint32_t id = (uint32_t)u16(0x1838) << 16 | u16(0x183A);
    if (id != level_id) {
        level_id = id;
        memset(guards, 0, sizeof(guards));
        snaps[0].valid = snaps[1].valid = snaps[2].valid = 0;
    }
    cur = (cur + 1) % 3;
    take_snapshot(&snaps[cur]);
    /* the sprite table on screen matches this snapshot (or, around a change, the one
     * before it) */
    const snap_t *s = &snaps[cur], *older = &snaps[(cur + 2) % 3];
    out_list = list; out_n = 0; out_max = max;
    if (!margin || !s->valid || !older->valid)
        return 0;
    learn(s, older);
    int extra = margin + 16;
    if (patrol_return)
        ghosts(s, older, extra, width);
    else
        for (int i = 0; i < NOBJ; i++) guards[i].ghost = 0;
    for (int v = 0; v < 2; v++) {
        if (!view_on(s, v)) continue;
        /* draw order of sub_0582E: state 2 first, then state 1 */
        for (int pass = 2; pass >= 1; pass--)
            for (int i = 0; i < NOBJ; i++) {
                const obj_t *b = &s->o[i];
                /* hidden in either of the last two snapshots: the sprite table on screen
                 * may come from either (pieces it already has are dropped by the renderer) */
                if (b->st != pass || ((b->vis & older->o[i].vis) & (1 << v)) || !b->frame || guards[i].ghost) continue;
                if (!in_range(s, v, b->x, b->y, b->z, b->w, b->h, extra)) continue;
                emit_frame(s, v, b->frame, b->x, b->y, b->z, b->flip != 0, b->attr, 0, width);
            }
    }
    return out_n;
}

void wobj_reset(void)
{
    level_id = 0;
}
