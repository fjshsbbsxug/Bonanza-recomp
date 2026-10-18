/* Race: an online game mode the host chooses (Online menu: Mode).
 *
 * Both players play the same stage, each in a 1-player game of their own (no lockstep:
 * each machine runs its own game at its own pace). Whoever clears the stage first wins
 * the round; then both vote for the next stage. Rounds:
 *   - vote: each picks a stage (left / right, A / C / Start to vote, B to change it); once
 *     both have voted, that stage is played - if the votes differ, the host's pick in odd
 *     rounds and the other player's in even ones
 *   - countdown, 3 seconds
 *   - play: the stage straight from the start; lives never run out (a death costs time);
 *     a game that ends without clearing (time up) starts the stage again, the clock
 *     still running
 *   - result: the first to clear it, measured in game frames since the stage started
 *     (the same for both machines however late a message arrives; a tie goes to the
 *     host), 5 seconds; then the next vote
 * Over the network only each player's race status goes (net_race_send, many times a
 * second, the last one counts): round, phase, vote, frames played, cleared and when,
 * treasures, wins.
 *
 * Game RAM used: $C070 game mode (0x0C: in a stage), $C05A stage entry, $C054 player 1's
 * lives, $1818 exit open (all treasures taken), $9E0C / $9E0E treasures needed / taken,
 * $6C4B-$6C4D the clock (minutes, tens, seconds). */
#include <stdio.h>
#include <string.h>
#include "race.h"
#include "md.h"
#include "net.h"
#include "menu.h"

#define COUNT_FRAMES (3 * 60)
#define RESULT_FRAMES (5 * 60)
#define FAIL_FRAMES (8 * 60)     /* out of the stage this long without clearing it: again */
#define SEND_EVERY 6

/* the stages (entries of $C05A: 4, 8 and 12 are the coffee breaks, 14 the ending) */
static const int stage_entry[] = { 1, 2, 3, 5, 6, 7, 9, 10, 11, 13 };
#define NSTAGES ((int)(sizeof(stage_entry) / sizeof(stage_entry[0])))

enum { R_VOTE, R_COUNT, R_PLAY, R_DONE, R_RESULT };

typedef struct {
    int round, phase, vote;        /* vote: stage index + 1, 0 none */
    uint32_t played;               /* game frames since the stage started */
    int cleared;
    uint32_t clear_at;             /* played when it was cleared */
    int treasures, needed, wins;
} status_t;

static struct {
    int on;
    status_t me, them;
    int have_them;
    int pick;                      /* the stage index the cursor is on */
    int stage;                     /* the stage index being raced */
    int timer;
    int won;                       /* the last round: 1 won, 0 lost */
    int seen_stage;                /* the game has been in the stage */
    int out_frames;                /* frames out of the stage since */
    int start;                     /* a (re)start is due */
    uint8_t last_pad;
    uint32_t sends;
} r;

static int rw(int a) { return (md.ram[a & 0xFFFF] << 8) | md.ram[(a + 1) & 0xFFFF]; }
static void ww(int a, int v) { md.ram[a & 0xFFFF] = (uint8_t)(v >> 8); md.ram[(a + 1) & 0xFFFF] = (uint8_t)v; }

static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

static void send_status(void)
{
    uint8_t b[20];
    b[0] = (uint8_t)(r.me.round >> 8); b[1] = (uint8_t)r.me.round;
    b[2] = (uint8_t)r.me.phase;
    b[3] = (uint8_t)r.me.vote;
    put32(b + 4, r.me.played);
    b[8] = (uint8_t)r.me.cleared;
    put32(b + 9, r.me.clear_at);
    b[13] = (uint8_t)r.me.treasures;
    b[14] = (uint8_t)r.me.needed;
    b[15] = (uint8_t)r.me.wins;
    net_race_send(b, 16);
}

static void read_status(void)
{
    uint8_t b[64];
    if (net_race_peer(b, sizeof(b)) < 16) return;
    r.have_them = 1;
    r.them.round = b[0] << 8 | b[1];
    r.them.phase = b[2];
    r.them.vote = b[3];
    r.them.played = get32(b + 4);
    r.them.cleared = b[8];
    r.them.clear_at = get32(b + 9);
    r.them.treasures = b[13];
    r.them.needed = b[14];
    r.them.wins = b[15];
}

void race_reset(void)
{
    memset(&r, 0, sizeof(r));
    r.on = net_option_race();
    r.me.round = 1;
    r.me.phase = R_VOTE;
}

int race_on(void) { return r.on && net_option_race(); }

static void to_vote(void)
{
    r.me.round++;
    r.me.phase = R_VOTE;
    r.me.vote = 0;
    r.me.cleared = 0;
    r.me.clear_at = r.me.played = 0;
    r.me.treasures = r.me.needed = 0;
    r.pick = (r.stage + 1) % NSTAGES;       /* the next stage, to start with */
}

/* the round's result, once it can be told: 1 won, 0 lost, -1 not yet */
static int result(void)
{
    int host = net_local_player() == 0;
    int them_here = r.have_them && r.them.round == r.me.round && r.them.phase >= R_PLAY;
    if (r.me.cleared) {
        if (them_here && r.them.cleared)
            return r.me.clear_at < r.them.clear_at || (r.me.clear_at == r.them.clear_at && host);
        if (them_here && r.them.played > r.me.clear_at) return 1;
        if (r.have_them && r.them.round > r.me.round) return 0;   /* (they moved on) */
        return -1;
    }
    if (them_here && r.them.cleared && r.me.played > r.them.clear_at) return 0;
    return -1;
}

int race_update(uint8_t pad, int *entry)
{
    if (!race_on()) return RACE_HOLD;
    read_status();
    uint8_t press = pad & ~r.last_pad;
    r.last_pad = pad;
    if (++r.sends % SEND_EVERY == 0) send_status();
    switch (r.me.phase) {
    case R_VOTE:
        if (!r.me.vote) {
            if (press & PAD_LEFT) r.pick = (r.pick + NSTAGES - 1) % NSTAGES;
            if (press & PAD_RIGHT) r.pick = (r.pick + 1) % NSTAGES;
            if (press & PAD_UP) r.pick = (r.pick + NSTAGES - 5) % NSTAGES;
            if (press & PAD_DOWN) r.pick = (r.pick + 5) % NSTAGES;
            if (press & (PAD_A | PAD_C | PAD_START)) { r.me.vote = r.pick + 1; send_status(); }
        } else if (press & PAD_B) {
            r.me.vote = 0;
        }
        if (r.me.vote && r.have_them && r.them.round == r.me.round && r.them.vote) {
            int mine = r.me.vote - 1, theirs = r.them.vote - 1;
            int host_pick = net_local_player() == 0 ? mine : theirs, guest_pick = net_local_player() == 0 ? theirs : mine;
            r.stage = mine == theirs ? mine : (r.me.round & 1) ? host_pick : guest_pick;
            if (r.stage < 0 || r.stage >= NSTAGES) r.stage = 0;
            r.me.phase = R_COUNT;
            r.timer = COUNT_FRAMES;
            send_status();
        }
        return RACE_HOLD;
    case R_COUNT:
        if (--r.timer > 0) return RACE_HOLD;
        r.me.phase = R_PLAY;
        r.seen_stage = r.out_frames = 0;
        r.start = 1;
        send_status();
        /* fall through */
    case R_PLAY: {
        if (r.start) {
            r.start = 0;
            r.seen_stage = r.out_frames = 0;
            *entry = stage_entry[r.stage];
            return RACE_START;
        }
        int res = result();
        if (res == 0) {                        /* they cleared it first */
            r.won = 0;
            r.me.phase = R_RESULT;
            r.timer = RESULT_FRAMES;
            send_status();
            return RACE_HOLD;
        }
        return RACE_RUN;
    }
    case R_DONE: {                             /* cleared: waiting to know who was first */
        int res = result();
        if (res >= 0) {
            r.won = res;
            if (res) r.me.wins++;
            r.me.phase = R_RESULT;
            r.timer = RESULT_FRAMES;
            send_status();
        }
        return RACE_HOLD;
    }
    case R_RESULT:
        if (--r.timer <= 0) { to_vote(); send_status(); }
        return RACE_HOLD;
    }
    return RACE_HOLD;
}

void race_pre_frame(void)
{
    if (!race_on() || r.me.phase != R_PLAY) return;
    if (rw(0xC070) == 0x0C && rw(0xC054) < 2)
        ww(0xC054, 2);                         /* lives never run out */
}

void race_post_frame(void)
{
    if (!race_on() || r.me.phase != R_PLAY) return;
    r.me.played++;
    int in_stage = rw(0xC070) == 0x0C;
    int entry = rw(0xC05A);
    if (r.seen_stage && entry != stage_entry[r.stage]) {   /* the game moved on: cleared */
        r.me.cleared = 1;
        r.me.clear_at = r.me.played;
        r.me.phase = R_DONE;
        send_status();
        return;
    }
    if (in_stage) {
        r.seen_stage = 1;
        r.out_frames = 0;
        r.me.needed = rw(0x9E0C) & 0xFF;
        r.me.treasures = rw(0x9E0E) & 0xFF;
        return;
    }
    if (!r.seen_stage) return;
    int clock = md.ram[0x6C4B] | md.ram[0x6C4C] | md.ram[0x6C4D];
    /* out of the stage with the exit open and time left: escaped */
    if (rw(0x1818) && clock && r.out_frames == 0) {
        r.me.cleared = 1;
        r.me.clear_at = r.me.played;
        r.me.phase = R_DONE;
        send_status();
        return;
    }
    if (++r.out_frames > FAIL_FRAMES) r.start = 1;   /* the game ended: the stage again */
}

/* ---- the picture */
static void box(uint32_t *fb, int stride, int width, int height, int y0, int y1, uint32_t c)
{
    for (int y = y0 < 0 ? 0 : y0; y < y1 && y < height; y++)
        for (int x = 0; x < width; x++)
            fb[y * stride + x] = c;
}

static void centred(uint32_t *fb, int stride, int width, int height, int y, const char *s, uint32_t c)
{
    int x = (width - 8 * (int)strlen(s)) / 2;
    /* menu_text draws into a buffer of width w: give it the rows from y on */
    menu_text(fb, stride, height, x < 0 ? 0 : x, y, s, c);
}

void race_overlay(uint32_t *fb, int stride, int width, int height)
{
    if (!race_on()) return;
    const uint32_t white = 0xFFFFFFFFu, gold = 0xFFFFD040u, dim = 0xFF9C98AAu, dark = 0xFF101018u, red = 0xFFFF5060u;
    char t[64];
    switch (r.me.phase) {
    case R_VOTE: {
        box(fb, stride, width, height, 0, height, dark);
        snprintf(t, sizeof(t), "RACE - ROUND %d", r.me.round);
        centred(fb, stride, width, height, 16, t, gold);
        snprintf(t, sizeof(t), "WINS  YOU %d - %d THEM", r.me.wins, r.have_them ? r.them.wins : 0);
        centred(fb, stride, width, height, 30, t, white);
        centred(fb, stride, width, height, 50, "VOTE FOR THE NEXT STAGE", white);
        int cw = (width - 16) / 5 < 56 ? (width - 16) / 5 : 56, x0 = (width - 5 * cw) / 2;
        for (int i = 0; i < NSTAGES; i++) {
            int x = x0 + (i % 5) * cw, y = 70 + (i / 5) * 28;
            int mine = r.me.vote ? r.me.vote - 1 == i : r.pick == i;
            int theirs = r.have_them && r.them.round == r.me.round && r.them.vote - 1 == i;
            if (mine) box(fb + x, stride, cw - 6, height, y - 4, y + 12, r.me.vote ? 0xFF4A2030u : 0xFF3A3850u);
            snprintf(t, sizeof(t), "%d", i + 1);
            menu_text(fb, stride, height, x + (cw - 6 - 8 * (int)strlen(t)) / 2, y, t, mine ? white : dim);
            if (theirs) menu_text(fb, stride, height, x + cw - 14, y - 4, "*", red);
        }
        centred(fb, stride, width, height, 132, r.me.vote ? "VOTED - B: CHANGE" : "ARROWS: PICK   A / START: VOTE", r.me.vote ? gold : white);
        if (!r.have_them || r.them.round != r.me.round)
            centred(fb, stride, width, height, 150, "WAITING FOR THE OTHER PLAYER...", dim);
        else if (r.them.vote) {
            snprintf(t, sizeof(t), "THEY VOTED: STAGE %d (*)", r.them.vote);
            centred(fb, stride, width, height, 150, t, red);
        } else
            centred(fb, stride, width, height, 150, "THEY ARE STILL CHOOSING", dim);
        /* different votes: the host's pick in odd rounds, the other player's in even ones */
        int mine_counts = (net_local_player() == 0) == ((r.me.round & 1) != 0);
        centred(fb, stride, width, height, 172, mine_counts ? "VOTES DIFFER? YOUR PICK GOES" : "VOTES DIFFER? THEIR PICK GOES", dim);
        break;
    }
    case R_COUNT:
        box(fb, stride, width, height, 0, height, dark);
        snprintf(t, sizeof(t), "ROUND %d - STAGE %d", r.me.round, r.stage + 1);
        centred(fb, stride, width, height, 80, t, gold);
        snprintf(t, sizeof(t), "%d", (r.timer + 59) / 60);
        centred(fb, stride, width, height, 104, t, white);
        centred(fb, stride, width, height, 128, "FIRST TO CLEAR IT WINS!", white);
        break;
    case R_PLAY:
    case R_DONE: {
        box(fb, stride, width, height, 0, 12, 0xC0000000u);
        if (r.me.phase == R_DONE)
            snprintf(t, sizeof(t), "CLEARED! WAITING FOR THE RESULT...");
        else if (r.have_them && r.them.round == r.me.round && r.them.phase >= R_PLAY)
            snprintf(t, sizeof(t), "STAGE %d  THEM: %d/%d TREASURES%s", r.stage + 1, r.them.treasures, r.them.needed,
                     r.them.cleared ? " - CLEARED" : "");
        else
            snprintf(t, sizeof(t), "STAGE %d  RACE!", r.stage + 1);
        centred(fb, stride, width, height, 2, t, r.me.phase == R_DONE ? gold : white);
        break;
    }
    case R_RESULT:
        box(fb, stride, width, height, 70, 150, dark);
        centred(fb, stride, width, height, 84, r.won ? "YOU WIN THE ROUND!" : "THEY CLEARED IT FIRST", r.won ? gold : red);
        snprintf(t, sizeof(t), "WINS  YOU %d - %d THEM", r.me.wins, r.have_them ? r.them.wins : 0);
        centred(fb, stride, width, height, 104, t, white);
        snprintf(t, sizeof(t), "NEXT VOTE IN %d", (r.timer + 59) / 60);
        centred(fb, stride, width, height, 124, t, dim);
        break;
    }
}
