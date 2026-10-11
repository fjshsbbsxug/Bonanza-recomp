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

typedef struct {
    int screen;              /* SCREEN_* */
    int smooth;              /* 1 = smooth scaling, 0 = sharp pixels */
    int preset;              /* PRESET_* */
    int brightness;          /* -100..100 */
    int contrast;            /* -100..100 */
    int saturation;          /* -100..100 */
    int hue;                 /* -180..180 degrees */
    int nrecolor;
    struct { uint16_t md; uint32_t rgb; } recolor[MAX_RECOLOR]; /* Mega Drive colour -> RGB */
} settings_t;

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
