/* Recording the game's sounds from the user's ROM, once (sndbank.h).
 *
 * Each song, effect and voice is played alone through the game's own sound driver and
 * sound chips (md_snd_*: the driver loaded as the game's boot code does it, the request
 * written where the game writes it) and kept in four parts: the channels the game's
 * effects take over (FM5, FM6, PSG3 + noise) and everything else, so the player can
 * leave out a part while an effect plays on it, as the driver does.
 *
 * Where a song repeats is found from the driver's own state: its tracks (position,
 * timers, loops) in Z80 RAM are the same again as at an earlier moment. A song or effect
 * that ends is kept until its last note has faded. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "md.h"
#include "sndbank.h"
#include "sndrec.h"

#define MAX_SECONDS 360

typedef struct { int16_t *s[SB_STEMS]; uint32_t len, cap; } pcm;

static void pcm_add(pcm *p, int16_t *const in[SB_STEMS], int n)
{
    if (p->len + (uint32_t)n > p->cap) {
        uint32_t c = p->cap ? p->cap * 2 : 1 << 20;
        while (c < p->len + (uint32_t)n) c *= 2;
        for (int k = 0; k < SB_STEMS; k++) p->s[k] = realloc(p->s[k], (size_t)c * 4);
        p->cap = c;
    }
    for (int k = 0; k < SB_STEMS; k++) memcpy(p->s[k] + p->len * 2, in[k], (size_t)n * 4);
    p->len += (uint32_t)n;
}

static void pcm_free(pcm *p) { for (int k = 0; k < SB_STEMS; k++) free(p->s[k]); memset(p, 0, sizeof(*p)); }

/* the driver's state (Z80 RAM 0x1C11-0x1F9F: tempo, fade, the music tracks and the
 * effect slots), not the refresh register it saves at 0x1C17 */
static uint32_t state_hash(void)
{
    uint32_t h = 2166136261u;
    for (int a = 0x1C11; a < 0x1FA0; a++)
        if (a != 0x1C17) h = (h ^ md.zram[a]) * 16777619u;
    return h;
}
static int music_playing(void) { for (int a = 0x1C40; a < 0x1E20; a += 0x30) if (md.zram[a] & 0x80) return 1; return 0; }
/* effect slots (0x1E80 + 0x30 n) that are playing, as parts: FM5 0x1EB0, FM6 0x1EE0,
 * PSG3 0x1F70; other slots count as part 0 */
static int effect_parts(void)
{
    int m = 0;
    for (int k = 0; k < 6; k++)
        if (md.zram[0x1E80 + 0x30 * k] & 0x80)
            m |= k == 1 ? 2 : k == 2 ? 4 : k == 5 ? 8 : 1;
    for (int a = 0x1E20; a < 0x1E80; a += 0x30) if (md.zram[a] & 0x80) m |= 1;
    return m;
}
static int silent_tail(const pcm *p, uint32_t n)
{
    if (p->len < n) return 0;
    for (int k = 0; k < SB_STEMS; k++)
        for (uint32_t i = (p->len - n) * 2; i < p->len * 2; i++)
            if (p->s[k][i] > 8 || p->s[k][i] < -8) return 0;
    return 1;
}

/* ---------------------------------------------------------------- recording, a bit at a time */

enum { MAXH = 65536 };                     /* driver states seen (hash table) */

struct sndrec {
    sb_writer *w;
    uint8_t ids[64];
    int n, i;                              /* sounds, the one being recorded */
    int phase, f, ended, used, kind;       /* phase 0: the driver starts, 1: playing */
    int16_t dc[SB_STEMS][2];
    pcm out;
    int32_t loop;
    uint32_t hkey[MAXH], hpos[MAXH];
    uint8_t hused[MAXH];
    uint8_t prev[0x1FA0 - 0x1C20];
    int error;
};

static int16_t b[SB_STEMS][4096 * 2];
static int16_t *bp[SB_STEMS] = { b[0], b[1], b[2], b[3] };

static void begin_sound(sndrec *r)
{
    uint8_t id = r->ids[r->i];
    r->kind = id < 0x98 ? SB_SONG : id >= 0xE0 ? SB_VOICE : SB_EFFECT;
    r->phase = 0; r->f = 0; r->ended = 0; r->used = 0; r->loop = -1;
    memset(&r->out, 0, sizeof(r->out));
    memset(r->hused, 0, sizeof(r->hused));
    memset(r->prev, 0, sizeof(r->prev));
    md_snd_begin(0, 0);
}

/* a driver state seen before: where it was (or -1); remembered otherwise */
static int32_t seen(sndrec *r, uint32_t h, uint32_t pos)
{
    for (uint32_t k = h & (MAXH - 1), tries = 0; tries < MAXH; k = (k + 1) & (MAXH - 1), tries++) {
        if (!r->hused[k]) { r->hused[k] = 1; r->hkey[k] = h; r->hpos[k] = pos; return -1; }
        if (r->hkey[k] == h) return (int32_t)r->hpos[k];
    }
    return -1;
}

/* one video frame's time of the sound being recorded; 1 when it is done */
static int sound_frame(sndrec *r)
{
    md_snd_frame();
    int n = md_snd_take(bp, 4096);
    if (r->phase == 0) {                   /* the driver starts and waits, as in the game */
        if (++r->f < 30) return 0;
        /* the chips' output when nothing plays is a constant level (the YM2612's DC):
         * taken off, so silence is 0 and parts can be added up without it */
        for (int k = 0; k < SB_STEMS; k++) {
            r->dc[k][0] = n ? bp[k][(n - 1) * 2] : 0;
            r->dc[k][1] = n ? bp[k][(n - 1) * 2 + 1] : 0;
        }
        md_snd_command(r->ids[r->i]);
        r->phase = 1;
        r->f = 0;
        return 0;
    }
    for (int k = 0; k < SB_STEMS; k++)
        for (int i = 0; i < n * 2; i++) {
            int v = bp[k][i] - r->dc[k][i & 1];
            bp[k][i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        }
    pcm_add(&r->out, bp, n);
    int f = r->f++;
    if (f >= MAX_SECONDS * 60) return 1;
    if (r->kind == SB_VOICE)
        return f > 20 && silent_tail(&r->out, SB_RATE / 10);
    r->used |= effect_parts();
    if (!r->ended && f > 8 && (r->kind == SB_SONG ? !music_playing() : !effect_parts()))
        r->ended = f;                      /* the last note: let it fade */
    if (r->ended)
        return silent_tail(&r->out, SB_RATE / 20) || f - r->ended > 120;
    if (memcmp(r->prev, md.zram + 0x1C20, sizeof(r->prev))) {
        memcpy(r->prev, md.zram + 0x1C20, sizeof(r->prev));
        r->loop = seen(r, state_hash(), r->out.len);
        if (r->loop >= 0) return 1;        /* the same again: it repeats from there */
    }
    return 0;
}

sndrec *snd_record_start(const char *path)
{
    sndrec *r = calloc(1, sizeof(*r));
    if (!r) return 0;
    /* every sound the game can ask for: songs 0x81-0x8C, effects 0x98-0xBD, voices E4/E5 */
    for (int id = 0x81; id <= 0x8C; id++) r->ids[r->n++] = (uint8_t)id;
    for (int id = 0x98; id <= 0xBD; id++) r->ids[r->n++] = (uint8_t)id;
    r->ids[r->n++] = 0xE4;
    r->ids[r->n++] = 0xE5;
    r->w = sb_write_begin(path);
    if (!r->w) { free(r); return 0; }
    md_audio_set_rate(SB_RATE);
    begin_sound(r);
    return r;
}

int snd_record_step(sndrec *r, int frames)
{
    while (frames-- > 0 && r->i < r->n) {
        if (!sound_frame(r)) continue;
        uint8_t id = r->ids[r->i];
        sb_write_sound(r->w, id, (uint8_t)r->kind, (uint8_t)(r->kind == SB_EFFECT ? r->used : 0),
                       r->out.len, r->loop, r->out.s);
        pcm_free(&r->out);
        if (++r->i < r->n) begin_sound(r);
    }
    if (r->i < r->n) return 0;
    md_snd_end();
    int bad = sb_write_end(r->w);
    r->w = 0;
    return bad ? -1 : 1;
}

void snd_record_progress(const sndrec *r, int *done, int *total) { *done = r->i; *total = r->n; }

void snd_record_free(sndrec *r)
{
    if (!r) return;
    if (r->w) { sb_write_end(r->w); md_snd_end(); }
    pcm_free(&r->out);
    free(r);
}

int snd_record_bank(const char *path, void (*progress)(int done, int total, void *u), void *u)
{
    sndrec *r = snd_record_start(path);
    if (!r) return -1;
    int res;
    while ((res = snd_record_step(r, 600)) == 0)
        if (progress) progress(r->i, r->n, u);
    snd_record_free(r);
    return res < 0 ? -1 : 0;
}
