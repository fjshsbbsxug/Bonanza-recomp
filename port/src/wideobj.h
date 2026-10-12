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

#endif
