#ifndef VDP_H
#define VDP_H
#include <stdint.h>

#define VDP_FB_W 576   /* widest picture: 320 + 2 * 128 */
#define VDP_BLACK 0x200 /* colour index used outside the picture (always black) */
#define VDP_CHAR_BASE 0x400 /* + slot: player-character colour (see settings.h) */
#define VDP_SHADOW    0x1000 /* added to a colour: shadowed (half bright) */
#define VDP_HIGHLIGHT 0x2000 /* added to a colour: highlighted */
#define VDP_COLOR(c)  ((c) & 0x0FFF)

typedef struct {
    uint8_t  reg[32];
    uint8_t  vram[0x10000];
    uint16_t cram[64];
    uint16_t vsram[40];

    int      pending;      /* first half of a command written */
    uint16_t addr;
    uint8_t  code;
    int      fill_pending;

    int      frame_due, band_due;  /* the frame routine / a band routine is due */
    int      band_next;            /* the line the next band routine runs at */

    uint16_t fbmd[224][VDP_FB_W];  /* picture as 9-bit Mega Drive colours (see md_color_index) */
} vdp_t;

extern vdp_t vdp;

/* widescreen: extra pixels drawn on each side (survives vdp_reset) */
void     vdp_set_aspect(double aspect);  /* 4/3 = original, 16/9 = widescreen */

void     vdp_reset(void);
/* the console's port (video.c's stores behind it), for a mod's own code only */
extern void     video_port_ctrl(uint16_t w);
extern void     video_port_data(uint16_t w);
extern uint16_t video_port_read(void);
extern uint16_t video_port_status(void);
extern uint16_t video_port_hv(void);
extern unsigned long video_port_words;  /* words through it */
void     vdp_line_capture(int y);  /* line y's look, as it starts (while the frame runs) */
void     vdp_render_frame(void);   /* the picture: video memory now, and the lines' looks */
void     render_reset(void);       /* render.c: no picture drawn yet */
int      vdp_width(void);       /* real screen width (256/320) */
int      vdp_margin(void);      /* widescreen extra pixels per side */
int      vdp_out_width(void);   /* width of the last picture in vdp.fbmd (as it was drawn) */
int      vdp_irq_level(void);   /* highest pending & enabled interrupt (0, 4 or 6) */
void     vdp_ack_irq(int level);
void     vdp_line_start(int line); /* a line starts: a band routine, or the frame routine, may be due */

/* online games: the player at this screen (0 or 1), -1 = local game. Each player sees
 * their own view at the bottom and the floor above them at the top. Display only. */
extern int vdp_local_player;

#endif
