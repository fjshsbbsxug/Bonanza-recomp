/* The game's code (translated from the ROM: port/gen) as the engine calls it: linked
 * into the program (a build made with the ROM), or a library loaded at start
 * (gamelib.h: the distributed builds, made by the builder pages). */
#ifndef GAME_H
#define GAME_H
#include <stdint.h>
#include "recomp.h"
#include "z80recomp.h"

typedef struct {
    recomp_fn (*recomp_find)(uint32_t pc);
    int (*recomp_invalidate)(uint32_t addr);
    void (*recomp_validate_all)(void);
    const unsigned char *m68k_cost;           /* each instruction's time, by address / 2 */
    void (*z80r_init)(z80 *z);
    void (*z80r_gen_int)(z80 *z, uint8_t data);
    int (*z80r_program)(const uint8_t *zram);
    int (*z80r_run)(int program, z80 *z, int *lc, int limit);
    unsigned long *z80r_insns;
    int *z80r_code_changed;
    const char *rom_sha1;                     /* the ROM it was made from */
} game_code;

extern game_code game;

/* finds the game's code: linked in, or the library in dir (0: next to the program).
 * 0 when it is there, else what is wrong (for the player). */
const char *game_load(const char *dir);
int game_loaded(void);

/* the library's file name on this system */
const char *game_lib_name(void);

#endif
