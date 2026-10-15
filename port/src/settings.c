/* Player options: screen shape and colours (bonanza.ini). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "settings.h"
#include "crt.h"

static const char *crt_keys[CRT_COUNT] = { "off", "scanlines", "tv", "monitor" };
#include "gamemods.h"
#include "md.h"
#include "vdp.h"

settings_t settings = { .crop_cutscenes = 1, .patrol_return = 1 };
uint32_t md_color_lut[512];
uint32_t char_color_lut[16];
static const char *char_keys[CHAR_COUNT] = {"robo", "mobo"};
static const char *part_keys[PART_COUNT] = {"shirt", "pants", "skin"};
static const char *char_names[CHAR_COUNT] = {"Robo (P1)", "Mobo (P2)"};
static const char *part_names[PART_COUNT] = {"Shirt", "Pants", "Skin"};
const char *settings_char_name(int c) { return c >= 0 && c < CHAR_COUNT ? char_names[c] : "?"; }
const char *settings_part_name(int p) { return p >= 0 && p < PART_COUNT ? part_names[p] : "?"; }

static uint32_t shade(uint32_t rgb, double f)
{
    uint32_t r = (uint32_t)(((rgb >> 16) & 255) * f), g = (uint32_t)(((rgb >> 8) & 255) * f), b = (uint32_t)((rgb & 255) * f);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}
int md_color_lut_ready;

static const char *screen_names[SCREEN_COUNT] = {
    "Original 4:3", "Widescreen 16:9", "Widescreen 16:10", "Ultrawide 21:9", "Stretch to window",
};
static const char *screen_keys[SCREEN_COUNT] = {"original", "16:9", "16:10", "21:9", "stretch"};
static const char *preset_names[PRESET_COUNT] = {
    "Original", "Vivid", "Soft", "Arcade", "Grayscale", "Sepia", "Night", "Game Boy", "Inverted",
};
static const char *preset_keys[PRESET_COUNT] = {
    "original", "vivid", "soft", "arcade", "grayscale", "sepia", "night", "gameboy", "inverted",
};

const char *settings_screen_name(int s) { return s >= 0 && s < SCREEN_COUNT ? screen_names[s] : "?"; }
const char *settings_preset_name(int p) { return p >= 0 && p < PRESET_COUNT ? preset_names[p] : "?"; }

double settings_aspect(void)
{
    switch (settings.screen) {
    case SCREEN_WIDE_16_9: return 16.0 / 9.0;
    case SCREEN_WIDE_16_10: return 16.0 / 10.0;
    case SCREEN_WIDE_21_9: return 21.0 / 9.0;
    default: return 4.0 / 3.0;
    }
}

void settings_defaults(void)
{
    memset(&settings, 0, sizeof(settings));
    settings.screen = SCREEN_ORIGINAL;
    settings.preset = PRESET_ORIGINAL;
    settings.crop_cutscenes = 1;
    settings.patrol_return = 1;
    settings.ph_minutes = 3;
}

/* ------------------------------------------------------------ colour */

static double clamp01(double v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

static void rgb_to_hsv(double r, double g, double b, double *h, double *s, double *v)
{
    double mx = fmax(r, fmax(g, b)), mn = fmin(r, fmin(g, b)), d = mx - mn;
    *v = mx;
    *s = mx > 0 ? d / mx : 0;
    if (d <= 0) { *h = 0; return; }
    if (mx == r) *h = fmod((g - b) / d, 6.0);
    else if (mx == g) *h = (b - r) / d + 2;
    else *h = (r - g) / d + 4;
    *h *= 60;
    if (*h < 0) *h += 360;
}

static void hsv_to_rgb(double h, double s, double v, double *r, double *g, double *b)
{
    double c = v * s, x = c * (1 - fabs(fmod(h / 60.0, 2) - 1)), m = v - c;
    double rr = 0, gg = 0, bb = 0;
    if (h < 60) { rr = c; gg = x; }
    else if (h < 120) { rr = x; gg = c; }
    else if (h < 180) { gg = c; bb = x; }
    else if (h < 240) { gg = x; bb = c; }
    else if (h < 300) { rr = x; bb = c; }
    else { rr = c; bb = x; }
    *r = rr + m; *g = gg + m; *b = bb + m;
}

uint32_t settings_base_color(uint16_t md)
{
    /* 3 bits per channel -> 0..1 (the usual full-range ramp) */
    double r = (md & 7) / 7.0, g = ((md >> 3) & 7) / 7.0, b = ((md >> 6) & 7) / 7.0;
    double luma = 0.299 * r + 0.587 * g + 0.114 * b;
    switch (settings.preset) {
    case PRESET_VIVID: {
        double h, s, v;
        rgb_to_hsv(r, g, b, &h, &s, &v);
        s = clamp01(s * 1.35);
        v = clamp01(v * 1.05);
        hsv_to_rgb(h, s, v, &r, &g, &b);
        break;
    }
    case PRESET_SOFT:
        r = 0.18 + 0.72 * (0.75 * r + 0.25 * luma);
        g = 0.18 + 0.72 * (0.75 * g + 0.25 * luma);
        b = 0.20 + 0.72 * (0.75 * b + 0.25 * luma);
        break;
    case PRESET_ARCADE: /* brighter, a little more contrast, like the System 16 version */
        r = clamp01(pow(r, 0.8) * 1.05);
        g = clamp01(pow(g, 0.8) * 1.05);
        b = clamp01(pow(b, 0.8) * 1.05);
        break;
    case PRESET_GRAYSCALE:
        r = g = b = luma;
        break;
    case PRESET_SEPIA:
        r = clamp01(luma * 1.07 + 0.08);
        g = clamp01(luma * 0.88 + 0.04);
        b = clamp01(luma * 0.62);
        break;
    case PRESET_NIGHT:
        r = 0.35 * r + 0.10 * luma;
        g = 0.45 * g + 0.15 * luma;
        b = clamp01(0.75 * b + 0.30 * luma + 0.05);
        break;
    case PRESET_GAMEBOY: {
        static const double pal[4][3] = {{0.06, 0.22, 0.06}, {0.19, 0.38, 0.19}, {0.55, 0.67, 0.06}, {0.61, 0.74, 0.06}};
        int k = (int)(luma * 3.999);
        r = pal[k][0]; g = pal[k][1]; b = pal[k][2];
        break;
    }
    case PRESET_INVERTED:
        r = 1 - r; g = 1 - g; b = 1 - b;
        break;
    default:
        break;
    }
    /* user adjustments */
    if (settings.hue || settings.saturation) {
        double h, s, v;
        rgb_to_hsv(r, g, b, &h, &s, &v);
        h = fmod(h + settings.hue + 360.0, 360.0);
        s = clamp01(s * (1.0 + settings.saturation / 100.0));
        hsv_to_rgb(h, s, v, &r, &g, &b);
    }
    double c = 1.0 + settings.contrast / 100.0, br = settings.brightness / 200.0;
    r = clamp01((r - 0.5) * c + 0.5 + br);
    g = clamp01((g - 0.5) * c + 0.5 + br);
    b = clamp01((b - 0.5) * c + 0.5 + br);
    return 0xFF000000u | ((uint32_t)(r * 255 + 0.5) << 16) | ((uint32_t)(g * 255 + 0.5) << 8) | (uint32_t)(b * 255 + 0.5);
}

int settings_find_recolor(uint16_t md)
{
    for (int i = 0; i < settings.nrecolor; i++)
        if (settings.recolor[i].md == md)
            return i;
    return -1;
}

void settings_set_recolor(uint16_t md, uint32_t rgb)
{
    int i = settings_find_recolor(md);
    if (i < 0) {
        if (settings.nrecolor >= MAX_RECOLOR)
            return;
        i = settings.nrecolor++;
    }
    settings.recolor[i].md = md & 511;
    settings.recolor[i].rgb = rgb & 0xFFFFFF;
}

void settings_clear_recolor(uint16_t md)
{
    int i = settings_find_recolor(md);
    if (i < 0)
        return;
    settings.recolor[i] = settings.recolor[--settings.nrecolor];
}

void settings_apply(void)
{
    for (int c = 0; c < 512; c++)
        md_color_lut[c] = settings_base_color((uint16_t)c);
    for (int i = 0; i < settings.nrecolor; i++)
        md_color_lut[settings.recolor[i].md & 511] = 0xFF000000u | settings.recolor[i].rgb;
    md_color_lut_ready = 1;
    for (int c = 0; c < CHAR_COUNT; c++) {
        uint32_t *l = &char_color_lut[c * 8];
        l[CHAR_SLOT_SHIRT] = 0xFF000000u | settings.char_rgb[c][PART_SHIRT];
        l[CHAR_SLOT_SHIRT_DARK] = shade(settings.char_rgb[c][PART_SHIRT], 0.62);   /* like 500 -> 300 */
        l[CHAR_SLOT_PANTS] = 0xFF000000u | settings.char_rgb[c][PART_PANTS];
        l[CHAR_SLOT_SKIN] = 0xFF000000u | settings.char_rgb[c][PART_SKIN];
        l[CHAR_SLOT_SKIN_DARK] = shade(settings.char_rgb[c][PART_SKIN], 0.80);     /* like 740 -> 620 */
    }
    vdp_refresh_palette();
    md_set_aspect(settings_aspect());
    game_mods_apply();
}

/* ------------------------------------------------------------ file */

static char *trim(char *s)
{
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

int settings_set(const char *key, const char *value)
{
    if (!strcmp(key, "screen")) {
        for (int i = 0; i < SCREEN_COUNT; i++)
            if (!strcmp(value, screen_keys[i])) { settings.screen = i; return 1; }
        return 0;
    }
    if (!strcmp(key, "colors")) {
        for (int i = 0; i < PRESET_COUNT; i++)
            if (!strcmp(value, preset_keys[i])) { settings.preset = i; return 1; }
        return 0;
    }
    if (!strcmp(key, "smooth")) { settings.smooth = atoi(value) != 0; return 1; }
    if (!strcmp(key, "prop_hunt")) { settings.prop_hunt = atoi(value) != 0 || !strcmp(value, "on"); return 1; }
    if (!strcmp(key, "prop_hunt_minutes")) {
        int v = atoi(value);
        settings.ph_minutes = v < 1 ? 1 : v > 9 ? 9 : v;
        return 1;
    }
    if (!strcmp(key, "crt")) {
        for (int i = 0; i < CRT_COUNT; i++)
            if (!strcmp(value, crt_keys[i])) { settings.crt = i; return 1; }
        return 0;
    }
    if (!strcmp(key, "cutscenes")) { settings.crop_cutscenes = strcmp(value, "wide") != 0; return 1; }
    if (!strcmp(key, "mods")) { snprintf(settings.mods, sizeof(settings.mods), "%s", value); return 1; }
    if (!strcmp(key, "join")) { snprintf(settings.join_address, sizeof(settings.join_address), "%s", value); return 1; }
    if (!strcmp(key, "floor_alert")) {
        settings.floor_alert = !strcmp(value, "semi") ? FLOOR_ALERT_SEMI
                             : (!strcmp(value, "on") || atoi(value) == 1) ? FLOOR_ALERT_ON : FLOOR_ALERT_OFF;
        return 1;
    }
    if (!strcmp(key, "guards")) { settings.patrol_return = strcmp(value, "original") != 0; return 1; }
    if (!strcmp(key, "upper_view")) { settings.show_2p_panel = !strcmp(value, "panel"); return 1; }
    if (!strcmp(key, "brightness")) { settings.brightness = clampi(atoi(value), -100, 100); return 1; }
    if (!strcmp(key, "contrast")) { settings.contrast = clampi(atoi(value), -100, 100); return 1; }
    if (!strcmp(key, "saturation")) { settings.saturation = clampi(atoi(value), -100, 100); return 1; }
    if (!strcmp(key, "hue")) { settings.hue = clampi(atoi(value), -180, 180); return 1; }
    for (int c = 0; c < CHAR_COUNT; c++)
        for (int p = 0; p < PART_COUNT; p++) {
            char k[32];
            snprintf(k, sizeof(k), "%s.%s", char_keys[c], part_keys[p]);
            if (strcmp(key, k))
                continue;
            if (!*value || !strcmp(value, "original") || !strcmp(value, "none")) {
                settings.char_on[c][p] = 0;
            } else {
                settings.char_on[c][p] = 1;
                settings.char_rgb[c][p] = (uint32_t)strtoul(value[0] == '#' ? value + 1 : value, NULL, 16) & 0xFFFFFF;
            }
            return 1;
        }
    if (!strncmp(key, "color.", 6) && strlen(key) == 9) {
        /* color.RGB = RRGGBB   (R, G, B = Mega Drive levels 0-7) */
        int r = key[6] - '0', g = key[7] - '0', b = key[8] - '0';
        if (r < 0 || r > 7 || g < 0 || g > 7 || b < 0 || b > 7)
            return 0;
        uint16_t md = (uint16_t)(r | (g << 3) | (b << 6));
        if (!strcmp(value, "none") || !*value) {
            settings_clear_recolor(md);
            return 1;
        }
        settings_set_recolor(md, (uint32_t)strtoul(value[0] == '#' ? value + 1 : value, NULL, 16));
        return 1;
    }
    return 0;
}

int settings_load(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return 0;
    char line[1280];
    while (fgets(line, sizeof(line), f)) {
        char *s = trim(line);
        if (!*s || *s == '#' || *s == ';' || *s == '[')
            continue;
        char *eq = strchr(s, '=');
        if (!eq)
            continue;
        *eq = 0;
        settings_set(trim(s), trim(eq + 1));
    }
    fclose(f);
    return 1;
}

int settings_save(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f)
        return 0;
    fprintf(f, "# Bonanza Bros port settings\n");
    fprintf(f, "# screen: original, 16:9, 16:10, 21:9, stretch\n");
    fprintf(f, "screen = %s\n", screen_keys[settings.screen]);
    fprintf(f, "smooth = %d\n", settings.smooth);
    fprintf(f, "# online game mode when hosting: prop_hunt = 1 (Prop Hunt), round length in minutes\n");
    fprintf(f, "prop_hunt = %d\nprop_hunt_minutes = %d\n", settings.prop_hunt, settings.ph_minutes);
    fprintf(f, "# CRT filter: off, scanlines, tv (consumer TV: slot mask, curved), monitor (PVM: aperture grille, flat)\n");
    fprintf(f, "crt = %s\n", crt_keys[settings.crt >= 0 && settings.crt < CRT_COUNT ? settings.crt : 0]);
    fprintf(f, "# top of the screen in a 1-player game: room (the floor above) or panel (original)\n");
    fprintf(f, "upper_view = %s\n", settings.show_2p_panel ? "panel" : "room");
    fprintf(f, "# widescreen during cutscenes/title: 4:3 (cropped, game overs stay wide) or wide\n");
    fprintf(f, "cutscenes = %s\n", settings.crop_cutscenes ? "4:3" : "wide");
    fprintf(f, "# widescreen, Patrol Return: guards leaving the 4:3 picture walk back to their posts (return)\n");
    fprintf(f, "# instead of vanishing (original)\n");
    fprintf(f, "guards = %s\n", settings.patrol_return ? "return" : "original");
    fprintf(f, "# Floor Alert (changes the game): on = guards on your floor keep chasing you off screen\n");
    fprintf(f, "# and never jump back to their posts until you change floors; semi = guards anywhere in\n");
    fprintf(f, "# the (wide) picture stay active, off screen they act as in the original; off = original\n");
    fprintf(f, "# online: the address last joined\n");
    fprintf(f, "join = %s\n", settings.join_address);
    fprintf(f, "# enabled mods from the mods folder, in order (later ones win)\n");
    fprintf(f, "mods = %s\n", settings.mods);
    fprintf(f, "floor_alert = %s\n", settings.floor_alert == FLOOR_ALERT_SEMI ? "semi" : settings.floor_alert ? "on" : "off");
    fprintf(f, "# colors: original, vivid, soft, arcade, grayscale, sepia, night, gameboy, inverted\n");
    fprintf(f, "colors = %s\n", preset_keys[settings.preset]);
    fprintf(f, "brightness = %d\ncontrast = %d\nsaturation = %d\nhue = %d\n",
            settings.brightness, settings.contrast, settings.saturation, settings.hue);
    fprintf(f, "# player characters: RRGGBB or original\n");
    for (int c = 0; c < CHAR_COUNT; c++)
        for (int p = 0; p < PART_COUNT; p++) {
            if (settings.char_on[c][p])
                fprintf(f, "%s.%s = %06X\n", char_keys[c], part_keys[p], settings.char_rgb[c][p]);
            else
                fprintf(f, "%s.%s = original\n", char_keys[c], part_keys[p]);
        }
    if (settings.nrecolor)
        fprintf(f, "# recoloured Mega Drive colours: color.RGB (levels 0-7) = new colour RRGGBB\n");
    for (int i = 0; i < settings.nrecolor; i++) {
        uint16_t md = settings.recolor[i].md;
        fprintf(f, "color.%d%d%d = %06X\n", md & 7, (md >> 3) & 7, (md >> 6) & 7, settings.recolor[i].rgb);
    }
    fclose(f);
    return 1;
}
