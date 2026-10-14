/* Headless runner for testing: runs N frames, dumps screenshots and state hashes.
 *
 *   bonanza-headless ROM [--frames N] [--shot FRAME:file.ppm]... [--input FRAME:PAD:buttons]...
 *                        [--interp] [--hash] [--wav file.wav]
 *
 * buttons: any of U D L R A B C S (S = Start). An input entry stays active until the next one
 * for the same pad.
 *
 * Mods: --mods-dir DIR --set "mods=a.bbro|b.bbro" applies those mods from DIR.
 *
 * Online testing: --net-host PORT / --net-join ADDR play a lockstep game against another
 * runner (only this player's --input entries are used; frames count from the start of the
 * online game). --player N renders player N's screen as in an online game.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "extrastages.h"
#include "md.h"
#include "cheats.h"
#include "vdp.h"
#include "settings.h"
#include "net.h"
#include "mods.h"
#ifdef _WIN32
#include <windows.h>
static void nap(void) { Sleep(1); }
#else
#include <unistd.h>
static void nap(void) { usleep(1000); }
#endif

extern uint64_t cpu_stat_recomp_insns, cpu_stat_interp_insns;

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
    const uint32_t *fb = md_framebuffer();
    for (int y = 0; y < MD_SCREEN_H; y++)
        for (int x = 0; x < w; x++) {
            uint32_t p = fb[y * MD_FB_W + x];
            uint8_t rgb[3] = {(uint8_t)(p >> 16), (uint8_t)(p >> 8), (uint8_t)p};
            fwrite(rgb, 1, 3, f);
        }
    fclose(f);
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
        fprintf(stderr, "usage: %s ROM [--frames N] [--shot F:file.ppm] [--input F:P:btn] [--interp] [--hash] [--wav f]\n", argv[0]);
        return 1;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    static uint8_t buf[0x400000];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    const char *err = md_rom_check(buf, n);
    if (err) { fprintf(stderr, "%s\n", err); return 1; }
    md_load_rom(buf, n);

    int frames = 600, hash = 0;
    const char *dumpvdp = NULL;
    struct { int frame; char path[256]; } shots[64];
    struct { int frame, pad; uint8_t b; } inputs[256];
    struct { int frame; unsigned addr, val; } pokes[64];
    int npokes = 0, play_extra = 0;
    int nshots = 0, ninputs = 0;
    const char *wav = NULL;
    const char *net_host_port = NULL, *net_join_addr = NULL;
    md.use_recomp = 1;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--interp")) md.use_recomp = 0;
        else if (!strcmp(argv[i], "--hash")) hash = 1;
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
        vdp_local_player = net_local_player();
        fprintf(stderr, "net: playing as player %d\n", vdp_local_player + 1);
    }
    md_reset();
    if (play_extra > 0) xs_play(play_extra - 1);
    uint8_t mine = 0;
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
        md_run_frame();
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
    fprintf(stderr, "instructions: recompiled %llu, interpreted %llu\n",
            (unsigned long long)cpu_stat_recomp_insns, (unsigned long long)cpu_stat_interp_insns);
    return 0;
}
