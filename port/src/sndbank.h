/* The recorded sounds (songs, effects, voices) the port plays instead of running the
 * sound hardware. See sndbank.c (file, codec), sndrec.c (recording), sndmix.c (playing). */
#ifndef SNDBANK_H
#define SNDBANK_H
#include <stdint.h>

#define SB_RATE   44100            /* samples per second, stereo */
#define SB_BLOCK  1024             /* samples per coded block */
#define SB_STEMS  4                /* parts: other channels, FM5, FM6, PSG3 + noise */

enum { SB_SONG, SB_EFFECT, SB_VOICE };

typedef struct {
    uint32_t len;                  /* samples; 0: the part is silent throughout */
    uint32_t nblocks;
    const uint8_t *data;           /* coded blocks, one after the other */
    uint32_t *block_off;           /* offset of each block in data */
} sb_stem;

typedef struct {
    uint8_t id, kind;
    uint8_t channels;              /* effect: parts it takes over (bit n = part n) */
    uint32_t len;                  /* samples */
    int32_t loop;                  /* sample the sound loops back to, -1: it ends */
    sb_stem stem[SB_STEMS];
} sb_sound;

typedef struct {
    int nsounds;
    sb_sound sound[96];
    uint8_t *file;                 /* the loaded file (the stems point into it) */
} sb_bank;

/* the sound with this id, or 0 */
const sb_sound *sb_find(const sb_bank *b, uint8_t id);

/* file: load (0 on success) and free */
int  sb_load(sb_bank *b, const char *path);
void sb_free(sb_bank *b);

/* coding (lossless): one block of up to SB_BLOCK stereo samples -> bytes (returns the
 * size; at most SB_BLOCK * 16), and back */
int  sb_encode_block(const int16_t *pcm, int n, uint8_t *out);
void sb_decode_block(const uint8_t *in, int16_t *pcm, int n);

/* writing a bank: start, add parts, finish */
typedef struct sb_writer sb_writer;
sb_writer *sb_write_begin(const char *path);
void sb_write_sound(sb_writer *w, uint8_t id, uint8_t kind, uint8_t channels, uint32_t len, int32_t loop,
                    int16_t *const stems[SB_STEMS]);
int  sb_write_end(sb_writer *w);   /* 0 on success */

#endif
