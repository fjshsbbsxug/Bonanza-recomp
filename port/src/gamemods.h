/* Optional changes to how the game plays (off by default). See gamemods.c. */
#ifndef GAMEMODS_H
#define GAMEMODS_H
#include <stdint.h>

extern int game_hooks;              /* nonzero: the hooked routines call game_hook() */
extern uint8_t game_drawn_vis[0x56];/* per object: views the game draws it in (Floor Alert) */

/* Called at a hooked place; returns 1 if the routine starting there was handled (the
 * caller then returns from it as if it had run), 2 if the hook set REG_PC itself, 0 to
 * run the game's code. */
extern int game_hook(uint32_t pc);
void game_mods_apply(void);         /* after settings change */

#define GAME_HOOK_VISIBILITY 0x58B4  /* sub_058B4: is an object near a view's centre */
#define GAME_HOOK_SPRITES    0x59C6  /* sub_059C6: make an object's sprites */
#define GAME_HOOK_MENU       0x3B08  /* Options menu: the call that draws EXIT (cheats.c) */

#endif
