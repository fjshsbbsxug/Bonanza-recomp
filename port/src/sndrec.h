/* Recording the game's sounds from the user's ROM, once. See sndrec.c. */
#ifndef SNDREC_H
#define SNDREC_H

/* the ROM must be loaded (md_load_rom); writes the bank to path (0 on success). The
 * machine must be reset (md_reset) afterwards. */
int snd_record_bank(const char *path, void (*progress)(int done, int total, void *u), void *u);

/* the same, a bit at a time (a window or page stays responsive): step runs up to the
 * given number of frames of sound; 0 working, 1 done, -1 the file could not be written */
typedef struct sndrec sndrec;
sndrec *snd_record_start(const char *path);
int  snd_record_step(sndrec *r, int frames);
void snd_record_progress(const sndrec *r, int *done, int *total);
void snd_record_free(sndrec *r);

#endif
