#ifndef PSG_H
#define PSG_H
#include <stdint.h>

typedef struct {
    int      latch;
    int      tone[3];
    int      noise;
    int      vol[4];
    int      counter[4];
    int      out[4];
    int      noise_toggle;
    uint16_t lfsr;
    int      mute;      /* channels (bit n = channel n) left out of the output (recording) */
    double   frac;
} psg_t;

extern psg_t psg;

void    psg_reset(void);
void    psg_write(uint8_t v);
int16_t psg_sample(double ticks);
void psg_sample_parts(double ticks, int mask_a, int16_t *a, int16_t *b);   /* mono sample after advancing `ticks` PSG ticks */

#endif
