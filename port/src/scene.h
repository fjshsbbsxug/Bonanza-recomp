/* The frame's scene: everything the picture is drawn from, as plain tables (render.c
 * fills it when asked; render_gl.c draws it on the graphics card). Drawing it gives
 * the same picture as render.c's own drawing, pixel for pixel. */
#ifndef SCENE_H
#define SCENE_H
#include <stdint.h>

#define SCENE_LINES 224
#define SCENE_PICKS 80          /* sprites a line can show (the console: 20; settings.sprite_limit) */
#define SCENE_WOBJ 64           /* port-made pieces a line can show */
#define SCENE_SPRITES 80
#define SCENE_LIST 256          /* pieces in each port-made list */

/* line flags */
enum {
    SL_OFF = 1 << 0,            /* the display is off: the backdrop colour */
    SL_A_MAP = 1 << 1,          /* plane A's sides from the level map */
    SL_A_MAP_ALL = 1 << 2,      /* ... all of plane A */
    SL_B_WRAP = 1 << 3,         /* plane B's sides repeat its picture */
    SL_ROOM_ABOVE = 1 << 4,     /* the floor above: see render.c plan_line */
    SL_SPRITES = 1 << 5,        /* sprites of line spr_y */
    SL_STACKED = 1 << 6,        /* + the stacked floor's pieces */
    SL_CROP = 1 << 7,           /* sides black */
    SL_CLAMP = 1 << 8,          /* sides: the edge colours */
    SL_SHADE = 1 << 9,          /* shadow / highlight */
    SL_COLUMN_VS = 1 << 10,     /* vertical scroll by columns */
    SL_WINDOW = 1 << 11         /* the window plane is on */
};

typedef struct {
    uint32_t flags;
    int16_t width, m;
    int16_t a_camx, a_wy;                       /* plane A from the map */
    int16_t ra_hs, ra_vs, ra_camx, ra_wy, sky_hs, sky_vs;   /* the floor above */
    int16_t spr_y;
    int16_t hs[2];                              /* horizontal scroll, plane A / B */
    int16_t pw, ph;                             /* plane size in tiles */
    uint16_t name_a, name_b, name_w;            /* name tables in video memory */
    uint8_t reg17, reg18;                       /* window position */
    uint16_t bg;                                /* backdrop colour (code) */
    uint16_t pal[64];                           /* colour codes (md_color_index) */
    uint16_t vsram[40];
    uint8_t npick;
    uint16_t pick[SCENE_PICKS];                 /* sprite | columns << 8 */
    uint8_t nwobj;
    uint16_t wobj[SCENE_WOBJ];                  /* list << 10 | piece (lists: below) */
} scene_line;

typedef struct {
    int16_t sx, sy, hsz, vsz;                   /* as in the sprite table (128: the edge) */
    uint16_t attr;
    int8_t ch, shirt_end, pants_end;            /* player colours: character or -1 */
    uint32_t body[32];
} scene_sprite;

typedef struct {
    int16_t x, y, hsz, vsz, ymin, ymax;
    uint16_t attr;
    uint8_t only_outside;
} scene_piece;

enum { SCENE_WOBJ_LIST, SCENE_PH_LIST, SCENE_STACK_LIST };

typedef struct {
    int valid;                                  /* 0: draw this frame on the CPU */
    int out_w;                                  /* width of the picture (with its sides) */
    const uint8_t *vram;
    int16_t src_line[SCENE_LINES];              /* output row -> line */
    scene_line line[SCENE_LINES];
    int nsprites;
    scene_sprite sprite[SCENE_SPRITES];
    int npieces[3];
    scene_piece piece[3][SCENE_LIST];
    uint8_t char_on[2][3];                      /* player colours: [character][shirt, pants, skin] */
    /* the level map: name-table entries per 8x8 cell (0xFFFF outside the building), and
     * per 16x16 cell whether it is open sky above the roof */
    int map_w;                                  /* in 16-pixel cells; 0: no map */
    int roof_row;
    uint32_t map_gen;                           /* changes when the map does */
    const uint16_t *map_cells;                  /* (map_w * 2) x 128 */
    const uint8_t *roof_cells;                  /* map_w x 64 */
} render_scene;

extern render_scene scene;
extern int scene_wanted;        /* fill the scene (the graphics card draws) */
extern int scene_cpu_too;       /* ... and draw on the CPU as well (tests, the menu) */
extern int scene_cpu_drew;      /* this frame's picture is in vdp.fbmd */

#endif
