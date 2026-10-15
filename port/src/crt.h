/* The picture as a CRT shows it (crt.c): scanlines, the beam, the phosphor mask, glow,
 * the tube's curve. */
#ifndef CRT_H
#define CRT_H
#include <stdint.h>

enum {
    CRT_OFF,
    CRT_SCANLINES,     /* the beam's lines alone: no mask, flat */
    CRT_TV,            /* a consumer TV over RGB: softer, slot mask, curved glass, glow */
    CRT_MONITOR,       /* a broadcast/PVM monitor: sharp, aperture grille, flat */
    CRT_COUNT
};

const char *crt_name(int kind);

/* runs job(u, from, to) over 0..n, split among threads (or all at once) */
typedef void (*crt_parallel)(void (*job)(void *u, int from, int to), void *u, int n);

/* src: XRGB8888 picture of sw x sh pixels, stride pixels per line; dst: XRGB8888, dw x dh
 * pixels (dw per line), the screen pixels the picture covers. par may be 0. */
void crt_render(int kind, const uint32_t *src, int stride, int sw, int sh,
                uint32_t *dst, int dw, int dh, crt_parallel par);

#endif
