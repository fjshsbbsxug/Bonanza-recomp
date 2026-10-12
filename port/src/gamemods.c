/* Floor Alert (option): guards on a player's floor keep going when they are off screen.
 *
 * The game only runs a guard while it is near a view (sub_058B4: within 144 pixels plus
 * its half width of the view's centre, same floor). A guard that drops out of that is
 * frozen, or put straight back at its post in its neutral state (sub_14232). With Floor
 * Alert every guard on the same floor as a player counts as near that player's view for
 * the game's logic, so it keeps walking, chasing and remembering you until you change
 * floors; it never teleports or disappears. Sprites are still only made for guards near
 * the picture (the game does not clip sprites; far ones would wrap round the screen);
 * the widescreen renderer draws the others beside the picture.
 *
 * This changes the game, so it is off by default and only active while chosen. */
#include <stdlib.h>
#include "gamemods.h"
#include "md.h"
#include "settings.h"
#include "m68k.h"
#include "m68kcpu.h"

int game_hooks;
uint8_t game_drawn_vis[0x56];

static int16_t r16(uint32_t a) { return (int16_t)((md.ram[a & 0xFFFF] << 8) | md.ram[(a + 1) & 0xFFFF]); }
static void w16(uint32_t a, int v) { md.ram[a & 0xFFFF] = (uint8_t)(v >> 8); md.ram[(a + 1) & 0xFFFF] = (uint8_t)v; }

/* sub_058B4 for one view: 1 if the object is near the view's centre */
static int near_view(int o, int v)
{
    uint16_t on = (uint16_t)r16(v ? 0x2EF8 : 0x2EF6);
    if (!on || on == 0x8000) return 0;
    uint16_t dx = (uint16_t)abs((int16_t)(r16(v ? 0x229A : 0x1856) - r16(0x304E + o)));
    if (dx > (uint16_t)(r16(0x3706 + o) + 0x90)) return 0;
    uint16_t dy = (uint16_t)abs((int16_t)(r16(v ? 0x2298 : 0x1854) + 0x10 + r16(0x31A6 + o) - r16(0x30FA + o)));
    return dy <= (uint16_t)(r16(0x37B2 + o) + 0x28);
}

/* a guard on the floor of player v (players are objects 0 and 1) */
static int on_players_floor(int o, int v)
{
    uint16_t on = (uint16_t)r16(v ? 0x2EF8 : 0x2EF6);
    if (!on || on == 0x8000) return 0;
    int i = o >> 1, first = 6, n = r16(0x6CC2) + 1;
    if (i < first || i >= first + n) return 0;
    return abs(r16(0x30FA + o) - r16(0x30FA + v * 2)) < 40;
}

int game_hook(uint32_t pc)
{
    if (!settings.floor_alert) return 0;
    int o = REG_D[0] & 0xFFFF;
    if (o >= 0x56 * 2 || (o & 1)) return 0;
    uint32_t ret = m68ki_read_32(REG_A[7]) & 0xFFFFFF;
    if (pc == GAME_HOOK_VISIBILITY) {
        /* only the per-frame pass over all objects (sub_0582E) is widened; the guard
         * loop's own "is its post on screen" test stays as it was */
        if (ret < 0x582E || ret >= 0x58B4) return 0;
        int drawn = near_view(o, 0) | near_view(o, 1) << 1;
        int run = drawn | on_players_floor(o, 0) | on_players_floor(o, 1) << 1;
        game_drawn_vis[o >> 1] = (uint8_t)drawn;
        w16(0x2FA2 + o, run);
        REG_D[5] = 0;
        return 1;
    }
    if (pc == GAME_HOOK_SPRITES) {
        /* no sprites for guards far from the picture */
        return !game_drawn_vis[o >> 1];
    }
    return 0;
}

void game_mods_apply(void)
{
    game_hooks = settings.floor_alert != 0;
}
