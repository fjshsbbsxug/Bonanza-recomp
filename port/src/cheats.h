/* Codes typed on the title screen (offline only). See cheats.c. */
#ifndef CHEATS_H
#define CHEATS_H

void cheat_frame(void);         /* after each offline frame: watch the title menu's buttons */
void cheat_reset(void);         /* machine reset: the cartridge as it was */
int  cheat_stage_select(void);  /* the stage select is unlocked */

#endif
