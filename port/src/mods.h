/* Mods: .bbro files (or folders) in a "mods" folder, applied to the ROM at load time.
 * See mods.c and port/dist/README.txt for the format. */
#ifndef MODS_H
#define MODS_H
#include <stddef.h>
#include <stdint.h>

#define MODS_MAX 64

typedef struct {
    char file[128];          /* file or folder name in the mods folder (its id) */
    char path[600];          /* full path */
    int  is_dir;
    char name[48], author[40], version[16], description[120];
    int  enabled;
    int  patches;            /* patch files found */
    char error[80];          /* why it could not be used ("" = fine) */
} mod_info;

void mods_add_dir(const char *dir);         /* a folder to look in (ends with a slash) */
const char *mods_main_dir(void);            /* where to put new mods (first folder added) */
void mods_scan(void);                       /* (re)read the folders; enabled list from settings */
int  mods_count(void);
mod_info *mods_get(int i);
void mods_toggle(int i);                    /* enable / disable (saved in settings.mods) */
int  mods_enabled_count(void);

/* Apply the enabled mods, in list order, to a copy of the ROM. out must have room for
 * cap bytes; returns the new size (0 on failure: then out holds the unmodified ROM).
 * Recompiled code whose bytes a mod changes is handed to the interpreter. */
size_t mods_apply(const uint8_t *rom, size_t size, uint8_t *out, size_t cap);
const char *mods_status(void);              /* one line about the last mods_apply */

/* a file inside a .bbro / zip archive in memory (stored or deflated); malloc'ed */
uint8_t *zip_extract(const uint8_t *zip, size_t zsize, const char *name, size_t *out_size);

#endif
