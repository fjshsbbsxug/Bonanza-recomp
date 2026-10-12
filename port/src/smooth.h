/* Smooth Motion: pictures in between game frames on fast displays (see smooth.c). */
#ifndef SMOOTH_H
#define SMOOTH_H
#include <stdint.h>

typedef struct {
    int on;             /* the band can be moved (no cut) */
    int y0, h;          /* lines */
    float ax, ay;       /* shift of plane A and of sprites fixed in the level, in pixels */
    float bx, by;       /* shift of plane B */
} smooth_band;

void smooth_frame_done(void);                    /* after every game frame */
int  smooth_band_at(int band, double t, smooth_band *out);  /* t: 0 = previous frame, 1 = newest */
/* shift of a layer (LAYER_*) in a band */
void smooth_layer_shift(const smooth_band *b, int layer, float *dx, float *dy);
/* software version for tests: the newest picture moved back to t (whole pixels) */
void smooth_compose(double t, uint32_t *out);    /* MD_FB_W x 224, like md_framebuffer() */

#endif
