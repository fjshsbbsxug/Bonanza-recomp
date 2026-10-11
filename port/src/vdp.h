#ifndef VDP_H
#define VDP_H
#include <stdint.h>

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

    uint32_t fb[224][320];
    uint32_t palette[64];  /* cached CRAM -> RGB */
} vdp_t;

extern vdp_t vdp;

void     vdp_reset(void);
void     vdp_write_ctrl(uint16_t w);
void     vdp_write_data(uint16_t w);
uint16_t vdp_read_ctrl(void);
uint16_t vdp_read_data(void);
uint16_t vdp_read_hv(void);
void     vdp_render_line(int y);
int      vdp_width(void);
int      vdp_irq_level(void);   /* highest pending & enabled interrupt (0, 4 or 6) */
void     vdp_ack_irq(int level);
void     vdp_line_start(int line); /* raster bookkeeping (HINT/VINT) */

#endif
