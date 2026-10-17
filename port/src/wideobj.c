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
 * where (and how) the game really has it. A guard knocked down when it leaves is
 * put back a little later, still lying (it is drawn so until met again): its ghost
 * gets up, walks back and stands at its post too. A post on another floor: the
 * ghost is standing there.
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
#include "gamemods.h"
#include "settings.h"

#define NOBJ 0x56
#define FRAMES_TBL frames_tbl()
/* the sprite frame table (0x24B80); a mod may move it (the game reads it with
 * lea (table).l,a6 at 0x59E4) */
static uint32_t frames_tbl(void)
{
    uint32_t a = (uint32_t)md.rom[0x59E6] << 24 | md.rom[0x59E7] << 16 | md.rom[0x59E8] << 8 | md.rom[0x59E9];
    return a < md.rom_size ? a : 0x24B80;
}
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
#define GHOST_SPEED 3       /* ghosts walk back three times as fast as guards walk */
#define RECOVER_FRAMES 60   /* a knocked down ghost: 40 frames down, 20 standing */
#define LEFT_FRAMES 120     /* how long after leaving the picture the game may put it back */

static int in_seq(const uint16_t *seq, int n, uint16_t f)
{
    for (int i = 0; i < n; i++)
        if (seq[i] == f) return 1;
    return 0;
}
typedef struct {
    uint16_t walk[SEQ]; int nwalk; int walk_dir; uint16_t walk_flip; int speed16;
    uint16_t idle[SEQ]; int nidle;
} anim_t;

typedef struct {
    int seen;               /* was drawn by the game in the previous snapshot */
    int16_t lx, ly, lz;     /* last position drawn by the game */
    uint16_t lframe, lflip;
    int lview;              /* the view it was last drawn in */
    int left;               /* frames since it left the picture (0: not lately) */
    int still;              /* frames without moving */
    int age;                /* frames it has been drawn by the game */
    anim_t a;
    /* ghost */
    int ghost;              /* 1 walking to its post, 2 standing at it, 3 not met yet (at its post) */
    int gx16, gy, gz, gview;
    int gstep;
    int recover;            /* frames left before it is up again (was knocked down) */
} guard_t;

static guard_t guards[NOBJ];

/* Doors (state 2, 20x20 objects) rest on a frame R; when someone goes through, the game
 * shows R+1 for a few frames, R+2 (open) while they pass, then R+1 again. Ghosts open
 * the doors in the side areas the same way. */
typedef struct { int phase, timer; } door_t;
static door_t doors[NOBJ];
static int ghost_x[NOBJ], ghost_y[NOBJ], ghost_dir[NOBJ], ghost_on[NOBJ];   /* walking ghosts this frame */

static int is_door(const obj_t *b) { return b->st == 2 && b->w == 20 && b->h == 20; }

static void doors_update(const snap_t *s)
{
    for (int i = 0; i < NOBJ; i++) {
        const obj_t *b = &s->o[i];
        door_t *d = &doors[i];
        if (!is_door(b)) { d->phase = 0; continue; }
        /* a ghost opens the door just before reaching it and shuts it right behind */
        int near = 0;
        for (int j = 0; j < NOBJ && !near; j++) {
            if (!ghost_on[j] || abs(ghost_y[j] - b->y) >= 40) continue;
            int ahead = (b->x - ghost_x[j]) * ghost_dir[j];   /* > 0: door still ahead */
            near = ahead < 14 && ahead > -8;
        }
        switch (d->phase) {
        case 0: if (near) { d->phase = 1; d->timer = 2; } break;
        case 1: if (--d->timer <= 0) d->phase = 2; break;
        case 2: if (!near) { d->phase = 3; d->timer = 2; } break;
        default: if (--d->timer <= 0) d->phase = 0; break;
        }
    }
}

static uint16_t door_frame(int i, uint16_t frame)
{
    static const int add[4] = {0, 1, 2, 1};
    return (uint16_t)(frame + add[doors[i].phase]);
}
static uint32_t level_id;   /* map pointer: new level -> forget everything */

static void take_snapshot(snap_t *s)
{
    s->valid = 1;
    for (int i = 0; i < NOBJ; i++) {
        uint32_t o = i * 2;
        obj_t *b = &s->o[i];
        b->st = u16(0x2EF6 + o);
        /* with Floor Alert the game runs more guards than it draws: what counts here is
         * whether the game made sprites for it */
        b->vis = settings.floor_alert ? game_drawn_vis[i] : u16(0x2FA2 + o);
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
            /* guards face left unless mirrored; moving the other way is being knocked back */
            int forward = (moved > 0) == (b->flip != 0);
            if (g->seen && moved && abs(moved) <= 3 && p->frame && forward) {
                if (g->a.walk_dir != (moved > 0 ? 1 : -1)) g->a.nwalk = 0;
                g->a.walk_dir = moved > 0 ? 1 : -1;
                g->a.walk_flip = b->flip;
                push_seq(g->a.walk, &g->a.nwalk, b->frame);
                g->a.speed16 = g->a.speed16 ? (g->a.speed16 * 7 + abs(moved) * 16) / 8 : abs(moved) * 16;
                g->still = 0;
            } else if (g->seen && !moved) {
                /* standing at its post, not yet alarmed */
                /* only right after it is first met, before anything happened to it
                 * (a guard shot at its post must not teach "lying flat" as standing) */
                if (++g->still > 4 && g->age < 60 && b->x == s->post[i][0] && b->y == s->post[i][1])
                    push_seq(g->a.idle, &g->a.nidle, b->frame);
            }
            g->seen = 1;
            g->age++;
            g->lx = b->x; g->ly = b->y; g->lz = b->z;
            g->lframe = b->frame; g->lflip = b->flip;
            g->lview = (b->vis & 1) ? 0 : 1;
            g->ghost = 0;
            g->left = 0;
        } else {
            if (g->seen) g->left = 1;
            else if (g->left && ++g->left > LEFT_FRAMES) g->left = 0;
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
        /* same size and colours: the same kind of guard */
        if (s->o[j].w == s->o[i].w && s->o[j].h == s->o[i].h && s->o[j].attr == s->o[i].attr &&
            (walking ? b->nwalk >= 8 : b->nidle >= 8))
            return b;
    }
    return NULL;
}

static void ghosts(const snap_t *s, int extra, int width)
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
            /* it left the picture and the game has put it back at its post (at once, or
             * a little later - a guard knocked down keeps lying there, as the game has
             * it, until met again) */
            if (!g->left || b->x != s->post[i][0] || b->y != s->post[i][1]) continue;
            g->left = 0;
            if (b->x == g->lx && b->y == g->ly) continue;   /* it was there already */
            g->gview = g->lview;
            g->gz = g->lz;
            g->gstep = 0;
            g->recover = 0;
            if (abs(s->post[i][1] - g->ly) >= 40) {
                /* its post is on another floor: it is standing there */
                g->ghost = 2;
                g->gx16 = s->post[i][0] * 16;
                g->gy = s->post[i][1];
                g->gz = b->z;
            } else {
                /* it walks back from where it was last seen */
                g->ghost = 1;
                g->gx16 = g->lx * 16;
                g->gy = g->ly;
                /* knocked down or getting up when it left: it stands up before walking */
                const anim_t *wa = anim_for(s, i, 1), *ia = anim_for(s, i, 0);
                g->recover = in_seq(wa ? wa->walk : NULL, wa ? wa->nwalk : 0, g->lframe) ||
                             in_seq(ia ? ia->idle : NULL, ia ? ia->nidle : 0, g->lframe) ? 0 : RECOVER_FRAMES;
            }
        }
        int v = g->gview;
        if (g->ghost == 3 && view_on(s, 1) && !in_range(s, 0, b->x, b->y, b->z, b->w, b->h, extra))
            v = 1;   /* not met yet: whichever player's view it is in */
        if (!view_on(s, v)) { if (g->ghost != 3) g->ghost = 0; continue; }
        int post = s->post[i][0];
        int gx = g->gx16 / 16;
        const anim_t *walk = anim_for(s, i, 1), *idle = anim_for(s, i, 0);
        if (g->ghost == 1 && g->recover > 0)
            g->recover--;
        else if (g->gy != s->post[i][1])     /* change lane on the way */
            g->gy += s->post[i][1] > g->gy ? 1 : -1;
        if (g->ghost == 1 && !g->recover) {
            int dir = post > gx ? 1 : -1;
            int sp = GHOST_SPEED * (walk && walk->speed16 > 0 ? walk->speed16 : 16);
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
        if (g->ghost == 1 && g->recover > RECOVER_FRAMES / 3) {
            frame = g->lframe;            /* still down */
            flip = g->lflip != 0;
        } else if (g->ghost == 1 && g->recover) {
            /* up on its feet, turning towards its post */
            frame = idle ? idle->idle[0] : walk ? walk->walk[0] : g->lframe;
            flip = post * 16 > g->gx16;
        } else if (g->ghost == 1) {
            int dir = post * 16 > g->gx16 ? 1 : -1;
            g->gstep += GHOST_SPEED;
            flip = dir > 0;               /* guards face left unless mirrored */
            frame = walk ? walk->walk[g->gstep % walk->nwalk] : g->lframe;
        } else {
            g->gstep++;
            if (idle) frame = idle->idle[g->gstep % idle->nidle];
            else if (walk) frame = walk->walk[0];
            else frame = g->lframe;
            flip = 0;
        }
        if (g->ghost == 1 && !g->recover) {
            ghost_on[i] = 1; ghost_x[i] = gx; ghost_y[i] = g->gy; ghost_dir[i] = post > gx ? 1 : -1;
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
    memset(ghost_on, 0, sizeof(ghost_on));
    if (patrol_return)
        ghosts(s, extra, width);
    else
        for (int i = 0; i < NOBJ; i++) guards[i].ghost = 0;
    doors_update(s);
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
                /* a door the game shows closed may be opened by a ghost */
                uint16_t f = doors[i].phase && is_door(b) ? door_frame(i, b->frame) : b->frame;
                emit_frame(s, v, f, b->x, b->y, b->z, b->flip != 0, b->attr, 0, width);
            }
    }
    return out_n;
}

/* Prop Hunt: a player drawn as another object's frame, in each view that shows him */
int wobj_build_disguise(int obj, uint16_t frame, uint16_t attr_or, wobj_sprite *list, int max)
{
    static snap_t s;
    take_snapshot(&s);
    out_list = list; out_n = 0; out_max = max;
    const obj_t *b = &s.o[obj];
    for (int v = 0; v < 2; v++)
        if (view_on(&s, v) && (b->vis & (1 << v)))
            emit_frame(&s, v, frame, b->x, b->y, b->z, b->flip != 0, attr_or, 0, 0);
    return out_n;
}

void wobj_reset(void)
{
    level_id = 0;
}

/* Stacked views (see vdp.c): player 1's view shows a fixed floor of the map while the
 * game's camera is elsewhere (on the last stair, on the exit floor). Objects on that
 * floor that the game's own view does not cover are drawn here, at their place in it:
 * line = their line in the game's view + (real_wy0 - vtop). */
int wobj_build_stacked(int v, int vtop, int real_wy0, wobj_sprite *list, int max)
{
    const snap_t *s = &snaps[cur];
    if (!s->valid || !view_on(s, v))
        return 0;
    out_list = list; out_n = 0; out_max = max;
    int shift = real_wy0 - vtop;
    int y0 = v ? 7 : 135;
    for (int pass = 2; pass >= 1; pass--)
        for (int i = 0; i < NOBJ; i++) {
            const obj_t *b = &s->o[i];
            if (b->st != pass || !b->frame) continue;
            /* line of its feet in the game's view of that player */
            int feet = b->y - b->z - s->cam[v][0] + (v ? 0x85 : 0x105) + 0x14 - 128;
            int inside_real = (b->vis & (1 << v)) && feet >= y0 && feet < y0 + 80;
            if (inside_real) continue;                 /* the game draws it */
            int fv = feet + shift;
            if (fv < y0 || fv >= y0 + 80 + 48) continue;  /* not on the floor shown */
            int n0 = out_n;
            emit_frame(s, v, b->frame, b->x, b->y, b->z, b->flip != 0, b->attr, 0, 0);
            for (int k = n0; k < out_n; k++)
                list[k].y += shift;
        }
    return out_n;
}

/* the players' frame numbers seen while this picture is drawn (and the one before);
 * called on every line. Returns 1 if a new one was seen. */
static uint16_t seen[2][2][4];              /* [player][this picture / the one before][...] */
static int nseen[2][2];
static uint32_t seen_version;
int wobj_sample_players(void)
{
    static uint32_t built = 0xFFFFFFFF;
    int changed = 0;
    if (built != md.frame) {
        built = md.frame;
        for (int c = 0; c < 2; c++) {
            memcpy(seen[c][1], seen[c][0], sizeof(seen[c][0]));
            nseen[c][1] = nseen[c][0];
            nseen[c][0] = 0;
        }
        changed = 1;
    }
    for (int c = 0; c < 2; c++) {
        uint16_t f = u16(0x3252 + 2 * c);
        if (!f) continue;
        int known = 0;
        for (int k = 0; k < nseen[c][0]; k++) known |= seen[c][0][k] == f;
        if (!known && nseen[c][0] < 4) { seen[c][0][nseen[c][0]++] = f; changed = 1; }
    }
    if (changed) seen_version++;
    return changed;
}

/* Which player a sprite piece belongs to (CHAR_ROBO / CHAR_MOBO), or -1: the pieces of
 * the players' (objects 0 and 1) current animation frames, so that only they get the
 * player colours - not guards whose graphics happen to use the same tiles. */
int wobj_player_of(uint16_t attr)
{
    /* The game writes a player's frame number while it works on the object and clears it
     * again, so one reading can miss it (0), or already be the next frame while the sprite
     * table still shows the last one. The frame is read again on every line drawn, and the
     * pieces of every frame seen in this picture or the one before count (else the
     * colours flicker back to the original). */
    static uint16_t tiles[2][64];
    static int n[2];
    static uint32_t built_version = 0xFFFFFFFF;
    wobj_sample_players();
    if (built_version != seen_version) {
        built_version = seen_version;
        for (int c = 0; c < 2; c++) {
            n[c] = 0;
            for (int w = 0; w < 2; w++)
                for (int k = 0; k < nseen[c][w]; k++) {
                    uint32_t p = FRAMES_TBL + seen[c][w][k] * 4u;
                    if (p + 4 > md.rom_size) continue;
                    p = ((uint32_t)md.rom[p] << 24 | md.rom[p + 1] << 16 | md.rom[p + 2] << 8 | md.rom[p + 3]) & 0xFFFFFF;
                    if (p + 2 > md.rom_size) continue;
                    int cnt = md.rom[p] + 1;
                    p += 2;
                    for (int q = 0; q < cnt && n[c] < 64 && p + 6 <= md.rom_size; q++, p += 6)
                        tiles[c][n[c]++] = (uint16_t)(((md.rom[p + 4] << 8) | md.rom[p + 5]) & 0x07FF);
                }
        }
    }
    for (int c = 0; c < 2; c++)
        for (int i = 0; i < n[c]; i++)
            if (tiles[c][i] == (attr & 0x07FF))
                return c;
    return -1;
}
