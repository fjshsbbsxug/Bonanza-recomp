/* In-game options menu. */
#include <stdio.h>
#include <string.h>
#include "menu.h"
#include "md.h"
#include "settings.h"
#include "font8x8_basic.h"

int menu_allow_quit = 1;
int menu_settings_dirty;

enum { PAGE_MAIN, PAGE_RECOLOR, PAGE_EDIT, PAGE_CHARS };

static int open_, page, sel;
static int recolor_sel;            /* index into colors[] on the recolour page */
static uint16_t colors[64];        /* colours visible in the current picture, most used first */
static int ncolors;
static uint16_t edit_md;           /* colour being edited (recolour) */
static int edit_char = -1, edit_part; /* or: character part being edited */

/* original Mega Drive colours of the character parts (palette line 0) */
static const uint16_t char_orig_md[CHAR_COUNT][PART_COUNT] = {
    {5, 0, 7 | (4 << 3)},          /* Robo: red 500, black pants, skin 740 */
    {5 << 6, 0, 7 | (4 << 3)},     /* Mobo: blue 005, black pants, skin 740 */
};
static const uint32_t cloth_presets[] = {
    0x101010, 0xF0F0F0, 0x808080, 0xE02020, 0xF08020, 0xF0D020, 0x20B040, 0x20B0B0,
    0x2050E0, 0x8030C0, 0xF070B0, 0x7A4A20, 0x203060,
};
static const uint32_t skin_presets[] = {
    0xFFE0C8, 0xF1C27D, 0xE0AC69, 0xC68642, 0x8D5524, 0x6B4226, 0x3B2219, 0x60C050, 0x70A0F0,
};

/* ------------------------------------------------------------ drawing */

#define PANEL_X 12
#define PANEL_Y 8
#define PANEL_W (MENU_W - 24)
#define PANEL_H (MENU_H - 16)
#define ROW_Y0 34
#define ROW_H 10
#define VALUE_X 150

#define C_TEXT   0xFFF2F0F7u
#define C_DIM    0xFF9C98AAu
#define C_ACCENT 0xFFFF4F5Eu
#define C_GOLD   0xFFF2B632u
#define C_PANEL  0xE0141320u
#define C_SEL    0xFF3A2030u

static void fill(uint32_t *buf, int x, int y, int w, int h, uint32_t c)
{
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= MENU_H) continue;
        for (int i = x; i < x + w; i++)
            if (i >= 0 && i < MENU_W)
                buf[j * MENU_W + i] = c;
    }
}

static void frame(uint32_t *buf, int x, int y, int w, int h, uint32_t c)
{
    fill(buf, x, y, w, 1, c);
    fill(buf, x, y + h - 1, w, 1, c);
    fill(buf, x, y, 1, h, c);
    fill(buf, x + w - 1, y, 1, h, c);
}

void menu_text(uint32_t *buf, int w, int h, int x, int y, const char *s, uint32_t color)
{
    for (; *s; s++, x += 8) {
        unsigned char ch = (unsigned char)*s;
        if (ch >= 128) ch = '?';
        for (int r = 0; r < 8; r++) {
            unsigned char bits = font8x8_basic[ch][r];
            for (int c = 0; c < 8; c++)
                if (bits & (1 << c)) {
                    int px = x + c, py = y + r;
                    if (px >= 0 && px < w && py >= 0 && py < h)
                        buf[py * w + px] = color;
                }
        }
    }
}

static void text(uint32_t *buf, int x, int y, const char *s, uint32_t c) { menu_text(buf, MENU_W, MENU_H, x, y, s, c); }

static void slider(uint32_t *buf, int x, int y, int v, int lo, int hi, int selected)
{
    int w = 96;
    fill(buf, x, y + 3, w, 2, C_DIM);
    int k = (v - lo) * (w - 4) / (hi - lo);
    fill(buf, x + k, y, 4, 8, selected ? C_ACCENT : C_TEXT);
    char t[16];
    snprintf(t, sizeof(t), "%+d", v);
    text(buf, x + w + 6, y, v ? t : "0", C_DIM);
}

/* ------------------------------------------------------------ items */

enum {
    IT_RESUME, IT_SCREEN, IT_UPPER, IT_CUTSCENES, IT_GUARDS, IT_ALERT, IT_SCALING, IT_COLORS, IT_BRIGHT, IT_CONTRAST, IT_SAT, IT_HUE,
    IT_CHARS, IT_RECOLOR, IT_RESETCOLORS, IT_RESETGAME, IT_QUIT, IT_COUNT
};
static const char *item_label[IT_COUNT] = {
    "Resume", "Screen", "Top (1P)", "Cutscenes", "Patrol Return", "Floor Alert", "Scaling", "Colors", "Brightness", "Contrast", "Saturation", "Hue",
    "Characters...", "Recolor...", "Reset all colors", "Reset game", "Quit",
};

static int main_count(void) { return menu_allow_quit ? IT_COUNT : IT_COUNT - 1; }

static void changed(void)
{
    settings_apply();
    menu_settings_dirty = 1;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void collect_colors(void)
{
    static uint32_t count[512];
    memset(count, 0, sizeof(count));
    const uint16_t *fb = md_framebuffer_md();
    int w = md_screen_width();
    for (int y = 0; y < MD_SCREEN_H; y++)
        for (int x = 0; x < w; x++) {
            uint16_t c = fb[y * MD_FB_W + x];
            if (c < 512) count[c]++;
        }
    /* already recoloured colours first so they can be found again, then by use */
    ncolors = 0;
    for (int i = 0; i < settings.nrecolor && ncolors < 64; i++)
        if (count[settings.recolor[i].md]) {
            colors[ncolors++] = settings.recolor[i].md;
            count[settings.recolor[i].md] = 0;
        }
    while (ncolors < 64) {
        int best = -1;
        for (int c = 0; c < 512; c++)
            if (count[c] && (best < 0 || count[c] > count[best]))
                best = c;
        if (best < 0) break;
        colors[ncolors++] = (uint16_t)best;
        count[best] = 0;
    }
    if (recolor_sel >= ncolors) recolor_sel = ncolors ? ncolors - 1 : 0;
}

static uint32_t char_rgb(int c, int p)
{
    return settings.char_on[c][p] ? settings.char_rgb[c][p] : (settings_base_color(char_orig_md[c][p]) & 0xFFFFFF);
}

static void char_cycle(int c, int p, int dir)
{
    const uint32_t *list = p == PART_SKIN ? skin_presets : cloth_presets;
    int n = p == PART_SKIN ? (int)(sizeof(skin_presets) / 4) : (int)(sizeof(cloth_presets) / 4);
    int cur = -1; /* -1 = original */
    if (settings.char_on[c][p])
        for (int i = 0; i < n; i++)
            if (list[i] == settings.char_rgb[c][p]) cur = i;
    cur += dir;
    if (cur < -1) cur = n - 1;
    if (cur >= n) cur = -1;
    if (cur < 0) settings.char_on[c][p] = 0;
    else { settings.char_on[c][p] = 1; settings.char_rgb[c][p] = list[cur]; }
}

static uint32_t current_rgb(uint16_t md)
{
    if (edit_char >= 0 && page == PAGE_EDIT)
        return char_rgb(edit_char, edit_part);
    int i = settings_find_recolor(md);
    return i >= 0 ? settings.recolor[i].rgb : (settings_base_color(md) & 0xFFFFFF);
}

static int adjust_main(int item, int dir)
{
    switch (item) {
    case IT_SCREEN: settings.screen = (settings.screen + dir + SCREEN_COUNT) % SCREEN_COUNT; break;
    case IT_SCALING: settings.smooth = !settings.smooth; break;
    case IT_UPPER: settings.show_2p_panel = !settings.show_2p_panel; break;
    case IT_CUTSCENES: settings.crop_cutscenes = !settings.crop_cutscenes; break;
    case IT_GUARDS: settings.patrol_return = !settings.patrol_return; break;
    case IT_ALERT: settings.floor_alert = (settings.floor_alert + dir + FLOOR_ALERT_COUNT) % FLOOR_ALERT_COUNT; break;
    case IT_COLORS: settings.preset = (settings.preset + dir + PRESET_COUNT) % PRESET_COUNT; break;
    case IT_BRIGHT: settings.brightness = clampi(settings.brightness + dir * 5, -100, 100); break;
    case IT_CONTRAST: settings.contrast = clampi(settings.contrast + dir * 5, -100, 100); break;
    case IT_SAT: settings.saturation = clampi(settings.saturation + dir * 5, -100, 100); break;
    case IT_HUE: settings.hue = clampi(settings.hue + dir * 10, -180, 180); break;
    default: return 0;
    }
    changed();
    return 1;
}

static int activate_main(int item)
{
    switch (item) {
    case IT_RESUME: open_ = 0; return MENU_ACT_CLOSE;
    case IT_RECOLOR: page = PAGE_RECOLOR; collect_colors(); return MENU_ACT_NONE;
    case IT_CHARS: page = PAGE_CHARS; sel = 0; return MENU_ACT_NONE;
    case IT_RESETCOLORS: {
        int screen = settings.screen, smooth = settings.smooth;
        /* keeps character colours; they have their own reset */
        int on[CHAR_COUNT][PART_COUNT];
        uint32_t rgb[CHAR_COUNT][PART_COUNT];
        memcpy(on, settings.char_on, sizeof(on));
        memcpy(rgb, settings.char_rgb, sizeof(rgb));
        settings_defaults();
        settings.screen = screen;
        settings.smooth = smooth;
        memcpy(settings.char_on, on, sizeof(on));
        memcpy(settings.char_rgb, rgb, sizeof(rgb));
        changed();
        return MENU_ACT_NONE;
    }
    case IT_RESETGAME: open_ = 0; return MENU_ACT_RESET;
    case IT_QUIT: return MENU_ACT_QUIT;
    default: adjust_main(item, 1); return MENU_ACT_NONE;
    }
}

/* edit page rows: R, G, B, reset, done */
static void edit_change(int row, int dir)
{
    uint32_t rgb = current_rgb(edit_md);
    int ch[3] = {(int)(rgb >> 16) & 255, (int)(rgb >> 8) & 255, (int)rgb & 255};
    if (row > 2) return;
    ch[row] = clampi(ch[row] + dir * 15, 0, 255);
    uint32_t v = ((uint32_t)ch[0] << 16) | ((uint32_t)ch[1] << 8) | (uint32_t)ch[2];
    if (edit_char >= 0) {
        settings.char_on[edit_char][edit_part] = 1;
        settings.char_rgb[edit_char][edit_part] = v;
    } else {
        settings_set_recolor(edit_md, v);
    }
    changed();
}

/* ------------------------------------------------------------ API */

int menu_is_open(void) { return open_; }

void menu_open(void)
{
    open_ = 1;
    page = PAGE_MAIN;
    sel = 0;
    menu_settings_dirty = 0;
}

int menu_highlight(void)
{
    if (!open_) return -1;
    if (page == PAGE_CHARS || (page == PAGE_EDIT && edit_char >= 0)) return -1;
    if (page == PAGE_RECOLOR && ncolors) return colors[recolor_sel];
    if (page == PAGE_EDIT) return edit_md;
    return -1;
}

static int back(void)
{
    if (page == PAGE_EDIT && edit_char >= 0) {
        page = PAGE_CHARS;
        sel = edit_char * PART_COUNT + edit_part;
        edit_char = -1;
        return MENU_ACT_NONE;
    }
    if (page == PAGE_CHARS) { page = PAGE_MAIN; sel = IT_CHARS; return MENU_ACT_NONE; }
    if (page == PAGE_EDIT) { page = PAGE_RECOLOR; collect_colors(); return MENU_ACT_NONE; }
    if (page == PAGE_RECOLOR) { page = PAGE_MAIN; sel = IT_RECOLOR; return MENU_ACT_NONE; }
    open_ = 0;
    return MENU_ACT_CLOSE;
}

int menu_key(int key)
{
    if (!open_) return MENU_ACT_NONE;
    if (key == MK_BACK) return back();
    if (page == PAGE_MAIN) {
        int n = main_count();
        if (key == MK_UP) sel = (sel + n - 1) % n;
        else if (key == MK_DOWN) sel = (sel + 1) % n;
        else if (key == MK_LEFT) adjust_main(sel, -1);
        else if (key == MK_RIGHT) adjust_main(sel, 1);
        else if (key == MK_OK) return activate_main(sel);
    } else if (page == PAGE_RECOLOR) {
        /* grid of 8 columns, then a "Back" row */
        int n = ncolors;
        if (key == MK_LEFT && recolor_sel > 0) recolor_sel--;
        else if (key == MK_RIGHT && recolor_sel < n - 1) recolor_sel++;
        else if (key == MK_UP && recolor_sel >= 8) recolor_sel -= 8;
        else if (key == MK_DOWN && recolor_sel + 8 < n) recolor_sel += 8;
        else if (key == MK_OK && n) {
            edit_md = colors[recolor_sel];
            edit_char = -1;
            page = PAGE_EDIT;
            sel = 0;
        }
    } else if (page == PAGE_CHARS) {
        /* rows: 6 parts, reset, back */
        int n = CHAR_COUNT * PART_COUNT + 2;
        if (key == MK_UP) sel = (sel + n - 1) % n;
        else if (key == MK_DOWN) sel = (sel + 1) % n;
        else if (sel < CHAR_COUNT * PART_COUNT && (key == MK_LEFT || key == MK_RIGHT)) {
            char_cycle(sel / PART_COUNT, sel % PART_COUNT, key == MK_LEFT ? -1 : 1);
            changed();
        } else if (key == MK_OK) {
            if (sel < CHAR_COUNT * PART_COUNT) {
                edit_char = sel / PART_COUNT;
                edit_part = sel % PART_COUNT;
                page = PAGE_EDIT;
                sel = 0;
            } else if (sel == CHAR_COUNT * PART_COUNT) {
                memset(settings.char_on, 0, sizeof(settings.char_on));
                changed();
            } else {
                return back();
            }
        }
    } else {
        if (key == MK_UP) sel = (sel + 4) % 5;
        else if (key == MK_DOWN) sel = (sel + 1) % 5;
        else if (key == MK_LEFT) edit_change(sel, -1);
        else if (key == MK_RIGHT) edit_change(sel, 1);
        else if (key == MK_OK) {
            if (sel == 3) {
                if (edit_char >= 0) settings.char_on[edit_char][edit_part] = 0;
                else settings_clear_recolor(edit_md);
                changed();
            }
            else if (sel == 4) return back();
            else edit_change(sel, 1);
        }
    }
    return MENU_ACT_NONE;
}

#define SW_X0 28
#define SW_Y0 40
#define SW_W 32
#define SW_H 18

int menu_pointer(int x, int y)
{
    if (!open_) return MENU_ACT_NONE;
    if (x < PANEL_X || x >= PANEL_X + PANEL_W || y < PANEL_Y || y >= PANEL_Y + PANEL_H)
        return back();
    if (y < ROW_Y0 - 6 && x >= PANEL_X + PANEL_W - 40) /* "Back" in the title bar */
        return back();
    if (page == PAGE_RECOLOR) {
        int c = (x - SW_X0) / SW_W, r = (y - SW_Y0) / SW_H;
        if (x >= SW_X0 && c < 8 && y >= SW_Y0 && r >= 0) {
            int i = r * 8 + c;
            if (i < ncolors) {
                if (i == recolor_sel) return menu_key(MK_OK);
                recolor_sel = i;
            }
        }
        return MENU_ACT_NONE;
    }
    int row;
    int n = page == PAGE_MAIN ? main_count() : page == PAGE_CHARS ? CHAR_COUNT * PART_COUNT + 2 : 5;
    int y0 = page == PAGE_EDIT ? ROW_Y0 + 40 : ROW_Y0;
    row = (y - y0) / ROW_H;
    if (y < y0 || row < 0 || row >= n) return MENU_ACT_NONE;
    sel = row;
    int is_value = page == PAGE_MAIN ? (row >= IT_SCREEN && row <= IT_HUE)
                 : page == PAGE_CHARS ? row < CHAR_COUNT * PART_COUNT : row <= 2;
    if (is_value && x >= VALUE_X)
        return menu_key(x < VALUE_X + 50 ? MK_LEFT : MK_RIGHT);
    return menu_key(MK_OK);
}

void menu_draw(uint32_t *buf)
{
    memset(buf, 0, MENU_W * MENU_H * 4);
    if (!open_) return;
    fill(buf, PANEL_X, PANEL_Y, PANEL_W, PANEL_H, C_PANEL);
    frame(buf, PANEL_X, PANEL_Y, PANEL_W, PANEL_H, 0xFF3A3850u);
    fill(buf, PANEL_X, PANEL_Y, PANEL_W, 2, C_ACCENT);
    char t[64];

    if (page == PAGE_MAIN) {
        text(buf, PANEL_X + 12, PANEL_Y + 10, "OPTIONS", C_GOLD);
        text(buf, PANEL_X + PANEL_W - 40, PANEL_Y + 10, "X", C_DIM);
        for (int i = 0; i < main_count(); i++) {
            int y = ROW_Y0 + i * ROW_H;
            int s = i == sel;
            if (s) fill(buf, PANEL_X + 4, y - 3, PANEL_W - 8, ROW_H, C_SEL);
            if (s) fill(buf, PANEL_X + 4, y - 3, 2, ROW_H, C_ACCENT);
            text(buf, PANEL_X + 14, y, item_label[i], s ? C_TEXT : C_DIM);
            switch (i) {
            case IT_SCREEN:
                snprintf(t, sizeof(t), "< %s >", settings_screen_name(settings.screen));
                text(buf, VALUE_X - 16, y, t, C_TEXT);
                break;
            case IT_SCALING:
                text(buf, VALUE_X - 16, y, settings.smooth ? "< Smooth >" : "< Sharp >", C_TEXT);
                break;
            case IT_UPPER:
                text(buf, VALUE_X - 16, y, settings.show_2p_panel ? "< 2P panel >" : "< Room above >", C_TEXT);
                break;
            case IT_CUTSCENES:
                text(buf, VALUE_X - 16, y, settings.crop_cutscenes ? "< 4:3 >" : "< Wide >", C_TEXT);
                break;
            case IT_GUARDS:
                text(buf, VALUE_X - 16, y, settings.floor_alert ? "< Off (Alert) >" : settings.patrol_return ? "< On >" : "< Off >",
                     settings.floor_alert ? C_DIM : C_TEXT);
                break;
            case IT_ALERT:
                text(buf, VALUE_X - 16, y, settings.floor_alert == FLOOR_ALERT_SEMI ? "< Semi >" : settings.floor_alert ? "< On >" : "< Off >", C_TEXT);
                break;
            case IT_COLORS:
                snprintf(t, sizeof(t), "< %s >", settings_preset_name(settings.preset));
                text(buf, VALUE_X - 16, y, t, C_TEXT);
                break;
            case IT_BRIGHT: slider(buf, VALUE_X, y, settings.brightness, -100, 100, s); break;
            case IT_CONTRAST: slider(buf, VALUE_X, y, settings.contrast, -100, 100, s); break;
            case IT_SAT: slider(buf, VALUE_X, y, settings.saturation, -100, 100, s); break;
            case IT_HUE: slider(buf, VALUE_X, y, settings.hue, -180, 180, s); break;
            case IT_CHARS: {
                int k = 0;
                for (int c = 0; c < CHAR_COUNT; c++)
                    for (int p = 0; p < PART_COUNT; p++)
                        k += settings.char_on[c][p];
                if (k) {
                    snprintf(t, sizeof(t), "%d changed", k);
                    text(buf, VALUE_X, y, t, C_DIM);
                }
                break;
            }
            case IT_RECOLOR:
                if (settings.nrecolor) {
                    snprintf(t, sizeof(t), "%d changed", settings.nrecolor);
                    text(buf, VALUE_X, y, t, C_DIM);
                }
                break;
            }
        }
        text(buf, PANEL_X + 14, PANEL_Y + PANEL_H - 14, "Arrows/tap: change  Esc: close", 0xFF6E6A7Eu);
    } else if (page == PAGE_RECOLOR) {
        text(buf, PANEL_X + 12, PANEL_Y + 10, "RECOLOR", C_GOLD);
        text(buf, PANEL_X + PANEL_W - 48, PANEL_Y + 10, "Back", C_DIM);
        text(buf, PANEL_X + 12, PANEL_Y + 22, "Pick a color (it flashes in the game)", C_DIM);
        for (int i = 0; i < ncolors; i++) {
            int x = SW_X0 + (i % 8) * SW_W, y = SW_Y0 + (i / 8) * SW_H;
            uint32_t c = 0xFF000000u | current_rgb(colors[i]);
            fill(buf, x + 2, y + 2, SW_W - 4, SW_H - 4, c);
            if (settings_find_recolor(colors[i]) >= 0)
                fill(buf, x + SW_W - 7, y + 2, 3, 3, C_GOLD);
            if (i == recolor_sel) {
                frame(buf, x, y, SW_W, SW_H, C_ACCENT);
                frame(buf, x + 1, y + 1, SW_W - 2, SW_H - 2, C_TEXT);
            }
        }
        if (!ncolors)
            text(buf, SW_X0, SW_Y0, "No colors on screen.", C_DIM);
        text(buf, PANEL_X + 14, PANEL_Y + PANEL_H - 14, "Enter/tap again: edit  Esc: back", 0xFF6E6A7Eu);
    } else if (page == PAGE_CHARS) {
        text(buf, PANEL_X + 12, PANEL_Y + 10, "CHARACTERS", C_GOLD);
        text(buf, PANEL_X + PANEL_W - 48, PANEL_Y + 10, "Back", C_DIM);
        int n = CHAR_COUNT * PART_COUNT + 2;
        for (int i = 0; i < n; i++) {
            int y = ROW_Y0 + i * ROW_H;
            int s = i == sel;
            if (s) fill(buf, PANEL_X + 4, y - 3, PANEL_W - 8, ROW_H, C_SEL);
            if (s) fill(buf, PANEL_X + 4, y - 3, 2, ROW_H, C_ACCENT);
            if (i < CHAR_COUNT * PART_COUNT) {
                int c = i / PART_COUNT, p = i % PART_COUNT;
                snprintf(t, sizeof(t), "%s %s", c == CHAR_ROBO ? "Robo" : "Mobo", settings_part_name(p));
                text(buf, PANEL_X + 14, y, t, s ? C_TEXT : C_DIM);
                text(buf, VALUE_X - 16, y, "<", C_TEXT);
                fill(buf, VALUE_X, y - 1, 34, 10, 0xFF000000u | char_rgb(c, p));
                frame(buf, VALUE_X, y - 1, 34, 10, 0xFF5A5870u);
                text(buf, VALUE_X + 40, y, settings.char_on[c][p] ? ">" : "> original", settings.char_on[c][p] ? C_TEXT : C_DIM);
            } else {
                text(buf, PANEL_X + 14, y, i == CHAR_COUNT * PART_COUNT ? "Reset characters" : "Back", s ? C_TEXT : C_DIM);
            }
        }
        text(buf, PANEL_X + 14, PANEL_Y + PANEL_H - 14, "Left/right: presets  Enter: mix", 0xFF6E6A7Eu);
    } else {
        uint16_t md = edit_char >= 0 ? char_orig_md[edit_char][edit_part] : edit_md;
        uint32_t rgb = current_rgb(md);
        if (edit_char >= 0)
            snprintf(t, sizeof(t), "%s %s", edit_char == CHAR_ROBO ? "ROBO" : "MOBO",
                     edit_part == PART_SHIRT ? "SHIRT" : edit_part == PART_PANTS ? "PANTS" : "SKIN");
        else
            snprintf(t, sizeof(t), "EDIT COLOR %d%d%d", md & 7, (md >> 3) & 7, (md >> 6) & 7);
        text(buf, PANEL_X + 12, PANEL_Y + 10, t, C_GOLD);
        text(buf, PANEL_X + PANEL_W - 48, PANEL_Y + 10, "Back", C_DIM);
        text(buf, 28, 40, "Original", C_DIM);
        fill(buf, 100, 36, 40, 16, settings_base_color(md));
        text(buf, 160, 40, "New", C_DIM);
        fill(buf, 196, 36, 40, 16, 0xFF000000u | rgb);
        static const char *rows[5] = {"Red", "Green", "Blue", "Reset this color", "Done"};
        int ch[3] = {(int)(rgb >> 16) & 255, (int)(rgb >> 8) & 255, (int)rgb & 255};
        for (int i = 0; i < 5; i++) {
            int y = ROW_Y0 + 40 + i * ROW_H;
            int s = i == sel;
            if (s) fill(buf, PANEL_X + 4, y - 3, PANEL_W - 8, ROW_H, C_SEL);
            if (s) fill(buf, PANEL_X + 4, y - 3, 2, ROW_H, C_ACCENT);
            text(buf, PANEL_X + 14, y, rows[i], s ? C_TEXT : C_DIM);
            if (i < 3) {
                int w = 96, k = ch[i] * (w - 4) / 255;
                uint32_t col = i == 0 ? 0xFFE04040u : i == 1 ? 0xFF40C060u : 0xFF4070F0u;
                fill(buf, VALUE_X, y + 3, w, 2, C_DIM);
                fill(buf, VALUE_X + k, y, 4, 8, col);
                snprintf(t, sizeof(t), "%d", ch[i]);
                text(buf, VALUE_X + w + 6, y, t, C_DIM);
            }
        }
        snprintf(t, sizeof(t), "#%06X", rgb);
        text(buf, 28, ROW_Y0 + 40 + 5 * ROW_H + 8, t, C_DIM);
        text(buf, PANEL_X + 14, PANEL_Y + PANEL_H - 14, "Left/right: change  Esc: back", 0xFF6E6A7Eu);
    }
}
