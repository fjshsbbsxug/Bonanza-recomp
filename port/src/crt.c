/* The picture as a CRT shows it, worked out for every pixel of the screen it covers.
 *
 * In linear light (the tube's gamma, 2.4, then the screen's sRGB):
 *  - across a line the console sends each pixel as a flat step (nearest neighbour); the
 *    signal's limited bandwidth rounds off the steps: each screen column takes each
 *    picture pixel's box blurred by a Gaussian (the integral of the Gaussian over the
 *    pixel), so pixels keep flat middles and blend only at their edges;
 *  - down the screen the electron beam draws each line as a Gaussian spot whose height
 *    grows with its brightness (bright lines swell into the dark gap between lines, dark
 *    ones are thin), for each of red, green and blue on its own; the spot carries the
 *    line's light, so the picture's brightness is kept and only its spread changes;
 *  - the phosphor mask (aperture grille or slot mask) is laid on the screen's own pixels,
 *    three columns to a triad, so it never shimmers; its loss of light is made up;
 *  - halation: some light spreads in the glass (a blurred copy of the picture);
 *  - the tube: a curved face (the picture bows out), rounded corners, darker edges.
 * Profiles: scanlines alone; a consumer TV over RGB (softer, slot mask, curved, glow); a
 * broadcast monitor (sharp, aperture grille, flat). */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "crt.h"

typedef struct {
    float hsig;          /* signal softness along a line (bandwidth): Gaussian sigma, pixels */
    float vmin, vmax;    /* beam height (sigma, in lines) for black and for full brightness */
    int   mask;          /* 0 none, 1 aperture grille, 2 slot mask */
    float dark;          /* light the mask lets through over the other phosphors */
    float curve;         /* the face's curve */
    float corner;        /* corner radius, share of the picture height */
    float glow;          /* halation: share of the light spread in the glass */
    float vignette;      /* darker towards the edges */
} profile;

static const profile PROFILES[CRT_COUNT] = {
    [CRT_SCANLINES] = { 0.16f, 0.20f, 0.38f, 0, 1.00f, 0.000f, 0.000f, 0.02f, 0.00f },
    [CRT_TV]        = { 0.24f, 0.22f, 0.42f, 2, 0.60f, 0.040f, 0.035f, 0.05f, 0.22f },
    [CRT_MONITOR]   = { 0.13f, 0.18f, 0.35f, 1, 0.50f, 0.000f, 0.000f, 0.025f, 0.08f },
};

const char *crt_name(int kind)
{
    static const char *names[CRT_COUNT] = { "Off", "Scanlines", "CRT TV", "CRT monitor" };
    return kind > 0 && kind < CRT_COUNT ? names[kind] : names[0];
}

#define DQ_PER_LINE 64           /* beam: steps per line of distance */
#define DQ (DQ_PER_LINE * 5 / 2) /* up to 2.5 lines away */
#define OUTQ 4096                /* linear light -> sRGB table */
#define GLOW_TAPS 4              /* glow blur: taps each side (half size) */
#define MAXW 8192

/* The beam's spot for a line at distance d, carrying light I: I * w(d, I), its height
 * growing with sqrt(I). Kept as I * (b0 + b1 s + b2 s^2 + b3 s^3), s = sqrt(I), fitted per
 * distance (so the screen pass is plain arithmetic the compiler can vectorise). */
typedef struct { float b[4]; int on; } beamfit;   /* on: adds visible light */

static struct {
    int kind, sw, sh, dw, dh, curved, mask;
    const profile *p;
    int hw2, hh2;                 /* the picture at half size (glow) */
    int *hidx; float *hwt;        /* per screen column: 4 picture pixels and their weights */
    int *gx; float *gt;           /* per screen column: glow column, fraction */
    int *widx; float *wtx, *wty, *wa;   /* curved, per screen pixel: where in the flat picture, how much shows */
    float *vy;                    /* edge darkening by row (flat) */
    float *mpat;                  /* mask (with its gain and the column darkening): [4 rows][3][dw] */
    float *lin;                   /* the picture in linear light, planar: 3 x sh x sw */
    float *half, *blur, *tmp;     /* glow: half size, planar */
    float *hs;                    /* each line across the screen, planar: 3 x sh x dw */
    float *hq;                    /* and its square root (the beam's height goes with it) */
    float *gh;                    /* the glow across the screen, planar: 3 x hh2 x dw */
    float *flat;                  /* curved: the flat picture, planar: 3 x dh x dw */
    float *ones;                  /* dw of 1 */
    beamfit beam[DQ];
    float enc_fit[6];             /* sRGB from linear: polynomial in sqrt (vectorised) */
    float dec[256];
    uint8_t enc[OUTQ];
    float gw[GLOW_TAPS + 1];
    int tables;
    /* this frame */
    const uint32_t *src;
    int stride;
    uint32_t *dst;
} C;

#define PL(base, c, n) ((base) + (size_t)(c) * (n))   /* channel c of a planar buffer of n per channel */

static void free_all(void)
{
    void *p[] = { C.hidx, C.hwt, C.gx, C.gt, C.widx, C.wtx, C.wty, C.wa, C.vy, C.mpat, C.lin, C.half, C.blur,
                  C.tmp, C.hs, C.hq, C.gh, C.flat, C.ones };
    for (size_t i = 0; i < sizeof(p) / sizeof(p[0]); i++) free(p[i]);
    C.hidx = 0; C.hwt = 0; C.gx = 0; C.gt = 0; C.widx = 0; C.wtx = C.wty = C.wa = 0; C.vy = 0; C.mpat = 0;
    C.lin = C.half = C.blur = C.tmp = C.hs = C.hq = C.gh = C.flat = C.ones = 0;
}

static void fit_encode(void);

static void gamma_tables(void)
{
    for (int i = 0; i < 256; i++) C.dec[i] = powf(i / 255.0f, 2.4f);       /* the tube */
    for (int i = 0; i < OUTQ; i++) {                                        /* the screen */
        float v = (float)i / (OUTQ - 1);
        float s = v <= 0.0031308f ? 12.92f * v : 1.055f * powf(v, 1 / 2.4f) - 0.055f;
        C.enc[i] = (uint8_t)(s * 255.0f + 0.5f);
    }
    float sum = 0;
    for (int k = 0; k <= GLOW_TAPS; k++) {                                  /* glow: sigma 1.6 (half size) */
        C.gw[k] = expf(-(float)(k * k) / (2 * 1.6f * 1.6f));
        sum += k ? 2 * C.gw[k] : C.gw[k];
    }
    for (int k = 0; k <= GLOW_TAPS; k++) C.gw[k] /= sum;
    fit_encode();
    C.tables = 1;
}

/* least squares: the beam's weight at distance d as a cubic in s = sqrt(brightness) */
static void fit_beam(const profile *p)
{
    for (int dq = 0; dq < DQ; dq++) {
        double d = (double)dq / DQ_PER_LINE, A[4][5] = { { 0 } };
        for (int i = 0; i <= 64; i++) {
            double s = i / 64.0, sig = p->vmin + (p->vmax - p->vmin) * s;
            double w = exp(-d * d / (2 * sig * sig)) / (sig * 2.5066282746);
            double x[4] = { 1, s, s * s, s * s * s };
            for (int r = 0; r < 4; r++) {
                for (int c = 0; c < 4; c++) A[r][c] += x[r] * x[c];
                A[r][4] += x[r] * w;
            }
        }
        for (int c = 0; c < 4; c++)                 /* Gauss-Jordan */
            for (int r = 0; r < 4; r++) {
                if (r == c) continue;
                double f = A[r][c] / A[c][c];
                for (int k = c; k < 5; k++) A[r][k] -= f * A[c][k];
            }
        for (int k = 0; k < 4; k++) C.beam[dq].b[k] = (float)(A[k][4] / A[k][k]);
        /* a line this far away at full brightness: under a quarter of a step of 8-bit light */
        C.beam[dq].on = exp(-d * d / (2 * p->vmax * p->vmax)) / (p->vmax * 2.5066282746) > 0.5 / OUTQ;
    }
}

/* sRGB from linear light as a polynomial in t = sqrt(v) (least squares, t in 0..1):
 * within half a step of 8-bit, and plain arithmetic */
static void fit_encode(void)
{
    double A[6][7] = { { 0 } };
    for (int i = 0; i <= 4096; i++) {
        double t = i / 4096.0, v = t * t;
        double sv = v <= 0.0031308 ? 12.92 * v : 1.055 * pow(v, 1 / 2.4) - 0.055;
        double x[6] = { 1, t, t * t, t * t * t, t * t * t * t, t * t * t * t * t };
        double w = 1 + 8 * (1 - t);              /* the darks matter most */
        for (int r = 0; r < 6; r++) {
            for (int c = 0; c < 6; c++) A[r][c] += w * x[r] * x[c];
            A[r][6] += w * x[r] * sv;
        }
    }
    for (int c = 0; c < 6; c++)
        for (int r = 0; r < 6; r++) {
            if (r == c) continue;
            double f = A[r][c] / A[c][c];
            for (int k = c; k < 7; k++) A[r][k] -= f * A[c][k];
        }
    for (int k = 0; k < 6; k++) C.enc_fit[k] = (float)(A[k][6] / A[k][k] * 255.0);
}

static int setup(int kind, int sw, int sh, int dw, int dh)
{
    if (C.kind == kind && C.sw == sw && C.sh == sh && C.dw == dw && C.dh == dh && C.hs)
        return 1;
    free_all();
    if (!C.tables) gamma_tables();
    const profile *p = &PROFILES[kind];
    C.kind = kind; C.sw = sw; C.sh = sh; C.dw = dw; C.dh = dh; C.p = p;
    C.curved = p->curve > 0;
    /* the mask needs three screen pixels to a triad and a few to a line */
    C.mask = (dh * 10 >= sh * 25) ? p->mask : 0;
    float avg = 1;
    if (C.mask) {
        avg = (1 + 2 * p->dark) / 3;
        if (C.mask == 2) avg *= (3 + p->dark) / 4;
    }
    float gain = 1 / avg;
    C.hw2 = sw / 2 > 2 ? sw / 2 : 2;
    C.hh2 = sh / 2 > 2 ? sh / 2 : 2;
    fit_beam(p);

    size_t n = (size_t)dw * dh;
    C.hidx = malloc(sizeof(int) * 4 * dw);
    C.hwt = malloc(sizeof(float) * 4 * dw);
    C.gx = malloc(sizeof(int) * dw);
    C.gt = malloc(sizeof(float) * dw);
    C.vy = malloc(sizeof(float) * dh);
    C.mpat = malloc(sizeof(float) * 12 * dw);
    C.ones = malloc(sizeof(float) * dw);
    C.lin = malloc(sizeof(float) * 3 * sw * sh);
    C.half = malloc(sizeof(float) * 3 * C.hw2 * C.hh2);
    C.blur = malloc(sizeof(float) * 3 * C.hw2 * C.hh2);
    C.tmp = malloc(sizeof(float) * 3 * C.hw2 * C.hh2);
    C.hs = malloc(sizeof(float) * 3 * (size_t)dw * sh);
    C.hq = malloc(sizeof(float) * 3 * (size_t)dw * sh);
    C.gh = malloc(sizeof(float) * 3 * (size_t)dw * C.hh2);
    if (C.curved) {
        C.widx = malloc(sizeof(int) * n);
        C.wtx = malloc(sizeof(float) * n);
        C.wty = malloc(sizeof(float) * n);
        C.wa = malloc(sizeof(float) * n);
        C.flat = malloc(sizeof(float) * 3 * n);
    }
    if (!C.hidx || !C.hwt || !C.gx || !C.gt || !C.vy || !C.mpat || !C.ones || !C.lin || !C.half || !C.blur || !C.tmp ||
        !C.hs || !C.hq || !C.gh || (C.curved && (!C.widx || !C.wtx || !C.wty || !C.wa || !C.flat))) {
        free_all();
        C.kind = -1;
        return 0;
    }

    for (int x = 0; x < dw; x++) C.ones[x] = 1;
    for (int x = 0; x < dw; x++) {
        float sx = (x + 0.5f) * sw / dw - 0.5f;
        int i0 = (int)floorf(sx) - 1;
        float sum = 0;
        for (int t = 0; t < 4; t++) {
            /* pixel i covers i-0.5 .. i+0.5: the blurred box at sx */
            float d = sx - (float)(i0 + t), k = 0.70710678f / p->hsig;
            float w = 0.5f * (erff((d + 0.5f) * k) - erff((d - 0.5f) * k));
            int i = i0 + t;
            C.hidx[x * 4 + t] = i < 0 ? 0 : i >= sw ? sw - 1 : i;
            C.hwt[x * 4 + t] = w;
            sum += w;
        }
        for (int t = 0; t < 4; t++) C.hwt[x * 4 + t] /= sum;
        float gxp = (x + 0.5f) * C.hw2 / dw - 0.5f;
        int g = (int)floorf(gxp);
        float f = gxp - g;
        if (g < 0) { g = 0; f = 0; }
        if (g > C.hw2 - 2) { g = C.hw2 - 2; f = 1; }
        C.gx[x] = g;
        C.gt[x] = f;
    }

    /* the mask on the screen's pixels: 4 row patterns (the slot mask's rows), gain in */
    for (int r = 0; r < 4; r++)
        for (int x = 0; x < dw; x++) {
            float X = (x + 0.5f) / dw;
            float v = (!C.curved && p->vignette > 0) ? powf(4 * X * (1 - X), p->vignette) : 1;
            for (int c = 0; c < 3; c++) {
                float m = 1;
                if (C.mask) {
                    m = (x % 3 == c) ? 1 : p->dark;
                    if (C.mask == 2 && ((r + (((x / 3) & 1) << 1)) & 3) == 3) m *= p->dark;
                }
                C.mpat[((size_t)r * 3 + c) * dw + x] = m * gain * v;
            }
        }
    for (int y = 0; y < dh; y++) {
        float Y = (y + 0.5f) / dh;
        C.vy[y] = (!C.curved && p->vignette > 0) ? powf(4 * Y * (1 - Y), p->vignette) : 1;
    }

    if (C.curved) {
        float c = p->curve, r = p->corner * dh, bx = dw * 0.5f, by = dh * 0.5f;
        for (int y = 0; y < dh; y++)
            for (int x = 0; x < dw; x++) {
                size_t k = (size_t)y * dw + x;
                float u = (x + 0.5f) / dw * 2 - 1, v = (y + 0.5f) / dh * 2 - 1;
                float tu = u * (1 + c * v * v), tv = v * (1 + c * u * u);
                float fx = (tu * 0.5f + 0.5f) * dw, fy = (tv * 0.5f + 0.5f) * dh;
                /* rounded corners: signed distance to the picture's edge, in pixels */
                float qx = fabsf(fx - bx) - (bx - r), qy = fabsf(fy - by) - (by - r);
                float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
                float in = qx > qy ? qx : qy;
                float dist = sqrtf(ox * ox + oy * oy) + (in < 0 ? in : 0) - r;
                float a = 0.5f - dist;
                a = a < 0 ? 0 : a > 1 ? 1 : a;
                float X = fx / dw, Y = fy / dh;
                float e = 16 * X * Y * (1 - X) * (1 - Y);
                if (a > 0 && e > 0 && p->vignette > 0) a *= powf(e, p->vignette);
                /* bilinear in the flat picture (pixel centres at +0.5) */
                float sx = fx - 0.5f, sy = fy - 0.5f;
                int ix = (int)floorf(sx), iy = (int)floorf(sy);
                float tx = sx - ix, ty = sy - iy;
                if (ix < 0) { ix = 0; tx = 0; }
                if (ix > dw - 2) { ix = dw - 2; tx = 1; }
                if (iy < 0) { iy = 0; ty = 0; }
                if (iy > dh - 2) { iy = dh - 2; ty = 1; }
                C.widx[k] = a > 0 ? iy * dw + ix : -1;
                C.wtx[k] = tx;
                C.wty[k] = ty;
                C.wa[k] = a;
            }
    }
    return 1;
}

/* ---------------------------------------------------------------- per frame */

/* each picture line in linear light, then across the screen: the nearest pixels, softly */
static void job_lines(void *u, int from, int to)
{
    (void)u;
    int sw = C.sw, sh = C.sh, dw = C.dw;
    size_t ln = (size_t)sw * sh, hn = (size_t)dw * sh;
    for (int y = from; y < to; y++) {
        const uint32_t *s = C.src + (size_t)y * C.stride;
        float *lr = PL(C.lin, 0, ln) + (size_t)y * sw, *lg = PL(C.lin, 1, ln) + (size_t)y * sw,
              *lb = PL(C.lin, 2, ln) + (size_t)y * sw;
        for (int x = 0; x < sw; x++) {
            uint32_t c = s[x];
            lr[x] = C.dec[(c >> 16) & 255];
            lg[x] = C.dec[(c >> 8) & 255];
            lb[x] = C.dec[c & 255];
        }
        const float *l[3] = { lr, lg, lb };
        for (int c = 0; c < 3; c++) {
            float *restrict h = PL(C.hs, c, hn) + (size_t)y * dw, *restrict q = PL(C.hq, c, hn) + (size_t)y * dw;
            const float *lc = l[c];
            for (int x = 0; x < dw; x++) {
                const int *i = C.hidx + x * 4;
                const float *w = C.hwt + x * 4;
                h[x] = lc[i[0]] * w[0] + lc[i[1]] * w[1] + lc[i[2]] * w[2] + lc[i[3]] * w[3];
            }
            for (int x = 0; x < dw; x++) q[x] = sqrtf(h[x]);
        }
    }
}

static void glow(void)
{
    int w = C.hw2, h = C.hh2, sw = C.sw, sh = C.sh;
    size_t ln = (size_t)sw * sh, n = (size_t)w * h;
    for (int c = 0; c < 3; c++) {
        const float *l = PL(C.lin, c, ln);
        float *hf = PL(C.half, c, n), *t = PL(C.tmp, c, n), *b = PL(C.blur, c, n);
        for (int y = 0; y < h; y++)                 /* half size */
            for (int x = 0; x < w; x++) {
                int x0 = x * 2 < sw ? x * 2 : sw - 1, x1 = x * 2 + 1 < sw ? x * 2 + 1 : sw - 1;
                int y0 = y * 2 < sh ? y * 2 : sh - 1, y1 = y * 2 + 1 < sh ? y * 2 + 1 : sh - 1;
                hf[y * w + x] = 0.25f * (l[y0 * sw + x0] + l[y0 * sw + x1] + l[y1 * sw + x0] + l[y1 * sw + x1]);
            }
        for (int y = 0; y < h; y++)                 /* blurred across */
            for (int x = 0; x < w; x++) {
                float s = 0;
                for (int k = -GLOW_TAPS; k <= GLOW_TAPS; k++) {
                    int xx = x + k < 0 ? 0 : x + k >= w ? w - 1 : x + k;
                    s += C.gw[k < 0 ? -k : k] * hf[y * w + xx];
                }
                t[y * w + x] = s;
            }
        for (int y = 0; y < h; y++)                 /* and down */
            for (int x = 0; x < w; x++) {
                float s = 0;
                for (int k = -GLOW_TAPS; k <= GLOW_TAPS; k++) {
                    int yy = y + k < 0 ? 0 : y + k >= h ? h - 1 : y + k;
                    s += C.gw[k < 0 ? -k : k] * t[yy * w + x];
                }
                b[y * w + x] = s;
            }
    }
}

static void job_glow_lines(void *u, int from, int to)
{
    (void)u;
    int dw = C.dw, w = C.hw2;
    size_t n = (size_t)w * C.hh2, gn = (size_t)dw * C.hh2;
    for (int c = 0; c < 3; c++)
        for (int y = from; y < to; y++) {
            const float *b = PL(C.blur, c, n) + (size_t)y * w;
            float *g = PL(C.gh, c, gn) + (size_t)y * dw;
            for (int x = 0; x < dw; x++) {
                float a0 = b[C.gx[x]], a1 = b[C.gx[x] + 1];
                g[x] = a0 + (a1 - a0) * C.gt[x];
            }
        }
}

/* one row of the flat picture (beam and glow, before the mask): out 3 x dw */
static void flat_row(int y, float *out)
{
    const profile *p = C.p;
    int dw = C.dw, sh = C.sh;
    size_t hn = (size_t)dw * sh, gn = (size_t)dw * C.hh2;
    float sy = (y + 0.5f) * sh / C.dh - 0.5f;
    int y0 = (int)floorf(sy);
    float keep = 1 - p->glow;
    for (int c = 0; c < 3; c++) {
        float *o = out + (size_t)c * dw;
        for (int x = 0; x < dw; x++) o[x] = 0;
        for (int line = y0 - 1; line <= y0 + 2; line++) {
            if (line < 0 || line >= sh) continue;
            int dq = (int)(fabsf(sy - line) * DQ_PER_LINE + 0.5f);
            if (dq >= DQ || !C.beam[dq].on) continue;
            const float b0 = C.beam[dq].b[0] * keep, b1 = C.beam[dq].b[1] * keep,
                        b2 = C.beam[dq].b[2] * keep, b3 = C.beam[dq].b[3] * keep;
            const float *restrict h = PL(C.hs, c, hn) + (size_t)line * dw;
            const float *restrict q = PL(C.hq, c, hn) + (size_t)line * dw;
            float *restrict oo = o;
            for (int x = 0; x < dw; x++) {
                float i = h[x], s = q[x];
                oo[x] += i * (b0 + s * (b1 + s * (b2 + s * b3)));
            }
        }
        /* the glow */
        float gy = (y + 0.5f) * C.hh2 / C.dh - 0.5f;
        int g0 = (int)floorf(gy);
        float f = gy - g0;
        if (g0 < 0) { g0 = 0; f = 0; }
        if (g0 > C.hh2 - 2) { g0 = C.hh2 - 2; f = 1; }
        const float *restrict p0 = PL(C.gh, c, gn) + (size_t)g0 * dw, *restrict p1 = p0 + dw;
        float *restrict oo = o;
        float gl = p->glow;
        for (int x = 0; x < dw; x++) oo[x] += gl * (p0[x] + (p1[x] - p0[x]) * f);
    }
}

/* a screen row from its light (3 x dw): the mask, the edges, to sRGB; a: per pixel (curved) */
static void finish_row(int y, const float *restrict l, const float *a, uint32_t *restrict o)
{
    int dw = C.dw;
    const float *restrict m = C.mpat + (size_t)(y & 3) * 3 * dw;
    const float *restrict sc = a ? a : C.ones;
    const float v = C.vy[y];
    const float *restrict lr = l, *restrict lg = l + dw, *restrict lb = l + 2 * (size_t)dw;
    const float *restrict mr = m, *restrict mg = m + dw, *restrict mb = m + 2 * (size_t)dw;
    const float e0 = C.enc_fit[0] + 0.5f, e1 = C.enc_fit[1], e2 = C.enc_fit[2], e3 = C.enc_fit[3],
                e4 = C.enc_fit[4], e5 = C.enc_fit[5];
#define TO_SRGB(val, out) do { \
        float q_ = (val); q_ = q_ > 0.0f ? q_ : 0.0f; q_ = q_ < 1.0f ? q_ : 1.0f; float t_ = sqrtf(q_); \
        float o_ = e0 + t_ * (e1 + t_ * (e2 + t_ * (e3 + t_ * (e4 + t_ * e5)))); \
        o_ = o_ > 0.0f ? o_ : 0.0f; o_ = o_ < 255.0f ? o_ : 255.0f; \
        out = (int32_t)o_; \
    } while (0)
    for (int x = 0; x < dw; x++) {
        float s = sc[x] * v;
        int32_t r, g, b;                       /* signed: one vector instruction */
        TO_SRGB(lr[x] * mr[x] * s, r);
        TO_SRGB(lg[x] * mg[x] * s, g);
        TO_SRGB(lb[x] * mb[x] * s, b);
        o[x] = 0xFF000000u | (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
    }
#undef TO_SRGB
}

static void job_flat_screen(void *u, int from, int to)        /* flat tubes: straight to the screen */
{
    (void)u;
    float *row = malloc(sizeof(float) * 3 * C.dw);
    if (!row) return;
    for (int y = from; y < to; y++) {
        flat_row(y, row);
        finish_row(y, row, 0, C.dst + (size_t)y * C.dw);
    }
    free(row);
}

static void job_flat(void *u, int from, int to)              /* curved: the flat picture first */
{
    (void)u;
    int dw = C.dw;
    size_t n = (size_t)dw * C.dh;
    float *row = malloc(sizeof(float) * 3 * dw);
    if (!row) return;
    for (int y = from; y < to; y++) {
        flat_row(y, row);
        for (int c = 0; c < 3; c++)
            memcpy(PL(C.flat, c, n) + (size_t)y * dw, row + (size_t)c * dw, sizeof(float) * dw);
    }
    free(row);
}

static void job_curved(void *u, int from, int to)            /* then seen through the curved glass */
{
    (void)u;
    int dw = C.dw;
    size_t n = (size_t)dw * C.dh;
    float *row = malloc(sizeof(float) * 3 * dw);
    if (!row) return;
    const float *fr = PL(C.flat, 0, n), *fg = PL(C.flat, 1, n), *fb = PL(C.flat, 2, n);
    for (int y = from; y < to; y++) {
        size_t k = (size_t)y * dw;
        for (int x = 0; x < dw; x++) {
            int i = C.widx[k + x];
            if (i < 0) { row[x] = row[dw + x] = row[2 * dw + x] = 0; continue; }
            float tx = C.wtx[k + x], ty = C.wty[k + x];
            float w00 = (1 - tx) * (1 - ty), w01 = tx * (1 - ty), w10 = (1 - tx) * ty, w11 = tx * ty;
            row[x] = fr[i] * w00 + fr[i + 1] * w01 + fr[i + dw] * w10 + fr[i + dw + 1] * w11;
            row[dw + x] = fg[i] * w00 + fg[i + 1] * w01 + fg[i + dw] * w10 + fg[i + dw + 1] * w11;
            row[2 * dw + x] = fb[i] * w00 + fb[i + 1] * w01 + fb[i + dw] * w10 + fb[i + dw + 1] * w11;
        }
        finish_row(y, row, C.wa + k, C.dst + k);
    }
    free(row);
}

static void run(crt_parallel par, void (*job)(void *, int, int), int n)
{
    if (par) par(job, 0, n);
    else job(0, 0, n);
}

void crt_render(int kind, const uint32_t *src, int stride, int sw, int sh,
                uint32_t *dst, int dw, int dh, crt_parallel par)
{
    if (kind <= 0 || kind >= CRT_COUNT || sw < 4 || sh < 4 || dw < 4 || dh < 4 || dw > MAXW ||
        !setup(kind, sw, sh, dw, dh)) {
        for (int y = 0; y < dh; y++)               /* nothing to do it with: the picture as it is */
            for (int x = 0; x < dw; x++)
                dst[(size_t)y * dw + x] = src[(size_t)(y * sh / dh) * stride + x * sw / dw];
        return;
    }
    C.src = src;
    C.stride = stride;
    C.dst = dst;
    run(par, job_lines, sh);
    glow();
    run(par, job_glow_lines, C.hh2);
    if (C.curved) {
        run(par, job_flat, dh);
        run(par, job_curved, dh);
    } else {
        run(par, job_flat_screen, dh);
    }
}
