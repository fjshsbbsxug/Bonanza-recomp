/* Mega Drive machine: memory map, I/O, Z80 + sound, the frame (md_run_frame). */
#include <string.h>
#include <stdio.h>
#include "extrastages.h"
#include "game.h"
#include "z80recomp.h"
#include "sndmix.h"
#include "cheats.h"
#include "insane.h"
#include "md.h"
#include "vdp.h"
#include "psg.h"
#include "sha1.h"
#include "settings.h"
#include "../ext/z80/z80.h"
#include "../ext/nuked-opn2/ym3438.h"

md_t md;

static z80 zcpu;
static ym3438_t ym;

/* ------------------------------------------------------------ timing */

#define MASTER_LINE     3420
#define YM_DIV          42          /* master clocks per OPN2_Clock */
#define YM_RATE         (53693175.0 / 7 / 144) /* NTSC master/7/144 */

static uint64_t master_line_start;   /* master clock at the start of the current line */
static uint64_t ym_time;             /* master clock the YM has been run up to */
static int      ym_phase;            /* 0..23 clock within the current sample */
static int32_t  ym_acc[2];
static int      z80_line_cyc;        /* Z80 cycles executed in this line */
static int      zram_dirty = 1;      /* the programs' part of Z80 RAM may have changed */
static int      z80_prog = -1;       /* the Z80 program in RAM (z80recomp.h), -1: none known */
unsigned long   z80_stat_no_code;    /* Z80 cycles spent where no native program was */

/* audio output */
static int      out_rate = 48000;
static double   resample_pos;
static int16_t  last_in[2], prev_in[2];
#define AUDIO_BUF 16384
static int16_t  audio_buf[AUDIO_BUF * 2];
static int      audio_len;

static void push_output(int16_t l, int16_t r)
{
    if (audio_len < AUDIO_BUF) {
        audio_buf[audio_len * 2] = l;
        audio_buf[audio_len * 2 + 1] = r;
        audio_len++;
    }
}

static void ym_emit_sample(int32_t l, int32_t r)
{
    int16_t p = psg_sample(3579545.0 / 16.0 / YM_RATE);
    l = l * 6 + p;
    r = r * 6 + p;
    if (l > 32767) l = 32767; else if (l < -32768) l = -32768;
    if (r > 32767) r = 32767; else if (r < -32768) r = -32768;
    prev_in[0] = last_in[0]; prev_in[1] = last_in[1];
    last_in[0] = (int16_t)l; last_in[1] = (int16_t)r;
    /* linear resample YM_RATE -> out_rate */
    double step = YM_RATE / out_rate;
    while (resample_pos < 1.0) {
        double t = resample_pos;
        push_output((int16_t)(prev_in[0] + (last_in[0] - prev_in[0]) * t),
                    (int16_t)(prev_in[1] + (last_in[1] - prev_in[1]) * t));
        resample_pos += step;
    }
    resample_pos -= 1.0;
}

/* recording (sndrec.c): the output in parts, by the channels the sound effects take
 * over: 0 everything else, 1 FM5, 2 FM6, 3 PSG3 + noise */
static int      snd_stems;
static int32_t  st_acc[MD_SND_STEMS][2];
static int16_t  st_last[MD_SND_STEMS][2], st_prev[MD_SND_STEMS][2];
static int16_t  st_buf[MD_SND_STEMS][4096 * 2];
static int      st_len;

static int16_t clamp16(int32_t v) { return (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }

static void ym_emit_stems(void)
{
    int16_t pm, p3;
    psg_sample_parts(3579545.0 / 16.0 / YM_RATE, 0x3, &pm, &p3);
    int32_t v[MD_SND_STEMS][2] = {
        { st_acc[0][0] * 6 + pm, st_acc[0][1] * 6 + pm },
        { st_acc[1][0] * 6, st_acc[1][1] * 6 },
        { st_acc[2][0] * 6, st_acc[2][1] * 6 },
        { p3, p3 },
    };
    for (int s = 0; s < MD_SND_STEMS; s++)
        for (int c = 0; c < 2; c++) {
            st_prev[s][c] = st_last[s][c];
            st_last[s][c] = clamp16(v[s][c]);
            st_acc[s][c] = 0;
        }
    double step = YM_RATE / out_rate;
    while (resample_pos < 1.0) {
        double t = resample_pos;
        if (st_len < 4096) {
            for (int s = 0; s < MD_SND_STEMS; s++)
                for (int c = 0; c < 2; c++)
                    st_buf[s][st_len * 2 + c] = (int16_t)(st_prev[s][c] + (st_last[s][c] - st_prev[s][c]) * t);
            st_len++;
        }
        resample_pos += step;
    }
    resample_pos -= 1.0;
}

static void ym_sync(uint64_t target)
{
    Bit16s buf[2];
    if (snd_ready() && !snd_stems) {      /* recorded sounds: the sound chips do not run */
        ym_time = target;
        return;
    }
    if (snd_stems) {
        while (ym_time + YM_DIV <= target) {
            OPN2_Clock(&ym, buf);
            int s = ym.out_ch == 4 ? 1 : ym.out_ch == 5 ? 2 : 0;
            st_acc[s][0] += buf[0];
            st_acc[s][1] += buf[1];
            ym_time += YM_DIV;
            if (++ym_phase == 24) {
                ym_phase = 0;
                ym_emit_stems();
            }
        }
        return;
    }
    while (ym_time + YM_DIV <= target) {
        OPN2_Clock(&ym, buf);
        ym_acc[0] += buf[0];
        ym_acc[1] += buf[1];
        ym_time += YM_DIV;
        if (++ym_phase == 24) {
            ym_phase = 0;
            ym_emit_sample(ym_acc[0], ym_acc[1]);
            ym_acc[0] = ym_acc[1] = 0;
        }
    }
}

static uint64_t now_z80(void) { return master_line_start + (uint64_t)z80_line_cyc * 15; }
/* the 68000's frame takes no sound time: the sound chips run after it (md_snd_frame) */
static uint64_t now_68k(void) { return master_line_start; }

void md_audio_set_rate(int rate) { out_rate = rate; }

int md_audio_take(int16_t *out, int max_frames)
{
    int n = audio_len < max_frames ? audio_len : max_frames;
    memcpy(out, audio_buf, n * 4);
    memmove(audio_buf, audio_buf + n * 2, (audio_len - n) * 4);
    audio_len -= n;
    return n;
}

static void ym_write(uint64_t t, int port, uint8_t v)
{
    ym_sync(t);
    OPN2_Write(&ym, port, v);
}

/* ------------------------------------------------------------ Z80 bus */

static uint8_t z80_bus_read(uint32_t a, uint64_t t)
{
    a &= 0xFFFF;
    if (a < 0x4000)
        return md.zram[a & 0x1FFF];
    if (a < 0x6000) {
        ym_sync(t);
        return OPN2_Read(&ym, a & 3);
    }
    if (a >= 0x8000)
        return (uint8_t)md_read8(md.z80_bank | (a & 0x7FFF));
    return 0xFF;
}

static void z80_bus_write(uint32_t a, uint8_t v, uint64_t t)
{
    a &= 0xFFFF;
    if (a < 0x4000) {
        md.zram[a & 0x1FFF] = v;
        if ((a & 0x1FFF) < Z80R_LEN) zram_dirty = *game.z80r_code_changed = 1;
    } else if (a < 0x6000) {
        ym_write(t, a & 3, v);
    } else if (a < 0x6100) {
        md.z80_bank = ((md.z80_bank >> 1) | ((uint32_t)(v & 1) << 23)) & 0xFF8000;
    } else if (a >= 0x7F00 && a < 0x7F20) {
        if (a == 0x7F11 || a == 0x7F13 || a == 0x7F15 || a == 0x7F17)
            psg_write(v);
    } else if (a >= 0x8000) {
        md_write8(md.z80_bank | (a & 0x7FFF), v);
    }
}

static uint8_t zr(void *u, uint16_t a) { (void)u; return z80_bus_read(a, now_z80()); }
static void zw(void *u, uint16_t a, uint8_t v) { (void)u; z80_bus_write(a, v, now_z80()); }
static uint8_t zin(z80 *z, uint8_t p) { (void)z; (void)p; return 0xFF; }
static void zout(z80 *z, uint8_t p, uint8_t v) { (void)z; (void)p; (void)v; }

static int z80_running(void) { return !md.z80_busreq && !md.z80_reset; }

/* the 68000 writing Z80 RAM: with the recorded sounds (sndmix.c), the game's requests
 * to the sound driver go to the player */
static void z80_write_68k(uint32_t a, uint8_t v)
{
    if (snd_ready()) {
        if (a == 0x1C0A) snd_request(v);
        else if (a == 0x1C10) snd_pause(v);
    }
    z80_bus_write(a, v, now_68k());
}

/* the Z80's programs (the sound driver the 68000 copies at boot and never changes, the
 * boot stub before it) run as recompiled code (tools/z80recomp.py) */
static int z80_program(void)
{
    if (zram_dirty) {
        z80_prog = game.z80r_program(md.zram);
        zram_dirty = 0;
    }
    return z80_prog;
}

static void z80_do_reset(void)
{
    game.z80r_init(&zcpu);
    zcpu.read_byte = zr;
    zcpu.write_byte = zw;
    zcpu.port_in = zin;
    zcpu.port_out = zout;
    md.z80_bank = 0;
}

/* ------------------------------------------------------------ I/O */

static uint8_t pad_read(int port)
{
    uint8_t ctrl = md.io_ctrl[port];
    uint8_t th = (ctrl & 0x40) ? (md.io_data[port] & 0x40) : 0x40;
    uint8_t in;
    if (port >= 2)
        in = 0x7F;
    else {
        uint8_t b = md.pad[port];
        if (th)
            in = 0x40 | (~b & 0x3F);
        else
            in = (~b & 0x03) | ((~b >> 2) & 0x30);
    }
    return (md.io_data[port] & ctrl & 0x7F) | (in & ~ctrl & 0x7F) | (md.io_data[port] & 0x80);
}

static uint8_t io_read(uint32_t a)
{
    switch ((a >> 1) & 0x0F) {
    case 0: return md.region_overseas ? 0xA0 : 0x20;
    case 1: return pad_read(0);
    case 2: return pad_read(1);
    case 3: return pad_read(2);
    case 4: return md.io_ctrl[0];
    case 5: return md.io_ctrl[1];
    case 6: return md.io_ctrl[2];
    default: return 0;
    }
}

static void io_write(uint32_t a, uint8_t v)
{
    int r = (a >> 1) & 0x0F;
    if (r >= 1 && r <= 3)
        md.io_data[r - 1] = v;
    else if (r >= 4 && r <= 6)
        md.io_ctrl[r - 4] = v;
}

/* ------------------------------------------------------------ 68k bus */

uint32_t md_read8(uint32_t a)
{
    a &= 0xFFFFFF;
    if (a < 0x400000)
        return md.rom[a];
    if (a >= 0xE00000)
        return md.ram[a & 0xFFFF];
    if (a >= 0xC00000 && a < 0xC00020) {
        uint16_t w;
        if (a < 0xC00004) w = video_port_read();
        else if (a < 0xC00008) w = video_port_status();
        else if (a < 0xC00010) w = video_port_hv();
        else return 0xFF;
        return (a & 1) ? (w & 0xFF) : (w >> 8);
    }
    if (a >= 0xA00000 && a < 0xA10000)
        return md.z80_busreq || md.z80_reset ? z80_bus_read(a & 0x7FFF, now_68k()) : 0xFF;
    if (a >= 0xA10000 && a < 0xA10020)
        return io_read(a);
    if (a == 0xA11100)
        return (md.z80_busreq && !md.z80_reset) ? 0x00 : 0x01;
    if (a == 0xA11101)
        return 0;
    return 0xFF;
}

uint32_t md_read16(uint32_t a)
{
    a &= 0xFFFFFE;
    if (a < 0x400000)
        return (md.rom[a] << 8) | md.rom[a + 1];
    if (a >= 0xE00000)
        return (md.ram[a & 0xFFFF] << 8) | md.ram[(a + 1) & 0xFFFF];
    if (a >= 0xC00000 && a < 0xC00020) {
        if (a < 0xC00004) return video_port_read();
        if (a < 0xC00008) return video_port_status();
        if (a < 0xC00010) return video_port_hv();
        return 0xFFFF;
    }
    if (a >= 0xA00000 && a < 0xA10000) {
        uint32_t b = md_read8(a);
        return (b << 8) | b;
    }
    if (a >= 0xA10000 && a < 0xA10020) {
        uint32_t b = io_read(a | 1);
        return (b << 8) | b;
    }
    if (a == 0xA11100)
        return (md_read8(a) << 8) | 0;
    return 0xFFFF;
}

void md_write8(uint32_t a, uint32_t v)
{
    a &= 0xFFFFFF;
    v &= 0xFF;
    if (a >= 0xE00000) {
        md.ram[a & 0xFFFF] = (uint8_t)v;
        return;
    }
    if (a >= 0xC00000 && a < 0xC00020) {
        if (a < 0xC00004) video_port_data((uint16_t)(v | (v << 8)));
        else if (a < 0xC00008) video_port_ctrl((uint16_t)(v | (v << 8)));
        else if (a >= 0xC00010 && a < 0xC00018 && (a & 1)) psg_write((uint8_t)v);
        return;
    }
    if (a >= 0xA00000 && a < 0xA10000) {
        if (md.z80_busreq || md.z80_reset)
            z80_write_68k(a & 0x7FFF, (uint8_t)v);
        return;
    }
    if (a >= 0xA10000 && a < 0xA10020) {
        io_write(a, (uint8_t)v);
        return;
    }
    if (a == 0xA11100) {
        md.z80_busreq = v & 1;
        return;
    }
    if (a == 0xA11200) {
        int reset = !(v & 1);
        if (reset && !md.z80_reset) {
            ym_sync(now_68k());
            OPN2_Reset(&ym);
        }
        if (!reset && md.z80_reset)
            z80_do_reset();
        md.z80_reset = reset;
        return;
    }
}

void md_write16(uint32_t a, uint32_t v)
{
    a &= 0xFFFFFE;
    v &= 0xFFFF;
    if (a >= 0xE00000) {
        md.ram[a & 0xFFFF] = (uint8_t)(v >> 8);
        md.ram[(a + 1) & 0xFFFF] = (uint8_t)v;
        return;
    }
    if (a >= 0xC00000 && a < 0xC00020) {
        if (a < 0xC00004) video_port_data((uint16_t)v);
        else if (a < 0xC00008) video_port_ctrl((uint16_t)v);
        else if (a >= 0xC00010 && a < 0xC00018) psg_write((uint8_t)v);
        return;
    }
    if (a >= 0xA00000 && a < 0xA10000) {
        md_write8(a, v >> 8);
        return;
    }
    if (a >= 0xA10000 && a < 0xA10020) {
        io_write(a | 1, (uint8_t)v);
        return;
    }
    if (a == 0xA11100 || a == 0xA11200) {
        md_write8(a, v >> 8);
        return;
    }
}

/* ------------------------------------------------------------ machine */

const char *md_rom_check(const uint8_t *data, size_t size)
{
    char hex[41];
    if (size != MD_ROM_SIZE)
        return "This is not the Bonanza Bros. (JUE) ROM: wrong size (expected 512 KiB).";
    sha1_hex(data, size, hex);
    if (strcmp(hex, MD_ROM_SHA1) != 0)
        return "This is not the Bonanza Bros. (JUE) ROM: SHA-1 does not match.";
    return NULL;
}

int md_load_rom(const uint8_t *data, size_t size)
{
    if (size > sizeof(md.rom))
        return -1;
    memset(md.rom, 0xFF, sizeof(md.rom));
    memcpy(md.rom, data, size);
    md.rom_size = (uint32_t)size;
    md.region_overseas = 1;
    xs_scan();
    return 0;
}

void md_reset(void)
{
    memset(md.ram, 0, sizeof(md.ram));
    memset(md.zram, 0, sizeof(md.zram));
    zram_dirty = 1;
    memset(md.io_data, 0, sizeof(md.io_data));
    memset(md.io_ctrl, 0, sizeof(md.io_ctrl));
    md.z80_busreq = 0;
    md.z80_reset = 1;
    md.frame = 0;
    vdp_reset();
    psg_reset();
    OPN2_SetChipType(ym3438_mode_ym2612);
    OPN2_Reset(&ym);
    z80_do_reset();
    master_line_start = ym_time = 0;
    ym_phase = 0;
    xs_reset();
    cheat_reset();
    insane_reset();
    snd_reset();
    snd_stems = 0;
    cpu_init();
    cpu_reset();
}

/* the Z80's share of a line */
static void z80_line(void)
{
    if (z80_running()) {
        while (z80_line_cyc < MD_CYCZ80_LINE) {
            if (z80_program() >= 0) {
                int r = game.z80r_run(z80_prog, &zcpu, &z80_line_cyc, MD_CYCZ80_LINE);
                if (r == 1) break;
                if (r == 2) continue;         /* its code was written: look again */
            }
            if (z80_line_cyc >= MD_CYCZ80_LINE)
                break;
            /* no native program at its PC (never, for the game): it waits out the line */
            if (z80_stat_no_code++ < 8)
                fprintf(stderr, "z80: no native code at %04X\n", zcpu.pc);
            z80_line_cyc = MD_CYCZ80_LINE;
        }
        z80_line_cyc -= MD_CYCZ80_LINE;
    }
}

/* The frame, as a game runs it: the picture's lines, where the game's band routines
 * (its line interrupts) set up the split screen's views, the HUD and the backgrounds -
 * each run as a function at its line - then the picture is drawn at once, then the
 * game's frame routine runs (the vertical blank: the whole game). A frame routine that
 * takes longer than a frame (a stage being set up) goes on in the next one. The play
 * screen's routine waits for the next picture's top band (cpu_waiting) and goes on
 * from that line. No time is kept within a frame. */
void md_run_frame(void)
{
    for (int line = 0; line < 224; line++) {
        md.line = line;
        vdp_line_capture(line);
        vdp_line_start(line);           /* the line counter: a band routine may be due */
        cpu_run_interrupt();
        if (cpu_wait_over())            /* the band it waited for: the logic goes on */
            cpu_run_until_idle(MD_CYC68K_LINE * MD_LINES);
    }
    md.line = 224;
    vdp_render_frame();
    vdp_line_start(224);                /* the frame routine */
    cpu_run_until_idle(MD_CYC68K_LINE * MD_LINES);
    for (int line = 225; line < MD_LINES; line++)
        vdp_line_start(line);           /* the line counter starts again (as the routine set it) */
    md.line = 0;
    if (snd_ready()) {                  /* this frame's sound from the player */
        static double due;
        int16_t buf[2048 * 2];
        due += (double)out_rate * MASTER_LINE * MD_LINES / 53693175.0;
        int n = (int)due;
        if (n > 2048) n = 2048;
        due -= n;
        snd_render(buf, n, out_rate);
        for (int i = 0; i < n; i++) push_output(buf[i * 2], buf[i * 2 + 1]);
    } else {
        md_snd_frame();                 /* no recorded sounds: the sound chips, for this frame */
    }
    md.frame++;
}

const uint32_t *md_framebuffer(void)
{
    static uint32_t fb[MD_SCREEN_H][MD_FB_W];
    int w = md_screen_width();
    for (int y = 0; y < MD_SCREEN_H; y++)
        for (int x = 0; x < w; x++) {
            uint16_t c = vdp.fbmd[y][x], k = VDP_COLOR(c);
            uint32_t rgb = k < 512 ? md_color_lut[k] : k >= VDP_CHAR_BASE ? char_color_lut[(k - VDP_CHAR_BASE) & 15] : 0xFF000000u;
            if (c & VDP_SHADOW)          /* half as bright */
                rgb = 0xFF000000u | ((rgb >> 1) & 0x7F7F7F);
            else if (c & VDP_HIGHLIGHT)  /* half as bright, plus half of white */
                rgb = 0xFF000000u | (((rgb >> 1) & 0x7F7F7F) + 0x7F7F7F);
            fb[y][x] = rgb;
        }
    return &fb[0][0];
}

const uint16_t *md_framebuffer_md(void) { return &vdp.fbmd[0][0]; }
int md_screen_width(void) { return vdp_out_width(); }
void md_set_aspect(double aspect) { vdp_set_aspect(aspect); }

uint32_t md_state_hash(void)
{
    uint32_t h = 2166136261u;
#define HB(p, n) for (size_t i_ = 0; i_ < (n); i_++) { h ^= ((const uint8_t *)(p))[i_]; h *= 16777619u; }
    /* the work memory, but not what lies below the stack pointer (the stack runs down
     * from 0xFFFE00; the game's own variables end below 0xFFC200): left-over words the
     * game never reads, which differ when routines written in C (logic.c) push less */
    uint32_t sp = cpu_get_sp() & 0xFFFF;
    if (sp < 0xC200 || sp > 0xFE00) sp = 0xC200;
    HB(md.ram, 0xC200);
    HB(md.ram + sp, sizeof(md.ram) - sp);
    HB(vdp.vram, sizeof(vdp.vram));
    HB(vdp.cram, sizeof(vdp.cram));
    HB(vdp.vsram, sizeof(vdp.vsram));
    /* not Z80 RAM: the game never reads it, and with the recorded sounds no Z80 runs */
#undef HB
    return h;
}

/* ------------------------------------------------- sound alone (the recorder) */

/* The sound driver as the game's boot code loads it (sub_02C76: the program, its data
 * and settings), with the Z80 and the sound chips only: no 68000, no video. Channels in
 * the masks are left out of the output (YM2612 bit n = FM n+1, PSG bit n = channel n). */
void md_snd_begin(int ym_mute, int psg_mute)
{
    snd_stems = 1;
    st_len = 0;
    memset(st_acc, 0, sizeof(st_acc));
    memset(st_last, 0, sizeof(st_last));
    memset(st_prev, 0, sizeof(st_prev));
    static const uint8_t cfg[10] = { 0x03, 0x80, 0x00, 0x80, 0x00, 0x00, 0xCD, 0x80, 0x00, 0x00 };
    memset(md.zram, 0, sizeof(md.zram));
    memcpy(md.zram, md.rom + 0x33000, 0x118B);
    memcpy(md.zram + 0x1200, md.rom + 0x3418C, 0x6FE);
    memcpy(md.zram + 0x1C00, cfg, sizeof(cfg));
    md.zram[0x1BFF] = 0;
    md.zram[0x1C10] = 0x80;
    zram_dirty = 1;
    psg_reset();
    psg.mute = psg_mute;
    OPN2_SetChipType(ym3438_mode_ym2612);
    OPN2_Reset(&ym);
    ym.mute = (Bit8u)ym_mute;
    z80_do_reset();
    md.z80_busreq = 0;
    md.z80_reset = 0;
    master_line_start = ym_time = 0;
    z80_line_cyc = 0;
    ym_phase = 0;
    ym_acc[0] = ym_acc[1] = 0;
    resample_pos = 0;
    last_in[0] = last_in[1] = prev_in[0] = prev_in[1] = 0;
    audio_len = 0;
}

/* the parts made so far (each stereo), taken out */
int md_snd_take(int16_t *out[MD_SND_STEMS], int max)
{
    int n = st_len < max ? st_len : max;
    for (int s = 0; s < MD_SND_STEMS; s++) {
        memcpy(out[s], st_buf[s], n * 4);
        memmove(st_buf[s], st_buf[s] + n * 2, (st_len - n) * 4);
    }
    st_len -= n;
    return n;
}

/* back to the normal machine (the next md_reset sets everything up) */
void md_snd_end(void) { snd_stems = 0; }

/* a request, as the game's sub_02DC2 / sub_02D98 make it */
void md_snd_command(uint8_t id) { md.zram[0x1C0A] = id; }

/* one video frame's time of sound (output in md_audio_take): for the recorder, and for
 * the game without recorded sounds, after the 68000's frame */
void md_snd_frame(void)
{
    for (int line = 0; line < MD_LINES; line++) {
        if (line == 224)
            game.z80r_gen_int(&zcpu, 0xFF);
        z80_line_cyc = 0;
        z80_line();
        if (line == 225)
            zcpu.int_pending = 0;
        z80_line_cyc = 0;
        master_line_start += MASTER_LINE;
        ym_sync(master_line_start);
    }
}
