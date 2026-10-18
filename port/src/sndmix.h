/* Playing the recorded sounds as the game's driver would. See sndmix.c. */
#ifndef SNDMIX_H
#define SNDMIX_H
#include <stdint.h>

int  snd_open(const char *bank_path);    /* 1: the bank is there and loaded */
void snd_close(void);
int  snd_ready(void);
void snd_reset(void);                     /* machine reset: silence */
void snd_request(uint8_t id);             /* the game wrote id to Z80 RAM 0x1C0A */
void snd_pause(uint8_t v);                /* ... to 0x1C10 */
void snd_render(int16_t *out, int n, int rate);   /* n stereo samples at rate */
void snd_music_gain(double g);            /* the songs' volume, 0..1 (eased in / out) */

#endif
