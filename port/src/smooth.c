/* Smooth Motion: on displays faster than the Mega Drive's 59.92 Hz, show pictures in
 * between two game frames. The game keeps running at its own speed and never sees
 * these pictures: the renderer keeps each play view as layers (vdp.c), and a picture
 * at time t between the previous game frame (0) and the newest one (1) is the newest
 * frame with the level layers (planes A and B, and sprites of objects that stand still
 * in the level) moved back by the part of the last camera step not yet "reached".
 * Moving characters stay where the game put them. Where a view's camera jumps (new
 * level, floor change, respawn) nothing is moved. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "smooth.h"
#include "vdp.h"
#include "md.h"
#include "settings.h"

static vdp_band prev[2], cur[2];

void smooth_frame_done(void)
{
    memcpy(prev, cur, sizeof(prev));
    memcpy(cur, vdp_bands, sizeof(cur));
}

static int wrap10(int d)            /* difference of two 10-bit scroll values */
{
    d &= 0x3FF;
    return d >= 512 ? d - 1024 : d;
}

int smooth_band_at(int band, double t, smooth_band *o)
{
    const vdp_band *p = &prev[band], *c = &cur[band];
    memset(o, 0, sizeof(*o));
    o->y0 = c->y0;
    o->h = 80;
    if (!c->valid || !p->valid || p->y0 != c->y0)
        return 0;
    int dx = c->camx - p->camx, dy = c->wy - p->wy;
    int dbx = wrap10(c->hsB - p->hsB), dby = wrap10(c->vsB - p->vsB);
    if (abs(dx) > 24 || abs(dy) > 24 || abs(dbx) > 24 || abs(dby) > 24)
        return 0;                           /* a cut: show the newest picture as it is */
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    float k = (float)(1.0 - t);
    o->ax = k * dx;
    o->ay = k * dy;
    o->bx = -k * dbx;
    o->by = k * dby;
    o->on = 1;
    return 1;
}

void smooth_layer_shift(const smooth_band *b, int layer, float *dx, float *dy)
{
    switch (layer) {
    case LAYER_B_LO: case LAYER_B_HI: *dx = b->bx; *dy = b->by; break;
    case LAYER_SS_LO: case LAYER_SS_HI: *dx = 0; *dy = 0; break;
    default: *dx = b->ax; *dy = b->ay; break;
    }
}

static uint32_t to_rgb(uint16_t c)
{
    return c < 512 ? md_color_lut[c] : c >= VDP_CHAR_BASE ? char_color_lut[(c - VDP_CHAR_BASE) & 15] : 0xFF000000u;
}

void smooth_compose(double t, uint32_t *out)
{
    const uint32_t *base = md_framebuffer();
    int w = md_screen_width();
    memcpy(out, base, sizeof(uint32_t) * MD_FB_W * MD_SCREEN_H);
    for (int band = 0; band < 2; band++) {
        smooth_band b;
        if (!smooth_band_at(band, t, &b))
            continue;
        for (int y = b.y0; y < b.y0 + b.h; y++)
            for (int x = 0; x < w; x++) {
                uint32_t px = out[y * MD_FB_W + x];
                for (int l = 0; l < LAYER_COUNT; l++) {
                    float fx, fy;
                    smooth_layer_shift(&b, l, &fx, &fy);
                    int sx = x - (int)lroundf(fx), sy = y - (int)lroundf(fy);
                    if (sx < 0 || sx >= w || sy < b.y0 || sy >= b.y0 + b.h)
                        continue;                       /* uncovered edge: the newest picture */
                    uint16_t c = vdp_layer[l][sy][sx];
                    if (c != VDP_LAYER_EMPTY)
                        px = to_rgb(c);
                }
                out[y * MD_FB_W + x] = px;
            }
    }
}
