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
    uint16_t read_buf;

    int      vint_pending, hint_pending;
    int      vint_flag;    /* status F bit */
    int      hint_counter;
    int      in_vblank;

    uint16_t fbmd[224][VDP_FB_W];  /* picture as 9-bit Mega Drive colours (see md_color_index) */
    uint32_t palette[64];  /* cached CRAM -> RGB */
} vdp_t;

extern vdp_t vdp;

/* widescreen: extra pixels drawn on each side (survives vdp_reset) */
void     vdp_set_aspect(double aspect);  /* 4/3 = original, 16/9 = widescreen */

void     vdp_reset(void);
void     vdp_refresh_palette(void);  /* after md_color_lut changes */
void     vdp_write_ctrl(uint16_t w);
void     vdp_write_data(uint16_t w);
void     vdp_set_reg(int r, uint8_t v);            /* a register (decoded command) */
void     vdp_command(uint8_t code, uint16_t addr); /* where data goes (decoded command) */
void     vdp_reg_word(uint16_t w);                 /* a register word the game keeps in RAM */
void     vdp_command_long(uint32_t v);             /* a command (or two register words) made at run time */
extern unsigned long vdp_raw_words;                /* control words that needed the port's two-word latch */
uint16_t vdp_read_ctrl(void);
uint16_t vdp_read_data(void);
uint16_t vdp_read_hv(void);
void     vdp_render_line(int y);
void     vdp_line_capture(int y);  /* the state line y is drawn with (while the frame runs) */
void     vdp_render_frame(void);   /* the picture, from the captured lines */
int      vdp_width(void);       /* real screen width (256/320) */
int      vdp_margin(void);      /* widescreen extra pixels per side */
int      vdp_out_width(void);   /* width of the picture in vdp.fb */
int      vdp_irq_level(void);   /* highest pending & enabled interrupt (0, 4 or 6) */
void     vdp_ack_irq(int level);
void     vdp_line_start(int line); /* raster bookkeeping (HINT/VINT) */
int      vdp_irq_within(int lines, int mask);
#define VDP_AHEAD_NONE (-0x40000000)
extern int vdp_ahead;               /* see vdp.c */ /* an interrupt at one of the next line starts? */

/* online games: the player at this screen (0 or 1), -1 = local game. Each player sees
 * their own view at the bottom and the floor above them at the top. Display only. */
extern int vdp_local_player;

#endif
