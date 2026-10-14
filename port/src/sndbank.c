/* The recorded sounds: file format and codecs.
 *
 * File (all little-endian):
 *   "BBSND001" (lossless) or "BBSND002" (Opus), u32 rate, u32 directory offset, then the
 *   coded blocks; the directory: u32 count (BBSND002: then u32 the Opus delay), per
 *   sound: u8 id, u8 kind, u8 channels, u8 0, u32 len, i32 loop, and per part: u32 len,
 *   u32 blocks, u32 offset of its block table (u32 offsets of its blocks from the file
 *   start).
 * Lossless blocks hold SB_BLOCK stereo samples (codec below), so any block can be decoded
 * on its own (looping, starting anywhere). Opus blocks are packets of SB_FRAME samples
 * (u16 length, the packet) at 48 kHz: the player decodes them in order, and from a few
 * packets before when it jumps. A part's Opus output lags it by the delay. Opus keeps
 * the file small enough to ship with the game (about a sixth of the lossless size). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <opus.h>
#include "sndbank.h"

/* ---------------------------------------------------------------- codec
 * Lossless, as FLAC does it: per block and side, the best of four fixed predictors
 * (order 0-3: the sample, its change, the change of the change, ...) and the prediction
 * errors in Rice codes, with the Rice parameter chosen per 256-sample partition. Stereo
 * blocks may be stored as mid/side. A block type byte: 0 silence, 1 both sides alike,
 * 2 left/right, 3 mid/side. */

typedef struct { uint8_t *p; uint32_t bit; } bw;
typedef struct { const uint8_t *p; uint32_t bit; } br;

static void put_bits(bw *w, uint32_t v, int n)
{
    for (int i = n - 1; i >= 0; i--) {
        uint32_t byte = w->bit >> 3;
        if ((w->bit & 7) == 0) w->p[byte] = 0;
        if ((v >> i) & 1) w->p[byte] |= (uint8_t)(0x80 >> (w->bit & 7));
        w->bit++;
    }
}
static uint32_t get_bits(br *r, int n)
{
    uint32_t v = 0;
    for (int i = 0; i < n; i++) {
        v = v << 1 | ((r->p[r->bit >> 3] >> (7 - (r->bit & 7))) & 1);
        r->bit++;
    }
    return v;
}
static void put_rice(bw *w, int32_t e, int k)
{
    uint32_t u = e < 0 ? ((uint32_t)(-e) << 1) - 1 : (uint32_t)e << 1;
    uint32_t q = u >> k;
    if (q >= 40) {                          /* escape: 40 ones, then 24 bits */
        for (int i = 0; i < 40; i++) put_bits(w, 1, 1);
        put_bits(w, 0, 1);
        put_bits(w, u, 24);
        return;
    }
    for (uint32_t i = 0; i < q; i++) put_bits(w, 1, 1);
    put_bits(w, 0, 1);
    if (k) put_bits(w, u & ((1u << k) - 1), k);
}
static int32_t get_rice(br *r, int k)
{
    uint32_t q = 0;
    while (get_bits(r, 1)) q++;
    uint32_t u = q >= 40 ? get_bits(r, 24) : (q << k) | (k ? get_bits(r, k) : 0);
    return u & 1 ? -(int32_t)((u + 1) >> 1) : (int32_t)(u >> 1);
}

#define PART 256

static int32_t predict(const int32_t *x, int i, int order)
{
    switch (order) {
    case 0: return 0;
    case 1: return x[i - 1];
    case 2: return 2 * x[i - 1] - x[i - 2];
    default: return 3 * x[i - 1] - 3 * x[i - 2] + x[i - 3];
    }
}

static int rice_k(const int32_t *e, int n)
{
    uint64_t sum = 0;
    for (int i = 0; i < n; i++) sum += (uint64_t)(e[i] < 0 ? -2 * (int64_t)e[i] - 1 : 2 * (int64_t)e[i]);
    int k = 0;
    while (k < 15 && ((uint64_t)n << (k + 1)) < sum) k++;
    return k;
}

static uint64_t cost(const int32_t *e, int n)
{
    uint64_t bits = 0;
    for (int a = 0; a < n; a += PART) {
        int m = n - a < PART ? n - a : PART, k = rice_k(e + a, m);
        bits += 4;
        for (int i = 0; i < m; i++) {
            uint32_t u = e[a + i] < 0 ? ((uint32_t)(-e[a + i]) << 1) - 1 : (uint32_t)e[a + i] << 1;
            bits += (u >> k) >= 40 ? 65 : (u >> k) + 1 + k;
        }
    }
    return bits;
}

static void put_side(bw *w, const int32_t *x, int n)
{
    static int32_t e[4][SB_BLOCK];
    int best = 0;
    uint64_t bc = ~0ull;
    for (int o = 0; o < 4; o++) {
        for (int i = 0; i < n; i++) e[o][i] = i < o ? x[i] : x[i] - predict(x, i, o);
        uint64_t c = cost(e[o], n);
        if (c < bc) { bc = c; best = o; }
    }
    put_bits(w, (uint32_t)best, 2);
    for (int i = 0; i < best; i++) put_bits(w, (uint32_t)x[i] & 0x1FFFF, 17);
    for (int a = best; a < n; ) {
        int end = (a / PART + 1) * PART, m = (end < n ? end : n) - a, k = rice_k(e[best] + a, m);
        put_bits(w, (uint32_t)k, 4);
        for (int i = 0; i < m; i++) put_rice(w, e[best][a + i], k);
        a += m;
    }
}

static void get_side(br *r, int32_t *x, int n)
{
    int o = (int)get_bits(r, 2);
    for (int i = 0; i < o && i < n; i++) { int32_t v = (int32_t)get_bits(r, 17); x[i] = v & 0x10000 ? v - 0x20000 : v; }
    for (int a = o; a < n; ) {
        int end = (a / PART + 1) * PART, m = (end < n ? end : n) - a, k = (int)get_bits(r, 4);
        for (int i = 0; i < m; i++) x[a + i] = get_rice(r, k) + predict(x, a + i, o);
        a += m;
    }
}

int sb_encode_block(const int16_t *pcm, int n, uint8_t *out)
{
    int silent = 1, mono = 1;
    for (int i = 0; i < n; i++) {
        if (pcm[i * 2] || pcm[i * 2 + 1]) silent = 0;
        if (pcm[i * 2] != pcm[i * 2 + 1]) mono = 0;
    }
    if (silent) { out[0] = 0; return 1; }
    static int32_t l[SB_BLOCK], r[SB_BLOCK], m[SB_BLOCK], s[SB_BLOCK];
    for (int i = 0; i < n; i++) {
        l[i] = pcm[i * 2]; r[i] = pcm[i * 2 + 1];
        m[i] = (l[i] + r[i]) >> 1; s[i] = l[i] - r[i];
    }
    bw w = { out + 1, 0 };
    if (mono) { out[0] = 1; put_side(&w, l, n); }
    else {
        static uint8_t a[SB_BLOCK * 8], b[SB_BLOCK * 8];
        bw wa = { a, 0 }, wb = { b, 0 };
        put_side(&wa, l, n); put_side(&wa, r, n);
        put_side(&wb, m, n); put_side(&wb, s, n);
        const uint8_t *src = wb.bit < wa.bit ? b : a;
        uint32_t bits = wb.bit < wa.bit ? wb.bit : wa.bit;
        out[0] = wb.bit < wa.bit ? 3 : 2;
        memcpy(out + 1, src, (bits + 7) >> 3);
        return 1 + (int)((bits + 7) >> 3);
    }
    return 1 + (int)((w.bit + 7) >> 3);
}

void sb_decode_block(const uint8_t *in, int16_t *pcm, int n)
{
    if (!n) return;
    if (in[0] == 0) { memset(pcm, 0, (size_t)n * 4); return; }
    static int32_t a[SB_BLOCK], b[SB_BLOCK];
    br rd = { in + 1, 0 };
    get_side(&rd, a, n);
    if (in[0] == 1) {
        for (int i = 0; i < n; i++) pcm[i * 2] = pcm[i * 2 + 1] = (int16_t)a[i];
        return;
    }
    get_side(&rd, b, n);
    for (int i = 0; i < n; i++) {
        int32_t L = a[i], R = b[i];
        if (in[0] == 3) {                   /* mid (L+R)>>1, side L-R */
            int32_t mid = a[i] * 2 | (b[i] & 1);
            L = (mid + b[i]) >> 1;
            R = (mid - b[i]) >> 1;
        }
        pcm[i * 2] = (int16_t)L;
        pcm[i * 2 + 1] = (int16_t)R;
    }
}

static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static uint32_t get32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

const sb_sound *sb_find(const sb_bank *b, uint8_t id)
{
    for (int i = 0; i < b->nsounds; i++)
        if (b->sound[i].id == id) return &b->sound[i];
    return 0;
}

/* ---------------------------------------------------------------- reading */

int sb_load(sb_bank *b, const char *path)
{
    memset(b, 0, sizeof(*b));
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 16) { fclose(f); return -1; }
    b->file = malloc((size_t)size);
    if (!b->file || fread(b->file, 1, (size_t)size, f) != (size_t)size) { fclose(f); sb_free(b); return -1; }
    fclose(f);
    const uint8_t *p = b->file;
    if (!memcmp(p, "BBSND001", 8)) b->codec = SB_LOSSLESS;
    else if (!memcmp(p, "BBSND002", 8)) b->codec = SB_OPUS;
    else { sb_free(b); return -1; }
    b->rate = get32(p + 8);
    if (b->rate < 8000 || b->rate > 96000) { sb_free(b); return -1; }
    uint32_t dir = get32(p + 12);
    if (dir + 8 > (uint32_t)size) { sb_free(b); return -1; }
    uint32_t n = get32(p + dir), o = dir + 4;
    if (b->codec == SB_OPUS) { b->delay = get32(p + o); o += 4; }
    if (n > sizeof(b->sound) / sizeof(b->sound[0])) { sb_free(b); return -1; }
    for (uint32_t i = 0; i < n; i++) {
        if (o + 12 + 12 * SB_STEMS > (uint32_t)size) { sb_free(b); return -1; }
        sb_sound *s = &b->sound[i];
        s->id = p[o]; s->kind = p[o + 1]; s->channels = p[o + 2];
        s->len = get32(p + o + 4);
        s->loop = (int32_t)get32(p + o + 8);
        o += 12;
        for (int k = 0; k < SB_STEMS; k++, o += 12) {
            sb_stem *t = &s->stem[k];
            t->len = get32(p + o);
            t->nblocks = get32(p + o + 4);
            uint32_t tab = get32(p + o + 8);
            if (t->nblocks && tab + 4 * t->nblocks > (uint32_t)size) { sb_free(b); return -1; }
            t->block_off = t->nblocks ? (uint32_t *)(b->file + tab) : 0;
            t->data = b->file;
            for (uint32_t j = 0; j < t->nblocks; j++) {      /* to this machine's order */
                uint32_t v = get32(b->file + tab + 4 * j);
                if (v >= (uint32_t)size) { sb_free(b); return -1; }
                t->block_off[j] = v;
            }
        }
        b->nsounds++;
    }
    return 0;
}

void sb_free(sb_bank *b)
{
    free(b->file);
    memset(b, 0, sizeof(*b));
}

int sb_decoder_size(void) { return opus_decoder_get_size(2); }

void sb_decoder_reset(void *dec)
{
    opus_decoder_init((OpusDecoder *)dec, 48000, 2);
}

int sb_decode_packet(void *dec, const sb_stem *st, uint32_t k, int16_t *pcm)
{
    if (k >= st->nblocks) { memset(pcm, 0, SB_FRAME * 4); return 0; }
    const uint8_t *q = st->data + st->block_off[k];
    int len = q[0] | q[1] << 8;
    int n = opus_decode((OpusDecoder *)dec, q + 2, len, pcm, SB_FRAME, 0);
    if (n != SB_FRAME) { memset(pcm, 0, SB_FRAME * 4); return -1; }
    return 0;
}

/* ---------------------------------------------------------------- writing */

typedef struct { uint8_t id, kind, channels; uint32_t len; int32_t loop; uint32_t slen[SB_STEMS], nb[SB_STEMS], tab[SB_STEMS]; } wsound;

struct sb_writer {
    FILE *f;
    uint32_t pos;
    int n;
    int opus;
    uint32_t delay;
    wsound s[96];
};

int sb_write_lossless;
uint32_t sb_write_rate(void) { return sb_write_lossless ? 44100 : 48000; }

/* per stereo part: songs 96 kbit/s; effects and voices (short, sharp) 160 */
#define OPUS_BITRATE_SONG  96000
#define OPUS_BITRATE_OTHER 160000

static void wbytes(sb_writer *w, const void *p, size_t n) { fwrite(p, 1, n, w->f); w->pos += (uint32_t)n; }

sb_writer *sb_write_begin(const char *path)
{
    sb_writer *w = calloc(1, sizeof(*w));
    if (!w) return 0;
    w->f = fopen(path, "wb");
    if (!w->f) { free(w); return 0; }
    w->opus = !sb_write_lossless;
    uint8_t h[16];
    memcpy(h, w->opus ? "BBSND002" : "BBSND001", 8);
    put32(h + 8, sb_write_rate());
    put32(h + 12, 0);
    wbytes(w, h, 16);
    return w;
}

void sb_write_sound(sb_writer *w, uint8_t id, uint8_t kind, uint8_t channels, uint32_t len, int32_t loop,
                    int16_t *const stems[SB_STEMS])
{
    if (w->n >= 96) return;
    wsound *s = &w->s[w->n++];
    s->id = id; s->kind = kind; s->channels = channels; s->len = len; s->loop = loop;
    static uint8_t blk[SB_BLOCK * 16];
    for (int k = 0; k < SB_STEMS; k++) {
        int any = 0;
        for (uint32_t i = 0; stems[k] && i < len * 2 && !any; i++) any = stems[k][i] != 0;
        if (!any) continue;                      /* a silent part is not stored */
        if (w->opus) {
            int err;
            OpusEncoder *enc = opus_encoder_create(48000, 2, OPUS_APPLICATION_AUDIO, &err);
            if (!enc) continue;
            opus_encoder_ctl(enc, OPUS_SET_BITRATE(kind == SB_SONG ? OPUS_BITRATE_SONG : OPUS_BITRATE_OTHER));
            opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(10));
            opus_encoder_ctl(enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
            opus_int32 la = 0;
            opus_encoder_ctl(enc, OPUS_GET_LOOKAHEAD(&la));
            w->delay = (uint32_t)la;
            /* the decoder's output lags by the delay: frames to cover it, the rest silence */
            uint32_t nb = (len + (uint32_t)la + SB_FRAME - 1) / SB_FRAME;
            uint32_t *off = malloc(4 * nb);
            static int16_t frame[SB_FRAME * 2];
            static uint8_t pk[4000];
            for (uint32_t j = 0; j < nb; j++) {
                uint32_t a = j * SB_FRAME;
                memset(frame, 0, sizeof(frame));
                if (a < len) memcpy(frame, stems[k] + a * 2, (size_t)(len - a < SB_FRAME ? len - a : SB_FRAME) * 4);
                opus_int32 n = opus_encode(enc, frame, SB_FRAME, pk + 2, (opus_int32)sizeof(pk) - 2);
                if (n < 0) n = 0;
                pk[0] = (uint8_t)n; pk[1] = (uint8_t)(n >> 8);
                off[j] = w->pos;
                wbytes(w, pk, (size_t)n + 2);
            }
            opus_encoder_destroy(enc);
            s->slen[k] = len;
            s->nb[k] = nb;
            s->tab[k] = w->pos;
            for (uint32_t j = 0; j < nb; j++) { uint8_t q[4]; put32(q, off[j]); wbytes(w, q, 4); }
            free(off);
            continue;
        }
        uint32_t nb = (len + SB_BLOCK - 1) / SB_BLOCK;
        uint32_t *off = malloc(4 * nb);
        for (uint32_t j = 0; j < nb; j++) {
            uint32_t a = j * SB_BLOCK, m = len - a < SB_BLOCK ? len - a : SB_BLOCK;
            off[j] = w->pos;
            wbytes(w, blk, (size_t)sb_encode_block(stems[k] + a * 2, (int)m, blk));
        }
        s->slen[k] = len;
        s->nb[k] = nb;
        s->tab[k] = w->pos;
        for (uint32_t j = 0; j < nb; j++) { uint8_t q[4]; put32(q, off[j]); wbytes(w, q, 4); }
        free(off);
    }
}

int sb_write_end(sb_writer *w)
{
    uint32_t dir = w->pos;
    uint8_t q[16];
    put32(q, (uint32_t)w->n);
    wbytes(w, q, 4);
    if (w->opus) { put32(q, w->delay); wbytes(w, q, 4); }
    for (int i = 0; i < w->n; i++) {
        wsound *s = &w->s[i];
        q[0] = s->id; q[1] = s->kind; q[2] = s->channels; q[3] = 0;
        put32(q + 4, s->len);
        put32(q + 8, (uint32_t)s->loop);
        wbytes(w, q, 12);
        for (int k = 0; k < SB_STEMS; k++) {
            put32(q, s->slen[k]); put32(q + 4, s->nb[k]); put32(q + 8, s->tab[k]);
            wbytes(w, q, 12);
        }
    }
    fseek(w->f, 12, SEEK_SET);
    put32(q, dir);
    fwrite(q, 1, 4, w->f);
    int bad = ferror(w->f);
    bad |= fclose(w->f) != 0;
    free(w);
    return bad ? -1 : 0;
}
