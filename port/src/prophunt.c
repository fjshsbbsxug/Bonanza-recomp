/* Prop Hunt: an online game mode the host chooses (Online menu: Mode, Round).
 *
 * One player hides, disguised as one of the stage's guards, treasures or gadgets; the
 * other seeks and wins the round by shooting the disguised player. The hider wins if
 * the round's time runs out. The roles swap every round, and the stage starts again.
 *   - the first 10 seconds of a round only the hider moves (the seeker's screen is dark)
 *   - the hider: guards ignore him; he never attacks (Shoot does nothing); A changes
 *     the disguise. As a guard he moves as guards do (half a pixel per frame, lanes at
 *     about half speed, no jumping); as a treasure or gadget he runs 1.1 times as fast
 *     as a player, jumps 1.1 times sooner and changes lanes 1.1 times slower
 *   - nobody loses lives; the exit stays shut (the stage is the arena)
 *
 * The game logic here runs on both machines around every frame, in lockstep: it reads
 * only the players' buttons (the same on both) and the game's RAM, and keeps its own
 * state the same way, so both games stay identical (the state hash checks it). What
 * each player sees (the disguise drawn instead of the hider, the banner) is drawn by
 * each machine for itself (vdp.c, ph_overlay).
 *
 * Game RAM used (see the research notes in the commit): per object (index*2): $2EF6
 * state, $304E x, $30FA y, $31A6 lane height, $3252 frame, $32FE mirrored, $3706/$37B2
 * half width/height, $385E walking step, $3C66 attribute; per player (+$AA for player
 * 2): $5688 state, $567C state timer, $56BE/$56C0 protection (2 = guards ignore him);
 * $99F0/$9BF4 effect type/owner (shots: objects $46-$55, type 8 = player 1's, $C =
 * player 2's), $9DF8/$9DFA shots alive; $9E12+obj*2 item kinds; $C054/$C056 lives;
 * $6C4B-$6C4D the clock (minutes, tens, seconds); $1818 exit open; $C070 game mode
 * (0x0C: in a stage); $C068 the routine the mode runs. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "prophunt.h"
#include "md.h"
#include "net.h"
#include "menu.h"

#define HIDE_FRAMES (10 * 60)
#define END_FRAMES (4 * 60)
#define START_WAIT 90           /* frames of a running stage before a round starts */

enum { PH_OFF, PH_WAIT, PH_HIDE, PH_SEEK, PH_END };
enum { D_GUARD, D_TREASURE, D_GADGET };

typedef struct { uint16_t frame, attr; int kind; } disguise_t;

static struct {
    int phase;
    int round;                  /* rounds started */
    int hider;                  /* 0 or 1 */
    int timer;                  /* frames left in this phase */
    int wins[2];
    int seeker_won;             /* the last round */
    int end_left;               /* frames that were left when it ended */
    int wait;
    uint32_t frame;
    uint8_t last_pad[2];
    int dir;                    /* the hider's facing: -1 / +1 */
    int16_t lane_before;
    disguise_t list[16];
    int nlist, cur;
} ph;

static uint16_t rw(uint32_t a) { return (uint16_t)(md.ram[a & 0xFFFF] << 8 | md.ram[(a + 1) & 0xFFFF]); }
static void ww(uint32_t a, uint16_t v) { md.ram[a & 0xFFFF] = (uint8_t)(v >> 8); md.ram[(a + 1) & 0xFFFF] = (uint8_t)v; }
static int16_t rs(uint32_t a) { return (int16_t)rw(a); }

void ph_reset(void) { memset(&ph, 0, sizeof(ph)); ph.dir = 1; }

/* online with the host's option on; PH_OFFLINE=minutes: the rules offline, for the
 * headless runner's tests */
static int test_minutes = -1;
static int on(void)
{
    if (test_minutes < 0) test_minutes = getenv("PH_OFFLINE") ? atoi(getenv("PH_OFFLINE")) : 0;
    return net_option_prop_hunt() || test_minutes > 0;
}
int ph_active(void) { return on() && ph.phase != PH_OFF && ph.phase != PH_WAIT; }
int ph_hidden_player(void) { return ph_active() ? ph.hider : -1; }

static int in_stage(void) { return rw(0xC070) == 0x0C && rw(0x2EF6) != 0 && rw(0x2EF8) != 0; }

/* the stage's guards, treasures and gadgets the hider can look like (from RAM, as the
 * stage is set up: the same on both machines) */
static void collect(void)
{
    ph.nlist = 0;
    for (int o = 6; o < 38 && !ph.nlist; o++)           /* a guard */
        if (rw(0x2EF6 + o * 2) && rw(0x3252 + o * 2)) {
            disguise_t *d = &ph.list[ph.nlist++];
            d->frame = rw(0x3252 + o * 2);
            d->attr = rw(0x3C66 + o * 2);
            d->kind = D_GUARD;
        }
    for (int pass = 0; pass < 2; pass++)                 /* treasures, then gadgets */
        for (int o = 38; o < 70 && ph.nlist < 16; o++) {
            uint16_t kind = rw(0x9E12 + o * 2), f = rw(0x3252 + o * 2);
            if (!rw(0x2EF6 + o * 2) || !f || kind == 0x04 || kind == 0x2C) continue;   /* doors, the exit */
            if ((kind == 0x00) != (pass == 0)) continue;
            int dup = 0;
            for (int i = 0; i < ph.nlist; i++) dup |= ph.list[i].frame == f;
            if (dup) continue;
            disguise_t *d = &ph.list[ph.nlist++];
            d->frame = f;
            d->attr = rw(0x3C66 + o * 2);
            d->kind = pass == 0 ? D_TREASURE : D_GADGET;
        }
    ph.cur = 0;
}

int ph_disguise(uint16_t *frame, uint16_t *attr)
{
    if (!ph_active() || ph.nlist == 0) return 0;
    *frame = ph.list[ph.cur].frame;
    *attr = ph.list[ph.cur].attr;
    return 1;
}

static void start_round(void)
{
    ph.hider = (ph.round & 1) ? 0 : 1;      /* round 1: player 2 hides */
    ph.round++;
    if (getenv("PHLOG")) fprintf(stderr, "ph: round %d starts, frame %u, hider %d\n", ph.round, ph.frame, ph.hider);
    ph.phase = PH_HIDE;
    ph.timer = HIDE_FRAMES;
    collect();
}

/* the stage again, from its start (the stage-setup routine; lives and score kept) */
static void restart_stage(void)
{
    ww(0xC068, 0x0000);
    ww(0xC06A, 0x2E06);
    ww(0xC070, 0x000C);
    ww(0x1814, 0);
    for (uint32_t p = 0; p < 2; p++) {      /* the next round's seeker is not protected */
        ww(0x56BE + p * 0xAA, 0);
        ww(0x56C0 + p * 0xAA, 0);
    }
}

static int round_frames(void) { return (net_option_prop_hunt() ? net_option_ph_minutes() : test_minutes) * 60 * 60; }

/* the clock on the HUD shows the round's time left (the game's own never reaches 0) */
static void show_clock(void)
{
    int left = ph.phase == PH_SEEK ? ph.timer : ph.phase == PH_HIDE ? round_frames() : ph.end_left;
    int s = (left + 59) / 60;
    if (s < 1) s = 1;
    if (s > 599) s = 599;
    md.ram[0x6C4B] = (uint8_t)(s / 60);
    md.ram[0x6C4C] = (uint8_t)(s % 60 / 10);
    md.ram[0x6C4D] = (uint8_t)(s % 10);
}

void ph_pre_frame(void)
{
    if (!on()) { ph.phase = PH_OFF; return; }
    ph.frame++;
    if (ph.phase == PH_OFF) { ph.phase = PH_WAIT; ph.wait = 0; }
    if (ph.phase == PH_WAIT) {
        ph.wait = in_stage() ? ph.wait + 1 : 0;
        if (ph.wait >= START_WAIT) start_round();
        else return;
    }
    int h = ph.hider, sk = 1 - h;
    uint32_t hp = (uint32_t)h * 0xAA, ho = (uint32_t)h * 2;
    uint8_t pad = md.pad[h];
    /* A: the next disguise (on press) */
    if ((pad & PAD_A) && !(ph.last_pad[h] & PAD_A) && ph.nlist)
        ph.cur = (ph.cur + 1) % ph.nlist;
    ph.last_pad[h] = pad;
    int guard = ph.nlist && ph.list[ph.cur].kind == D_GUARD;
    /* the hider never attacks; as a guard he cannot jump */
    pad &= (uint8_t)~(PAD_A | PAD_B);
    if (guard) pad &= (uint8_t)~PAD_C;
    if (ph.phase == PH_END) pad = 0;
    md.pad[h] = pad;
    if (ph.phase == PH_HIDE || ph.phase == PH_END)
        md.pad[sk] = 0;                     /* the seeker waits (and nobody moves at the end) */
    /* guards ignore the hider (the game's own protection, which its sight, catch and
     * bullet tests skip) - except in the two states that wait for it to run out
     * (5: sub_0C366, 0x18: sub_0D59E), which would otherwise wait for ever */
    int st = rw(0x5688 + hp) & 0xFF;
    if (st == 0x05 || st == 0x18) {
        ww(0x56BE + hp, 0);
        ww(0x56C0 + hp, 0);
    } else {
        ww(0x56BE + hp, 2);
        ww(0x56C0 + hp, 0x7FFF);
    }
    /* no lives lost, the exit stays shut */
    ww(0xC054, 3);
    ww(0xC056, 3);
    ww(0x1818, 0);
    /* the hider's pace */
    int16_t step = rs(0x385E + ho);
    if (step > 0) ph.dir = 1;
    else if (step < 0) ph.dir = -1;
    if (guard)
        ww(0x385E + ho, (uint16_t)(ph.frame & 1 ? 0 : ph.dir));           /* half a pixel */
    else if (ph.nlist)
        ww(0x385E + ho, (uint16_t)(ph.frame % 10 == 0 ? 2 * ph.dir : ph.dir)); /* 1.1 */
    /* jumping: the crouch before the jump 1.1 times shorter (9 frames -> 8) */
    if (!guard && (rw(0x5688 + hp) & 0xFF) == 2 && rw(0x567C + hp) == 9)
        ww(0x567C + hp, 8);
    ph.lane_before = rs(0x31A6 + ho);
    show_clock();
}

/* a shot of the seeker touching the hider (the game's own test, sub_1A95C: same lane,
 * within the two half sizes; shots have none) */
static int seeker_hits(void)
{
    int h = ph.hider, sk = 1 - h;
    uint32_t ho = (uint32_t)h * 2;
    int hx = rs(0x304E + ho), hy = rs(0x30FA + ho), hz = rs(0x31A6 + ho);
    int w = rs(0x3706 + ho), hh = rs(0x37B2 + ho);
    for (int o = 0x46; o <= 0x55; o++) {
        uint32_t so = (uint32_t)o * 2;
        if (!rw(0x2EF6 + so) || rw(0x99F0 + so) != (sk == 0 ? 8 : 0x0C)) continue;
        int dx = rs(0x304E + so) - hx, dy = rs(0x30FA + so) - hy;
        if (rs(0x31A6 + so) != hz || dx >= w || -dx >= w || dy >= hh || -dy >= hh) continue;
        ww(0x2EF6 + so, 0);                 /* the shot is gone */
        uint32_t cnt = sk == 0 ? 0x9DF8 : 0x9DFA;
        if (rw(cnt)) ww(cnt, (uint16_t)(rw(cnt) - 1));
        return 1;
    }
    return 0;
}

void ph_post_frame(void)
{
    if (!on() || ph.phase == PH_OFF || ph.phase == PH_WAIT) return;
    int h = ph.hider;
    uint32_t hp = (uint32_t)h * 0xAA, ho = (uint32_t)h * 2;
    /* lane changes: some steps undone (never the last one, which ends the move) */
    if ((rw(0x5688 + hp) & 0xFF) == 0x19 && ph.nlist) {
        int guard = ph.list[ph.cur].kind == D_GUARD;
        int16_t z = rs(0x31A6 + ho);
        int undo = guard ? (ph.frame & 1) : (ph.frame % 11 == 0);
        if (undo && z != ph.lane_before && z > 0 && z < 0x10)
            ww(0x31A6 + ho, (uint16_t)ph.lane_before);
    }
    switch (ph.phase) {
    case PH_HIDE:
        if (--ph.timer <= 0) { ph.phase = PH_SEEK; ph.timer = round_frames(); }
        break;
    case PH_SEEK:
        if (seeker_hits()) {
            ph.end_left = ph.timer;
            ph.seeker_won = 1;
            ph.wins[1 - h]++;
            ph.phase = PH_END;
            ph.timer = END_FRAMES;
            if (getenv("PHLOG")) fprintf(stderr, "ph: frame %u, the seeker hit the prop\n", ph.frame);
        } else if (--ph.timer <= 0) {
            ph.end_left = 0;
            if (getenv("PHLOG")) fprintf(stderr, "ph: frame %u, time up: the prop wins\n", ph.frame);
            ph.seeker_won = 0;
            ph.wins[h]++;
            ph.phase = PH_END;
            ph.timer = END_FRAMES;
        }
        break;
    case PH_END:
        if (--ph.timer <= 0) {
            restart_stage();
            ph.phase = PH_WAIT;
            ph.wait = 0;
        }
        break;
    }
    show_clock();
}

/* ------------------------------------------------------------------ the banner */

static void box(uint32_t *fb, int stride, int width, int height, int y0, int y1, uint32_t c)
{
    for (int y = y0; y < y1 && y < height; y++)
        for (int x = 0; x < width; x++)
            fb[y * stride + x] = c;
}

static void centred(uint32_t *fb, int stride, int width, int height, int y, const char *s, uint32_t c)
{
    int x = (width - (int)strlen(s) * 8) / 2;
    menu_text(fb, stride, height, x < 0 ? 0 : x, y, s, c);
}

void ph_overlay(uint32_t *fb, int stride, int width, int height, int me)
{
    if (!on() || me < 0 || ph.phase == PH_OFF) return;
    char a[64], b[64];
    const uint32_t white = 0xFFFFFFFFu, gold = 0xFFFFD040u, dim = 0xFF000000u;
    if (ph.phase == PH_WAIT) {
        box(fb, stride, width, height, 8, 22, dim);
        centred(fb, stride, width, height, 11, "PROP HUNT - GET READY", gold);
        return;
    }
    int hider = me == ph.hider;
    /* the seeker sees nothing while the hider hides */
    if (ph.phase == PH_HIDE && !hider) {
        box(fb, stride, width, height, 0, 87, 0xFF000000u);
        box(fb, stride, width, height, 135, height, 0xFF000000u);
    }
    box(fb, stride, width, height, 8, 32, dim);
    snprintf(a, sizeof(a), "ROUND %d  YOU: %s  %d-%d", ph.round, hider ? "PROP" : "SEEKER",
             ph.wins[me], ph.wins[1 - me]);
    centred(fb, stride, width, height, 11, a, gold);
    const char *dname[] = { "GUARD", "TREASURE", "GADGET" };
    const char *dn = ph.nlist ? dname[ph.list[ph.cur].kind] : "-";
    switch (ph.phase) {
    case PH_HIDE:
        if (hider) snprintf(b, sizeof(b), "HIDE! %d  A: DISGUISE (%s)", (ph.timer + 59) / 60, dn);
        else snprintf(b, sizeof(b), "THE PROP IS HIDING... %d", (ph.timer + 59) / 60);
        if (!hider) centred(fb, stride, width, height, 170, "WAIT", white);
        break;
    case PH_SEEK:
        if (hider) snprintf(b, sizeof(b), "STAY HIDDEN  A: DISGUISE (%s)", dn);
        else snprintf(b, sizeof(b), "FIND THE PROP AND SHOOT IT!");
        break;
    default:
        snprintf(b, sizeof(b), "%s", ph.seeker_won ? "FOUND! THE SEEKER WINS" : "TIME UP! THE PROP WINS");
        break;
    }
    centred(fb, stride, width, height, 21, b, white);
}
