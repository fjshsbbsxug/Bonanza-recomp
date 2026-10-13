/* Extra stages from mods (the arcade's lost levels). See extrastages.c. */
#ifndef EXTRASTAGES_H
#define EXTRASTAGES_H

void xs_scan(void);          /* after a ROM is loaded: find the extra stages in it */
void xs_reset(void);         /* machine reset: back to the cartridge's own tables */
void xs_hook(void);          /* at the entry dispatcher (sub_11106) */
int  xs_count(void);
void xs_play(int k);         /* the next stage started is extra stage k (test play) */

#define GAME_HOOK_STAGE 0x11106

#endif
