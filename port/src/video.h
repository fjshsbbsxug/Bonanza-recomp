/* The game's video stores, written directly (video.c): video memory (tiles, the planes'
 * maps, the sprite table, the horizontal scroll table), the palette, the vertical scroll
 * and the registers - no console ports, no copies set up through registers. */
#ifndef VIDEO_H
#define VIDEO_H
#include <stdint.h>

/* where data goes (the same numbers as the chip's command codes) */
enum { VIDEO_VRAM = 1, VIDEO_CRAM = 3, VIDEO_VSRAM = 5 };

/* the write cursor: data goes to 'to' at byte address 'addr', moving on by register 15 */
void     video_at(int to, uint16_t addr);
void     video_at_long(uint32_t cmd);   /* a command long the game keeps in ROM or RAM */
void     video_put(uint16_t w);         /* one word at the cursor */
void     video_store(int to, uint16_t addr, uint16_t w);

void     video_set(int r, uint8_t v);   /* a register */
void     video_reg_word(uint16_t w);    /* a register word the game keeps (0x8000 | r << 8 | v) */

/* copies and fills; each returns where the destination ended */
uint16_t video_from_ram(int to, uint16_t dst, uint32_t src, uint32_t n, int stride);
uint16_t video_fill(uint16_t dst, uint32_t len, int stride, uint16_t w); /* the word, then len bytes */
uint16_t video_vram_copy(uint16_t dst, uint16_t src, uint32_t len, int stride);

uint16_t video_status(void);            /* what the game's status reads see: never busy */

#endif
