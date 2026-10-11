/* Bonanza Bros. port - SDL2 frontend (Linux, Windows, Android, Web).
 *
 * The game ROM is not part of the program. It is looked up at start-up:
 *   - path given on the command line
 *   - baserom.md / bonanza.md (or any 512 KiB .md/.bin/.gen with the right SHA-1)
 *     next to the executable, in the current directory, or in the user data folder
 *   - on desktop, a ROM file can also be dropped onto the window
 * Once found, a copy is kept in the user data folder so it only has to be given once.
 */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "md.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define APP_ORG  "bonanza-recomp"
#define APP_NAME "Bonanza Bros"

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_AudioDeviceID audio_dev;
static SDL_GameController *pads[2];
static int running = 1, paused, have_rom, show_touch;
static uint8_t key_pad[2], ctl_pad[2], touch_pad;
static char status_msg[256] = "Drop the Bonanza Bros. ROM file onto this window";
static uint8_t *rom_buf;

/* ------------------------------------------------------------------ ROM */

static uint8_t *read_file(const char *path, size_t *size)
{
    SDL_RWops *rw = SDL_RWFromFile(path, "rb");
    if (!rw)
        return NULL;
    Sint64 n = SDL_RWsize(rw);
    if (n <= 0 || n > 0x400000) {
        SDL_RWclose(rw);
        return NULL;
    }
    uint8_t *buf = malloc((size_t)n);
    if (buf && SDL_RWread(rw, buf, 1, (size_t)n) != (size_t)n) {
        free(buf);
        buf = NULL;
    }
    SDL_RWclose(rw);
    *size = (size_t)n;
    return buf;
}

static void save_rom_copy(const uint8_t *data, size_t size)
{
    char *pref = SDL_GetPrefPath(APP_ORG, APP_NAME);
    if (!pref)
        return;
    char path[1024];
    snprintf(path, sizeof(path), "%sbaserom.md", pref);
    SDL_free(pref);
    SDL_RWops *rw = SDL_RWFromFile(path, "rb");
    if (rw) { /* already there */
        SDL_RWclose(rw);
        return;
    }
    rw = SDL_RWFromFile(path, "wb");
    if (rw) {
        SDL_RWwrite(rw, data, 1, size);
        SDL_RWclose(rw);
    }
}

static int try_rom(const char *path, int report)
{
    size_t size = 0;
    uint8_t *data = read_file(path, &size);
    if (!data)
        return 0;
    const char *err = md_rom_check(data, size);
    if (err) {
        if (report)
            snprintf(status_msg, sizeof(status_msg), "%s", err);
        free(data);
        return 0;
    }
    free(rom_buf);
    rom_buf = data;
    md_load_rom(data, size);
    save_rom_copy(data, size);
    md_reset();
    have_rom = 1;
    SDL_Log("loaded ROM from %s", path);
    return 1;
}

static int find_rom_in(const char *dir)
{
    static const char *names[] = {
        "baserom.md", "bonanza.md", "Bonanza Bros.md", "Bonanza Bros. (Japan, USA, Europe).md",
        "Bonanza Bros. (Japan, USA, Europe).bin", "Bonanza Bros. (JUE).md", "Bonanza Bros. (JUE).bin",
        "bonanza.bin", "bonanza.gen", "BONANZAB_JE_1.bin",
    };
    char path[1024];
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        snprintf(path, sizeof(path), "%s%s", dir, names[i]);
        if (try_rom(path, 0))
            return 1;
    }
    return 0;
}

static void find_rom(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
        if (argv[i][0] != '-' && try_rom(argv[i], 1))
            return;
    char *pref = SDL_GetPrefPath(APP_ORG, APP_NAME);
    if (pref) {
        int ok = find_rom_in(pref);
        SDL_free(pref);
        if (ok)
            return;
    }
    char *base = SDL_GetBasePath();
    if (base) {
        int ok = find_rom_in(base);
        SDL_free(base);
        if (ok)
            return;
    }
    if (find_rom_in(""))
        return;
#ifdef __EMSCRIPTEN__
    find_rom_in("/");
#endif
}

/* ------------------------------------------------------------------ input */

static uint8_t key_bit(SDL_Keycode k, int *player)
{
    *player = 0;
    switch (k) {
    case SDLK_UP: return PAD_UP;
    case SDLK_DOWN: return PAD_DOWN;
    case SDLK_LEFT: return PAD_LEFT;
    case SDLK_RIGHT: return PAD_RIGHT;
    case SDLK_z: return PAD_A;
    case SDLK_x: return PAD_B;
    case SDLK_c: return PAD_C;
    case SDLK_RETURN: return PAD_START;
    }
    *player = 1;
    switch (k) {
    case SDLK_w: return PAD_UP;
    case SDLK_s: return PAD_DOWN;
    case SDLK_a: return PAD_LEFT;
    case SDLK_d: return PAD_RIGHT;
    case SDLK_j: return PAD_A;
    case SDLK_k: return PAD_B;
    case SDLK_l: return PAD_C;
    case SDLK_SPACE: return PAD_START;
    }
    return 0;
}

static void open_pads(void)
{
    for (int i = 0; i < 2; i++)
        if (pads[i]) {
            SDL_GameControllerClose(pads[i]);
            pads[i] = NULL;
        }
    int n = 0;
    for (int i = 0; i < SDL_NumJoysticks() && n < 2; i++)
        if (SDL_IsGameController(i))
            pads[n++] = SDL_GameControllerOpen(i);
}

static uint8_t controller_state(SDL_GameController *c)
{
    if (!c)
        return 0;
    uint8_t b = 0;
    int ax = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX);
    int ay = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY);
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_UP) || ay < -16000) b |= PAD_UP;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN) || ay > 16000) b |= PAD_DOWN;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || ax < -16000) b |= PAD_LEFT;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || ax > 16000) b |= PAD_RIGHT;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_X)) b |= PAD_A;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_A)) b |= PAD_B;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_B)) b |= PAD_C;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_START)) b |= PAD_START;
    return b;
}

/* --- touch overlay (shown on touch screens) --- */

typedef struct { float x, y, w, h; uint8_t bits; const char *label; } touch_btn;
static touch_btn tbtns[] = {
    /* positions are fractions of the window */
    {0.02f, 0.55f, 0.10f, 0.14f, PAD_UP | PAD_LEFT, ""},
    {0.12f, 0.55f, 0.08f, 0.14f, PAD_UP, "U"},
    {0.20f, 0.55f, 0.10f, 0.14f, PAD_UP | PAD_RIGHT, ""},
    {0.02f, 0.69f, 0.10f, 0.14f, PAD_LEFT, "L"},
    {0.20f, 0.69f, 0.10f, 0.14f, PAD_RIGHT, "R"},
    {0.02f, 0.83f, 0.10f, 0.15f, PAD_DOWN | PAD_LEFT, ""},
    {0.12f, 0.83f, 0.08f, 0.15f, PAD_DOWN, "D"},
    {0.20f, 0.83f, 0.10f, 0.15f, PAD_DOWN | PAD_RIGHT, ""},
    {0.70f, 0.78f, 0.09f, 0.16f, PAD_A, "A"},
    {0.80f, 0.70f, 0.09f, 0.16f, PAD_B, "B"},
    {0.90f, 0.62f, 0.09f, 0.16f, PAD_C, "C"},
    {0.44f, 0.90f, 0.12f, 0.08f, PAD_START, "START"},
};
#define NTB (int)(sizeof(tbtns) / sizeof(tbtns[0]))
#define MAX_FINGERS 10
static struct { SDL_FingerID id; float x, y; int active; } fingers[MAX_FINGERS];

static void touch_update(void)
{
    touch_pad = 0;
    for (int f = 0; f < MAX_FINGERS; f++) {
        if (!fingers[f].active)
            continue;
        for (int i = 0; i < NTB; i++) {
            touch_btn *b = &tbtns[i];
            if (fingers[f].x >= b->x && fingers[f].x < b->x + b->w && fingers[f].y >= b->y && fingers[f].y < b->y + b->h)
                touch_pad |= b->bits;
        }
    }
}

static void touch_event(SDL_Event *e)
{
    show_touch = 1;
    int slot = -1;
    for (int f = 0; f < MAX_FINGERS; f++)
        if (fingers[f].active && fingers[f].id == e->tfinger.fingerId)
            slot = f;
    if (e->type == SDL_FINGERUP) {
        if (slot >= 0)
            fingers[slot].active = 0;
    } else {
        if (slot < 0)
            for (int f = 0; f < MAX_FINGERS && slot < 0; f++)
                if (!fingers[f].active)
                    slot = f;
        if (slot >= 0) {
            fingers[slot].active = 1;
            fingers[slot].id = e->tfinger.fingerId;
            fingers[slot].x = e->tfinger.x;
            fingers[slot].y = e->tfinger.y;
        }
    }
    touch_update();
}

static void draw_touch(int ww, int wh)
{
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < NTB; i++) {
        touch_btn *b = &tbtns[i];
        if (!b->label[0])
            continue;
        SDL_Rect r = {(int)(b->x * ww), (int)(b->y * wh), (int)(b->w * ww), (int)(b->h * wh)};
        int on = (touch_pad & b->bits) == b->bits;
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, on ? 110 : 45);
        SDL_RenderFillRect(renderer, &r);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 120);
        SDL_RenderDrawRect(renderer, &r);
    }
}

/* ------------------------------------------------------------------ video */

static void present(void)
{
    int ww, wh;
    SDL_GetRendererOutputSize(renderer, &ww, &wh);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    if (have_rom) {
        int sw = md_screen_width();
        SDL_UpdateTexture(texture, NULL, md_framebuffer(), MD_SCREEN_W * 4);
        SDL_Rect src = {0, 0, sw, MD_SCREEN_H};
        /* 4:3 picture, as on a TV */
        int dw = ww, dh = ww * 3 / 4;
        if (dh > wh) {
            dh = wh;
            dw = wh * 4 / 3;
        }
        SDL_Rect dst = {(ww - dw) / 2, (wh - dh) / 2, dw, dh};
        SDL_RenderCopy(renderer, texture, &src, &dst);
        if (show_touch)
            draw_touch(ww, wh);
    }
    SDL_RenderPresent(renderer);
}

static void set_title(void)
{
    if (!have_rom)
        SDL_SetWindowTitle(window, status_msg);
    else
        SDL_SetWindowTitle(window, paused ? "Bonanza Bros (paused)" : "Bonanza Bros");
}

/* ------------------------------------------------------------------ loop */

static void handle_events(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            running = 0;
            break;
        case SDL_DROPFILE:
            if (try_rom(e.drop.file, 1))
                status_msg[0] = 0;
            SDL_free(e.drop.file);
            set_title();
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP: {
            SDL_Keycode k = e.key.keysym.sym;
            if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                if (k == SDLK_F11 || (k == SDLK_RETURN && (e.key.keysym.mod & KMOD_ALT))) {
                    Uint32 fs = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                    SDL_SetWindowFullscreen(window, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                    break;
                }
                if (k == SDLK_ESCAPE || k == SDLK_AC_BACK) {
#ifndef __EMSCRIPTEN__
                    running = 0;
#endif
                    break;
                }
                if (k == SDLK_p || k == SDLK_PAUSE) {
                    paused = !paused;
                    set_title();
                    break;
                }
                if (k == SDLK_F3 && have_rom) {
                    md_reset();
                    break;
                }
            }
            int pl;
            uint8_t bit = key_bit(k, &pl);
            if (e.type == SDL_KEYDOWN)
                key_pad[pl] |= bit;
            else
                key_pad[pl] &= ~bit;
            break;
        }
        case SDL_CONTROLLERDEVICEADDED:
        case SDL_CONTROLLERDEVICEREMOVED:
            open_pads();
            break;
        case SDL_FINGERDOWN:
        case SDL_FINGERUP:
        case SDL_FINGERMOTION:
            touch_event(&e);
            break;
        case SDL_APP_WILLENTERBACKGROUND:
            paused = 1;
            break;
        case SDL_APP_DIDENTERFOREGROUND:
            paused = 0;
            break;
        }
    }
}

static Uint64 next_frame;

static void frame(void)
{
    handle_events();
    if (have_rom && !paused) {
        for (int p = 0; p < 2; p++) {
            ctl_pad[p] = controller_state(pads[p]);
            md.pad[p] = key_pad[p] | ctl_pad[p] | (p == 0 ? touch_pad : 0);
        }
        md_run_frame();
        int16_t buf[2048 * 2];
        int n;
        while ((n = md_audio_take(buf, 2048)) > 0)
            if (audio_dev)
                SDL_QueueAudio(audio_dev, buf, n * 4);
    } else {
        int16_t buf[2048 * 2];
        while (md_audio_take(buf, 2048) > 0) {
        }
    }
    present();
}

int main(int argc, char **argv)
{
    int use_recomp = 1;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--interp"))
            use_recomp = 0;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    window = SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 960, 720,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return 1;
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
        renderer = SDL_CreateRenderer(window, -1, 0);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, MD_SCREEN_W, MD_SCREEN_H);
#if defined(__ANDROID__)
    show_touch = 1;
#endif

    SDL_AudioSpec want = {0}, have;
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    md_audio_set_rate(audio_dev ? have.freq : 48000);
    if (audio_dev)
        SDL_PauseAudioDevice(audio_dev, 0);
    open_pads();

    find_rom(argc, argv);
    md.use_recomp = use_recomp;
    set_title();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(frame, 0, 1);
#else
    const Uint64 freq = SDL_GetPerformanceFrequency();
    const Uint64 period = (Uint64)(freq / 59.922743);
    next_frame = SDL_GetPerformanceCounter();
    while (running) {
        frame();
        /* pace by the audio queue when possible, otherwise by the clock */
        if (audio_dev && have_rom && !paused) {
            Uint32 limit = (Uint32)(have.freq * 4 / 20); /* ~50 ms */
            while (running && SDL_GetQueuedAudioSize(audio_dev) > limit)
                SDL_Delay(1);
            next_frame = SDL_GetPerformanceCounter();
        } else {
            next_frame += period;
            Uint64 now = SDL_GetPerformanceCounter();
            if (now < next_frame)
                SDL_Delay((Uint32)((next_frame - now) * 1000 / freq));
            else
                next_frame = now;
        }
    }
    SDL_Quit();
#endif
    return 0;
}
