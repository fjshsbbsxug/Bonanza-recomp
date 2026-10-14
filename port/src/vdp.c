/* Mega Drive VDP (YM7101): ports, DMA and a per-scanline renderer. */
#include <stdlib.h>
#include <string.h>
#include "vdp.h"
#include "md.h"
#include "settings.h"
#include "wideobj.h"

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

static void vram_log(uint16_t a, uint8_t old, uint8_t now);

static inline void vram_set(uint16_t a, uint8_t v)
{
    if (vdp.vram[a] != v) {
        vram_log(a, vdp.vram[a], v);
        vdp.vram[a] = v;
    }
}

static void vram_write_word(uint16_t a, uint16_t w)
{
    if (a & 1)
        w = (uint16_t)((w >> 8) | (w << 8));
    vram_set(a & 0xFFFE, (uint8_t)(w >> 8));
    vram_set((a & 0xFFFE) | 1, (uint8_t)w);
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
        vram_set(vdp.addr, vdp.vram[src]);
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
            vram_set(vdp.addr ^ 1, b);
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
static int vlog = -1;   /* VDPLOG: every control word (the decoded commands go through it) */
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

/* The game's video commands, decoded when its code was translated (tools/recomp.py):
 * a register, or where the data that follows goes (and the copy it may start). They do
 * what the command words do when written to the control port one after the other. */
void vdp_set_reg(int r, uint8_t v)
{
    if (vlog < 0) vlog = getenv("VDPLOG") != NULL;
    if (vdp.pending || vlog > 0) {       /* half a command waits: the word completes it */
        vdp_write_ctrl((uint16_t)(0x8000 | (r << 8) | v));
        return;
    }
    vdp.reg[r] = v;
    if (r == 0 || r == 1)
        cpu_set_irq(vdp_irq_level());
}

void vdp_command(uint8_t code, uint16_t addr)
{
    if (vlog < 0) vlog = getenv("VDPLOG") != NULL;
    if (vdp.pending || vlog > 0) {
        vdp_write_ctrl((uint16_t)(((code & 3) << 14) | (addr & 0x3FFF)));
        vdp_write_ctrl((uint16_t)(((code & 0x3C) << 2) | (addr >> 14)));
        return;
    }
    vdp.addr = addr;
    vdp.code = code;
    if ((code & 0x20) && (vdp.reg[1] & 0x10)) {
        switch (vdp.reg[23] >> 6) {
        case 0: case 1: dma_68k(); break;
        case 2: vdp.fill_pending = 1; break;
        case 3: dma_copy(); break;
        }
    }
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
        return (entry >> 8) & 0x80;    /* transparent; the priority still counts for shadows */
    return c | ((entry >> 9) & 0x30) | ((entry >> 8) & 0x80);
}

/* Output can be wider than the real screen ("widescreen"): vdp.margin extra pixels are
 * drawn on each side from the same VRAM. This only affects the picture, never the
 * emulated machine state. Line buffers are indexed with x + margin. */

static int ovr_hs = -1, ovr_vs = -1;   /* scroll override (room-above view) */
static int wrap_margins;               /* widescreen: repeat the 4:3 picture's columns (static backdrops) */
/* Map mode for plane A (the rooms): pixels are taken from the game's level map in ROM
 * at world position (map_camx + x, map_wy) - only outside the real screen (map_all = 0)
 * or everywhere (map_all = 1, the room-above view). Outside the building: transparent. */
static int map_on, map_all, map_camx, map_wy;
static uint16_t map_entry(int wx, int wy);

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
        /* the game may leave the column at the screen edge half updated (a TV hid it
         * in the overscan): beside the picture and in its outer 16 pixels, use the map */
        if (map_on && !plane_b && (map_all || x < 16 || x >= width - 16)) {
            int wx = map_camx + x;
            uint16_t me = map_entry(wx, map_wy);
            out[x + m] = me == 0xFFFF ? 0 : (uint8_t)tile_pixel(me, wx & 7, map_wy & 7);
            continue;
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

static wobj_sprite wobj_list[256];
static int wobj_count;

/* sprites built by wideobj.c, below the real ones */
static void draw_wobj(uint8_t *out, int y, int width, int m, const wobj_sprite *list, int count)
{
    for (int i = 0; i < count; i++) {
        const wobj_sprite *w = &list[i];
        int row = y - w->y;
        if (y < w->ymin || y >= w->ymax || row < 0 || row >= w->vsz * 8)
            continue;
        int cy = row >> 3, py = row & 7;
        if (w->attr & 0x1000) { cy = w->vsz - 1 - cy; py = 7 - py; }
        for (int cx = 0; cx < w->hsz; cx++) {
            int tcx = (w->attr & 0x0800) ? w->hsz - 1 - cx : cx;
            uint16_t e = (uint16_t)((w->attr & 0xE800) | ((w->attr + tcx * w->vsz + cy) & 0x07FF));
            for (int px = 0; px < 8; px++) {
                int x = w->x + cx * 8 + px;
                if (x < -m || x >= width + m || (out[x + m] & 0x0F))
                    continue;
                if (w->only_outside && x >= 0 && x < width)
                    continue;
                out[x + m] = (uint8_t)tile_pixel(e, px, py);
            }
        }
    }
}

static wobj_sprite stack_list[256];
static int stack_count;

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
            /* player colours only for the players' own pieces */
            int ch = chars && char_of(attr) >= 0 ? wobj_player_of(attr) : -1;
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
    draw_wobj(out, y, width, m, wobj_list, wobj_count);
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
/* each view's camera at its first line: frame (+1), camera x, scroll of plane B, world y */
static struct { uint32_t frame; int hsA, hsB, vsA, vsB; } view_rec[2];
/* Online games: the player at this screen. His own view is always shown at the bottom
 * and the floor above him at the top - the other player has his own screen. */
int vdp_local_player = -1;
static int focus;           /* this frame: the view shown at the bottom (0 = player 1) */
static int room_above_wy;   /* world y of the room-above view's first line */
static uint32_t stacked_frame;  /* frame (+1) in which the views are stacked (see below) */
static int stacked_top;         /* world y of the top view's first line then */

/* ---- level map (what the game draws into plane A during play) ----
 *   $FF1838.l  map: words (metatile numbers), $FF2CCC words per row, one row per 16 px
 *   $FF183C.l  metatiles: 4 name-table entries each (2x2 cells)
 *   player 1: $FF1846 = -camera x, $FF1844 = vertical camera, $FF187A = floor row
 *   player 2: $FF228A, $FF2288, $FF22BE
 * The current floor's top is at plane row 80 (player 1) / 352 (player 2), so
 * world y of a view line = (floor row + 5) * 16 + (line + vertical camera - that row). */

static uint32_t ram32(int a) { return ((uint32_t)md.ram[a] << 24) | (md.ram[a + 1] << 16) | (md.ram[a + 2] << 8) | md.ram[a + 3]; }
static int ram16s(int a) { return (int16_t)((md.ram[a] << 8) | md.ram[a + 1]); }

static struct { uint32_t map, meta; int w; } level;

static int level_load(void)
{
    level.map = ram32(0x1838);
    level.meta = ram32(0x183C);
    level.w = ram16s(0x2CCC);
    return level.map < md.rom_size && level.meta < md.rom_size && level.w > 16 && level.w < 512;
}

static uint16_t map_entry(int wx, int wy)
{
    if (wx < 0 || wy < 0 || wx >= level.w * 16 || wy >= 64 * 16)
        return 0xFFFF;                      /* outside the building */
    uint32_t a = level.map + (uint32_t)((wy >> 4) * level.w + (wx >> 4)) * 2;
    if (a + 1 >= md.rom_size)
        return 0xFFFF;
    uint16_t mt = (md.rom[a] << 8) | md.rom[a + 1];
    uint32_t e = level.meta + mt * 8u + ((wy >> 3) & 1) * 4 + ((wx >> 3) & 1) * 2;
    if (e + 1 >= md.rom_size)
        return 0xFFFF;
    return (md.rom[e] << 8) | md.rom[e + 1];
}

/* nearest full value to a 10-bit register value, given the game's full value */
static int unwrap10(int reg, int full)
{
    int v = reg & 0x3FF;
    while (v + 512 < full) v += 1024;
    while (v - 512 > full) v -= 1024;
    return v;
}

/* ---- cutscenes: 4:3 crop ----
 * Widescreen is only used while playing. Game over / time-up screens keep the play
 * screen's HUD, so they stay wide too (edges extended); everything else (logo, title,
 * story, stage intros) is shown 4:3 with black side bars. The HUD is recognised by
 * comparing a line of it with what it looked like during play. */
#define HUD_Y 100
static uint16_t hud_sig[16];
static int hud_sig_ok;
static uint32_t hud_seen_frame;       /* last frame (+1) the HUD was on screen */

static void hud_entries(uint16_t *out, int width)
{
    static const int sizes[4] = {32, 64, 32, 128};
    int pw = sizes[vdp.reg[16] & 3], ph = sizes[(vdp.reg[16] >> 4) & 3];
    uint32_t name = (vdp.reg[4] & 0x07) << 13;     /* the HUD is in plane B */
    extern int line_hscroll_pub(int y, int plane_b);
    int hs = line_hscroll_pub(HUD_Y, 1), vs = vdp.vsram[1];
    for (int k = 0; k < 16; k++) {
        int x = k * width / 16 + 4;
        int px = (x - hs) & (pw * 8 - 1), py = (HUD_Y + vs) & (ph * 8 - 1);
        uint32_t ea = name + (((py >> 3) * pw + (px >> 3)) << 1);
        out[k] = (vdp.vram[ea & 0xFFFF] << 8) | vdp.vram[(ea + 1) & 0xFFFF];
    }
}

typedef struct {
    int ok;              /* the map matches this view (recently) */
    uint32_t frame;      /* frame (+1) of the last check */
    uint32_t good_frame; /* frame (+1) of the last successful check */
    int dwy;             /* correction to the camera's world y (see view_check) */
    int camx, wy0;
} view_map;
static view_map views[2];   /* 0 = player 1 (bottom), 1 = player 2 (top) */

/* camera of a view on this line (from the scroll registers, made whole with RAM) */
static void view_camera(int v, int y0, int hs_reg, int vs_reg, int *camx, int *wy0)
{
    int hx = v ? 0x228A : 0x1846, vy = v ? 0x2288 : 0x1844, fr = v ? 0x22BE : 0x187A;
    int top = v ? 352 : 80;
    *camx = unwrap10(-hs_reg, -ram16s(hx));
    int vcam = unwrap10(vs_reg, ram16s(vy));
    *wy0 = (ram16s(fr) + 5) * 16 + (y0 + vcam - top);
}

/* does plane A on this line really show the level map at that camera? */
static int view_matches_map(int y, int camx, int wy, int width)
{
    static const int sizes[4] = {32, 64, 32, 128};
    int pw = sizes[vdp.reg[16] & 3], ph = sizes[(vdp.reg[16] >> 4) & 3];
    uint32_t name = (vdp.reg[2] & 0x38) << 10;
    int hs = 0, vs = vdp.vsram[0];
    {
        extern int line_hscroll_pub(int y, int plane_b);
        hs = line_hscroll_pub(y, 0);
    }
    int good = 0, tested = 0, inside = 0, inside_good = 0;
    for (int x = 4; x < width; x += 16) {
        uint16_t me = map_entry(camx + x, wy);
        int px = (x - hs) & (pw * 8 - 1), py = (y + vs) & (ph * 8 - 1);
        uint32_t ea = name + (((py >> 3) * pw + (px >> 3)) << 1);
        uint16_t ve = (vdp.vram[ea & 0xFFFF] << 8) | vdp.vram[(ea + 1) & 0xFFFF];
        tested++;
        if (me == 0xFFFF) {
            /* outside the building: the game shows an empty tile there */
            const uint8_t *t = &vdp.vram[(ve & 0x07FF) << 5];
            int blank = 1;
            for (int i = 0; i < 32 && blank; i++) blank = !t[i];
            good += blank;
        } else {
            inside++;
            inside_good += ve == me;
            good += ve == me;
        }
    }
    return inside >= 4 && inside_good * 4 >= inside * 3 && good * 4 >= tested * 3;
}

/* Check a whole view against the map, once per frame, at its first line. While the
 * camera moves to another floor the game updates its floor number a few frames before
 * (or after) the scroll registers, so the floor above and below are tried as well. */
static void view_check(view_map *v, int y0, int camx, int wy0, int width)
{
    static const int tries[3] = {0, -80, 80};
    int order[3] = {v->dwy, 0, 0}, n = 1;
    for (int i = 0; i < 3; i++)
        if (tries[i] != v->dwy) order[n++] = tries[i];
    v->frame = md.frame + 1;
    /* The camera only moves a few pixels a frame: while it is being followed, the
     * floor that keeps it closest to last frame's position is the one, whatever the
     * map says (near the top of a level, rows of sky match the map at more than one
     * floor). The map check then only says whether the map can be trusted. */
    if (md.frame + 1 - v->good_frame <= 2) {
        int best = 0;
        for (int i = 1; i < 3; i++)
            if (abs(wy0 + tries[i] - v->wy0) < abs(wy0 + tries[best] - v->wy0)) best = i;
        int d = tries[best];
        if (abs(wy0 + d - v->wy0) <= 24) {
            int good = 0;
            for (int k = 8; k < VIEW_H; k += 24)
                good += view_matches_map(y0 + k, camx, wy0 + d + k, width);
            v->dwy = d;
            v->wy0 = wy0 + d;
            if (good >= 2) {
                v->good_frame = md.frame + 1;
                v->ok = 1;
            } else {
                v->ok = md.frame + 1 - v->good_frame < 8;
            }
            return;
        }
    }
    for (int i = 0; i < 3; i++) {
        int d = order[i], good = 0;
        for (int k = 8; k < VIEW_H; k += 24)   /* lines 8, 32, 56 */
            good += view_matches_map(y0 + k, camx, wy0 + d + k, width);
        if (good >= 2) {
            v->dwy = d;
            v->wy0 = wy0 + d;
            v->good_frame = md.frame + 1;
            v->ok = 1;
            return;
        }
    }
    /* a frame or two without a match (a door being redrawn...) keeps the map */
    v->ok = md.frame + 1 - v->good_frame < 8;
}

static int line_opaque(const uint8_t *buf, int m, int width)
{
    for (int x = m; x < m + width; x++)
        if (buf[x] & 0x0F)
            return 1;
    return 0;
}

static int line_hscroll(int y, int plane_b);
int line_hscroll_pub(int y, int plane_b) { return line_hscroll(y, plane_b); }

/* ---------------------------------------------------------------- the frame, drawn at once
 *
 * The game's code runs for the whole frame first; while it runs, the state the picture
 * takes line by line (registers, palette, vertical scroll, the line's horizontal scroll:
 * what the game's line effects change between the split screen's bands) is kept for
 * each line, and the picture is drawn from it when the frame is over. */
typedef struct {
    uint8_t  reg[24];
    uint16_t cram[64];
    uint16_t vsram[40];
} line_state;
static line_state lines[224];

/* tiles and maps the game changes while the picture is shown (its frame's work goes on
 * into the top of the screen, and the split screen's bands reuse rows of the planes):
 * each change made during the shown lines, with what was there before, so the picture
 * is drawn with what each line had */
typedef struct { uint16_t a; uint8_t old, now, line; } vram_change;
static vram_change *changes;
static int nchanges, changes_cap, changes_lost;

static void vram_log(uint16_t a, uint8_t old, uint8_t now)
{
    if (md.line >= 224) return;          /* between pictures: nothing shown changes */
    if (nchanges == changes_cap) {
        int c = changes_cap ? changes_cap * 2 : 4096;
        vram_change *n = c <= (1 << 20) ? realloc(changes, (size_t)c * sizeof(*n)) : 0;
        if (!n) { changes_lost = 1; return; }
        changes = n; changes_cap = c;
    }
    changes[nchanges].a = a;
    changes[nchanges].old = old;
    changes[nchanges].now = now;
    changes[nchanges].line = (uint8_t)md.line;
    nchanges++;
}

void vdp_line_capture(int y)
{
    line_state *l = &lines[y];
    memcpy(l->reg, vdp.reg, sizeof(l->reg));
    memcpy(l->cram, vdp.cram, sizeof(l->cram));
    memcpy(l->vsram, vdp.vsram, sizeof(l->vsram));
}

void vdp_render_frame(void)
{
    static uint8_t live_vram[0x10000];
    line_state live;
    memcpy(live.reg, vdp.reg, sizeof(live.reg));
    memcpy(live.cram, vdp.cram, sizeof(live.cram));
    memcpy(live.vsram, vdp.vsram, sizeof(live.vsram));
    int replay = nchanges && !changes_lost;
    if (replay) {                        /* back to the tiles and maps of the first line */
        memcpy(live_vram, vdp.vram, sizeof(live_vram));
        for (int i = nchanges - 1; i >= 0; i--) vdp.vram[changes[i].a] = changes[i].old;
    }
    int k = 0;
    for (int y = 0; y < 224; y++) {
        /* what was changed during the lines before this one is in place */
        for (; replay && k < nchanges && changes[k].line < y; k++)
            vdp.vram[changes[k].a] = changes[k].now;
        memcpy(vdp.reg, lines[y].reg, sizeof(live.reg));
        memcpy(vdp.cram, lines[y].cram, sizeof(live.cram));
        memcpy(vdp.vsram, lines[y].vsram, sizeof(live.vsram));
        vdp_render_line(y);
    }
    if (replay) memcpy(vdp.vram, live_vram, sizeof(live_vram));
    nchanges = 0; changes_lost = 0;
    memcpy(vdp.reg, live.reg, sizeof(live.reg));
    memcpy(vdp.cram, live.cram, sizeof(live.cram));
    memcpy(vdp.vsram, live.vsram, sizeof(live.vsram));
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

/* Shadow / highlight (register 12 bit 3), used for the spotlights of the story and
 * targets screens. Where neither plane's tile has priority, the picture is shadowed
 * (half bright). Sprite colours 62 and 63 are not drawn: they light up (shadowed to
 * normal, normal to highlighted) or shadow what is beneath them. Other sprite pixels
 * are normal when the sprite has priority or uses colour 14 of its palette, otherwise
 * they take the shading beneath them. */
static void shade_line(uint16_t *fb, const uint8_t *s, const uint8_t *a, const uint8_t *b,
                       const uint8_t *part, const uint16_t *pal, uint16_t bg, int ow)
{
    for (int x = 0; x < ow; x++) {
        int sp = s[x], ap = a[x], bp = b[x];
        int shade = (ap & 0x80) || (bp & 0x80) ? 0 : VDP_SHADOW;
        uint16_t under;                  /* the planes' pixel */
        int plane_prio = 0;
        if ((ap & 0x80) && (ap & 0x0F)) { under = pal[ap & 0x3F]; plane_prio = 1; }
        else if ((bp & 0x80) && (bp & 0x0F)) { under = pal[bp & 0x3F]; plane_prio = 1; }
        else if (ap & 0x0F) under = pal[ap & 0x3F];
        else if (bp & 0x0F) under = pal[bp & 0x3F];
        else under = bg;
        int c = sp & 0x3F;
        if ((sp & 0x0F) && (c == 0x3E || c == 0x3F)) {
            if (c == 0x3F) shade = VDP_SHADOW;
            else shade = shade ? 0 : VDP_HIGHLIGHT;
            fb[x] = under | shade;
        } else if ((sp & 0x0F) && ((sp & 0x80) || !plane_prio)) {
            uint16_t col = part[x] ? VDP_CHAR_BASE + part[x] - 1 : pal[c];
            fb[x] = col | ((sp & 0x80) || (sp & 0x0F) == 0x0E ? 0 : shade);
        } else {
            fb[x] = under | shade;
        }
    }
}

void vdp_render_line(int y)
{
    int width = vdp_width();
    int m = vdp_margin();
    int ow = width + 2 * m;
    if (y == 0) {
        /* player 2's screen in an online game: while he plays, his view (lines 7-86)
         * moves to the bottom and the floor above him takes the top */
        uint16_t on = (uint16_t)((md.ram[0x2EF8] << 8) | md.ram[0x2EF9]);
        /* only in a stage (mode 0x0C): the intro, targets screen, coffee breaks and the
         * ending are one picture and stay as they are */
        uint16_t mode = (uint16_t)((md.ram[0xC070] << 8) | md.ram[0xC071]);
        focus = vdp_local_player == 1 && on && on != 0x8000 && mode == 0x0C;
    }
    int online = vdp_local_player >= 0;
    int real_y0 = focus ? VIEW_TOP_Y0 : VIEW_BOT_Y0, other_y0 = focus ? VIEW_BOT_Y0 : VIEW_TOP_Y0;
    int in_real = y >= real_y0 && y < real_y0 + VIEW_H;
    int in_other = y >= other_y0 && y < other_y0 + VIEW_H;
    uint16_t *fb = vdp.fbmd[in_real ? VIEW_BOT_Y0 + y - real_y0 : in_other ? VIEW_TOP_Y0 + y - other_y0 : y];
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
    /* Widescreen during play: the side areas of each view are drawn from the level map,
     * the backdrop is repeated, the HUD rows extend their edges. In a 1-player game the
     * top view shows the floor above player 1 (also from the map). */
    int in_top = y >= VIEW_TOP_Y0 && y < VIEW_TOP_Y0 + VIEW_H;
    int in_bot = y >= VIEW_BOT_Y0 && y < VIEW_BOT_Y0 + VIEW_H;
    int in_view = in_top || in_bot;
    int playing = views[0].ok && (views[0].frame == md.frame || views[0].frame == md.frame + 1);
    if (y == 0 && !level_load())
        views[0].ok = views[1].ok = 0;
    if (any_char_colors()) wobj_sample_players();
    if (y == 0)   /* objects beside the picture (also keeps its snapshots up to date) */
        wobj_count = wobj_build(m && playing ? m : 0, width, settings.patrol_return && !settings.floor_alert,
                                wobj_list, (int)(sizeof(wobj_list) / sizeof(wobj_list[0])));
    if (y == 0 && wobj_count) {
        /* drop pieces the game's own sprite table already shows */
        int h40 = width == 320, n = 0;
        uint32_t sat = (vdp.reg[5] & (h40 ? 0x7E : 0x7F)) << 9;
        for (int i = 0; i < wobj_count; i++) {
            const wobj_sprite *w = &wobj_list[i];
            int dup = 0, link = 0, count = 0;
            do {
                const uint8_t *e = &vdp.vram[(sat + link * 8) & 0xFFFF];
                link = e[3] & 0x7F;
                if ((((e[0] << 8) | e[1]) & 0x3FF) - 128 == w->y && (((e[6] << 8) | e[7]) & 0x1FF) - 128 == w->x &&
                    ((e[4] << 8) | e[5]) == w->attr) { dup = 1; break; }
            } while (link && ++count < 80);
            if (!dup) wobj_list[n++] = *w;
        }
        wobj_count = n;
    }
    if (y == 0) {
        static int wlog = -1;
        if (wlog < 0) wlog = getenv("WOBJLOG") != NULL;
        if (wlog && wobj_count) {
            fprintf(stderr, "wobj %u:", md.frame);
            for (int i = 0; i < wobj_count; i++)
                fprintf(stderr, " (%d,%d %04x%s)", wobj_list[i].x, wobj_list[i].y, wobj_list[i].attr, wobj_list[i].only_outside ? " g" : "");
            fprintf(stderr, "\n");
        }
    }
    int vi = in_bot ? 0 : 1;
    int camx = 0, wy0 = 0, map_view = 0;
    if (in_view && level.w) {
        view_camera(vi, in_bot ? VIEW_BOT_Y0 : VIEW_TOP_Y0, line_hscroll(y, 0), vdp.vsram[0], &camx, &wy0);
        if (y == (in_bot ? VIEW_BOT_Y0 : VIEW_TOP_Y0)) {
            /* once per frame and view: check the map against what the game drew */
            view_check(&views[vi], y, camx, wy0, width);
        }
        wy0 += views[vi].dwy;
        map_view = views[vi].ok && (views[vi].frame == md.frame || views[vi].frame == md.frame + 1);
        if (in_top && y == VIEW_TOP_Y0 + 40) {
            static int vlog_top = -1;
            if (vlog_top < 0) vlog_top = getenv("VIEWLOG") != NULL;
            if (vlog_top)
                fprintf(stderr, "top %u hs %d vs %d\n", md.frame, line_hscroll(y, 0), vdp.vsram[0]);
        }
        if (y == (in_bot ? VIEW_BOT_Y0 : VIEW_TOP_Y0)) {
            view_rec[vi].frame = md.frame + 1;
            view_rec[vi].hsB = line_hscroll(y, 1);
            view_rec[vi].vsB = vdp.vsram[1];
            view_rec[vi].hsA = camx;      /* full camera x */
            view_rec[vi].vsA = wy0;       /* world y of the view's first line */
            static int vlog_view = -1;
            if (vlog_view < 0) vlog_view = getenv("VIEWLOG") != NULL;
            if (vlog_view && in_bot)
                fprintf(stderr, "view %u camx %d wy0 %d ok %d/%d hsB %d vsB %d\n", md.frame, camx, wy0, views[0].ok, views[1].ok, view_rec[0].hsB, view_rec[0].vsB);
            if (focus && in_real && playing && wy0 < VIEW_H) {
                /* player 2's screen: his view comes first, so the views are stacked
                 * from his camera of this frame (see room_above below) */
                stacked_frame = md.frame + 1;
                stacked_top = wy0 < 0 ? wy0 : 0;
            }
        }
    }
    if (y == HUD_Y && m) {
        /* the HUD (plane B) counts only if it can be seen: a cutscene drawn in plane A
         * may leave it in video memory, hidden behind its picture */
        uint8_t pa[VDP_FB_W];
        draw_plane(pa, y, width, m, 0);
        int cover = 0;
        for (int x = m; x < m + width; x++)
            cover += (pa[x] & 0x0F) != 0;
        uint16_t e[16];
        hud_entries(e, width);
        if (playing) {
            memcpy(hud_sig, e, sizeof(hud_sig));
            hud_sig_ok = 1;
            hud_seen_frame = md.frame + 1;
        } else if (hud_sig_ok && cover * 4 < width) {
            int same = 0;
            for (int k = 0; k < 16; k++) same += e[k] == hud_sig[k];
            if (same >= 10) hud_seen_frame = md.frame + 1;
        }
    }
    int hud_screen = !playing && (hud_seen_frame == md.frame || hud_seen_frame == md.frame + 1);
    int crop = m && settings.crop_cutscenes && !playing && !hud_screen;
    int clamp_edges = m && (playing || hud_screen) && (!in_view || !playing);   /* extend edge colours */
    /* Stacked views (1 player, top view stopped at the top of the level): the two
     * views show two floors one above the other, so player 1's view shows the floor
     * below the top view's, from the map, with the characters where they really are -
     * instead of the same floor twice. */
    int stacked = in_real && map_view && stacked_frame == md.frame + 1;
    map_on = m && map_view;
    map_all = 0;
    map_camx = camx;
    map_wy = wy0 + y - (in_bot ? VIEW_BOT_Y0 : VIEW_TOP_Y0);
    if (stacked) {
        map_on = map_all = 1;
        map_wy = stacked_top + VIEW_H + (y - real_y0);
    }
    draw_plane(a, y, width, m, 0);
    map_on = map_all = 0;
    /* a real room is shown on this line (the outer 16 pixels may come from the map) */
    int room_view = line_opaque(a, m + 16, width - 32);
    int room_above = 0;
    int rec = view_rec[focus].frame == md.frame || view_rec[focus].frame == md.frame + 1;
    if (in_other && online && playing && rec) {
        /* online: the other player's view is on his own screen */
        room_above = 1;
    } else if (in_top && !room_view) {
        /* 1-player game: the game shows nothing in plane A here (the 2P panel) */
        if (!settings.show_2p_panel && playing && view_rec[0].frame == md.frame)
            room_above = 1;
        else if (m && playing)
            clamp_edges = 1;   /* original 2P panel: extend its edges */
    }
    /* the backdrop repeats every 256 pixels; also on lines that show only sky (above a
     * roof), which is what plane B to the right of the backdrop must never show */
    if (room_above) {
        /* The floor above player 1, following his camera - but never above the top of
         * the level (the exit floor / roof): from there on it stays still, and as it
         * then shows player 1's own floor, he is seen in it (escaping, too). */
        room_above_wy = view_rec[focus].vsA - VIEW_H;
        if (room_above_wy < 0) {
            room_above_wy = view_rec[focus].vsA < 0 ? view_rec[focus].vsA : 0;
            stacked_frame = md.frame + 1;     /* player 1's view: the floor below */
            stacked_top = room_above_wy;
        }
    }
    wrap_margins = m && playing && in_view && (room_view || map_view);
    draw_plane(b, y, width, m, 1);
    wrap_margins = 0;
    if (room_above) {
        /* the floor above player 1, straight from the level map, following his camera */
        ovr_hs = view_rec[focus].hsB;          /* same sky/city background as below */
        ovr_vs = (view_rec[focus].vsB + real_y0 - other_y0) & 0x3FF;
        wrap_margins = m != 0;
        draw_plane(b, y, width, m, 1);
        wrap_margins = 0;
        ovr_hs = ovr_vs = -1;
        map_on = map_all = 1;
        map_camx = view_rec[focus].hsA;
        map_wy = room_above_wy + (y - other_y0);
        draw_plane(a, y, width, m, 0);
        map_on = map_all = 0;
    }
    /* a view whose map could not be matched: rather than whatever video memory holds
     * beside the picture, extend its edges */
    if (m && playing && in_view && !map_view && !room_above)
        clamp_edges = 1;
    /* the window covers nothing when HP=0 && !RIGT and VP=0 && !DOWN */
    if ((vdp.reg[17] & 0x9F) || (vdp.reg[18] & 0x9F)) {
        draw_window(wbuf, y, width, m, win);
        for (int x = 0; x < ow; x++)
            if (win[x])
                a[x] = wbuf[x];
    }
    if (room_above) {
        /* The 2P panel's sprites do not belong to the room. Where the top view shows
         * the same part of the level as player 1's view (on the top floor, or on the
         * stairs) it shows the same characters: the sprites of that line of his view. */
        int yb = real_y0 + room_above_wy + (y - other_y0) - view_rec[focus].vsA;
        if (yb >= real_y0 && yb < real_y0 + VIEW_H) {
            draw_sprites(s, part, yb, width, m);
        } else {
            memset(s, 0, sizeof(s));
            memset(part, 0, sizeof(part));
        }
    } else if (stacked) {
        /* the characters of player 1's view that are on the floor shown here, and the
         * objects of that floor his view does not cover (they would vanish) */
        if (y == real_y0)
            stack_count = wobj_build_stacked(focus, stacked_top + VIEW_H, wy0, stack_list,
                                             (int)(sizeof(stack_list) / sizeof(stack_list[0])));
        int yb = real_y0 + map_wy - wy0;
        if (yb >= real_y0 && yb < real_y0 + VIEW_H) {
            draw_sprites(s, part, yb, width, m);
        } else {
            memset(s, 0, sizeof(s));
            memset(part, 0, sizeof(part));
        }
        draw_wobj(s, y, width, m, stack_list, stack_count);
    } else {
        draw_sprites(s, part, y, width, m);
    }
    if (vdp.reg[12] & 0x08) {
        shade_line(fb, s, a, b, part, pal, bg, ow);
    } else for (int x = 0; x < ow; x++) {
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
    if (crop) {
        for (int x = 0; x < m; x++)
            fb[x] = VDP_BLACK;
        for (int x = m + width; x < ow; x++)
            fb[x] = VDP_BLACK;
    } else if (clamp_edges) {
        /* the edge colour; a lone different pixel at the very edge (a border's corner)
         * is not repeated across the side area */
        uint16_t l = fb[m], r = fb[m + width - 1];
        if (l != fb[m + 1] && fb[m + 1] == fb[m + 2]) l = fb[m + 1];
        if (r != fb[m + width - 2] && fb[m + width - 2] == fb[m + width - 3]) r = fb[m + width - 2];
        for (int x = 0; x < m; x++)
            fb[x] = l;
        for (int x = m + width; x < ow; x++)
            fb[x] = r;
    }
    for (int x = ow; x < VDP_FB_W; x++)
        fb[x] = VDP_BLACK;
}
