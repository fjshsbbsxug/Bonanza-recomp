/* Player options: screen shape and colours. Stored in bonanza.ini. */
#ifndef SETTINGS_H
#define SETTINGS_H
#include <stdint.h>

enum {
    SCREEN_ORIGINAL,     /* 4:3, as on a TV */
    SCREEN_WIDE_16_9,    /* extra picture on the sides */
    SCREEN_WIDE_16_10,
    SCREEN_WIDE_21_9,
    SCREEN_STRETCH,      /* original picture stretched to the window */
    SCREEN_COUNT
};

enum {
    PRESET_ORIGINAL, PRESET_VIVID, PRESET_SOFT, PRESET_ARCADE, PRESET_GRAYSCALE, PRESET_SEPIA,
    PRESET_NIGHT, PRESET_GAMEBOY, PRESET_INVERTED, PRESET_COUNT
};

#define MAX_RECOLOR 128

/* player characters: Robo (player 1, red) and Mobo (player 2, blue) */
enum { CHAR_ROBO, CHAR_MOBO, CHAR_COUNT };
enum { PART_SHIRT, PART_PANTS, PART_SKIN, PART_COUNT };

typedef struct {
    int screen;              /* SCREEN_* */
    int smooth;              /* 1 = smooth scaling, 0 = sharp pixels */
    int show_2p_panel;       /* 1-player game: 0 = show the room above, 1 = original 2P panel */
    int preset;              /* PRESET_* */
    int brightness;          /* -100..100 */
    int contrast;            /* -100..100 */
    int saturation;          /* -100..100 */
    int hue;                 /* -180..180 degrees */
    int nrecolor;
    struct { uint16_t md; uint32_t rgb; } recolor[MAX_RECOLOR]; /* Mega Drive colour -> RGB */
    int      char_on[CHAR_COUNT][PART_COUNT];   /* part has a custom colour */
    uint32_t char_rgb[CHAR_COUNT][PART_COUNT];  /* RRGGBB */
} settings_t;

/* Colours for player-character pixels. The renderer marks them in the picture as
 * VDP_CHAR_BASE + slot, slot = character * 8 + CHAR_SLOT_*. */
enum { CHAR_SLOT_SHIRT, CHAR_SLOT_SHIRT_DARK, CHAR_SLOT_PANTS, CHAR_SLOT_SKIN, CHAR_SLOT_SKIN_DARK };
extern uint32_t char_color_lut[16];
const char *settings_char_name(int c);
const char *settings_part_name(int p);

extern settings_t settings;
extern uint32_t md_color_lut[512];   /* 9-bit Mega Drive colour -> XRGB8888 */
extern int md_color_lut_ready;

const char *settings_screen_name(int s);
const char *settings_preset_name(int p);
double settings_aspect(void);        /* picture aspect the VDP should render */

void settings_defaults(void);
void settings_apply(void);           /* rebuild colour table, set widescreen, refresh palette */
int  settings_load(const char *path);
int  settings_save(const char *path);
int  settings_set(const char *key, const char *value); /* one "key = value" line */

/* recolouring */
uint32_t settings_base_color(uint16_t md);          /* colour after preset/adjustments */
int      settings_find_recolor(uint16_t md);         /* index or -1 */
void     settings_set_recolor(uint16_t md, uint32_t rgb);
void     settings_clear_recolor(uint16_t md);

static inline uint16_t md_color_index(uint16_t cram) /* CRAM word -> 0..511 */
{
    return (uint16_t)(((cram >> 1) & 7) | (((cram >> 5) & 7) << 3) | (((cram >> 9) & 7) << 6));
}

#endif
