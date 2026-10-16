/* The game's code as a library (the native port's distributed builds).
 *
 * The game's code is translated from the player's ROM (tools/recomp.py,
 * tools/z80recomp.py) and built on its own: by the builder pages in the browser
 * (TinyCC), or with the port's build when a ROM is given. The engine (everything
 * else) loads it at start: bonanza_game.dll / libbonanza_game.so next to the program
 * (on Android: in the app's libraries).
 *
 * The library takes nothing from the system: every name it uses from the engine
 * (memory, the CPU's state, the video stores, the drawing routines in C) is a pointer it
 * is given here, by name; it gives back what the engine calls. */
#ifndef GAMELIB_H
#define GAMELIB_H

#define BONANZA_GAME_ABI 2
#define BONANZA_GAME_ENTRY "bonanza_game"

typedef struct { const char *name; void *p; } bonanza_sym;

typedef struct {
    int abi;                            /* BONANZA_GAME_ABI */
    /* the 68000's code */
    void *recomp_find;                  /* recomp_fn (*)(uint32_t pc) */
    void *recomp_invalidate;            /* int (*)(uint32_t addr) */
    void *recomp_validate_all;          /* void (*)(void) */
    const unsigned char *m68k_cost;     /* each instruction's time, by address / 2 */
    /* the Z80's code (the sound driver) */
    void *z80r_init, *z80r_gen_int, *z80r_program, *z80r_run;
    unsigned long *z80r_insns;
    int *z80r_code_changed;
    const char *rom_sha1;               /* the ROM it was made from */
} bonanza_game_exports;

/* the library's one exported function: 0, or the name of an engine symbol it lacks */
typedef const char *(*bonanza_game_fn)(const bonanza_sym *engine, bonanza_game_exports *out);

#endif
