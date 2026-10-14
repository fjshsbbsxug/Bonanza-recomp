/* Title-screen code: Left, Left, Right, Up, Shoot, Jump on the 1 PLAYER / 2 PLAYERS /
 * OPTIONS menu unlocks the stage select the cartridge has but never shows.
 *
 * The Options menu has a sixth item, STAGE (sub_03E52 / sub_03F48: stages 1-10, the
 * three coffee breaks and the ending, written to the stage number 0xFFC05A, which a new
 * game keeps), but its cursor wraps after EXIT and the menu never draws the line. The
 * code changes two operands in the ROM buffer, which the recompiled code reads from it
 * as it runs (tools/recomp.py LIVE):
 *   0x3C24  Up on the first item goes to item 5 (STAGE), not 4
 *   0x3C44  Down wraps after item 5, not 4
 * and the menu's "draw EXIT" call at 0x3B08 (a hook, gamemods.c) also draws STAGE:
 * sub_011F2 (EXIT) returns into sub_03F48 (STAGE and its value), which returns to the
 * menu. No new 68000 code: everything stays native. The operands are put back at a
 * machine reset (the boot code checksums the ROM).
 *
 * Only offline: the front ends call cheat_frame() only for frames they run on their own,
 * never in an online game. */
#include <stdlib.h>
#include "cheats.h"
#include "md.h"
#include "net.h"
#include "gamemods.h"
#include "m68k.h"
#include "m68kcpu.h"
#include "recomp.h"

typedef struct { uint32_t addr; uint8_t old; } undo_rec;
static undo_rec undo[32];
static int nundo, unlocked, step;

enum { C_LEFT = 1, C_RIGHT, C_UP, C_SHOOT, C_JUMP };
static const int CODE[] = { C_LEFT, C_LEFT, C_RIGHT, C_UP, C_SHOOT, C_JUMP };
#define CODE_LEN ((int)(sizeof(CODE) / sizeof(CODE[0])))

static int ram8(uint32_t a) { return md.ram[a & 0xFFFF]; }
static int ram16(uint32_t a) { return ram8(a) << 8 | ram8(a + 1); }
static uint32_t ram32(uint32_t a) { return (uint32_t)ram16(a) << 16 | (uint32_t)ram16(a + 2); }

static void put8(uint32_t a, uint8_t v)
{
    if (nundo == (int)(sizeof(undo) / sizeof(undo[0]))) return;
    undo[nundo].addr = a;
    undo[nundo].old = md.rom[a];
    nundo++;
    md.rom[a] = v;
    recomp_invalidate(a);
}
void cheat_reset(void)
{
    while (nundo) {
        nundo--;
        md.rom[undo[nundo].addr] = undo[nundo].old;
    }
    unlocked = 0;
    step = 0;
    game_mods_apply();
}

int cheat_stage_select(void) { return unlocked; }

static void unlock_stage_select(void)
{
    put8(0x3C25, 5);
    put8(0x3C45, 5);
    unlocked = 1;
    game_mods_apply();                    /* turns on the menu hook */
    /* the game's sound request word: a chime, unless a music command is waiting */
    if ((ram16(0xFF5668) & 0xFFF0) != 0x00E0) {
        md.ram[0x5668] = 0x00;
        md.ram[0x5669] = 0xB0;
    }
}

/* at 0x3B08 (jsr sub_011F2, drawing EXIT): call it so that it returns into sub_03F48 */
int cheat_menu_hook(void)
{
    if (!unlocked) return 0;
    m68ki_push_32(0x3B0E);                /* where the menu goes on */
    m68ki_push_32(0x3F48);                /* sub_03F48: STAGE and its value */
    REG_PC = 0x11F2;                      /* sub_011F2: EXIT */
    USE_CYCLES(52);
    return 2;
}

/* what this frame's new press means: one of C_*, 0 for nothing, -1 for anything else */
static int press(int newly)
{
    int ctl = ram16(0xFFC058);
    if (!newly) return 0;
    if (newly & 0x04) return C_LEFT;
    if (newly & 0x08) return C_RIGHT;
    if (newly & 0x01) return C_UP;
    if ((newly & 0x10) || ((newly & 0x20) && ctl == 1)) return C_SHOOT;
    if ((newly & 0x40) || ((newly & 0x20) && ctl == 0)) return C_JUMP;
    return -1;
}

void cheat_frame(void)
{
    if (unlocked || net_state() == NET_PLAYING) return;
    /* the title menu (sub_037DE), before Start was pressed on it */
    if (ram32(0xFFC068) != 0x000037C0 || ram16(0xFF6C5C) != 0) { step = 0; return; }
    for (int p = 0; p < 2; p++) {
        int k = press(ram8(0xFF17C1 + 2 * p));     /* newly pressed, after the CONTROL remap */
        if (!k) continue;
        if (k == CODE[step]) step++;
        else step = k == C_LEFT ? (step == 2 ? 2 : 1) : 0;    /* a third Left: still Left, Left */
        if (step == CODE_LEN) {
            unlock_stage_select();
            return;
        }
    }
}
