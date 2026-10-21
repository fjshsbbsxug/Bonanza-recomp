/* Bonanza Bros. port - Mega Drive hardware layer */
#ifndef MD_H
#define MD_H

#include <stdint.h>
#include <stddef.h>

#define MD_ROM_SHA1 "f7313ecaf7143873597e3a6ebf58d542d06c3ef3"
#define MD_ROM_SIZE 0x80000

#define MD_LINES        262   /* NTSC */
#define MD_CYC68K_LINE  488   /* 3420 master clocks / 7 */
#define MD_CYCZ80_LINE  228   /* 3420 master clocks / 15 */
#define MD_FPS          60

#define MD_FB_W     576   /* framebuffer stride (pixels); widescreen output can use all of it */
#define MD_SCREEN_H 224

/* Pad buttons (active high in this API) */
enum {
    PAD_UP = 1 << 0, PAD_DOWN = 1 << 1, PAD_LEFT = 1 << 2, PAD_RIGHT = 1 << 3,
    PAD_B = 1 << 4, PAD_C = 1 << 5, PAD_A = 1 << 6, PAD_START = 1 << 7,
};

typedef struct {
    uint8_t  rom[0x400000];
    uint32_t rom_size;
    uint8_t  ram[0x10000];
    uint8_t  zram[0x2000];

    /* I/O */
    uint8_t  io_data[3], io_ctrl[3];
    uint8_t  pad[2];          /* current button state, PAD_* */
    int      region_overseas;  /* 1 = export (US/EU), 0 = Japan */

    /* Z80 */
    int      z80_busreq;       /* 68k holds the Z80 bus */
    int      z80_reset;        /* Z80 held in reset */
    uint32_t z80_bank;         /* 68k bank for 0x8000-0xFFFF */
    int      z80_bank_bit;

    uint32_t frame;
    int      line;
} md_t;

extern md_t md;

/* machine */
int  md_load_rom(const uint8_t *data, size_t size);
const char *md_rom_check(const uint8_t *data, size_t size); /* NULL if OK, else message */
void md_reset(void);
void md_run_frame(void);       /* emulate one 60 Hz frame */

/* video output: XRGB8888, MD_FB_W stride, MD_SCREEN_H lines, picture width md_screen_width() */
const uint32_t *md_framebuffer(void);      /* converted with the current colour settings */
const uint16_t *md_framebuffer_md(void);   /* raw 9-bit Mega Drive colours (>= 512: black) */
int md_screen_width(void);
void md_set_aspect(double aspect);   /* 4/3 original, wider = widescreen */

/* audio: interleaved stereo int16 samples produced during the last frame */
void md_audio_set_rate(int rate);
int  md_audio_take(int16_t *out, int max_frames);

/* sound alone (the audio recorder, sndrec.c): the driver loaded as the game does it,
 * only the Z80 and the sound chips run */
void md_snd_begin(int ym_mute, int psg_mute);
void md_snd_command(uint8_t id);
void md_snd_frame(void);            /* one frame of it (also the game's sound, after its frame,
                                       when there are no recorded sounds) */
#define MD_SND_STEMS 4              /* other channels, FM5, FM6, PSG3 + noise */
int  md_snd_take(int16_t *out[MD_SND_STEMS], int max);
void md_snd_end(void);

/* bus (used by the CPU cores) */
uint32_t md_read8(uint32_t a);
uint32_t md_read16(uint32_t a);
void     md_write8(uint32_t a, uint32_t v);
void     md_write16(uint32_t a, uint32_t v);

/* 68k core glue (cpu68k.c) */
void cpu_init(void);
void cpu_reset(void);
int  cpu_run_until_idle(int cycles); /* the frame driver: run the code until it is idle */
void cpu_run_interrupt(void);   /* a waiting interrupt's routine, run to its return */
int  cpu_waiting(void);          /* paused where the frame routine waits for the picture */
int  cpu_wait_over(void);        /* ... and what it waits for has happened */
void cpu_set_irq(int level);
uint32_t cpu_get_pc(void);
uint32_t cpu_get_sp(void);
/* the game's routine at addr run as a function call from C (logic.c), as a jsr whose
 * next instruction is ret would: back when it returns there */
void cpu_call(uint32_t addr, uint32_t ret);
/* run the game's code from addr (a part of a routine) until it reaches one of the
 * places stops[] (tools/recomp.py STOPS): the address reached */
uint32_t cpu_run_part(uint32_t addr, const uint32_t *stops, int n);
extern int cpu_stopping;
extern uint32_t cpu_call_ret;      /* where the innermost cpu_call() waits for the routine's rts */
int cpu_stop_here(uint32_t addr);

/* save state of the whole machine to a buffer (for differential testing) */
uint32_t md_state_hash(void);

#endif
