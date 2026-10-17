/* Mega Drive VDP (YM7101): ports, DMA, and the picture: drawn once a frame from video
 * memory and its bands (see "the picture" below). */
#include <stdlib.h>
#include <string.h>
#include "vdp.h"
#include "md.h"
#include "settings.h"
#include "wideobj.h"
#include "prophunt.h"
#include "video.h"
#include "picture.h"

vdp_t vdp;
picture pic;

void vdp_reset(void)
{
    if (!md_color_lut_ready)
        settings_apply(); /* default colours (all-zero settings are the defaults) */
    memset(&vdp, 0, sizeof(vdp));
    render_reset();
}

int vdp_width(void) { return (vdp.reg[12] & 0x01) ? 320 : 256; }

/* The frame routine (the vertical blank) and the split screen's band routines (the
 * line interrupts): which is due. A band routine runs at the line band_next; register
 * 10, as the band set it, says how many lines later the next one comes; after the
 * picture the count starts again from the top. Register 0 bit 4 lets the bands in,
 * register 1 bit 5 the frame routine. */
int vdp_irq_level(void)
{
    if (vdp.frame_due && (vdp.reg[1] & 0x20))
        return 6;
    if (vdp.band_due && (vdp.reg[0] & 0x10))
        return 4;
    return 0;
}

void vdp_ack_irq(int level)
{
    if (level == 6)
        vdp.frame_due = 0;
    else if (level == 4)
        vdp.band_due = 0;
    cpu_set_irq(vdp_irq_level());
}

void vdp_line_start(int line)
{
    if (line <= 224) {
        if (line == vdp.band_next) {
            vdp.band_due = 1;
            vdp.band_next = line + vdp.reg[10] + 1;
        }
    } else {
        vdp.band_next = vdp.reg[10];
    }
    if (line == 224)
        vdp.frame_due = 1;
    cpu_set_irq(vdp_irq_level());
}

/* ---------------------------------------------------------------- ports */

/* ---- the console's port, for what still talks to it (a mod's own 68000 code in
 * shapes the slot translation does not take): its words decoded onto the native stores
 * (video.c), copies set up through its registers included */
static void port_write(uint16_t w)
{
    video_store(vdp.code, vdp.addr, w);
    vdp.addr += vdp.reg[15];
}

static uint32_t dma_len(void)
{
    uint32_t len = vdp.reg[19] | (vdp.reg[20] << 8);
    return len ? len : 0x10000;
}

static void dma_set_len0(void)
{
    vdp.reg[19] = vdp.reg[20] = 0;
}

static void dma_68k(void)
{
    uint32_t len = dma_len();
    uint32_t src = (vdp.reg[21] | (vdp.reg[22] << 8) | ((vdp.reg[23] & 0x7F) << 16)) << 1;
    vdp.addr = video_from_ram(vdp.code, vdp.addr, src, len, vdp.reg[15]);
    src = (src & 0xFE0000) | ((src + 2 * len) & 0x1FFFF);
    src >>= 1;
    vdp.reg[21] = src & 0xFF;
    vdp.reg[22] = (src >> 8) & 0xFF;
    dma_set_len0();
}

static void dma_copy(void)
{
    uint32_t len = dma_len();
    uint16_t src = vdp.reg[21] | (vdp.reg[22] << 8);
    vdp.addr = video_vram_copy(vdp.addr, src, len, vdp.reg[15]);
    src = (uint16_t)(src + len);
    vdp.reg[21] = src & 0xFF;
    vdp.reg[22] = src >> 8;
    dma_set_len0();
}

static void dma_fill(uint16_t w)            /* after the first word (port_write) */
{
    uint32_t len = dma_len();
    if ((vdp.code & 0x0F) == 1) {
        vdp.addr = (uint16_t)(video_fill((uint16_t)(vdp.addr - vdp.reg[15]), len, vdp.reg[15], w));
    } else {
        while (len--)
            port_write(w);
    }
    dma_set_len0();
}

#include <stdio.h>
#include <stdlib.h>
static int vlog = -1;   /* VDPLOG: every control word on the port */

/* words through the port (a mod's own code that the slot translation does not take) */
unsigned long video_port_words;

void video_port_ctrl(uint16_t w)
{
    video_port_words++;
    if (vlog < 0) vlog = getenv("VDPLOG") != NULL;
    if (vlog) fprintf(stderr, "ctrl %04x line %d frame %u pc %06x\n", w, md.line, md.frame, cpu_get_pc());
    if (vdp.pending) {
        vdp.pending = 0;
        vdp.addr = (vdp.addr & 0x3FFF) | ((w & 3) << 14);
        vdp.code = (vdp.code & 0x03) | ((w >> 2) & 0x3C);
        if ((vdp.code & 0x20) && (vdp.reg[1] & 0x10)) {
            switch (vdp.reg[23] >> 6) {
            case 0: case 1: dma_68k(); break;
            case 2: vdp.fill_pending = 1; break;
            case 3: dma_copy(); break;
            }
        }
        return;
    }
    if ((w & 0xC000) == 0x8000) {
        int r = (w >> 8) & 0x1F;
        if (r < 24)
            video_set(r, (uint8_t)w);
        return;
    }
    vdp.addr = (vdp.addr & 0xC000) | (w & 0x3FFF);
    vdp.code = (vdp.code & 0x3C) | (w >> 14);
    vdp.pending = 1;
}

void video_port_data(uint16_t w)
{
    video_port_words++;
    vdp.pending = 0;
    port_write(w);
    if (vdp.fill_pending) {
        vdp.fill_pending = 0;
        dma_fill(w);
    }
}

uint16_t video_port_read(void)
{
    video_port_words++;
    uint16_t w = 0;
    vdp.pending = 0;
    switch (vdp.code & 0x0F) {
    case 0: w = (vdp.vram[vdp.addr & 0xFFFE] << 8) | vdp.vram[vdp.addr | 1]; break;
    case 8: w = vdp.cram[(vdp.addr >> 1) & 0x3F]; break;
    case 4: { int i = (vdp.addr >> 1) & 0x3F; w = i < 40 ? vdp.vsram[i] : 0; break; }
    }
    vdp.addr += vdp.reg[15];
    return w;
}

uint16_t video_port_status(void)
{
    vdp.pending = 0;
    return video_status();
}

uint16_t video_port_hv(void)
{
    int line = md.line;
    int v = line <= 0xEA ? line : line - 6; /* NTSC jump EA -> E5 */
    return (uint16_t)((v & 0xFF) << 8);     /* no time is kept within a line (the game never reads it) */
}

/* ---------------------------------------------------------------- the picture
 * Each line's look as it starts; the picture is drawn from them (render.c). */
static int hscroll_now(int y, int plane_b);
static const uint8_t look_reg[24] = {0,1,1,1,1,1,0,1,0,0,0,1,1,1,0,0,1,1,1,0,0,0,0,0};

/* line y's look, as the line starts (md.c: before its band routine) */
void vdp_line_capture(int y)
{
    pic_look l;
    memset(&l, 0, sizeof(l));
    for (int r = 0; r < 24; r++)
        if (look_reg[r]) l.reg[r] = vdp.reg[r];
    memcpy(l.cram, vdp.cram, sizeof(l.cram));
    memcpy(l.vsram, vdp.vsram, sizeof(l.vsram));
    l.hs[0] = (uint16_t)hscroll_now(y, 0);
    l.hs[1] = (uint16_t)hscroll_now(y, 1);
    if (y == 0)
        pic.n = 0;
    if (!pic.n || memcmp(&pic.band[pic.n - 1].look, &l, sizeof(l))) {
        pic.band[pic.n].y0 = y;
        pic.band[pic.n].look = l;
        pic.n++;
    }
    pic.band[pic.n - 1].y1 = y + 1;
    pic.band_of[y] = (uint8_t)(pic.n - 1);
}

static int hscroll_now(int y, int plane_b)
{
    uint32_t hsbase = (vdp.reg[13] & 0x3F) << 10, hsa;
    switch (vdp.reg[11] & 3) {
    case 2: hsa = hsbase + (y & ~7) * 4; break;
    case 3: hsa = hsbase + y * 4; break;
    default: hsa = hsbase; break;
    }
    hsa += plane_b ? 2 : 0;
    return ((vdp.vram[hsa & 0xFFFF] << 8) | vdp.vram[(hsa + 1) & 0xFFFF]) & 0x3FF;
}
