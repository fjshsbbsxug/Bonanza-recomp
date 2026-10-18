/* In-game options menu. */
#include <stdio.h>
#include <string.h>
#include "menu.h"
#include "md.h"
#include "vdp.h"
#include "settings.h"
#include "crt.h"
#include "font8x8_basic.h"
#include "net.h"
#include "mods.h"

int menu_allow_quit = 1;
int menu_settings_dirty;

enum { PAGE_MAIN, PAGE_RECOLOR, PAGE_EDIT, PAGE_CHARS, PAGE_ONLINE, PAGE_MODS };
static int mods_changed;            /* mods toggled since the game was restarted */

static int editing;                /* typing into the join address */

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
#define ROW_H 9
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
    IT_RESUME, IT_ONLINE, IT_MODS, IT_INSANE, IT_SCREEN, IT_UPPER, IT_CUTSCENES, IT_GUARDS, IT_ALERT, IT_SCALING, IT_CRT, IT_SPRITES, IT_VOLUME, IT_MUTEMUSIC, IT_MUTEALL, IT_SPOTIFY, IT_COLORS, IT_BRIGHT, IT_CONTRAST, IT_SAT, IT_HUE,
    IT_CHARS, IT_RECOLOR, IT_RESETCOLORS, IT_RESETGAME, IT_QUIT, IT_COUNT
};
static const char *item_label[IT_COUNT] = {
    "Resume", "Online...", "Mods...", "Insane mode", "Screen", "Top (1P)", "Cutscenes", "Patrol Return", "Floor Alert", "Scaling", "CRT filter", "Sprite limit", "Volume", "Mute music", "Mute all", "Spotify", "Colors", "Brightness", "Contrast", "Saturation", "Hue",
    "Characters...", "Recolor...", "Reset all colors", "Reset game", "Quit",
};

/* The main page's tabs and their items */
enum { TAB_GAME, TAB_SCREEN, TAB_COLORS, TAB_SOUND, TAB_COUNT };
static const char *tab_name[TAB_COUNT] = { "Game", "Screen", "Colors", "Sound" };
#define TAB_MAX 8
static const int tab_items[TAB_COUNT][TAB_MAX] = {
    { IT_RESUME, IT_ONLINE, IT_MODS, IT_INSANE, IT_GUARDS, IT_ALERT, IT_RESETGAME, IT_QUIT },
    { IT_SCREEN, IT_UPPER, IT_CUTSCENES, IT_SCALING, IT_CRT, IT_SPRITES, -1 },
    { IT_COLORS, IT_BRIGHT, IT_CONTRAST, IT_SAT, IT_HUE, IT_CHARS, IT_RECOLOR, IT_RESETCOLORS },
    { IT_VOLUME, IT_MUTEMUSIC, IT_MUTEALL, IT_SPOTIFY, -1 },
};
static int tab;                     /* the tab shown; sel -1: the tab bar itself */
#define TAB_Y 32                    /* the tab bar */
#define TAB_W 64
#define MAIN_Y0 54                  /* the main page's first row */
#define MAIN_H 12                   /* ... and its rows' height */

static int main_count(void)
{
    int n = 0;
    while (n < TAB_MAX && tab_items[tab][n] >= 0 && (tab_items[tab][n] != IT_QUIT || menu_allow_quit)) n++;
    return n;
}

static int main_item(int row) { return row >= 0 && row < main_count() ? tab_items[tab][row] : -1; }

/* back on the main page, at an item */
static void go_main(int item)
{
    page = PAGE_MAIN;
    for (int t = 0; t < TAB_COUNT; t++)
        for (int i = 0; i < TAB_MAX; i++)
            if (tab_items[t][i] == item) { tab = t; sel = i; return; }
    sel = 0;
}

static int is_value_item(int item) { return (item >= IT_SCREEN && item <= IT_HUE) || item == IT_INSANE; }

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
            uint16_t c = VDP_COLOR(fb[y * MD_FB_W + x]);
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
    case IT_CRT: settings.crt = (settings.crt + dir + CRT_COUNT) % CRT_COUNT; break;
    case IT_SPOTIFY: settings.spotify_mute = !settings.spotify_mute; break;
    case IT_INSANE:
        if (!settings.insane_unlocked || net_state() == NET_PLAYING) return 0;
        settings.insane = !settings.insane;
        break;
    case IT_SPRITES: settings.sprite_limit = !settings.sprite_limit; break;
    case IT_VOLUME: settings.volume = clampi(settings.volume + dir * 5, 0, 100); break;
    case IT_MUTEMUSIC: settings.mute_music = !settings.mute_music; break;
    case IT_MUTEALL: settings.mute_all = !settings.mute_all; break;
    case IT_UPPER: settings.show_2p_panel = !settings.show_2p_panel; break;
    case IT_CUTSCENES: settings.crop_cutscenes = !settings.crop_cutscenes; break;
    case IT_GUARDS: settings.patrol_return = !settings.patrol_return; break;
    case IT_ALERT:
        if (net_state() == NET_PLAYING) return 0;   /* both machines must play the same game */
        settings.floor_alert = (settings.floor_alert + dir + FLOOR_ALERT_COUNT) % FLOOR_ALERT_COUNT;
        break;
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
    case IT_ONLINE: page = PAGE_ONLINE; sel = 0; return MENU_ACT_NONE;
    case IT_MODS: page = PAGE_MODS; sel = 0; mods_scan(); return MENU_ACT_NONE;
    case IT_RESETCOLORS: {
        int screen = settings.screen, smooth = settings.smooth, crt = settings.crt;
        /* keeps character colours; they have their own reset */
        int on[CHAR_COUNT][PART_COUNT];
        uint32_t rgb[CHAR_COUNT][PART_COUNT];
        memcpy(on, settings.char_on, sizeof(on));
        memcpy(rgb, settings.char_rgb, sizeof(rgb));
        settings_defaults();
        settings.screen = screen;
        settings.smooth = smooth;
        settings.crt = crt;
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

/* ------------------------------------------------------------ online page */
/* rows: Host, Mode (the host's: Normal / Prop Hunt / Race), Round length (Prop Hunt), Join
 * <address>, Search LAN, LAN games..., Leave / Back */
#define OR_HOST 0
#define OR_MODE 1
#define OR_ROUND 2
#define OR_JOIN 3
#define OR_LAN 4
#define OR_FIRST_GAME 5
static int online_rows(void) { return OR_FIRST_GAME + net_lan_count() + 1; }

/* the host's game options: changed only while not online (the joiner gets the host's) */
static void online_option(int row, int dir)
{
    if (net_state() != NET_OFF) return;
    if (row == OR_MODE) {
        /* Normal -> Prop Hunt -> Race */
        int m = settings.race ? 2 : settings.prop_hunt ? 1 : 0;
        m = (m + dir + 3) % 3;
        settings.prop_hunt = m == 1;
        settings.race = m == 2;
    }
    else if (row == OR_ROUND) settings.ph_minutes = (settings.ph_minutes - 1 + dir + 9) % 9 + 1;
    else return;
    menu_settings_dirty = 1;
}

static int online_activate(int row)
{
    int lan = net_lan_count();
    if (row == OR_HOST) { net_host(NET_DEFAULT_PORT); return MENU_ACT_NONE; }
    if (row == OR_MODE || row == OR_ROUND) { online_option(row, 1); return MENU_ACT_NONE; }
    if (row == OR_JOIN) { editing = 1; return MENU_ACT_NONE; }
    if (row == OR_LAN) { net_lan_search(); return MENU_ACT_NONE; }
    if (row < OR_FIRST_GAME + lan) {
        snprintf(settings.join_address, sizeof(settings.join_address), "%s", net_lan_address(row - OR_FIRST_GAME));
        menu_settings_dirty = 1;
        net_join(settings.join_address);
        return MENU_ACT_NONE;
    }
    if (net_state() != NET_OFF) { net_stop(); return MENU_ACT_NONE; }
    go_main(IT_ONLINE);
    return MENU_ACT_NONE;
}

/* ------------------------------------------------------------ mods page */
/* rows: the mods, "Apply", "Back" */
#define MODS_VISIBLE 12
static int mods_rows(void) { return mods_count() + 2; }

/* first mod row shown (the list scrolls with the selection) */
static int mods_top(void)
{
    int nm = mods_count();
    if (sel >= nm || nm <= MODS_VISIBLE) return nm > MODS_VISIBLE ? nm - MODS_VISIBLE : 0;
    int top = sel - MODS_VISIBLE / 2;
    if (top < 0) top = 0;
    if (top > nm - MODS_VISIBLE) top = nm - MODS_VISIBLE;
    return top;
}

/* row on screen -> list row */
static int mods_row_at(int r)
{
    int nm = mods_count(), top = mods_top();
    int shown = nm - top < MODS_VISIBLE ? nm - top : MODS_VISIBLE;
    if (r < shown) return top + r;
    if (r - shown < 2) return nm + (r - shown);
    return -1;
}

static int mods_activate(int row)
{
    int n = mods_count();
    if (row < n) {
        if (net_state() == NET_PLAYING) return MENU_ACT_NONE;
        mods_toggle(row);
        mods_changed = 1;
        menu_settings_dirty = 1;
        return MENU_ACT_NONE;
    }
    if (row == n) {
        if (net_state() == NET_PLAYING) return MENU_ACT_NONE;
        mods_changed = 0;
        open_ = 0;
        return MENU_ACT_MODS;
    }
    go_main(IT_MODS);
    return MENU_ACT_NONE;
}

int menu_text_editing(void) { return open_ && page == PAGE_ONLINE && editing; }

void menu_text_typed(const char *t)
{
    for (; *t; t++) {
        char c = *t;
        int ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == ':' || c == '-';
        size_t l = strlen(settings.join_address);
        if (ok && l + 1 < sizeof(settings.join_address)) {
            settings.join_address[l] = c;
            settings.join_address[l + 1] = 0;
        }
    }
}

void menu_text_backspace(void)
{
    size_t l = strlen(settings.join_address);
    if (l) settings.join_address[l - 1] = 0;
}

/* ------------------------------------------------------------ API */

int menu_is_open(void) { return open_; }

void menu_open(void)
{
    open_ = 1;
    page = PAGE_MAIN;
    tab = TAB_GAME;
    sel = 0;
    menu_settings_dirty = 0;
}

static uint32_t blink;
#define CURSOR_ON ((++blink / 20) & 1)

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
    if (page == PAGE_CHARS) { go_main(IT_CHARS); return MENU_ACT_NONE; }
    if (page == PAGE_MODS) { go_main(IT_MODS); return MENU_ACT_NONE; }
    if (page == PAGE_ONLINE) {
        if (editing) { editing = 0; return MENU_ACT_NONE; }
        go_main(IT_ONLINE); return MENU_ACT_NONE;
    }
    if (page == PAGE_EDIT) { page = PAGE_RECOLOR; collect_colors(); return MENU_ACT_NONE; }
    if (page == PAGE_RECOLOR) { go_main(IT_RECOLOR); return MENU_ACT_NONE; }
    open_ = 0;
    return MENU_ACT_CLOSE;
}

int menu_key(int key)
{
    if (!open_) return MENU_ACT_NONE;
    if (key == MK_BACK) return back();
    if (page == PAGE_MAIN) {
        /* rows: the tab bar (-1), then the tab's items; left / right on the tab bar, or
         * on an item that has no value, go to the next tab */
        int n = main_count();
        if (key == MK_UP) sel = sel < 0 ? n - 1 : sel - 1;
        else if (key == MK_DOWN) sel = sel >= n - 1 ? -1 : sel + 1;
        else if (key == MK_LEFT || key == MK_RIGHT) {
            int dir = key == MK_LEFT ? -1 : 1;
            if (sel < 0 || !adjust_main(main_item(sel), dir)) {
                tab = (tab + dir + TAB_COUNT) % TAB_COUNT;
                if (sel >= 0) sel = 0;
            }
        } else if (key == MK_OK) {
            if (sel < 0) sel = 0;
            else return activate_main(main_item(sel));
        }
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
    } else if (page == PAGE_ONLINE) {
        int n = online_rows();
        if (editing) {
            if (key == MK_OK) {
                editing = 0;
                menu_settings_dirty = 1;
                if (settings.join_address[0]) net_join(settings.join_address);
            }
            return MENU_ACT_NONE;
        }
        if (sel >= n) sel = n - 1;
        if (key == MK_UP) sel = (sel + n - 1) % n;
        else if (key == MK_DOWN) sel = (sel + 1) % n;
        else if (key == MK_LEFT || key == MK_RIGHT) online_option(sel, key == MK_LEFT ? -1 : 1);
        else if (key == MK_OK) return online_activate(sel);
    } else if (page == PAGE_MODS) {
        int n = mods_rows();
        if (sel >= n) sel = n - 1;
        if (key == MK_UP) sel = (sel + n - 1) % n;
        else if (key == MK_DOWN) sel = (sel + 1) % n;
        else if (key == MK_OK || ((key == MK_LEFT || key == MK_RIGHT) && sel < mods_count())) return mods_activate(sel);
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
    if (y < (page == PAGE_MAIN ? TAB_Y - 3 : ROW_Y0 - 6) && x >= PANEL_X + PANEL_W - 40) /* "Back" in the title bar */
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
    if (page == PAGE_MODS) {
        int r = (y - ROW_Y0) / ROW_H, row = y >= ROW_Y0 ? mods_row_at(r) : -1;
        if (row < 0) return MENU_ACT_NONE;
        if (row == sel || row >= mods_count()) { sel = row; return mods_activate(row); }
        sel = row;
        return MENU_ACT_NONE;
    }
    if (page == PAGE_ONLINE) {
        /* its rows are 2 pixels further apart */
        int row = (y - (ROW_Y0 + 4) + 3) / (ROW_H + 2);
        if (y < ROW_Y0 + 1 || row < 0 || row >= online_rows()) return MENU_ACT_NONE;
        if (editing && row != OR_JOIN) editing = 0;
        sel = row;
        return menu_key(MK_OK);
    }
    if (page == PAGE_MAIN && y >= TAB_Y - 3 && y < TAB_Y + 11) {
        int t = (x - (PANEL_X + 10)) / TAB_W;
        if (x >= PANEL_X + 10 && t < TAB_COUNT) { tab = t; sel = -1; }
        return MENU_ACT_NONE;
    }
    int row;
    int n = page == PAGE_MAIN ? main_count() : page == PAGE_CHARS ? CHAR_COUNT * PART_COUNT + 2
          : page == PAGE_ONLINE ? online_rows() : 5;
    int y0 = page == PAGE_EDIT ? ROW_Y0 + 40 : page == PAGE_ONLINE ? ROW_Y0 + 4 : page == PAGE_MAIN ? MAIN_Y0 : ROW_Y0;
    row = (y - y0 + (page == PAGE_MAIN ? 3 : 0)) / (page == PAGE_MAIN ? MAIN_H : ROW_H);
    if (y < y0 - (page == PAGE_MAIN ? 3 : 0) || row < 0 || row >= n) return MENU_ACT_NONE;
    sel = row;
    int is_value = page == PAGE_MAIN ? is_value_item(main_item(row))
                 : page == PAGE_CHARS ? row < CHAR_COUNT * PART_COUNT
                 : page == PAGE_ONLINE ? 0 : row <= 2;
    if (page == PAGE_ONLINE && editing && row != OR_JOIN) editing = 0;
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
        /* the tabs */
        for (int t = 0; t < TAB_COUNT; t++) {
            int x = PANEL_X + 10 + t * TAB_W, on = t == tab;
            if (on) fill(buf, x, TAB_Y - 3, TAB_W - 4, 13, C_SEL);
            if (on) fill(buf, x, TAB_Y + 9, TAB_W - 4, 2, sel < 0 ? C_GOLD : C_ACCENT);
            text(buf, x + (TAB_W - 4 - 8 * (int)strlen(tab_name[t])) / 2, TAB_Y, tab_name[t], on ? C_TEXT : C_DIM);
        }
        fill(buf, PANEL_X + 4, TAB_Y + 11, PANEL_W - 8, 1, 0xFF3A3850u);
        for (int r = 0; r < main_count(); r++) {
            int i = main_item(r);
            int y = MAIN_Y0 + r * MAIN_H;
            int s = r == sel;
            if (s) fill(buf, PANEL_X + 4, y - 3, PANEL_W - 8, MAIN_H - 1, C_SEL);
            if (s) fill(buf, PANEL_X + 4, y - 3, 2, MAIN_H - 1, C_ACCENT);
            text(buf, PANEL_X + 14, y, item_label[i], s ? C_TEXT : C_DIM);
            switch (i) {
            case IT_SCREEN:
                snprintf(t, sizeof(t), "< %s >", settings_screen_name(settings.screen));
                text(buf, VALUE_X - 16, y, t, C_TEXT);
                break;
            case IT_SCALING:
                text(buf, VALUE_X - 16, y, settings.smooth ? "< Smooth >" : "< Sharp >", settings.crt ? C_DIM : C_TEXT);
                break;
            case IT_CRT:
                snprintf(t, sizeof(t), "< %s >", crt_name(settings.crt));
                text(buf, VALUE_X - 16, y, t, C_TEXT);
                break;
            case IT_INSANE:
                text(buf, VALUE_X - 16, y, !settings.insane_unlocked ? "Beat Hard to unlock" : settings.insane ? "< On >" : "< Off >",
                     settings.insane_unlocked ? (settings.insane ? 0xFFFF5060u : C_TEXT) : C_DIM);
                break;
            case IT_SPRITES:
                text(buf, VALUE_X - 16, y, settings.sprite_limit ? "< Original >" : "< None >", C_TEXT);
                break;
            case IT_VOLUME:
                slider(buf, VALUE_X, y, settings.volume, 0, 100, s);
                snprintf(t, sizeof(t), "%d", settings.volume);
                fill(buf, VALUE_X + 102, y, 40, 8, C_PANEL);
                text(buf, VALUE_X + 102, y, t, settings.mute_all ? C_DIM : C_TEXT);
                break;
            case IT_MUTEMUSIC:
                text(buf, VALUE_X - 16, y, settings.mute_music ? "< On >" : "< Off >", settings.mute_all ? C_DIM : C_TEXT);
                break;
            case IT_MUTEALL:
                text(buf, VALUE_X - 16, y, settings.mute_all ? "< On >" : "< Off >", C_TEXT);
                break;
            case IT_SPOTIFY:
                text(buf, VALUE_X - 16, y, settings.spotify_mute ? "< Mutes music >" : "< Off >", C_TEXT);
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
            case IT_MODS:
                if (mods_enabled_count()) {
                    snprintf(t, sizeof(t), "%d on%s", mods_enabled_count(), mods_changed ? " (Apply!)" : "");
                    text(buf, VALUE_X, y, t, mods_changed ? C_GOLD : C_DIM);
                }
                break;
            case IT_RECOLOR:
                if (settings.nrecolor) {
                    snprintf(t, sizeof(t), "%d changed", settings.nrecolor);
                    text(buf, VALUE_X, y, t, C_DIM);
                }
                break;
            }
        }
        if (tab == TAB_SOUND) {
            int y = MAIN_Y0 + main_count() * MAIN_H + 8;
            text(buf, PANEL_X + 14, y, "While Spotify plays music, the", C_DIM);
            text(buf, PANEL_X + 14, y + 10, "game's music is muted; sound", C_DIM);
            text(buf, PANEL_X + 14, y + 20, "effects stay on.", C_DIM);
            text(buf, PANEL_X + 14, y + 36, "Works on Windows, Linux and Mac.", C_DIM);
        }
        text(buf, PANEL_X + 14, PANEL_Y + PANEL_H - 14, sel < 0 ? "Left/right: tabs  Esc: close" : "Arrows/tap: change  Esc: close", 0xFF6E6A7Eu);
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
    } else if (page == PAGE_ONLINE) {
        text(buf, PANEL_X + 12, PANEL_Y + 10, "ONLINE (2 PLAYERS)", C_GOLD);
        text(buf, PANEL_X + PANEL_W - 48, PANEL_Y + 10, "Back", C_DIM);
        int n = online_rows(), lan = net_lan_count();
        for (int i = 0; i < n; i++) {
            int y = ROW_Y0 + 4 + i * (ROW_H + 2);
            int s = i == sel;
            if (s) fill(buf, PANEL_X + 4, y - 3, PANEL_W - 8, ROW_H + 2, C_SEL);
            if (s) fill(buf, PANEL_X + 4, y - 3, 2, ROW_H + 2, C_ACCENT);
            const char *label = "";
            if (i == OR_HOST) {
                snprintf(t, sizeof(t), "Host a game (port %d)", NET_DEFAULT_PORT);
                label = t;
            } else if (i == OR_MODE) {
                snprintf(t, sizeof(t), "  Mode    < %s >", settings.race ? "Race" : settings.prop_hunt ? "Prop Hunt" : "Normal");
                label = t;
            } else if (i == OR_ROUND) {
                snprintf(t, sizeof(t), "  Round   < %d min >%s", settings.ph_minutes, settings.prop_hunt ? "" : "  (Prop Hunt)");
                label = t;
            } else if (i == OR_JOIN) {
                snprintf(t, sizeof(t), "Join: %s%s", settings.join_address, editing && (CURSOR_ON) ? "_" : "");
                label = t;
            } else if (i == OR_LAN) {
                label = "Search LAN";
            } else if (i < OR_FIRST_GAME + lan) {
                snprintf(t, sizeof(t), "  %s", net_lan_entry(i - OR_FIRST_GAME));
                label = t;
            } else {
                label = net_state() != NET_OFF ? "Leave online game" : "Back";
            }
            text(buf, PANEL_X + 14, y, label, s ? C_TEXT : C_DIM);
        }
        /* status, wrapped over two lines */
        const char *st = net_status();
        char l1[40], l2[40];
        snprintf(l1, sizeof(l1), "%.34s", st);
        snprintf(l2, sizeof(l2), "%.34s", strlen(st) > 34 ? st + 34 : "");
        text(buf, PANEL_X + 14, PANEL_Y + PANEL_H - 50, l1, C_GOLD);
        text(buf, PANEL_X + 14, PANEL_Y + PANEL_H - 40, l2, C_GOLD);
        text(buf, PANEL_X + 14, PANEL_Y + PANEL_H - 26, editing ? "Type the address, Enter: join" :
             "Internet: forward UDP 7777", 0xFF6E6A7Eu);
        text(buf, PANEL_X + 14, PANEL_Y + PANEL_H - 14, "Enter: choose  Esc: back", 0xFF6E6A7Eu);
    } else if (page == PAGE_MODS) {
        text(buf, PANEL_X + 12, PANEL_Y + 10, "MODS", C_GOLD);
        text(buf, PANEL_X + PANEL_W - 48, PANEL_Y + 10, "Back", C_DIM);
        int nm = mods_count(), i;
        for (int r = 0; (i = mods_row_at(r)) >= 0; r++) {
            int y = ROW_Y0 + r * ROW_H;
            int s = i == sel;
            if (s) fill(buf, PANEL_X + 4, y - 3, PANEL_W - 8, ROW_H, C_SEL);
            if (s) fill(buf, PANEL_X + 4, y - 3, 2, ROW_H, C_ACCENT);
            if (i < nm) {
                const mod_info *m = mods_get(i);
                snprintf(t, sizeof(t), "[%c] %.22s", m->error[0] ? '!' : m->enabled ? 'x' : ' ', m->name);
                text(buf, PANEL_X + 14, y, t, m->error[0] ? 0xFFB06060u : s ? C_TEXT : C_DIM);
                if (m->version[0]) text(buf, PANEL_X + PANEL_W - 64, y, m->version, C_DIM);
            } else {
                text(buf, PANEL_X + 14, y, i == nm ? (mods_changed ? "Apply (restarts the game)" : "Restart the game with these")
                                                    : "Back", i == nm && mods_changed ? C_GOLD : s ? C_TEXT : C_DIM);
            }
        }
        /* about the selected mod, or where to put mods */
        const mod_info *m = mods_get(sel);
        char l1[40], l2[40], l3[40];
        if (m) {
            snprintf(l1, sizeof(l1), "%.34s", m->error[0] ? m->error : m->author[0] ? m->author : m->file);
            if (m->author[0] && !m->error[0]) snprintf(l1, sizeof(l1), "by %.31s", m->author);
            snprintf(l2, sizeof(l2), "%.34s", m->description);
            snprintf(l3, sizeof(l3), "%.34s", strlen(m->description) > 34 ? m->description + 34 : "");
        } else {
            snprintf(l1, sizeof(l1), "%s", nm ? (net_state() == NET_PLAYING ? "Not during an online game" : mods_status())
                                              : "No mods yet. Put .bbro files in:");
            const char *d = mods_main_dir();
            size_t dl = strlen(d);
            snprintf(l2, sizeof(l2), "%.34s", dl > 68 ? d + dl - 68 : d);
            snprintf(l3, sizeof(l3), "%.34s", dl > 68 ? d + dl - 34 : dl > 34 ? d + 34 : "");
        }
        text(buf, PANEL_X + 14, PANEL_Y + PANEL_H - 50, l1, C_GOLD);
        text(buf, PANEL_X + 14, PANEL_Y + PANEL_H - 40, l2, C_DIM);
        text(buf, PANEL_X + 14, PANEL_Y + PANEL_H - 30, l3, C_DIM);
        text(buf, PANEL_X + 14, PANEL_Y + PANEL_H - 14, "Enter: on/off  Later ones win", 0xFF6E6A7Eu);
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
