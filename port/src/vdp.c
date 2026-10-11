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

static int ovr_hs = -1, ovr_vs = -1;   /* scroll override (room-above view) */
static int wrap_margins;               /* widescreen: repeat the 4:3 picture's columns (static backdrops) */
/* Building edges for plane A (the rooms): pixels whose world x lies outside [0, clip_right)
 * are left transparent so the city backdrop shows there instead of wrapped plane data. */
static int clip_on, clip_camx, clip_right;

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
    if (ovr_hs >= 0) { hs = ovr_hs; column_vs = 0; }
    int wmask = pw * 8 - 1, hmask = ph * 8 - 1;
    int vs = ovr_vs >= 0 ? ovr_vs : vdp.vsram[plane_b];
    int last_tile = -1, last_py = -1;
    uint16_t entry = 0;
    for (int x = -m; x < width + m; x++) {
        if (column_vs) {
            int col = x < 0 ? 0 : (x >> 4);
            if (col > (width >> 4) - 1) col = (width >> 4) - 1;
            vs = vdp.vsram[col * 2 + plane_b];
        }
        int sx = x;
        if (wrap_margins && (x < 0 || x >= width))
            sx = ((x % width) + width) % width;
        int px = (sx - hs) & wmask;
        int py = (y + vs) & hmask;
        int tile = px >> 3;
        if (tile != last_tile || py != last_py) {
            uint32_t ea = name + (((py >> 3) * pw + tile) << 1);
            entry = (vdp.vram[ea & 0xFFFF] << 8) | vdp.vram[(ea + 1) & 0xFFFF];
            last_tile = tile;
            last_py = py;
        }
        out[x + m] = (uint8_t)tile_pixel(entry, px & 7, py & 7);
        if (clip_on && !plane_b) {
            int wx = clip_camx + x;
            if (wx < 0 || wx >= clip_right)
                out[x + m] = 0;
        }
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

/* ---- player characters ----
 * Robo's animation frames are always uploaded to sprite tiles 0x100-0x17F and Mobo's
 * to 0x180-0x1FF (palette line 0). Inside such a sprite:
 *   shirt = the character's suit colours (Robo 9/A, Mobo B/C)
 *   pants = black (8) pixels in the rows right below the shirt, up to the gap above the shoes
 *   skin  = D/E
 * The analysis is done once per sprite per frame. */

typedef struct {
    uint32_t frame; uint16_t attr; uint8_t size; int8_t ch; int8_t shirt_end, pants_end;
    uint32_t body[32];   /* per row: pixels belonging to the body (largest coloured blob) */
} char_info;
static char_info char_cache[8];

static int char_of(uint16_t attr)
{
    if (attr & 0x6000) return -1;            /* palette line 0 only */
    int t = attr & 0x07FF;
    if (t >= 0x100 && t < 0x180) return CHAR_ROBO;
    if (t >= 0x180 && t < 0x200) return CHAR_MOBO;
    return -1;
}

static int any_char_colors(void)
{
    for (int c = 0; c < CHAR_COUNT; c++)
        for (int p = 0; p < PART_COUNT; p++)
            if (settings.char_on[c][p]) return 1;
    return 0;
}

/* colour index of displayed pixel (x,row) of a sprite */
static int sprite_index(uint16_t attr, int hsz, int vsz, int x, int row)
{
    int cy = row >> 3, py = row & 7, cx = x >> 3, px = x & 7;
    if (attr & 0x1000) { cy = vsz - 1 - cy; py = 7 - py; }
    if (attr & 0x0800) { cx = hsz - 1 - cx; px = 7 - px; }
    uint32_t a = (((attr + cx * vsz + cy) & 0x07FF) << 5) + (py << 2) + (px >> 1);
    uint8_t b = vdp.vram[a & 0xFFFF];
    return (px & 1) ? (b & 0x0F) : (b >> 4);
}

static const char_info *char_analyse(uint16_t attr, int hsz, int vsz, int ch)
{
    uint8_t size = (uint8_t)((hsz << 4) | vsz);
    char_info *slot = &char_cache[0];
    for (int i = 0; i < 8; i++) {
        char_info *c = &char_cache[i];
        if (c->frame == md.frame + 1 && c->attr == attr && c->size == size)
            return c;
        if (c->frame < slot->frame) slot = c;
    }
    int light = ch == CHAR_ROBO ? 9 : 0xB, dark = ch == CHAR_ROBO ? 0xA : 0xC;
    int rows = vsz * 8, shirt_end = -1, pants_end = -1, shirt_px = 0;
    for (int r = 0; r < rows; r++)
        for (int x = 0; x < hsz * 8; x++) {
            int c = sprite_index(attr, hsz, vsz, x, r);
            if (c == light || c == dark) { shirt_end = r; shirt_px++; }
        }
    if (shirt_px < 12)
        shirt_end = -1;   /* not the body (a prop or a small piece): no pants here */
    /* The body is the largest blob of non-black colours containing suit pixels; suit
     * colours outside it (the muzzle flash when shooting) are left alone. */
    memset(slot->body, 0, sizeof(slot->body));
    {
        static int16_t label[32][32];
        static uint8_t stk[1024][2];
        int w = hsz * 8, best = 0, best_n = 0, nl = 0;
        memset(label, 0, sizeof(label));
        for (int r = 0; r < rows; r++)
            for (int x = 0; x < w; x++) {
                if (label[r][x]) continue;
                int c = sprite_index(attr, hsz, vsz, x, r);
                if (!c || c == 8) continue;
                int id = ++nl, n = 0, sp = 0;
                label[r][x] = (int16_t)id;
                stk[sp][0] = (uint8_t)r; stk[sp][1] = (uint8_t)x; sp++;
                while (sp) {
                    sp--;
                    int rr = stk[sp][0], xx = stk[sp][1];
                    int cc = sprite_index(attr, hsz, vsz, xx, rr);
                    if (cc == light || cc == dark) n++;
                    for (int dy = -1; dy <= 1; dy++)
                        for (int dx = -1; dx <= 1; dx++) {
                            int ny = rr + dy, nx = xx + dx;
                            if (ny < 0 || ny >= rows || nx < 0 || nx >= w || label[ny][nx]) continue;
                            int c2 = sprite_index(attr, hsz, vsz, nx, ny);
                            if (!c2 || c2 == 8) continue;
                            label[ny][nx] = (int16_t)id;
                            stk[sp][0] = (uint8_t)ny; stk[sp][1] = (uint8_t)nx; sp++;
                        }
                }
                if (n > best_n) { best_n = n; best = id; }
            }
        for (int r = 0; r < rows; r++)
            for (int x = 0; x < w; x++)
                if (best && label[r][x] == best)
                    slot->body[r] |= 1u << x;
    }
    if (shirt_end >= 0) {
        /* pants: at most 4 rows right below the shirt, ending at the gap above the shoes
         * (in walking frames the shoes touch the legs, so the cap keeps them black) */
        pants_end = shirt_end;
        int last = shirt_end + 4 < rows - 1 ? shirt_end + 4 : rows - 1;
        for (int r = shirt_end + 1; r <= last; r++) {
            int empty = 1;
            for (int x = 0; x < hsz * 8 && empty; x++)
                if (sprite_index(attr, hsz, vsz, x, r)) empty = 0;
            if (empty) break;
            pants_end = r;
        }
    }
    {
        static int dump = -1;
        if (dump < 0) dump = getenv("CHARDUMP") != NULL;
        if (dump) {
            fprintf(stderr, "CHAR %d %dx%d attr %04x shirt_end %d pants_end %d\n", ch, hsz, vsz, attr, shirt_end, pants_end);
            for (int r = 0; r < rows; r++) {
                char line[40];
                int x;
                for (x = 0; x < hsz * 8; x++) {
                    int c = sprite_index(attr, hsz, vsz, x, r);
                    line[x] = c ? "0123456789abcdef"[c] : '.';
                }
                line[x] = 0;
                fprintf(stderr, "  %2d %s\n", r, line);
            }
        }
    }
    slot->frame = md.frame + 1;
    slot->attr = attr;
    slot->size = size;
    slot->ch = (int8_t)ch;
    slot->shirt_end = (int8_t)shirt_end;
    slot->pants_end = (int8_t)pants_end;
    return slot;
}

/* returns 0 or 1 + character slot for a player pixel */
static int char_part(int ch, int c, int row, int lx, const char_info *ci)
{
    int light = ch == CHAR_ROBO ? 9 : 0xB, dark = ch == CHAR_ROBO ? 0xA : 0xC;
    int base = ch * 8 + 1;
    int in_body = row >= 0 && row < 32 && lx >= 0 && lx < 32 && (ci->body[row] >> lx & 1);
    if (c == light && settings.char_on[ch][PART_SHIRT] && in_body) return base + CHAR_SLOT_SHIRT;
    if (c == dark && settings.char_on[ch][PART_SHIRT] && in_body) return base + CHAR_SLOT_SHIRT_DARK;
    if (c == 0xD && settings.char_on[ch][PART_SKIN]) return base + CHAR_SLOT_SKIN;
    if (c == 0xE && settings.char_on[ch][PART_SKIN]) return base + CHAR_SLOT_SKIN_DARK;
    if (c == 8 && settings.char_on[ch][PART_PANTS] && row > ci->shirt_end && row <= ci->pants_end)
        return base + CHAR_SLOT_PANTS;
    return 0;
}

static void draw_sprites(uint8_t *out, uint8_t *part, int y, int width, int m)
{
    int h40 = width == 320;
    uint32_t sat = (vdp.reg[5] & (h40 ? 0x7E : 0x7F)) << 9;
    int max_sprites = h40 ? 80 : 64, max_line = h40 ? 20 : 16, max_px = h40 ? 320 : 256;
    int link = 0, count = 0, line_count = 0, pixels = 0, masked = 0, seen_nonzero_x = 0;
    int chars = any_char_colors();
    memset(out, 0, width + 2 * m);
    memset(part, 0, width + 2 * m);
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
            int ch = chars ? char_of(attr) : -1;
            const char_info *ci = ch >= 0 ? char_analyse(attr, hsz, vsz, ch) : NULL;
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
                    if (ci && (out[x + m] & 0x0F))
                        part[x + m] = (uint8_t)char_part(ch, out[x + m] & 0x0F, row, cx * 8 + px, ci);
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

#include <stdlib.h>

/* Screen layout used by the game during play: player 2's view (or, in a 1-player game,
 * the "press 2P start" panel) on lines 7-86, the HUD, then player 1's view on lines
 * 135-214. */
#define VIEW_TOP_Y0 7
#define VIEW_BOT_Y0 135
#define VIEW_H 80
static struct { uint32_t frame; int hsA, hsB, vsA, vsB; } bottom_view;
/* Vertical camera position of the floor player 1 stands on. While he walks up or down
 * stairs the camera scrolls between floors; the room-above view stays anchored to the
 * floor he started from (those plane rows are fully drawn) instead of following the
 * camera into rows the game has not filled yet. */
static struct { int vs, last_vs, still; } floor_anchor = {-1, -1, 0};

/* Full camera x of a view: the scroll register only has 10 bits, the game keeps the
 * whole value at $FF1846 (player 1) / $FF184A (player 2) as a negative scroll. */
static int view_camx(int hs_line, int ram_addr)
{
    int16_t ram_hs = (int16_t)((md.ram[ram_addr] << 8) | md.ram[ram_addr + 1]);
    int cam_ram = -ram_hs;
    int cam = (-hs_line) & 0x3FF;
    while (cam + 512 < cam_ram) cam += 1024;
    while (cam - 512 > cam_ram) cam -= 1024;
    return cam;
}

/* right end of the building in world pixels: the camera stops when its right column
 * reaches the level width in 16-pixel columns ($FF2CD0) */
static int level_right(void)
{
    int cols = (md.ram[0x2CD0] << 8) | md.ram[0x2CD1];
    return cols > 17 && cols < 512 ? (cols - 1) * 16 : 0x7FFF;
}

static void update_floor_anchor(int vs)
{
    int d = vs - floor_anchor.last_vs;
    if (d < 0) d = -d;
    if (floor_anchor.vs < 0 || floor_anchor.last_vs < 0 || d > 16) {
        floor_anchor.vs = vs;        /* first view, or the game jumped the camera (new floor) */
        floor_anchor.still = 0;
    } else if (d == 0) {
        if (++floor_anchor.still >= 30)
            floor_anchor.vs = vs;    /* camera resting: this is the current floor */
    } else {
        floor_anchor.still = 0;      /* scrolling between floors: keep the anchor */
    }
    floor_anchor.last_vs = vs;
}

static int line_opaque(const uint8_t *buf, int m, int width)
{
    for (int x = m; x < m + width; x++)
        if (buf[x] & 0x0F)
            return 1;
    return 0;
}

static int line_hscroll(int y, int plane_b)
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
    uint8_t a[VDP_FB_W], b[VDP_FB_W], s[VDP_FB_W], win[VDP_FB_W], wbuf[VDP_FB_W], part[VDP_FB_W];
    /* during play (player 1's view seen last frame) tidy up the widescreen margins:
     * the city backdrop behind the rooms is only one screen wide, so it is repeated,
     * and the thin HUD strips at the very top and bottom just extend their edges */
    int playing = bottom_view.frame == md.frame || bottom_view.frame == md.frame + 1;
    int in_top = y >= VIEW_TOP_Y0 && y < VIEW_TOP_Y0 + VIEW_H;
    int in_view = in_top || (y >= VIEW_BOT_Y0 && y < VIEW_BOT_Y0 + VIEW_H);
    int clamp_edges = m && playing && !in_view;   /* HUD rows: extend their edge colours */
    int in_bot = y >= VIEW_BOT_Y0 && y < VIEW_BOT_Y0 + VIEW_H;
    draw_plane(a, y, width, m, 0);
    int room_view = line_opaque(a, m, width);   /* a real room is shown on this line */
    if (m && playing && room_view) {
        /* widescreen: nothing of the building beyond its left/right walls */
        clip_on = 1;
        clip_camx = view_camx(line_hscroll(y, 0), in_bot ? 0x1846 : 0x184A);
        clip_right = level_right();
        draw_plane(a, y, width, m, 0);
        clip_on = 0;
    }
    int room_above = 0;
    if (in_top) {
        /* in a 1-player game the game shows nothing in plane A here (the 2P panel) */
        if (!room_view && !settings.show_2p_panel && bottom_view.frame == md.frame)
            room_above = 1;
        if (!room_view && !room_above && m && playing)
            clamp_edges = 1;   /* original 2P panel: extend its edges */
    } else if (y == VIEW_BOT_Y0) {
        /* remember player 1's camera for the next frame's room-above view */
        if (room_view) {
            bottom_view.frame = md.frame + 1;
            bottom_view.hsA = line_hscroll(y, 0);
            bottom_view.hsB = line_hscroll(y, 1);
            bottom_view.vsA = vdp.vsram[0];
            bottom_view.vsB = vdp.vsram[1];
            update_floor_anchor(bottom_view.vsA & 0x3FF);
            static int vlog_view = -1;
            if (vlog_view < 0) vlog_view = getenv("VIEWLOG") != NULL;
            if (vlog_view)
                fprintf(stderr, "view %u hsA %d vsA %d hsB %d vsB %d\n", md.frame, bottom_view.hsA, bottom_view.vsA,
                        bottom_view.hsB, bottom_view.vsB);
        } else {
            bottom_view.frame = 0;
            floor_anchor.vs = floor_anchor.last_vs = -1;
        }
    }
    wrap_margins = m && playing && in_view && room_view;
    draw_plane(b, y, width, m, 1);
    wrap_margins = 0;
    if (room_above) {
        /* the floor above player 1: plane rows directly above the bottom view */
        ovr_hs = bottom_view.hsB;          /* same sky/city background as below */
        ovr_vs = (bottom_view.vsB + VIEW_BOT_Y0 - VIEW_TOP_Y0) & 0x3FF;
        wrap_margins = m != 0;
        draw_plane(b, y, width, m, 1);
        wrap_margins = 0;
        ovr_hs = bottom_view.hsA;          /* the 80 plane rows right above player 1's view */
        ovr_vs = (floor_anchor.vs + VIEW_BOT_Y0 - VIEW_TOP_Y0 - VIEW_H) & 0x3FF;
        clip_on = m != 0;
        clip_camx = view_camx(bottom_view.hsA, 0x1846);
        clip_right = level_right();
        draw_plane(a, y, width, m, 0);
        clip_on = 0;
        ovr_hs = ovr_vs = -1;
    }
    /* the window covers nothing when HP=0 && !RIGT and VP=0 && !DOWN */
    if ((vdp.reg[17] & 0x9F) || (vdp.reg[18] & 0x9F)) {
        draw_window(wbuf, y, width, m, win);
        for (int x = 0; x < ow; x++)
            if (win[x])
                a[x] = wbuf[x];
    }
    draw_sprites(s, part, y, width, m);
    if (room_above) { /* the 2P panel's sprites do not belong to the room */
        memset(s, 0, sizeof(s));
        memset(part, 0, sizeof(part));
    }
    for (int x = 0; x < ow; x++) {
        int p;
        int sp = s[x], ap = a[x], bp = b[x];
        if ((sp & 0x80) && (sp & 0x0F)) { if (part[x]) { fb[x] = VDP_CHAR_BASE + part[x] - 1; continue; } p = sp; }
        else if ((ap & 0x80) && (ap & 0x0F)) p = ap;
        else if ((bp & 0x80) && (bp & 0x0F)) p = bp;
        else if (sp & 0x0F) { if (part[x]) { fb[x] = VDP_CHAR_BASE + part[x] - 1; continue; } p = sp; }
        else if (ap & 0x0F) p = ap;
        else if (bp & 0x0F) p = bp;
        else { fb[x] = bg; continue; }
        fb[x] = pal[p & 0x3F];
    }
    if (clamp_edges) {
        for (int x = 0; x < m; x++)
            fb[x] = fb[m];
        for (int x = m + width; x < ow; x++)
            fb[x] = fb[m + width - 1];
    }
    for (int x = ow; x < VDP_FB_W; x++)
        fb[x] = VDP_BLACK;
}
