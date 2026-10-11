/* Mega Drive VDP (YM7101): ports, DMA and a per-scanline renderer. */
#include <string.h>
#include "vdp.h"
#include "md.h"
#include "settings.h"

vdp_t vdp;

extern int md_line_cycle(void);

static void update_palette(int i)
{
    vdp.palette[i] = md_color_lut[md_color_index(vdp.cram[i])];
}

void vdp_refresh_palette(void)
{
    for (int i = 0; i < 64; i++)
        update_palette(i);
}

void vdp_reset(void)
{
    if (!md_color_lut_ready)
        settings_apply(); /* default colours (all-zero settings are the defaults) */
    memset(&vdp, 0, sizeof(vdp));
    vdp_refresh_palette();
}

int vdp_width(void) { return (vdp.reg[12] & 0x01) ? 320 : 256; }

int vdp_irq_level(void)
{
    if (vdp.vint_pending && (vdp.reg[1] & 0x20))
        return 6;
    if (vdp.hint_pending && (vdp.reg[0] & 0x10))
        return 4;
    return 0;
}

void vdp_ack_irq(int level)
{
    if (level == 6)
        vdp.vint_pending = 0;
    else if (level == 4)
        vdp.hint_pending = 0;
    cpu_set_irq(vdp_irq_level());
}

void vdp_line_start(int line)
{
    if (line <= 224) {
        if (--vdp.hint_counter < 0) {
            vdp.hint_counter = vdp.reg[10];
            vdp.hint_pending = 1;
        }
    } else {
        vdp.hint_counter = vdp.reg[10];
    }
    if (line == 224) {
        vdp.in_vblank = 1;
        vdp.vint_pending = 1;
        vdp.vint_flag = 1;
    } else if (line == 0) {
        vdp.in_vblank = 0;
    }
    cpu_set_irq(vdp_irq_level());
}

/* ---------------------------------------------------------------- ports */

static void vram_write_word(uint16_t a, uint16_t w)
{
    if (a & 1)
        w = (uint16_t)((w >> 8) | (w << 8));
    vdp.vram[a & 0xFFFE] = (uint8_t)(w >> 8);
    vdp.vram[(a & 0xFFFE) | 1] = (uint8_t)w;
}

static void port_write(uint16_t w)
{
    switch (vdp.code & 0x0F) {
    case 1:
        vram_write_word(vdp.addr, w);
        break;
    case 3: {
        int i = (vdp.addr >> 1) & 0x3F;
        vdp.cram[i] = w & 0x0EEE;
        update_palette(i);
        break;
    }
    case 5: {
        int i = (vdp.addr >> 1) & 0x3F;
        if (i < 40)
            vdp.vsram[i] = w & 0x07FF;
        break;
    }
    default:
        break;
    }
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
    while (len--) {
        uint16_t w = (uint16_t)md_read16(src);
        /* source wraps inside a 128 KiB window */
        src = (src & 0xFE0000) | ((src + 2) & 0x1FFFF);
        port_write(w);
    }
    src >>= 1;
    vdp.reg[21] = src & 0xFF;
    vdp.reg[22] = (src >> 8) & 0xFF;
    dma_set_len0();
}

static void dma_copy(void)
{
    uint32_t len = dma_len();
    uint16_t src = vdp.reg[21] | (vdp.reg[22] << 8);
    while (len--) {
        vdp.vram[vdp.addr] = vdp.vram[src];
        src++;
        vdp.addr += vdp.reg[15];
    }
    vdp.reg[21] = src & 0xFF;
    vdp.reg[22] = src >> 8;
    dma_set_len0();
}

static void dma_fill(uint16_t w)
{
    uint32_t len = dma_len();
    uint8_t b = (uint8_t)(w >> 8);
    if ((vdp.code & 0x0F) == 1) {
        while (len--) {
            vdp.vram[vdp.addr ^ 1] = b;
            vdp.addr += vdp.reg[15];
        }
    } else {
        while (len--)
            port_write(w);
    }
    dma_set_len0();
}

#include <stdio.h>
#include <stdlib.h>
static int vlog = -1;
void vdp_write_ctrl(uint16_t w)
{
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
        if (r < 24) {
            vdp.reg[r] = (uint8_t)w;
            if (r == 0 || r == 1)
                cpu_set_irq(vdp_irq_level());
        }
        return;
    }
    vdp.addr = (vdp.addr & 0xC000) | (w & 0x3FFF);
    vdp.code = (vdp.code & 0x3C) | (w >> 14);
    vdp.pending = 1;
}

void vdp_write_data(uint16_t w)
{
    vdp.pending = 0;
    if (vdp.fill_pending) {
        vdp.fill_pending = 0;
        port_write(w);
        dma_fill(w);
        return;
    }
    port_write(w);
}

uint16_t vdp_read_data(void)
{
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

uint16_t vdp_read_ctrl(void)
{
    uint16_t s = 0x3400 | 0x0200; /* FIFO empty */
    int cyc = md_line_cycle();
    vdp.pending = 0;
    if (vdp.vint_flag)
        s |= 0x80;
    if (vdp.in_vblank || !(vdp.reg[1] & 0x40))
        s |= 0x08;
    if (cyc >= 400 || cyc < 20)
        s |= 0x04;
    vdp.vint_flag = 0;
    return s;
}

uint16_t vdp_read_hv(void)
{
    int line = md.line;
    int v = line <= 0xEA ? line : line - 6; /* NTSC jump EA -> E5 */
    int h = md_line_cycle() * 342 / MD_CYC68K_LINE;
    int hc = vdp_width() == 320 ? (h < 0xB7 ? h : h + (0xE4 - 0xB7)) : (h < 0x94 ? h : h + (0xE9 - 0x94));
    return (uint16_t)(((v & 0xFF) << 8) | ((hc >> 1) & 0xFF));
}

/* ---------------------------------------------------------------- render */

/* pixel encoding in line buffers: bit7 = priority, bits0-5 = palette index, 0 in low nibble = transparent */

static inline int tile_pixel(uint16_t entry, int px, int py)
{
    if (entry & 0x1000) py = 7 - py;
    if (entry & 0x0800) px = 7 - px;
    uint32_t a = ((entry & 0x07FF) << 5) + (py << 2) + (px >> 1);
    uint8_t b = vdp.vram[a & 0xFFFF];
    int c = (px & 1) ? (b & 0x0F) : (b >> 4);
    if (!c)
        return 0;
    return c | ((entry >> 9) & 0x30) | ((entry >> 8) & 0x80);
}

/* Output can be wider than the real screen ("widescreen"): vdp.margin extra pixels are
 * drawn on each side from the same VRAM. This only affects the picture, never the
 * emulated machine state. Line buffers are indexed with x + margin. */

static void draw_plane(uint8_t *out, int y, int width, int m, int plane_b)
{
    static const int sizes[4] = {32, 64, 32, 128};
    int pw = sizes[vdp.reg[16] & 3], ph = sizes[(vdp.reg[16] >> 4) & 3];
    uint32_t name = plane_b ? (vdp.reg[4] & 0x07) << 13 : (vdp.reg[2] & 0x38) << 10;
    uint32_t hsbase = (vdp.reg[13] & 0x3F) << 10;
    uint32_t hsa;
    switch (vdp.reg[11] & 3) {
    case 2: hsa = hsbase + (y & ~7) * 4; break;
    case 3: hsa = hsbase + y * 4; break;
    default: hsa = hsbase; break;
    }
    hsa += plane_b ? 2 : 0;
    int hs = ((vdp.vram[hsa & 0xFFFF] << 8) | vdp.vram[(hsa + 1) & 0xFFFF]) & 0x3FF;
    int column_vs = vdp.reg[11] & 4;
    int wmask = pw * 8 - 1, hmask = ph * 8 - 1;
    int vs = vdp.vsram[plane_b];
    int last_tile = -1, last_py = -1;
    uint16_t entry = 0;
    for (int x = -m; x < width + m; x++) {
        if (column_vs) {
            int col = x < 0 ? 0 : (x >> 4);
            if (col > (width >> 4) - 1) col = (width >> 4) - 1;
            vs = vdp.vsram[col * 2 + plane_b];
        }
        int px = (x - hs) & wmask;
        int py = (y + vs) & hmask;
        int tile = px >> 3;
        if (tile != last_tile || py != last_py) {
            uint32_t ea = name + (((py >> 3) * pw + tile) << 1);
            entry = (vdp.vram[ea & 0xFFFF] << 8) | vdp.vram[(ea + 1) & 0xFFFF];
            last_tile = tile;
            last_py = py;
        }
        out[x + m] = (uint8_t)tile_pixel(entry, px & 7, py & 7);
    }
}

static void draw_window(uint8_t *out, int y, int width, int m, uint8_t *in_window)
{
    int h40 = width == 320;
    uint32_t name = (vdp.reg[3] & (h40 ? 0x3C : 0x3E)) << 10;
    int ww = h40 ? 64 : 32;
    int vp = (vdp.reg[18] & 0x1F) * 8, hp = (vdp.reg[17] & 0x1F) * 16;
    int vin = (vdp.reg[18] & 0x80) ? (y >= vp) : (y < vp);
    for (int x = -m; x < width + m; x++) {
        /* outside the real screen the window extends its edge column */
        int cx = x < 0 ? 0 : (x >= width ? width - 1 : x);
        int hin = (vdp.reg[17] & 0x80) ? (cx >= hp) : (cx < hp);
        in_window[x + m] = (uint8_t)(vin || hin);
        if (!in_window[x + m])
            continue;
        int wx = x & (ww * 8 - 1);
        uint32_t ea = name + (((y >> 3) * ww + (wx >> 3)) << 1);
        uint16_t entry = (vdp.vram[ea & 0xFFFF] << 8) | vdp.vram[(ea + 1) & 0xFFFF];
        out[x + m] = (uint8_t)tile_pixel(entry, wx & 7, y & 7);
    }
}

static void draw_sprites(uint8_t *out, int y, int width, int m)
{
    int h40 = width == 320;
    uint32_t sat = (vdp.reg[5] & (h40 ? 0x7E : 0x7F)) << 9;
    int max_sprites = h40 ? 80 : 64, max_line = h40 ? 20 : 16, max_px = h40 ? 320 : 256;
    int link = 0, count = 0, line_count = 0, pixels = 0, masked = 0, seen_nonzero_x = 0;
    memset(out, 0, width + 2 * m);
    do {
        const uint8_t *s = &vdp.vram[(sat + link * 8) & 0xFFFF];
        int sy = ((s[0] << 8) | s[1]) & 0x3FF;
        int vsz = (s[2] & 3) + 1, hsz = ((s[2] >> 2) & 3) + 1;
        link = s[3] & 0x7F;
        uint16_t attr = (s[4] << 8) | s[5];
        int sx = ((s[6] << 8) | s[7]) & 0x1FF;
        int row = y + 128 - sy;
        if (row >= 0 && row < vsz * 8) {
            if (++line_count > max_line)
                break;
            if (sx == 0) {
                if (seen_nonzero_x)
                    masked = 1;
            } else {
                seen_nonzero_x = 1;
            }
            int cy = row >> 3, py = row & 7;
            if (attr & 0x1000) { cy = vsz - 1 - cy; py = 7 - py; }
            for (int cx = 0; cx < hsz; cx++) {
                int tcx = (attr & 0x0800) ? hsz - 1 - cx : cx;
                uint16_t tile = (attr & 0xF800) | ((attr + tcx * vsz + cy) & 0x07FF);
                for (int px = 0; px < 8; px++) {
                    if (pixels >= max_px)
                        goto done;
                    pixels++;
                    if (masked)
                        continue;
                    int x = sx - 128 + cx * 8 + px;
                    if (x < -m || x >= width + m || (out[x + m] & 0x0F))
                        continue;
                    /* flip already applied to py; tile_pixel would flip again */
                    uint16_t e = tile & ~0x1000;
                    out[x + m] = (uint8_t)tile_pixel(e, px, py);
                }
            }
        }
    } while (link && ++count < max_sprites);
done:
    return;
}

static int margin40, margin32;

void vdp_set_aspect(double aspect)
{
    /* the Mega Drive picture fills a 4:3 screen at either width */
    for (int i = 0; i < 2; i++) {
        int w = i ? 256 : 320;
        int m = (int)((w * aspect / (4.0 / 3.0) - w) / 2.0 + 0.5);
        if (m < 0) m = 0;
        m &= ~1;
        if (w + 2 * m > VDP_FB_W) m = (VDP_FB_W - w) / 2;
        if (i) margin32 = m; else margin40 = m;
    }
}

int vdp_margin(void)
{
    int width = vdp_width();
    int m = width == 320 ? margin40 : margin32;
    if (width + 2 * m > VDP_FB_W)
        m = (VDP_FB_W - width) / 2;
    return m;
}

int vdp_out_width(void)
{
    return vdp_width() + 2 * vdp_margin();
}

void vdp_render_line(int y)
{
    int width = vdp_width();
    int m = vdp_margin();
    int ow = width + 2 * m;
    uint16_t *fb = vdp.fbmd[y];
    uint16_t pal[64];
    for (int i = 0; i < 64; i++)
        pal[i] = md_color_index(vdp.cram[i]);
    uint16_t bg = pal[vdp.reg[7] & 0x3F];
    if (!(vdp.reg[1] & 0x40)) {
        for (int x = 0; x < VDP_FB_W; x++)
            fb[x] = bg;
        return;
    }
    uint8_t a[VDP_FB_W], b[VDP_FB_W], s[VDP_FB_W], win[VDP_FB_W], wbuf[VDP_FB_W];
    draw_plane(b, y, width, m, 1);
    draw_plane(a, y, width, m, 0);
    /* the window covers nothing when HP=0 && !RIGT and VP=0 && !DOWN */
    if ((vdp.reg[17] & 0x9F) || (vdp.reg[18] & 0x9F)) {
        draw_window(wbuf, y, width, m, win);
        for (int x = 0; x < ow; x++)
            if (win[x])
                a[x] = wbuf[x];
    }
    draw_sprites(s, y, width, m);
    for (int x = 0; x < ow; x++) {
        int p;
        int sp = s[x], ap = a[x], bp = b[x];
        if ((sp & 0x80) && (sp & 0x0F)) p = sp;
        else if ((ap & 0x80) && (ap & 0x0F)) p = ap;
        else if ((bp & 0x80) && (bp & 0x0F)) p = bp;
        else if (sp & 0x0F) p = sp;
        else if (ap & 0x0F) p = ap;
        else if (bp & 0x0F) p = bp;
        else { fb[x] = bg; continue; }
        fb[x] = pal[p & 0x3F];
    }
    for (int x = ow; x < VDP_FB_W; x++)
        fb[x] = VDP_BLACK;
}
