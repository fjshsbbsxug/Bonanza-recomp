/* The picture of a frame (vdp.c captures it, render.c draws it). */
#ifndef PICTURE_H
#define PICTURE_H
#include <stdint.h>

/* The picture of a frame: video memory as it is when the picture is drawn (the frame's
 * logic done, before its vertical blank), and its bands - runs of lines drawn with the
 * same registers, palette and scroll. The game's band routines (its line interrupts)
 * change the scroll between the split screen's parts, and once in a while the logic
 * turns the display off part way down. Each line's look is taken as the line starts
 * (vdp_line_capture), the picture is drawn from them at once (vdp_render_frame). */
typedef struct {
    uint8_t  reg[24];       /* the registers the picture uses (look_reg); the others 0 */
    uint16_t cram[64];      /* palette */
    uint16_t vsram[40];     /* vertical scroll */
    uint16_t hs[2];         /* horizontal scroll of plane A / B */
} pic_look;
typedef struct { int y0, y1; pic_look look; } pic_band;      /* lines y0 .. y1-1 */
typedef struct { int n; pic_band band[224]; uint8_t band_of[224]; } picture;
extern picture pic;

#endif
