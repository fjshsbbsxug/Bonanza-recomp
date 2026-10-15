/* The sound recorder alone, for the builder pages (WebAssembly: bonanza-sounds.js).
 *
 * The page records the game's sounds from the player's ROM while it builds the package,
 * so the game has them from its first start. The recorder is the game's (sndrec.c,
 * md_snd_*: the sound chips and the game's sound driver, loaded from the ROM as its boot
 * code does it), run on every core the computer has at once: each page worker records
 * one sound at a time into a bank of its own, and the page puts the banks together in
 * the game's order (builder/sounds.js); the result is the same file, byte for byte, as
 * the game records itself.
 *
 * Nothing of the game is built in: the driver is the Z80 code in the ROM, run here by
 * the Z80 core (ext/z80) rather than as recompiled code, cycle for cycle as md.c's step
 * loop ran it (the recompiled driver was checked against it: the same sound). */
#include <stdlib.h>
#include <string.h>
#include <emscripten.h>
#include "md.h"
#include "game.h"
#include "sndbank.h"
#include "sndrec.h"
#include "../ext/z80/z80.h"

static int code_changed;
static unsigned long insns;

static void z_init(z80 *z) { z80_init(z); }
static void z_gen_int(z80 *z, uint8_t data) { z80_gen_int(z, data); }
static int z_program(const uint8_t *zram) { (void)zram; return 0; }
static int z_run(int program, z80 *z, int *lc, int limit)
{
    (void)program;
    while (*lc < limit) {
        unsigned long c0 = z->cyc;
        z80_step(z);
        *lc += (int)(z->cyc - c0);
        insns++;
    }
    return 1;
}

static void setup(void)
{
    game.z80r_init = z_init;
    game.z80r_gen_int = z_gen_int;
    game.z80r_program = z_program;
    game.z80r_run = z_run;
    game.z80r_insns = &insns;
    game.z80r_code_changed = &code_changed;
}

/* the sounds, in the bank's order: written to ids (64 bytes), returns how many */
EMSCRIPTEN_KEEPALIVE int sndweb_ids(uint8_t *ids) { return snd_record_ids(ids); }

/* the ROM (once per worker) */
EMSCRIPTEN_KEEPALIVE int sndweb_rom(const uint8_t *rom, int size)
{
    setup();
    return md_load_rom(rom, (size_t)size);
}

/* these sounds into a lossless bank at path: 0 on success */
EMSCRIPTEN_KEEPALIVE int sndweb_record(const uint8_t *ids, int n, const char *path)
{
    sb_write_lossless = 1;
    sndrec *r = snd_record_start_ids(path, ids, n);
    if (!r) return -1;
    int res;
    while ((res = snd_record_step(r, 600)) == 0) {}
    snd_record_free(r);
    return res < 0 ? -1 : 0;
}

int main(void) { return 0; }
