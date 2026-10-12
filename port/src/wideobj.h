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
/* objects on the floor shown by player 1's view while the views are stacked (vdp.c) */
/* the player (0 Robo, 1 Mobo) a sprite piece belongs to, or -1 */
int wobj_player_of(uint16_t attr);
int wobj_build_stacked(int vtop, int real_wy0, wobj_sprite *list, int max);

#endif
