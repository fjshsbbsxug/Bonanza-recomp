/* Recording the game's sounds from the user's ROM, once. See sndrec.c. */
#ifndef SNDREC_H
#define SNDREC_H
#include <stdint.h>

/* the ROM must be loaded (md_load_rom); writes the bank to path (0 on success). The
 * machine must be reset (md_reset) afterwards. */
int snd_record_bank(const char *path, void (*progress)(int done, int total, void *u), void *u);

/* the same, a bit at a time (a window or page stays responsive): step runs up to the
 * given number of frames of sound; 0 working, 1 done, -1 the file could not be written */
typedef struct sndrec sndrec;
sndrec *snd_record_start(const char *path);
/* only these sounds (a bank of part of them: the builder pages record the sounds side
 * by side and put the parts together, port/src/sndrec_web.c) */
sndrec *snd_record_start_ids(const char *path, const uint8_t *ids, int n);
/* every sound the game can ask for, in the bank's order (returns how many, at most 64) */
int  snd_record_ids(uint8_t ids[64]);
int  snd_record_step(sndrec *r, int frames);
void snd_record_progress(const sndrec *r, int *done, int *total);
void snd_record_free(sndrec *r);

#endif
