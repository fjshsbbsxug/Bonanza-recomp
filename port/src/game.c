/* The game's code: linked in (BONANZA_GAME_STATIC) or loaded from its library. */
#include "game.h"
#include "gamelib.h"
#include <stdio.h>
#include <string.h>

game_code game;
static int loaded;

int game_loaded(void) { return loaded; }

const char *game_lib_name(void)
{
#if defined(_WIN32)
    return "bonanza_game.dll";
#else
    return "libbonanza_game.so";
#endif
}

#ifdef BONANZA_GAME_STATIC

extern const unsigned char m68k_cost[];
extern unsigned long z80r_insns;
extern int z80r_code_changed;

const char *game_load(const char *dir)
{
    (void)dir;
    game.recomp_find = recomp_find;
    game.recomp_invalidate = recomp_invalidate;
    game.recomp_validate_all = recomp_validate_all;
    game.m68k_cost = m68k_cost;
    game.z80r_init = z80r_init;
    game.z80r_gen_int = z80r_gen_int;
    game.z80r_program = z80r_program;
    game.z80r_run = z80r_run;
    game.z80r_insns = &z80r_insns;
    game.z80r_code_changed = &z80r_code_changed;
    game.rom_sha1 = 0;
    loaded = 1;
    return 0;
}

#else

#if defined(_WIN32)
#include <windows.h>
static void *open_lib(const char *path) { return (void *)LoadLibraryA(path); }
static const char *lib_error(void) { static char e[64]; snprintf(e, sizeof(e), "error %lu", (unsigned long)GetLastError()); return e; }
static void *lib_sym(void *h, const char *n) { return (void *)GetProcAddress((HMODULE)h, n); }
#else
#include <dlfcn.h>
static void *open_lib(const char *path) { return dlopen(path, RTLD_NOW | RTLD_LOCAL); }
static const char *lib_error(void) { const char *e = dlerror(); return e ? e : "not found"; }
static void *lib_sym(void *h, const char *n) { return dlsym(h, n); }
#endif

extern const bonanza_sym game_engine_syms[];     /* engine/game_syms.c */

const char *game_load(const char *dir)
{
    static char msg[512], path[1024];
    if (loaded) return 0;
    void *h = 0;
    if (dir && *dir) {
        snprintf(path, sizeof(path), "%s%s", dir, game_lib_name());
        h = open_lib(path);
    }
    static char why[256];
    if (!h) snprintf(why, sizeof(why), "%s", lib_error());
    if (!h) h = open_lib(game_lib_name());    /* Android: in the app's libraries */
    if (!h) {
        snprintf(msg, sizeof(msg), "The game's code (%s) is missing. Make this package with the "
                 "Bonanza Builder from your ROM. (%s)", game_lib_name(), why);
        return msg;
    }
    bonanza_game_fn entry = (bonanza_game_fn)lib_sym(h, BONANZA_GAME_ENTRY);
    bonanza_game_exports ex;
    memset(&ex, 0, sizeof(ex));
    const char *lack = entry ? entry(game_engine_syms, &ex) : "bonanza_game";
    if (lack || ex.abi != BONANZA_GAME_ABI) {
        snprintf(msg, sizeof(msg), "The game's code (%s) does not fit this program (%s). Make the "
                 "package again with the Bonanza Builder.", game_lib_name(), lack ? lack : "version");
        return msg;
    }
    game.recomp_find = (recomp_fn (*)(uint32_t))ex.recomp_find;
    game.recomp_invalidate = (int (*)(uint32_t))ex.recomp_invalidate;
    game.recomp_validate_all = (void (*)(void))ex.recomp_validate_all;
    game.m68k_cost = ex.m68k_cost;
    game.z80r_init = (void (*)(z80 *))ex.z80r_init;
    game.z80r_gen_int = (void (*)(z80 *, uint8_t))ex.z80r_gen_int;
    game.z80r_program = (int (*)(const uint8_t *))ex.z80r_program;
    game.z80r_run = (int (*)(int, z80 *, int *, int))ex.z80r_run;
    game.z80r_insns = ex.z80r_insns;
    game.z80r_code_changed = ex.z80r_code_changed;
    game.rom_sha1 = ex.rom_sha1;
    loaded = 1;
    return 0;
}

#endif
