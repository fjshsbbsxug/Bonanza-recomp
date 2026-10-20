/* The stage's map (map.c). */
#ifndef LOGIC_MAP_H
#define LOGIC_MAP_H
#include <stdint.h>

struct cell {
    uint8_t walls, walls_up;   /* walls met on the floor / standing on something: by lane */
    uint16_t info;             /* what is there (& $80 stairs) */
    uint32_t at;               /* (where the cell is) */
    uint16_t rest, start;      /* (the floor count's remainder, the floor's first cell) */
};

/* the cell at x, y; floors_down: how many floors down it is */
struct cell map_cell(uint16_t x, uint16_t y, int *floors_down);
/* the lane bits at depth y (and the lane's stairs bits) */
uint8_t map_lane(uint16_t y);
uint8_t map_lane_stairs(uint16_t y);

/* is there a wall in the way of object o at its point (table points) moved by dx, dy, in
 * the lane lane_dy pixels down from it? (sub_176B8 and the others) The cell's word in
 * *info. */
int probe_wall(uint16_t o, uint32_t points, int16_t dx, int16_t dy, int16_t lane_dy, int swapped,
               int stairs, uint16_t *info);

/* the time of the routines, for the parts still counting it */
void map_cell_time(int floors_down, int with_rts);
void wall_test_time(uint32_t routine, int up);

#endif
