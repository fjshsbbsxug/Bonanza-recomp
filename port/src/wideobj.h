/* Widescreen: sprites for the game's objects beside the 4:3 picture (see wideobj.c). */
#ifndef WIDEOBJ_H
#define WIDEOBJ_H
#include <stdint.h>

typedef struct {
    int x, y;               /* screen position of the top left corner */
    int hsz, vsz;           /* size in cells */
    uint16_t attr;          /* name table style attribute word */
    int ymin, ymax;         /* lines of the view it belongs to */
    int only_outside;       /* draw only outside the 4:3 picture */
} wobj_sprite;

/* Call once per frame, before the first line is drawn. Returns the number of sprites. */
int wobj_build(int margin, int width, int patrol_return, wobj_sprite *list, int max);
void wobj_reset(void);
/* the player (0 Robo, 1 Mobo) a sprite piece belongs to, or -1 */
int wobj_player_of(uint16_t attr);
int wobj_sample_players(void);    /* every line: the players' frame numbers this picture */
/* A view showing another part of the level than the game's camera of view v (the floor
 * above, the floor below; render.c): the objects as they lie in the world, moved down
 * by shift lines, whose feet are on that view's lines (l0 ..), drawn on lines ymin ..
 * ymax-1 only (where the game's own sprites do not reach). Appended to list from n;
 * returns the new count. */
int wobj_build_shifted(int v, int shift, int l0, int ymin, int ymax, wobj_sprite *list, int n, int max);
/* Prop Hunt: object obj drawn with another frame (prophunt.c) */
int wobj_build_disguise(int obj, uint16_t frame, uint16_t attr_or, wobj_sprite *list, int max);

#endif
