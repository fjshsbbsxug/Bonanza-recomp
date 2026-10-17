/* The picture: drawn once a frame from video memory and the lines' looks (picture.h),
 * with the port's extras (widescreen, the floor above, the players' colours, objects
 * beside the picture, Prop Hunt). Planning decides what each line shows, drawing makes
 * its pixels. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vdp.h"
#include "md.h"
#include "settings.h"
#include "wideobj.h"
#include "prophunt.h"
#include "picture.h"
#include "scene.h"

static int pic_out_w;       /* the width the last picture was drawn at (0: none yet) */
void render_reset(void) { pic_out_w = 0; }


static const pic_look *cur;     /* the look of the line being drawn */
static const uint8_t *V;        /* the picture's video memory */

/* a line's horizontal scroll (the split screen's band routines rewrite the table while
 * the picture is shown, so it is the line's own) */
static int pic_hscroll(int y, int plane_b)
{
    if (y < 0) y = 0;
    if (y > 223) y = 223;
    return pic.band[pic.band_of[y]].look.hs[plane_b];
}

/* pixel encoding in line buffers: bit7 = priority, bits0-5 = palette index, 0 in low nibble = transparent */

static inline int tile_pixel(uint16_t entry, int px, int py)
{
    if (entry & 0x1000) py = 7 - py;
    if (entry & 0x0800) px = 7 - px;
    uint32_t a = ((entry & 0x07FF) << 5) + (py << 2) + (px >> 1);
    uint8_t b = V[a & 0xFFFF];
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
    int pw = sizes[cur->reg[16] & 3], ph = sizes[(cur->reg[16] >> 4) & 3];
    uint32_t name = plane_b ? (cur->reg[4] & 0x07) << 13 : (cur->reg[2] & 0x38) << 10;
    int hs = pic_hscroll(y, plane_b);
    int column_vs = cur->reg[11] & 4;
    if (ovr_hs >= 0) { hs = ovr_hs; column_vs = 0; }
    int wmask = pw * 8 - 1, hmask = ph * 8 - 1;
    int vs = ovr_vs >= 0 ? ovr_vs : cur->vsram[plane_b];
    int last_tile = -1, last_py = -1;
    uint16_t entry = 0;
    for (int x = -m; x < width + m; x++) {
        if (column_vs) {
            int col = x < 0 ? 0 : (x >> 4);
            if (col > (width >> 4) - 1) col = (width >> 4) - 1;
            vs = cur->vsram[col * 2 + plane_b];
        }
        int sx = x;
        if (wrap_margins && (x < 0 || x >= width))
            sx = ((x % width) + width) % width;
        int px = (sx - hs) & wmask;
        int py = (y + vs) & hmask;
        int tile = px >> 3;
        if (tile != last_tile || py != last_py) {
            uint32_t ea = name + (((py >> 3) * pw + tile) << 1);
            entry = (V[ea & 0xFFFF] << 8) | V[(ea + 1) & 0xFFFF];
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
    uint32_t name = (cur->reg[3] & (h40 ? 0x3C : 0x3E)) << 10;
    int ww = h40 ? 64 : 32;
    int vp = (cur->reg[18] & 0x1F) * 8, hp = (cur->reg[17] & 0x1F) * 16;
    int vin = (cur->reg[18] & 0x80) ? (y >= vp) : (y < vp);
    for (int x = -m; x < width + m; x++) {
        /* outside the real screen the window extends its edge column */
        int cx = x < 0 ? 0 : (x >= width ? width - 1 : x);
        int hin = (cur->reg[17] & 0x80) ? (cx >= hp) : (cx < hp);
        in_window[x + m] = (uint8_t)(vin || hin);
        if (!in_window[x + m])
            continue;
        int wx = x & (ww * 8 - 1);
        uint32_t ea = name + (((y >> 3) * ww + (wx >> 3)) << 1);
        uint16_t entry = (V[ea & 0xFFFF] << 8) | V[(ea + 1) & 0xFFFF];
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
    uint8_t b = V[a & 0xFFFF];
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
/* Prop Hunt: the hider (drawn as his disguise instead of himself), the disguise's pieces */
static int ph_hide = -1, ph_blip = -1;   /* ph_blip: the hider's marker on the HUD map (tile) */
static wobj_sprite ph_list[32];
static int ph_count;
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

/* The frame's sprites, in the game's order (its sprite table in video memory, followed
 * link by link): position (the table's, 128 = the screen's edge), size in tiles, first
 * tile, palette, flips, priority (attr). Read once per picture. */
typedef struct { int sx, sy, hsz, vsz; uint16_t attr; } sprite_obj;
static struct { uint32_t gen, sat; int h40, n; sprite_obj o[80]; } sprites;
static uint32_t picture_gen;            /* counts the pictures drawn */

static void sprites_read(uint32_t sat, int h40)
{
    if (sprites.gen == picture_gen && sprites.sat == sat && sprites.h40 == h40)
        return;
    sprites.gen = picture_gen;
    sprites.sat = sat;
    sprites.h40 = h40;
    sprites.n = 0;
    int max_sprites = h40 ? 80 : 64, link = 0, count = 0;
    do {
        const uint8_t *s = &V[(sat + link * 8) & 0xFFFF];
        sprite_obj *o = &sprites.o[sprites.n++];
        o->sy = ((s[0] << 8) | s[1]) & 0x3FF;
        o->vsz = (s[2] & 3) + 1;
        o->hsz = ((s[2] >> 2) & 3) + 1;
        link = s[3] & 0x7F;
        o->attr = (uint16_t)((s[4] << 8) | s[5]);
        o->sx = ((s[6] << 8) | s[7]) & 0x1FF;
    } while (link && ++count < max_sprites);
}

/* The sprites a line shows, as the console picks them: at most 16 (20 at 320 wide) on
 * a line, at most 256 (320) of their pixels from the left of each in the game's order,
 * none after a sprite at x 0 that follows one elsewhere (masking). Each pick: sprite k,
 * its first cols columns. Prop Hunt leaves out the hider's own pieces (his disguise is
 * drawn instead) and, on the seeker's screen, his marker on the HUD map. */
typedef struct { uint8_t k, cols; } sprite_pick;

static int sprites_on_line(int y, int h40, sprite_pick *pick)
{
    int max_line = h40 ? 20 : 16, max_px = h40 ? 320 : 256;
    int n = 0, line_count = 0, pixels = 0, masked = 0, seen_nonzero_x = 0;
    for (int k = 0; k < sprites.n && pixels < max_px; k++) {
        const sprite_obj *o = &sprites.o[k];
        int row = y + 128 - o->sy;
        if (row < 0 || row >= o->vsz * 8)
            continue;
        if (++line_count > max_line)
            break;
        if (o->sx == 0) {
            if (seen_nonzero_x)
                masked = 1;
        } else {
            seen_nonzero_x = 1;
        }
        if (ph_hide >= 0 && char_of(o->attr) >= 0 && wobj_player_of(o->attr) == ph_hide)
            continue;
        if (ph_blip >= 0 && (o->attr & 0x07FF) == ph_blip)
            continue;
        int cols = o->hsz * 8;
        if (cols > max_px - pixels)
            cols = max_px - pixels;
        pixels += cols;
        if (!masked)
            pick[n++] = (sprite_pick){(uint8_t)k, (uint8_t)cols};
    }
    return n;
}

static void draw_sprites(uint8_t *out, uint8_t *part, int y, int width, int m)
{
    int h40 = width == 320;
    sprites_read((cur->reg[5] & (h40 ? 0x7E : 0x7F)) << 9, h40);
    sprite_pick pick[20];
    int npick = sprites_on_line(y, h40, pick);
    int chars = any_char_colors();
    memset(out, 0, width + 2 * m);
    memset(part, 0, width + 2 * m);
    for (int i = 0; i < npick; i++) {
        const sprite_obj *o = &sprites.o[pick[i].k];
        int vsz = o->vsz, hsz = o->hsz, sx = o->sx;
        uint16_t attr = o->attr;
        int row = y + 128 - o->sy;
        /* player colours only for the players' own pieces */
        int ch = chars && char_of(attr) >= 0 ? wobj_player_of(attr) : -1;
        const char_info *ci = ch >= 0 ? char_analyse(attr, hsz, vsz, ch) : NULL;
        int cy = row >> 3, py = row & 7;
        if (attr & 0x1000) { cy = vsz - 1 - cy; py = 7 - py; }
        for (int cx = 0; cx < hsz; cx++) {
            int tcx = (attr & 0x0800) ? hsz - 1 - cx : cx;
            uint16_t tile = (attr & 0xF800) | ((attr + tcx * vsz + cy) & 0x07FF);
            for (int px = 0; px < 8 && cx * 8 + px < pick[i].cols; px++) {
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
    draw_wobj(out, y, width, m, wobj_list, wobj_count);
    draw_wobj(out, y, width, m, ph_list, ph_count);
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

static int margin_for(int width)
{
    int m = width == 320 ? margin40 : margin32;
    if (width + 2 * m > VDP_FB_W)
        m = (VDP_FB_W - width) / 2;
    return m;
}

int vdp_margin(void) { return margin_for(vdp_width()); }

int vdp_out_width(void)
{
    return pic_out_w ? pic_out_w : vdp_width() + 2 * vdp_margin();
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

/* Above the roof: the map rows above the level's top floor - the floor its exit is on
 * (the escape table, 0x8FF2: x, y per stage), or the first floor (5 map rows) that is
 * mostly building, if that is higher. Open air there is sky (levels under an open
 * sky only: the top two map rows nearly empty). */
static int roof_row = -1;
static uint32_t roof_map, roof_meta;
static int roof_w, roof_stage;

static int above_roof(int wx, int wy)
{
    int stage = (md.ram[0xC05A] << 8) | md.ram[0xC05B];
    if (roof_map != level.map || roof_meta != level.meta || roof_w != level.w || roof_stage != stage) {
        roof_map = level.map;
        roof_meta = level.meta;
        roof_w = level.w;
        roof_stage = stage;
        int band = 12;
        for (int b = 0; b < 12 && band == 12; b++)
            for (int r = b * 5; r < b * 5 + 5; r++) {
                int n = 0;
                for (int c = 0; c < level.w; c++) {
                    uint32_t a = level.map + (uint32_t)(r * level.w + c) * 2;
                    if (a + 1 < md.rom_size && ((md.rom[a] << 8) | md.rom[a + 1])) n++;
                }
                if (n * 10 >= level.w * 6) { band = b; break; }
            }
        uint32_t e = 0x8FF2 + 4 * (uint32_t)(stage - 1) + 2;
        if (stage >= 1 && stage <= 13 && e + 1 < md.rom_size) {
            int exit_y = (md.rom[e] << 8) | md.rom[e + 1];
            if (exit_y > 0 && exit_y / 80 < band) band = exit_y / 80;
        }
        /* only under an open sky: the top of the map is (nearly) empty; not in a cave */
        for (int r = 0; r < 2; r++) {
            int n = 0;
            for (int c = 0; c < level.w; c++) {
                uint32_t a = level.map + (uint32_t)(r * level.w + c) * 2;
                if (a + 1 < md.rom_size && ((md.rom[a] << 8) | md.rom[a + 1])) n++;
            }
            if (n * 5 > level.w) band = 0;
        }
        roof_row = band * 5;
    }
    if (wy >= roof_row * 16)
        return 0;
    if (wy < 0 || wx < 0 || wx >= level.w * 16)
        return 1;                              /* outside the map */
    uint32_t m = level.map + (uint32_t)((wy >> 4) * level.w + (wx >> 4)) * 2;
    return m + 1 >= md.rom_size || !((md.rom[m] << 8) | md.rom[m + 1]);   /* a blank map tile */
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
    int pw = sizes[cur->reg[16] & 3], ph = sizes[(cur->reg[16] >> 4) & 3];
    uint32_t name = (cur->reg[4] & 0x07) << 13;     /* the HUD is in plane B */
    int hs = pic_hscroll(HUD_Y, 1), vs = cur->vsram[1];
    for (int k = 0; k < 16; k++) {
        int x = k * width / 16 + 4;
        int px = (x - hs) & (pw * 8 - 1), py = (HUD_Y + vs) & (ph * 8 - 1);
        uint32_t ea = name + (((py >> 3) * pw + (px >> 3)) << 1);
        out[k] = (V[ea & 0xFFFF] << 8) | V[(ea + 1) & 0xFFFF];
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
    int pw = sizes[cur->reg[16] & 3], ph = sizes[(cur->reg[16] >> 4) & 3];
    uint32_t name = (cur->reg[2] & 0x38) << 10;
    int hs = pic_hscroll(y, 0), vs = cur->vsram[0];  /* (the scroll of line y, the rest of the drawn line's) */
    int good = 0, tested = 0, inside = 0, inside_good = 0;
    for (int x = 4; x < width; x += 16) {
        uint16_t me = map_entry(camx + x, wy);
        int px = (x - hs) & (pw * 8 - 1), py = (y + vs) & (ph * 8 - 1);
        uint32_t ea = name + (((py >> 3) * pw + (px >> 3)) << 1);
        uint16_t ve = (V[ea & 0xFFFF] << 8) | V[(ea + 1) & 0xFFFF];
        tested++;
        if (me == 0xFFFF) {
            /* outside the building: the game shows an empty tile there */
            const uint8_t *t = &V[(ve & 0x07FF) << 5];
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

/* ---- the frame's plan: what each line shows ----
 * Planning goes down the picture making every decision (which views are shown, where
 * their sides come from, the floor above, the stacked floors, the HUD, the cutscene
 * crop) - some of them by looking at what a layer would show; drawing then makes the
 * pixels from the plan alone. */
enum { EDGE_KEEP, EDGE_CROP, EDGE_CLAMP };
enum { SPR_NONE, SPR_LINE };
typedef struct {
    const pic_look *look;
    int width, m, fb_row;
    uint8_t off;                    /* the display is off on this line */
    /* plane A: from video memory, its sides (or all of it) from the level map */
    uint8_t a_map, a_map_all;
    int a_camx, a_wy;
    uint8_t b_wrap;                 /* plane B: the 4:3 picture's columns repeated */
    /* the floor above (room_above): plane B scrolled as the bottom view, plane A from
     * the map at that camera, the sky above the roof lined up with the bottom view */
    uint8_t room_above;
    int ra_hs, ra_vs, ra_camx, ra_wy, sky_hs, sky_vs;
    /* sprites: those of line spr_y (another line for the floor above / stacked views) */
    uint8_t spr;
    int spr_y;
    uint8_t stacked;                /* + the stacked floor's objects */
    uint8_t edges;
} line_plan;
static line_plan plan[224];

/* A view on lines l0 .. l0+79 showing the level from world y vtop, while player 1's
 * camera (the game's sprites) starts at world y cam_wy: the objects for the lines his
 * camera does not cover (the stack list) */
static void shifted_pieces(int l0, int vtop, int cam_wy)
{
    int real_y0 = focus ? VIEW_TOP_Y0 : VIEW_BOT_Y0;
    int cov0 = l0 + cam_wy - vtop;          /* the lines his camera covers: cov0 .. +79 */
    int u0 = l0, u1 = l0 + VIEW_H;
    if (cov0 > l0) { if (cov0 < u1) u1 = cov0; }
    else u0 = cov0 + VIEW_H > l0 ? cov0 + VIEW_H : l0;
    stack_count = wobj_build_shifted(focus, l0 - real_y0 + cam_wy - vtop, l0, u0, u1, stack_list,
                                     stack_count, (int)(sizeof(stack_list) / sizeof(stack_list[0])));
}

static void plan_line(int y, line_plan *P);
static void draw_line(int y, const line_plan *P);

/* ---- the scene (scene.h): the plans as plain tables, for the graphics card ---- */
render_scene scene;
int scene_wanted, scene_cpu_too, scene_cpu_drew;

static uint16_t map_cells[1024 * 128];
static uint8_t roof_cells[512 * 64];
static struct { uint32_t map, meta; int w, stage; } map_key;

/* the level map's cells, once per level (render.c's map_entry / above_roof, tabled) */
static void scene_map(void)
{
    int stage = (md.ram[0xC05A] << 8) | md.ram[0xC05B];
    if (map_key.map == level.map && map_key.meta == level.meta && map_key.w == level.w && map_key.stage == stage)
        return;
    map_key.map = level.map; map_key.meta = level.meta; map_key.w = level.w; map_key.stage = stage;
    scene.map_gen++;
    int w = level.w;
    for (int cy = 0; cy < 128; cy++)
        for (int cx = 0; cx < w * 2; cx++)
            map_cells[cy * w * 2 + cx] = map_entry(cx * 8, cy * 8);
    above_roof(0, -1);                      /* (works out roof_row for this level) */
    for (int cy = 0; cy < 64; cy++)
        for (int cx = 0; cx < w; cx++)
            roof_cells[cy * w + cx] = (uint8_t)above_roof(cx * 16, cy * 16);
}

static void scene_pieces(scene_line *L, int list, const wobj_sprite *ws, int count, int y)
{
    for (int i = 0; i < count; i++) {
        const wobj_sprite *w = &ws[i];
        int row = y - w->y;
        if (y < w->ymin || y >= w->ymax || row < 0 || row >= w->vsz * 8)
            continue;
        if (L->nwobj == SCENE_WOBJ) { scene.valid = 0; return; }
        L->wobj[L->nwobj++] = (uint16_t)(list << 10 | i);
    }
}

static void scene_list(int list, const wobj_sprite *ws, int count)
{
    scene.npieces[list] = count;
    for (int i = 0; i < count; i++) {
        scene_piece *p = &scene.piece[list][i];
        p->x = (int16_t)ws[i].x; p->y = (int16_t)ws[i].y;
        p->hsz = (int16_t)ws[i].hsz; p->vsz = (int16_t)ws[i].vsz;
        p->ymin = (int16_t)ws[i].ymin; p->ymax = (int16_t)ws[i].ymax;
        p->attr = ws[i].attr;
        p->only_outside = (uint8_t)ws[i].only_outside;
    }
}

static void scene_fill(void)
{
    /* the frame's logic after the picture changes video memory: keep it as drawn */
    static uint8_t vram[0x10000];
    memcpy(vram, V, sizeof(vram));
    scene.valid = 1;
    scene.vram = vram;
    scene.out_w = pic_out_w;
    uint32_t sat0 = 0xFFFFFFFFu;
    int h400 = -1, map_used = 0;
    for (int y = 0; y < 224; y++) {
        const line_plan *P = &plan[y];
        scene.src_line[P->fb_row] = (int16_t)y;
        scene_line *L = &scene.line[y];
        const pic_look *k = P->look;
        memset(L, 0, sizeof(*L));
        L->width = (int16_t)P->width;
        L->m = (int16_t)P->m;
        for (int i = 0; i < 64; i++)
            L->pal[i] = md_color_index(k->cram[i]);
        L->bg = L->pal[k->reg[7] & 0x3F];
        if (P->off) { L->flags = SL_OFF; continue; }
        static const int sizes[4] = {32, 64, 32, 128};
        int h40 = P->width == 320;
        L->flags = (P->a_map ? SL_A_MAP : 0) | (P->a_map_all ? SL_A_MAP_ALL : 0) | (P->b_wrap ? SL_B_WRAP : 0) |
                   (P->room_above ? SL_ROOM_ABOVE : 0) | (P->spr == SPR_LINE ? SL_SPRITES : 0) |
                   (P->stacked ? SL_STACKED : 0) | (P->edges == EDGE_CROP ? SL_CROP : 0) |
                   (P->edges == EDGE_CLAMP ? SL_CLAMP : 0) | (k->reg[12] & 0x08 ? SL_SHADE : 0) |
                   (k->reg[11] & 4 ? SL_COLUMN_VS : 0) | ((k->reg[17] & 0x9F) || (k->reg[18] & 0x9F) ? SL_WINDOW : 0);
        L->a_camx = (int16_t)P->a_camx; L->a_wy = (int16_t)P->a_wy;
        L->ra_hs = (int16_t)P->ra_hs; L->ra_vs = (int16_t)P->ra_vs;
        L->ra_camx = (int16_t)P->ra_camx; L->ra_wy = (int16_t)P->ra_wy;
        L->sky_hs = (int16_t)P->sky_hs; L->sky_vs = (int16_t)P->sky_vs;
        L->spr_y = (int16_t)P->spr_y;
        L->hs[0] = (int16_t)pic_hscroll(y, 0);
        L->hs[1] = (int16_t)pic_hscroll(y, 1);
        L->pw = (int16_t)sizes[k->reg[16] & 3];
        L->ph = (int16_t)sizes[(k->reg[16] >> 4) & 3];
        L->name_a = (uint16_t)((k->reg[2] & 0x38) << 10);
        L->name_b = (uint16_t)((k->reg[4] & 0x07) << 13);
        L->name_w = (uint16_t)((k->reg[3] & (h40 ? 0x3C : 0x3E)) << 10);
        L->reg17 = k->reg[17];
        L->reg18 = k->reg[18];
        memcpy(L->vsram, k->vsram, sizeof(L->vsram));
        if (P->a_map || P->room_above) map_used = 1;
        if (P->spr == SPR_LINE) {
            uint32_t sat = (k->reg[5] & (h40 ? 0x7E : 0x7F)) << 9;
            if (sat0 == 0xFFFFFFFFu) { sat0 = sat; h400 = h40; }
            if (sat != sat0 || h40 != h400) { scene.valid = 0; return; }   /* one sprite table a picture */
            cur = k;
            sprites_read(sat, h40);
            sprite_pick pick[20];
            int n = sprites_on_line(P->spr_y, h40, pick);
            L->npick = (uint8_t)n;
            for (int i = 0; i < n; i++)
                L->pick[i] = (uint16_t)(pick[i].k | pick[i].cols << 8);
            scene_pieces(L, SCENE_WOBJ_LIST, wobj_list, wobj_count, P->spr_y);
            scene_pieces(L, SCENE_PH_LIST, ph_list, ph_count, P->spr_y);
        }
        if (P->stacked)
            scene_pieces(L, SCENE_STACK_LIST, stack_list, stack_count, y);
        if (!scene.valid) return;
    }
    /* the sprites, with what player colours need of each */
    scene.nsprites = sat0 == 0xFFFFFFFFu ? 0 : sprites.n;
    int chars = any_char_colors();
    for (int i = 0; i < scene.nsprites; i++) {
        const sprite_obj *o = &sprites.o[i];
        scene_sprite *d = &scene.sprite[i];
        d->sx = (int16_t)o->sx; d->sy = (int16_t)o->sy;
        d->hsz = (int16_t)o->hsz; d->vsz = (int16_t)o->vsz;
        d->attr = o->attr;
        int ch = chars && char_of(o->attr) >= 0 ? wobj_player_of(o->attr) : -1;
        d->ch = (int8_t)ch;
        if (ch >= 0) {
            const char_info *ci = char_analyse(o->attr, o->hsz, o->vsz, ch);
            d->shirt_end = ci->shirt_end;
            d->pants_end = ci->pants_end;
            memcpy(d->body, ci->body, sizeof(d->body));
        }
    }
    scene_list(SCENE_WOBJ_LIST, wobj_list, wobj_count);
    scene_list(SCENE_PH_LIST, ph_list, ph_count);
    scene_list(SCENE_STACK_LIST, stack_list, stack_count);
    for (int c = 0; c < 2; c++)
        for (int p = 0; p < 3; p++)
            scene.char_on[c][p] = (uint8_t)(settings.char_on[c][p] != 0);
    scene.map_w = 0;
    if (map_used && level.w) {
        scene_map();
        scene.map_w = level.w;
        scene.roof_row = roof_row;
        scene.map_cells = map_cells;
        scene.roof_cells = roof_cells;
    }
}

/* the picture, top to bottom (the port's extras carry decisions from line to line) */
/* The views' cameras in this picture, before its lines are planned: the floor above
 * (lines 7-86) comes before player 1's view (135-214) and must follow this picture's
 * camera, not the last one's (when it moves, the two would not line up) */
static void view_prime(void)
{
    if (!level_load())
        return;
    for (int vi = 0; vi < 2; vi++) {
        if (view_rec[vi].frame != md.frame)    /* not a view of the last picture */
            continue;
        int y0 = vi ? VIEW_TOP_Y0 : VIEW_BOT_Y0;
        const pic_look *k = &pic.band[pic.band_of[y0]].look;
        int camx, wy0;
        view_camera(vi, y0, k->hs[0], k->vsram[0], &camx, &wy0);
        view_rec[vi].hsA = camx;
        view_rec[vi].vsA = wy0 + views[vi].dwy;
        view_rec[vi].hsB = k->hs[1];
        view_rec[vi].vsB = k->vsram[1];
    }
}

void vdp_render_frame(void)
{
    V = vdp.vram;
    picture_gen++;
    pic_out_w = 0;
    stack_count = 0;
    view_prime();
    for (int b = 0; b < pic.n; b++) {
        cur = &pic.band[b].look;
        int width = (cur->reg[12] & 0x01) ? 320 : 256;
        if (width + 2 * margin_for(width) > pic_out_w)
            pic_out_w = width + 2 * margin_for(width);
        for (int y = pic.band[b].y0; y < pic.band[b].y1; y++)
            plan_line(y, &plan[y]);
    }
    scene.valid = 0;
    if (scene_wanted)
        scene_fill();
    scene_cpu_drew = !scene.valid || scene_cpu_too;
    if (scene_cpu_drew)
        for (int y = 0; y < 224; y++)
            draw_line(y, &plan[y]);
}

/* the horizontal scroll in the table in video memory, as it is now */


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


static void plan_line(int y, line_plan *P)
{
    int width = (cur->reg[12] & 0x01) ? 320 : 256;
    int m = margin_for(width);
    memset(P, 0, sizeof(*P));
    P->look = cur;
    P->width = width;
    P->m = m;
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
    P->fb_row = in_real ? VIEW_BOT_Y0 + y - real_y0 : in_other ? VIEW_TOP_Y0 + y - other_y0 : y;
    if (!(cur->reg[1] & 0x40)) {
        P->off = 1;
        return;
    }
    uint8_t a[VDP_FB_W];
    /* Widescreen during play: the side areas of each view are drawn from the level map,
     * the backdrop is repeated, the HUD rows extend their edges. In a 1-player game the
     * top view shows the floor above player 1 (also from the map). */
    int in_top = y >= VIEW_TOP_Y0 && y < VIEW_TOP_Y0 + VIEW_H;
    int in_bot = y >= VIEW_BOT_Y0 && y < VIEW_BOT_Y0 + VIEW_H;
    int in_view = in_top || in_bot;
    int playing = views[0].ok && (views[0].frame == md.frame || views[0].frame == md.frame + 1);
    if (y == 0 && !level_load())
        views[0].ok = views[1].ok = 0;
    if (y == 0) {                         /* Prop Hunt: the hider's disguise */
        uint16_t f, at;
        ph_hide = ph_hidden_player();
        ph_blip = ph_hide >= 0 && vdp_local_player >= 0 && vdp_local_player != ph_hide ? 0x510 + ph_hide : -1;
        ph_count = ph_hide >= 0 && ph_disguise(&f, &at)
                 ? wobj_build_disguise(ph_hide, f, at, ph_list, (int)(sizeof(ph_list) / sizeof(ph_list[0]))) : 0;
    }
    if (any_char_colors() || ph_hide >= 0) wobj_sample_players();
    if (y == 0)   /* objects beside the picture (also keeps its snapshots up to date) */
        wobj_count = wobj_build(m && playing ? m : 0, width, settings.patrol_return && !settings.floor_alert,
                                wobj_list, (int)(sizeof(wobj_list) / sizeof(wobj_list[0])));
    if (y == 0 && wobj_count) {
        /* drop pieces the game's own sprite table already shows */
        int h40 = width == 320, n = 0;
        uint32_t sat = (cur->reg[5] & (h40 ? 0x7E : 0x7F)) << 9;
        for (int i = 0; i < wobj_count; i++) {
            const wobj_sprite *w = &wobj_list[i];
            int dup = 0, link = 0, count = 0;
            do {
                const uint8_t *e = &V[(sat + link * 8) & 0xFFFF];
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
        view_camera(vi, in_bot ? VIEW_BOT_Y0 : VIEW_TOP_Y0, pic_hscroll(y, 0), cur->vsram[0], &camx, &wy0);
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
                fprintf(stderr, "top %u hs %d vs %d\n", md.frame, pic_hscroll(y, 0), cur->vsram[0]);
        }
        if (y == (in_bot ? VIEW_BOT_Y0 : VIEW_TOP_Y0)) {
            view_rec[vi].frame = md.frame + 1;
            view_rec[vi].hsB = pic_hscroll(y, 1);
            view_rec[vi].vsB = cur->vsram[1];
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
    P->a_map = m && map_view;
    P->a_camx = camx;
    P->a_wy = wy0 + y - (in_bot ? VIEW_BOT_Y0 : VIEW_TOP_Y0);
    if (stacked) {
        P->a_map = P->a_map_all = 1;
        P->a_wy = stacked_top + VIEW_H + (y - real_y0);
    }
    map_on = P->a_map; map_all = P->a_map_all; map_camx = P->a_camx; map_wy = P->a_wy;
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
    if (room_above) {
        /* The floor above player 1, following his camera - but never above the top of
         * the level (the exit floor / roof): from there on it stays still, and as it
         * then shows player 1's own floor, he is seen in it (escaping, too). */
        above_roof(0, -1);                    /* (works out roof_row for this level) */
        int top = roof_row > 0 ? roof_row * 16 : 0;
        room_above_wy = view_rec[focus].vsA - VIEW_H;
        if (room_above_wy < top) {
            room_above_wy = view_rec[focus].vsA < top ? view_rec[focus].vsA : top;
            stacked_frame = md.frame + 1;     /* player 1's view: the floor below */
            stacked_top = room_above_wy;
        }
    }
    P->b_wrap = m && playing && in_view && (room_view || map_view);
    if (room_above) {
        /* the floor above player 1, straight from the level map, following his camera
         * (plane B: the same sky/city background as below); open air above the roof:
         * the background lined up with player 1's own view, as it lies in the world, and
         * repeated upward above it (a view's height at a time) */
        P->room_above = 1;
        P->ra_hs = view_rec[focus].hsB;
        P->ra_vs = (view_rec[focus].vsB + real_y0 - other_y0) & 0x3FF;
        P->ra_camx = view_rec[focus].hsA;
        P->ra_wy = room_above_wy + (y - other_y0);
        int line = P->ra_wy - view_rec[focus].vsA;
        line = ((line % VIEW_H) + VIEW_H) % VIEW_H;
        P->sky_hs = view_rec[focus].hsB;
        P->sky_vs = (view_rec[focus].vsB + real_y0 + line - y) & 0x3FF;
    }
    /* a view whose map could not be matched: rather than whatever video memory holds
     * beside the picture, extend its edges */
    if (m && playing && in_view && !map_view && !room_above)
        clamp_edges = 1;
    P->spr = SPR_LINE;
    P->spr_y = y;
    if (room_above) {
        /* The 2P panel's sprites do not belong to the room. Where the top view shows
         * the same part of the level as player 1's view (on the top floor, or on the
         * stairs) it shows the same characters: the sprites of that line of his view. */
        int yb = real_y0 + room_above_wy + (y - other_y0) - view_rec[focus].vsA;
        if (yb >= real_y0 && yb < real_y0 + VIEW_H)
            P->spr_y = yb;
        else
            P->spr = SPR_NONE;
        /* where his camera does not reach, the characters of that floor come from the
         * objects - also the part of one that stands across the edge of his view */
        if (y == other_y0)
            shifted_pieces(other_y0, room_above_wy, view_rec[focus].vsA);
        P->stacked = 1;
    } else if (stacked) {
        /* the characters of player 1's view that are on the floor shown here: the game's
         * sprites where his camera covers it, the rest from the objects */
        if (y == real_y0)
            shifted_pieces(real_y0, stacked_top + VIEW_H, wy0);
        P->stacked = 1;
        int yb = real_y0 + P->a_wy - wy0;
        if (yb >= real_y0 && yb < real_y0 + VIEW_H)
            P->spr_y = yb;
        else
            P->spr = SPR_NONE;
    }
    P->edges = crop ? EDGE_CROP : clamp_edges ? EDGE_CLAMP : EDGE_KEEP;
}

/* a line's pixels, from its plan */
static void draw_line(int y, const line_plan *P)
{
    cur = P->look;
    int width = P->width, m = P->m, ow = width + 2 * m;
    uint16_t *fb = vdp.fbmd[P->fb_row];
    uint16_t pal[64];
    for (int i = 0; i < 64; i++)
        pal[i] = md_color_index(cur->cram[i]);
    uint16_t bg = pal[cur->reg[7] & 0x3F];
    if (P->off) {
        for (int x = 0; x < VDP_FB_W; x++)
            fb[x] = bg;
        return;
    }
    uint8_t a[VDP_FB_W], b[VDP_FB_W], s[VDP_FB_W], win[VDP_FB_W], wbuf[VDP_FB_W], part[VDP_FB_W];
    if (!P->room_above) {
        map_on = P->a_map; map_all = P->a_map_all; map_camx = P->a_camx; map_wy = P->a_wy;
        draw_plane(a, y, width, m, 0);
        map_on = map_all = 0;
        wrap_margins = P->b_wrap;
        draw_plane(b, y, width, m, 1);
        wrap_margins = 0;
    } else {
        ovr_hs = P->ra_hs;
        ovr_vs = P->ra_vs;
        wrap_margins = m != 0;
        draw_plane(b, y, width, m, 1);
        wrap_margins = 0;
        ovr_hs = ovr_vs = -1;
        map_on = map_all = 1;
        map_camx = P->ra_camx;
        map_wy = P->ra_wy;
        draw_plane(a, y, width, m, 0);
        map_on = map_all = 0;
        uint8_t sky[VDP_FB_W];
        ovr_hs = P->sky_hs;
        ovr_vs = P->sky_vs;
        wrap_margins = m != 0;
        draw_plane(sky, y, width, m, 1);
        wrap_margins = 0;
        ovr_hs = ovr_vs = -1;
        for (int x = -m; x < width + m; x++)
            if (above_roof(P->ra_camx + x, P->ra_wy))
                b[x + m] = sky[x + m];
    }
    /* the window covers nothing when HP=0 && !RIGT and VP=0 && !DOWN */
    if ((cur->reg[17] & 0x9F) || (cur->reg[18] & 0x9F)) {
        draw_window(wbuf, y, width, m, win);
        for (int x = 0; x < ow; x++)
            if (win[x])
                a[x] = wbuf[x];
    }
    if (P->spr == SPR_LINE) {
        draw_sprites(s, part, P->spr_y, width, m);
    } else {
        memset(s, 0, sizeof(s));
        memset(part, 0, sizeof(part));
    }
    if (P->stacked)
        draw_wobj(s, y, width, m, stack_list, stack_count);
    if (cur->reg[12] & 0x08) {
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
    if (P->edges == EDGE_CROP) {
        for (int x = 0; x < m; x++)
            fb[x] = VDP_BLACK;
        for (int x = m + width; x < ow; x++)
            fb[x] = VDP_BLACK;
    } else if (P->edges == EDGE_CLAMP) {
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
