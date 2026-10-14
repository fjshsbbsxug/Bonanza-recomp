/* Playing the recorded sounds (sndbank.h) the way the game's sound driver plays them,
 * so no sound hardware runs while the game plays.
 *
 * The game's requests arrive where it writes them for the driver (Z80 RAM 0x1C0A: a
 * sound or a command; 0x1C10: pause / resume) and follow the driver's rules
 * (port/gen/z80_driver.c, the driver at ROM 0x33000):
 *   0x81-0x97  a song: the song before it and every effect stop;
 *   0x98-0xDF  an effect: priority from the table at ROM 0x381EF (0x80: always taken;
 *              otherwise only if not below the playing effect's), an effect on the same
 *              channels is replaced, and the song's part on those channels (FM5, FM6,
 *              PSG3) is left out until it ends;
 *   E0 fade out (40 steps every 6 music ticks, then everything stops), E1 stop all,
 *   E2 PSG off, E3 special effects off, E4/E5 the voices (song and effects wait);
 *   0x1C10: 1 pause (silence, everything waits), 0x80 resume.
 * Positions run at the bank's rate, resampled to the output rate. Opus parts are decoded
 * packet after packet; a jump (a loop, a start) decodes from a few packets before. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "md.h"
#include "sndbank.h"
#include "sndmix.h"

typedef struct {               /* a recorded part being played */
    const sb_stem *st;
    int32_t block;             /* block (Opus: packet) in buf, -1: none */
    int16_t buf[SB_BLOCK * 2];
    void *dec;                 /* Opus: its decoder */
} part;

typedef struct {
    const sb_sound *s;
    double pos;                /* in samples of the bank */
    part p[SB_STEMS];
} voice;

static sb_bank bank;
static int have_bank;
static voice song, fx[4], vox;
static int prio;               /* the playing effect's priority (the driver's 0x1C18) */
static int paused, psg_off;
static double fade;            /* gain of the song while fading out */
static double fade_t;          /* seconds since the fade began, -1: no fade */

/* the frame's sound is made when the frame is over: a sound asked for on line L starts
 * that far into it */
static double request_delay(void)
{
    return md.line / 262.0 * bank.rate * (3420.0 * 262) / 53693175.0;
}

static void start(voice *v, const sb_sound *s)
{
    v->s = s;
    v->pos = -request_delay();
    for (int k = 0; k < SB_STEMS; k++) { v->p[k].st = &s->stem[k]; v->p[k].block = -1; }
}

#define PREROLL 4              /* Opus packets decoded before the one wanted, after a jump */

/* the Opus packet holding output sample i of the part (the delay counted in) */
static void opus_seek(part *p, int32_t k)
{
    if (p->block < 0 || k != p->block + 1) {
        sb_decoder_reset(p->dec);
        for (int32_t j = k - PREROLL < 0 ? 0 : k - PREROLL; j < k; j++)
            sb_decode_packet(p->dec, p->st, (uint32_t)j, p->buf);
    }
    sb_decode_packet(p->dec, p->st, (uint32_t)k, p->buf);
    p->block = k;
}
static void stop(voice *v) { v->s = 0; }

static void sample(part *p, uint32_t i, int32_t *l, int32_t *r)
{
    const sb_stem *st = p->st;
    if (!st->nblocks || i >= st->len) return;
    if (bank.codec == SB_OPUS) {
        uint32_t o = i + bank.delay;
        int32_t k = (int32_t)(o / SB_FRAME);
        if (k != p->block) opus_seek(p, k);
        uint32_t q = o - (uint32_t)k * SB_FRAME;
        *l += p->buf[q * 2];
        *r += p->buf[q * 2 + 1];
        return;
    }
    int32_t b = (int32_t)(i / SB_BLOCK);
    if (b != p->block) {
        uint32_t a = (uint32_t)b * SB_BLOCK, n = st->len - a < SB_BLOCK ? st->len - a : SB_BLOCK;
        sb_decode_block(st->data + st->block_off[b], p->buf, (int)n);
        p->block = b;
    }
    uint32_t k = i - (uint32_t)b * SB_BLOCK;
    *l += p->buf[k * 2];
    *r += p->buf[k * 2 + 1];
}

/* one output sample of a sound (parts in mask), then step on; 0 when it has ended */
static int play(voice *v, int mask, double step, int32_t *l, int32_t *r)
{
    const sb_sound *s = v->s;
    if (!s) return 0;
    if (v->pos < 0) { v->pos += step; return 1; }     /* not yet */
    uint32_t i = (uint32_t)v->pos;
    double t = v->pos - i;
    for (int k = 0; k < SB_STEMS; k++) {
        if (!((mask >> k) & 1)) continue;
        int32_t l0 = 0, r0 = 0, l1 = 0, r1 = 0;
        sample(&v->p[k], i, &l0, &r0);
        uint32_t j = i + 1 < s->len ? i + 1 : (s->loop >= 0 ? (uint32_t)s->loop : i);
        sample(&v->p[k], j, &l1, &r1);
        *l += (int32_t)(l0 + (l1 - l0) * t);
        *r += (int32_t)(r0 + (r1 - r0) * t);
    }
    v->pos += step;
    if (v->pos >= s->len) {
        if (s->loop < 0) { stop(v); return 0; }
        v->pos = s->loop + (v->pos - s->len);
    }
    return 1;
}

static void decoders(void)
{
    static void *mem;
    if (mem) return;
    int size = sb_decoder_size();
    voice *all[6] = { &song, &fx[0], &fx[1], &fx[2], &fx[3], &vox };
    mem = calloc(6 * SB_STEMS, (size_t)size);
    if (!mem) return;
    for (int v = 0; v < 6; v++)
        for (int k = 0; k < SB_STEMS; k++)
            all[v]->p[k].dec = (char *)mem + (size_t)(v * SB_STEMS + k) * (size_t)size;
}

int snd_open(const char *path)
{
    if (have_bank) sb_free(&bank);
    have_bank = sb_load(&bank, path) == 0 && sb_find(&bank, 0x81);
    if (have_bank && bank.codec == SB_OPUS) {
        decoders();
        if (!song.p[0].dec) { sb_free(&bank); have_bank = 0; }
    }
    snd_reset();
    return have_bank;
}
void snd_close(void) { if (have_bank) sb_free(&bank); have_bank = 0; }
int snd_ready(void) { return have_bank; }

void snd_reset(void)
{
    stop(&song); stop(&vox);
    for (int i = 0; i < 4; i++) stop(&fx[i]);
    prio = 0; paused = 0; psg_off = 0; fade = 1; fade_t = -1;
}

static void stop_all(void)
{
    stop(&song);
    for (int i = 0; i < 4; i++) stop(&fx[i]);
    prio = 0; fade = 1; fade_t = -1; psg_off = 0;
}

/* the parts of the song the effects have taken over */
static int taken(void)
{
    int m = 0;
    for (int i = 0; i < 4; i++) if (fx[i].s) m |= fx[i].s->channels & 0xE;
    return m;
}

void snd_request(uint8_t id)
{
    static int log = -1;
    if (log < 0) log = getenv("SNDLOG") != 0;
    if (log) fprintf(stderr, "snd request %02X frame %u\n", id, md.frame);
    if (!have_bank || !(id & 0x80)) return;
    if (id >= 0xE0) {                       /* commands */
        prio = 0;
        switch (id) {
        case 0xE0: if (fade_t < 0) fade_t = 0; break;   /* fade out */
        case 0xE1: stop_all(); break;
        case 0xE2: psg_off = 1; break;
        case 0xE4: case 0xE5: {               /* the driver silences every channel first: */
            const sb_sound *s = sb_find(&bank, id); /* effects are cut, the song waits */
            if (s) { start(&vox, s); for (int i = 0; i < 4; i++) stop(&fx[i]); }
        } break;
        default: break;
        }
        return;
    }
    const sb_sound *s = sb_find(&bank, id);
    if (!s) return;
    if (s->kind == SB_SONG) {
        stop_all();
        start(&song, s);
        return;
    }
    int p = md.rom[0x381EF + (id - 0x81)];
    if (p != 0x80 && p < prio) return;      /* a more important effect is playing */
    if (p != 0x80) prio = p;
    int used = s->channels ? s->channels : 1, slot = -1;
    for (int i = 0; i < 4; i++)              /* the effect on the same channels makes way */
        if (fx[i].s && (fx[i].s->channels & used)) { stop(&fx[i]); if (slot < 0) slot = i; }
    for (int i = 0; slot < 0 && i < 4; i++) if (!fx[i].s) slot = i;
    if (slot < 0) slot = 0;
    start(&fx[slot], s);
}

void snd_pause(uint8_t v)
{
    if (v == 0x01) paused = 1;
    else if (v == 0x80) {
        if (paused && fade_t >= 0) stop_all();  /* a fade cut short by a pause: all stops */
        paused = 0;
    }
}

void snd_render(int16_t *out, int n, int rate)
{
    double step = (double)bank.rate / rate;
    for (int i = 0; i < n; i++) {
        int32_t l = 0, r = 0;
        if (have_bank && !paused) {
            if (vox.s) {
                play(&vox, 0xF, step, &l, &r);   /* everything else waits for the voice */
            } else {
                int32_t ml = 0, mr = 0;
                int mask = 0xF & ~taken() & (psg_off ? ~8 : 0xF);
                play(&song, mask, step, &ml, &mr);
                for (int k = 0; k < 4; k++)
                    if (fx[k].s && !play(&fx[k], 0xF, step, &l, &r) && fx[k].s == 0)
                        prio = 0;                  /* an effect ended */
                if (fade_t >= 0) {
                    /* the driver: one step (0.75 dB) quieter every 6 music ticks
                     * (timer A, about 52 Hz), 40 steps, then everything stops */
                    double steps = fade_t * 52.0 / 6.0;
                    fade_t += 1.0 / rate;
                    if (steps >= 40) { stop_all(); ml = mr = 0; }
                    else fade = pow(10.0, -0.75 * steps / 20.0);
                }
                l += (int32_t)(ml * fade);
                r += (int32_t)(mr * fade);
            }
        }
        out[i * 2] = (int16_t)(l > 32767 ? 32767 : l < -32768 ? -32768 : l);
        out[i * 2 + 1] = (int16_t)(r > 32767 ? 32767 : r < -32768 ? -32768 : r);
    }
}
