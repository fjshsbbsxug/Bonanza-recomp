/* SN76489 PSG (as integrated in the Mega Drive VDP) */
#include <string.h>
#include "psg.h"

psg_t psg;

static const int16_t vol_table[16] = {
    2800, 2224, 1767, 1403, 1115, 886, 704, 559, 444, 353, 280, 222, 177, 140, 111, 0
};

void psg_reset(void)
{
    memset(&psg, 0, sizeof(psg));
    for (int i = 0; i < 4; i++) {
        psg.vol[i] = 15;
        psg.out[i] = 1;
    }
    psg.lfsr = 0x8000;
}

void psg_write(uint8_t v)
{
    if (v & 0x80) {
        psg.latch = (v >> 4) & 7;
        int ch = psg.latch >> 1;
        if (psg.latch & 1)
            psg.vol[ch] = v & 0x0F;
        else if (ch == 3) {
            psg.noise = v & 7;
            psg.lfsr = 0x8000;
        } else
            psg.tone[ch] = (psg.tone[ch] & 0x3F0) | (v & 0x0F);
    } else {
        int ch = psg.latch >> 1;
        if (psg.latch & 1)
            psg.vol[ch] = v & 0x0F;
        else if (ch == 3) {
            psg.noise = v & 7;
            psg.lfsr = 0x8000;
        } else
            psg.tone[ch] = (psg.tone[ch] & 0x00F) | ((v & 0x3F) << 4);
    }
}

/* advance by one PSG tick (clock / 16) */
static void tick(void)
{
    for (int ch = 0; ch < 3; ch++) {
        if (--psg.counter[ch] <= 0) {
            psg.counter[ch] = psg.tone[ch] ? psg.tone[ch] : 1;
            psg.out[ch] ^= 1;
        }
    }
    if (--psg.counter[3] <= 0) {
        int r = psg.noise & 3;
        psg.counter[3] = r == 3 ? (psg.tone[2] ? psg.tone[2] : 1) : (0x10 << r);
        psg.noise_toggle ^= 1;
        if (psg.noise_toggle) {
            int fb = (psg.noise & 4) ? ((psg.lfsr & 1) ^ ((psg.lfsr >> 3) & 1)) : (psg.lfsr & 1);
            psg.lfsr = (uint16_t)((psg.lfsr >> 1) | (fb << 15));
            psg.out[3] = psg.lfsr & 1;
        }
    }
}

/* the same, split: channels in mask_a and the others */
void psg_sample_parts(double ticks, int mask_a, int16_t *a, int16_t *b)
{
    psg.frac += ticks;
    int32_t acc[2] = {0, 0}, n = 0;
    while (psg.frac >= 1.0) {
        psg.frac -= 1.0;
        tick();
        for (int ch = 0; ch < 4; ch++)
            acc[(mask_a >> ch) & 1 ? 0 : 1] += psg.out[ch] ? vol_table[psg.vol[ch]] : -vol_table[psg.vol[ch]];
        n++;
    }
    *a = n ? (int16_t)(acc[0] / n / 2) : 0;
    *b = n ? (int16_t)(acc[1] / n / 2) : 0;
}

int16_t psg_sample(double ticks)
{
    psg.frac += ticks;
    int32_t acc = 0, n = 0;
    while (psg.frac >= 1.0) {
        psg.frac -= 1.0;
        tick();
        int32_t s = 0;
        for (int ch = 0; ch < 4; ch++)
            if (!((psg.mute >> ch) & 1))
                s += psg.out[ch] ? vol_table[psg.vol[ch]] : -vol_table[psg.vol[ch]];
        acc += s;
        n++;
    }
    return n ? (int16_t)(acc / n / 2) : 0;
}
