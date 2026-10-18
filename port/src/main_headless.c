/* Headless runner for testing: runs N frames, dumps screenshots and state hashes.
 *
 *   bonanza-headless ROM [--frames N] [--shot FRAME:file.ppm]... [--input FRAME:PAD:buttons]...
 *                        [--hash] [--picture-hash] [--wav file.wav] [--sounds bank]
 * --sounds: play the recorded sounds from that file (recorded there first if it is not
 * there yet) instead of running the sound hardware.
 *   bonanza-headless ROM --make-sounds FILE: only record the sounds into FILE (the build
 *   does this: see CMakeLists.txt). Lossless; --opus: in Opus (about a fifth the size).
 *
 * buttons: any of U D L R A B C S (S = Start). An input entry stays active until the next one
 * for the same pad.
 *
 * Mods: --mods-dir DIR --set "mods=a.bbro|b.bbro" applies those mods from DIR.
 *
 * Online testing: --net-host PORT / --net-join ADDR play a lockstep game against another
 * runner (only this player's --input entries are used; frames count from the start of the
 * online game). --player N renders player N's screen as in an online game.
 *
 * --gpu: the picture drawn on the graphics card (render_gl.c, an off-screen OpenGL ES
 * context) as well as on the CPU, every frame compared; the graphics card's picture is
 * the one hashed and saved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "extrastages.h"
#include "game.h"
#include "md.h"
#include "draw.h"
#include "cheats.h"
#include "z80recomp.h"
#include "sndmix.h"
#include "sndrec.h"
#include "sndbank.h"
#include "scene.h"
#include "render_gl.h"
#ifdef BONANZA_EGL
#include <EGL/egl.h>
#include <EGL/eglext.h>
static void *egl_proc(const char *name) { return (void *)eglGetProcAddress(name); }
/* an off-screen OpenGL ES 3 context (no window, no display) */
static int gpu_context(void)
{
    EGLDisplay d = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
    EGLint major, minor, n = 0;
    if (d == EGL_NO_DISPLAY || !eglInitialize(d, &major, &minor)) return 1;
    eglBindAPI(EGL_OPENGL_ES_API);
    EGLint ca[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE};
    EGLConfig c;
    eglChooseConfig(d, ca, &c, 1, &n);
    EGLint cx[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_NONE};
    EGLContext ctx = eglCreateContext(d, n ? c : (EGLConfig)0, EGL_NO_CONTEXT, cx);
    return !ctx || !eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx);
}
#endif
extern unsigned long z80_stat_no_code;
#include "vdp.h"
#include "settings.h"
#include "net.h"
#include "mods.h"
#include "prophunt.h"
#include "race.h"
#ifdef _WIN32
#include <windows.h>
static void nap(void) { Sleep(1); }
#else
#include <unistd.h>
static void nap(void) { usleep(1000); }
#endif

extern uint64_t cpu_stat_recomp_insns, cpu_stat_no_code;

static uint8_t parse_buttons(const char *s)
{
    uint8_t b = 0;
    for (; *s; s++) {
        switch (*s) {
        case 'U': b |= PAD_UP; break;
        case 'D': b |= PAD_DOWN; break;
        case 'L': b |= PAD_LEFT; break;
        case 'R': b |= PAD_RIGHT; break;
        case 'A': b |= PAD_A; break;
        case 'B': b |= PAD_B; break;
        case 'C': b |= PAD_C; break;
        case 'S': b |= PAD_START; break;
        }
    }
    return b;
}

static void write_ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    int w = md_screen_width();
    fprintf(f, "P6\n%d %d\n255\n", w, MD_SCREEN_H);
    uint32_t *fb = (uint32_t *)md_framebuffer();
    ph_overlay(fb, MD_FB_W, w, MD_SCREEN_H, net_state() == NET_PLAYING ? net_local_player() : -1);
    race_overlay(fb, MD_FB_W, w, MD_SCREEN_H);
    for (int y = 0; y < MD_SCREEN_H; y++)
        for (int x = 0; x < w; x++) {
            uint32_t p = fb[y * MD_FB_W + x];
            uint8_t rgb[3] = {(uint8_t)(p >> 16), (uint8_t)(p >> 8), (uint8_t)p};
            fwrite(rgb, 1, 3, f);
        }
    fclose(f);
}

/* a 1-player game straight into stage entry e (Race; as main_sdl.c's start_stage) */
static void start_stage(int entry)
{
    int16_t buf[2048 * 2];
    for (int f = 0; f < 912; f++) {
        int start = (f >= 620 && f < 630) || (f >= 760 && f < 770) || (f >= 900 && f < 910);
        md.pad[0] = start ? PAD_START : 0;
        md.pad[1] = 0;
        if (f == 765) {
            md.ram[0xC05A] = 0;
            md.ram[0xC05B] = (uint8_t)entry;
        }
        md_run_frame();
        while (md_audio_take(buf, 2048) > 0) {
        }
    }
    md.pad[0] = 0;
}

static void wav_header(FILE *f, int rate, uint32_t frames)
{
    uint32_t data = frames * 4;
    uint8_t h[44];
    memcpy(h, "RIFF", 4);
    uint32_t v = 36 + data; memcpy(h + 4, &v, 4);
    memcpy(h + 8, "WAVEfmt ", 8);
    v = 16; memcpy(h + 16, &v, 4);
    uint16_t s = 1; memcpy(h + 20, &s, 2);
    s = 2; memcpy(h + 22, &s, 2);
    v = rate; memcpy(h + 24, &v, 4);
    v = rate * 4; memcpy(h + 28, &v, 4);
    s = 4; memcpy(h + 32, &s, 2);
    s = 16; memcpy(h + 34, &s, 2);
    memcpy(h + 36, "data", 4);
    memcpy(h + 40, &data, 4);
    fseek(f, 0, SEEK_SET);
    fwrite(h, 1, 44, f);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s ROM [--frames N] [--shot F:file.ppm] [--input F:P:btn] [--hash] [--wav f]\n", argv[0]);
        return 1;
    }
    {                                       /* the game's code: linked in, or next to the program */
        static char dir[1024];
        snprintf(dir, sizeof(dir), "%s", argv[0]);
        char *slash = strrchr(dir, '/');
        if (slash) slash[1] = 0; else dir[0] = 0;
        const char *gerr = game_load(dir);
        if (gerr) { fprintf(stderr, "%s\n", gerr); return 1; }
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    static uint8_t buf[0x400000];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    const char *err = md_rom_check(buf, n);
    if (err) { fprintf(stderr, "%s\n", err); return 1; }
    md_load_rom(buf, n);

    /* --make-sounds FILE: only record the game's sounds (the build does this, so the
     * game does not have to on its first start) */
    for (int i = 2; i < argc; i++) {           /* new recordings lossless (default) or Opus */
        if (!strcmp(argv[i], "--lossless")) sb_write_lossless = 1;
        if (!strcmp(argv[i], "--opus")) sb_write_lossless = 0;
    }
    if (argc >= 4 && !strcmp(argv[2], "--make-sounds")) {
        md_reset();
        sndrec *r = snd_record_start(argv[3]);
        if (!r) { fprintf(stderr, "could not write %s\n", argv[3]); return 1; }
        int res, last = -1, done, total;
        while ((res = snd_record_step(r, 600)) == 0) {
            snd_record_progress(r, &done, &total);
            if (done != last) { fprintf(stderr, "\rrecording the sounds: %d / %d", done, total); last = done; }
        }
        snd_record_free(r);
        fprintf(stderr, "\n");
        if (res < 0) { fprintf(stderr, "could not write %s\n", argv[3]); remove(argv[3]); return 1; }
        return 0;
    }

    int frames = 600, hash = 0, picture_hash = 0, gpu = 0;
    unsigned long gpu_frames = 0, gpu_differ = 0;
    const char *dumpvdp = NULL;
    struct { int frame; char path[256]; } shots[64];
    struct { int frame, pad; uint8_t b; } inputs[256];
    struct { int frame; unsigned addr, val; } pokes[64];
    int npokes = 0, play_extra = 0;
    int nshots = 0, ninputs = 0;
    const char *wav = NULL;
    const char *sounds = NULL;
    const char *net_host_port = NULL, *net_join_addr = NULL;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--hash")) hash = 1;
        else if (!strcmp(argv[i], "--picture-hash")) picture_hash = 1;
        else if (!strcmp(argv[i], "--gpu")) gpu = 1;
        else if (!strcmp(argv[i], "--lossless") || !strcmp(argv[i], "--opus")) { /* above */ }
        else if (!strcmp(argv[i], "--dumpvdp") && i + 1 < argc) dumpvdp = argv[++i];
        else if (!strcmp(argv[i], "--ini") && i + 1 < argc) {
            settings_load(argv[++i]);
            settings_apply();
        } else if (!strcmp(argv[i], "--set") && i + 1 < argc) {
            char kv[256];
            snprintf(kv, sizeof(kv), "%s", argv[++i]);
            char *eq = strchr(kv, '=');
            if (eq) {
                *eq = 0;
                if (!settings_set(kv, eq + 1))
                    fprintf(stderr, "unknown setting %s\n", kv);
                settings_apply();
            }
        } else if (!strcmp(argv[i], "--aspect") && i + 1 < argc) {
            double a = 0, b = 0;
            if (sscanf(argv[++i], "%lf:%lf", &a, &b) == 2 && b > 0)
                md_set_aspect(a / b);
        }
        else if (!strcmp(argv[i], "--wav") && i + 1 < argc) wav = argv[++i];
        else if (!strcmp(argv[i], "--sounds") && i + 1 < argc) sounds = argv[++i];
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc && nshots < 64) {
            char *c = strchr(argv[++i], ':');
            if (!c) continue;
            shots[nshots].frame = atoi(argv[i]);
            snprintf(shots[nshots].path, sizeof(shots[nshots].path), "%s", c + 1);
            nshots++;
        } else if (!strcmp(argv[i], "--poke") && i + 1 < argc && npokes < 64) {
            /* --poke FRAME:ADDR:WORD (hex address/value in 68k RAM), for tests */
            if (sscanf(argv[++i], "%d:%x:%x", &pokes[npokes].frame, &pokes[npokes].addr, &pokes[npokes].val) == 3)
                npokes++;
        } else if (!strcmp(argv[i], "--extra") && i + 1 < argc) {
            play_extra = atoi(argv[++i]);    /* the next stage started is extra stage K */
        } else if (!strcmp(argv[i], "--mods-dir") && i + 1 < argc) {
            mods_add_dir(argv[++i]);     /* with --set mods=NAME.bbro */
        } else if (!strcmp(argv[i], "--player") && i + 1 < argc) {
            vdp_local_player = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--net-host") && i + 1 < argc) {
            net_host_port = argv[++i];
        } else if (!strcmp(argv[i], "--net-join") && i + 1 < argc) {
            net_join_addr = argv[++i];
        } else if (!strcmp(argv[i], "--input") && i + 1 < argc && ninputs < 256) {
            int fr, pad; char btn[32] = "";
            if (sscanf(argv[++i], "%d:%d:%31s", &fr, &pad, btn) >= 2) {
                inputs[ninputs].frame = fr;
                inputs[ninputs].pad = pad;
                inputs[ninputs].b = parse_buttons(btn);
                ninputs++;
            }
        }
    }

    FILE *wf = NULL;
    uint32_t wav_frames = 0;
    if (wav) {
        wf = fopen(wav, "wb");
        if (wf) wav_header(wf, 48000, 0);
    }
    md_audio_set_rate(48000);
    if (mods_main_dir()[0]) {
        static uint8_t modded[0x400000];
        mods_scan();
        size_t mn = mods_apply(buf, n, modded, sizeof(modded));
        md_load_rom(modded, mn);
        for (int k = 0; k < mods_count(); k++)
            fprintf(stderr, "mod %s: %s%s%s\n", mods_get(k)->file, mods_get(k)->name,
                    mods_get(k)->enabled ? " (on)" : "", mods_get(k)->error);
        fprintf(stderr, "mods: %s\n", mods_status());
    }
    if (sounds) {                       /* the recorded sounds: made once, then played */
        FILE *t = fopen(sounds, "rb");
        if (t) fclose(t);
        else {
            fprintf(stderr, "recording the sounds to %s...\n", sounds);
            if (snd_record_bank(sounds, 0, 0)) { fprintf(stderr, "could not write %s\n", sounds); return 1; }
            md_audio_set_rate(48000);
        }
        if (!snd_open(sounds)) { fprintf(stderr, "could not read %s\n", sounds); return 1; }
    }
    int online = net_host_port || net_join_addr;
    if (online) {
        if (!(net_host_port ? net_host(atoi(net_host_port)) : net_join(net_join_addr))) {
            fprintf(stderr, "net: %s\n", net_status());
            return 1;
        }
        while (!net_take_start()) {
            net_poll();
            if (net_state() == NET_OFF) { fprintf(stderr, "net: %s\n", net_status()); return 1; }
            nap();
        }
        vdp_local_player = net_option_race() ? -1 : net_local_player();
        fprintf(stderr, "net: playing as player %d%s\n", net_local_player() + 1, net_option_race() ? " (race)" : "");
    }
    if (gpu) {
#ifdef BONANZA_EGL
        if (gpu_context() || render_gl_init(egl_proc, 1)) {
            fprintf(stderr, "gpu: %s\n", render_gl_error()[0] ? render_gl_error() : "no OpenGL ES 3 context");
            return 1;
        }
        scene_cpu_too = 1;
#else
        fprintf(stderr, "gpu: this runner was built without EGL\n");
        return 1;
#endif
    }
    md_reset();
    ph_reset();
    race_reset();
    if (play_extra > 0) xs_play(play_extra - 1);
    uint8_t mine = 0;
    if (online && race_on()) {
        /* Race: this player's own game, the race status over the network; --input
         * buttons are this player's (pad 0), --poke frames count the turns here */
        for (int fr = 0; fr < frames; fr++) {
            for (int k = 0; k < ninputs; k++)
                if (inputs[k].frame == fr) mine = inputs[k].b;
            net_poll();
            if (net_state() != NET_PLAYING) { fprintf(stderr, "net: %s\n", net_status()); return 1; }
            int entry = 0, what = race_update(mine, &entry);
            if (what == RACE_START) {
                fprintf(stderr, "race %d: start stage entry %d\n", fr, entry);
                md_reset();
                start_stage(entry);
            } else if (what == RACE_RUN) {
                for (int k = 0; k < npokes; k++)
                    if (pokes[k].frame == fr) {
                        md.ram[pokes[k].addr & 0xFFFE] = pokes[k].val >> 8;
                        md.ram[(pokes[k].addr & 0xFFFE) + 1] = pokes[k].val;
                    }
                md.pad[0] = mine;
                md.pad[1] = 0;
                race_pre_frame();
                md_run_frame();
                race_post_frame();
            }
            int16_t abuf[4096 * 2];
            while (md_audio_take(abuf, 4096) > 0) {
            }
            for (int k = 0; k < nshots; k++)
                if (shots[k].frame == fr)
                    write_ppm(shots[k].path);
            nap();
        }
        fprintf(stderr, "net: %s\n", net_status());
        for (int k = 0; k < 300; k++) { net_poll(); nap(); }
        net_stop();
        return 0;
    }
    for (int fr = 0; fr < frames; fr++) {
        for (int k = 0; k < ninputs; k++)
            if (inputs[k].frame == fr) {
                if (!online)
                    md.pad[inputs[k].pad & 1] = inputs[k].b;
                else if ((inputs[k].pad & 1) == net_local_player())
                    mine = inputs[k].b;
            }
        if (online) {
            uint8_t pads2[2];
            for (;;) {
                net_poll();
                if (net_state() != NET_PLAYING) { fprintf(stderr, "net: %s\n", net_status()); return 1; }
                net_local_input(mine);
                if (net_inputs_for_next_frame(pads2)) break;
                nap();
            }
            md.pad[0] = pads2[0];
            md.pad[1] = pads2[1];
        }
        for (int k = 0; k < npokes; k++)
            if (pokes[k].frame == fr) {
                md.ram[pokes[k].addr & 0xFFFE] = pokes[k].val >> 8;
                md.ram[(pokes[k].addr & 0xFFFE) + 1] = pokes[k].val;
            }
        ph_pre_frame();                         /* Prop Hunt (online, if the host chose it) */
        md_run_frame();
        ph_post_frame();
        if (gpu && render_gl_frame()) {         /* the graphics card's picture, checked */
            static uint16_t codes[MD_SCREEN_H * VDP_FB_W];
            render_gl_read_codes(codes);
            gpu_frames++;
            int bad = 0;
            for (int k = 0; k < MD_SCREEN_H * VDP_FB_W; k++)
                if (codes[k] != (&vdp.fbmd[0][0])[k]) {
                    if (!bad && gpu_differ < 5)
                        fprintf(stderr, "gpu: frame %d differs first at x %d row %d: %04x, CPU %04x\n", fr,
                                k % VDP_FB_W, k / VDP_FB_W, codes[k], (&vdp.fbmd[0][0])[k]);
                    bad++;
                }
            if (bad) gpu_differ++;
            if (getenv("GPUDUMP") && atoi(getenv("GPUDUMP")) == fr) {    /* both pictures (tests) */
                FILE *d = fopen("gpudump.bin", "wb");
                if (d) { fwrite(codes, 2, MD_SCREEN_H * VDP_FB_W, d); fwrite(vdp.fbmd, 2, MD_SCREEN_H * VDP_FB_W, d); fclose(d); }
            }
            memcpy(vdp.fbmd, codes, sizeof(codes));
        }
        cheat_frame();                          /* does nothing in an online game */
        int16_t abuf[4096 * 2];
        int got;
        while ((got = md_audio_take(abuf, 4096)) > 0) {
            if (wf) { fwrite(abuf, 4, got, wf); wav_frames += got; }
        }
        for (int k = 0; k < nshots; k++)
            if (shots[k].frame == fr)
                write_ppm(shots[k].path);
        if (online)
            net_frame_done(net_frame() % 60 == 59 ? md_state_hash() : 0);
        if (hash)
            printf("frame %d hash %08x pc %06x\n", fr, md_state_hash(), cpu_get_pc());
        if (getenv("RAMLOG")) {              /* RAMLOG=ADDR: a RAM word every frame (tests) */
            unsigned a = (unsigned)strtoul(getenv("RAMLOG"), 0, 16) & 0xFFFE;
            printf("frame %d ram %04x=%04x\n", fr, a, md.ram[a] << 8 | md.ram[a + 1]);
        }
        if (picture_hash) {                  /* the picture as shown, every frame */
            const uint16_t *p = md_framebuffer_md();
            uint32_t h = 2166136261u;
            for (int k = 0; k < MD_SCREEN_H * MD_FB_W; k++) h = (h ^ p[k]) * 16777619u;
            printf("frame %d picture %08x\n", fr, h);
            if (getenv("PICDUMP") && atoi(getenv("PICDUMP")) == fr) {   /* the frame's raw picture (tests) */
                FILE *pd = fopen("picdump.bin", "wb");
                if (pd) { fwrite(p, 2, MD_SCREEN_H * MD_FB_W, pd); fclose(pd); }
            }
        }
    }
    if (online) {
        fprintf(stderr, "net: %s\n", net_status());
        for (int k = 0; k < 1000; k++) { net_poll(); nap(); }   /* the other may still need our inputs */
        net_stop();
    }
    if (hash) {
        printf("vdp regs:");
        for (int i = 0; i < 24; i++) printf(" %02x", vdp.reg[i]);
        printf("\ncram:");
        for (int i = 0; i < 64; i++) printf(" %03x", vdp.cram[i]);
        printf("\nram c070=%04x\n", (md.ram[0xc070] << 8) | md.ram[0xc071]);
    }
    if (dumpvdp) { /* raw VDP state for tools: vram, cram, vsram, regs */
        FILE *d = fopen(dumpvdp, "wb");
        if (d) {
            fwrite(vdp.vram, 1, sizeof(vdp.vram), d);
            fwrite(vdp.cram, 1, sizeof(vdp.cram), d);
            fwrite(vdp.vsram, 1, sizeof(vdp.vsram), d);
            fwrite(vdp.reg, 1, sizeof(vdp.reg), d);
            fwrite(md.ram, 1, sizeof(md.ram), d);
            fclose(d);
        }
    }
    if (wf) { wav_header(wf, 48000, wav_frames); fclose(wf); }
    fprintf(stderr, "68000 instructions: %llu native, %llu without native code\n",
            (unsigned long long)cpu_stat_recomp_insns, (unsigned long long)cpu_stat_no_code);
    fprintf(stderr, "drawing routines: %llu calls in C, %llu as translated (an interrupt came within)\n",
            (unsigned long long)draw_stat_c, (unsigned long long)draw_stat_translated);
    fprintf(stderr, "video port words (a mod's own code): %lu\n", video_port_words);
    if (gpu)
        fprintf(stderr, "gpu: %lu pictures drawn on the graphics card, %lu differ from the CPU's\n", gpu_frames, gpu_differ);
    fprintf(stderr, "z80 instructions: %lu native, %lu lines without native code\n", *game.z80r_insns, z80_stat_no_code);
    return 0;
}
