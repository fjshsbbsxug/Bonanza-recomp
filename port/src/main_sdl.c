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
#include "settings.h"
#include "net.h"
#include "gamemods.h"
#include "extrastages.h"
#include "cheats.h"
#include "vdp.h"
#include "menu.h"
#include "mods.h"
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "sndmix.h"
#include "sndrec.h"

#define APP_ORG  "bonanza-recomp"
#define APP_NAME "Bonanza Bros"

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_AudioDeviceID audio_dev;
static SDL_GameController *pads[2];
static SDL_Joystick *joys[2];      /* pads SDL has no button layout for (generic USB/Bluetooth) */
static int running = 1, paused, have_rom, show_touch;
static uint8_t key_pad[2], ctl_pad[2], touch_pad;
static char status_msg[256] = "Drop the Bonanza Bros. ROM file onto this window";
static uint8_t *rom_buf;          /* the original ROM */
static size_t rom_buf_size;
static uint8_t *mod_rom;          /* the ROM with the enabled mods applied */
static SDL_Texture *menu_tex;
static sndrec *recording;          /* the sounds being recorded from the ROM (first start) */
static char sounds_path[1024];
static int out_rate = 48000;
static int pending_stage, pending_extra;   /* --play-stage / --play-extra, after recording */
static uint32_t menu_buf[MENU_W * MENU_H];
static SDL_Rect menu_rect;              /* where the menu overlay is drawn (window pixels) */
static char settings_path[1024];

/* ------------------------------------------------------------------ settings */

static int file_exists(const char *path)
{
    SDL_RWops *rw = SDL_RWFromFile(path, "rb");
    if (rw)
        SDL_RWclose(rw);
    return rw != NULL;
}

/* bonanza.ini next to the program wins ("portable", e.g. made by the HTML builder);
 * otherwise it lives in the user data folder. */
static void load_settings(void)
{
    char *base = SDL_GetBasePath();
    if (base) {
        snprintf(settings_path, sizeof(settings_path), "%sbonanza.ini", base);
        SDL_free(base);
    }
    if (!settings_path[0] || !file_exists(settings_path)) {
        char *pref = SDL_GetPrefPath(APP_ORG, APP_NAME);
        if (pref) {
            snprintf(settings_path, sizeof(settings_path), "%sbonanza.ini", pref);
            SDL_free(pref);
        }
    }
    settings_defaults();
    if (settings_load(settings_path))
        SDL_Log("settings from %s", settings_path);
    settings_apply();
}

/* ------------------------------------------------------------------ mods */

static void make_dir(const char *path)
{
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}

/* The mods folder sits next to bonanza.ini (on Android: in the app's folder on the
 * phone's storage, Android/data/<app>/files/mods, reachable with a file manager). */
static void setup_mods(void)
{
    char dir[1100];
#if defined(__ANDROID__)
    const char *ext = SDL_AndroidGetExternalStoragePath();
    if (ext) {
        snprintf(dir, sizeof(dir), "%s/mods/", ext);
        make_dir(dir);
        mods_add_dir(dir);
    }
#endif
    snprintf(dir, sizeof(dir), "%s", settings_path);
    char *slash = strrchr(dir, '/');
#ifdef _WIN32
    char *bs = strrchr(dir, '\\');
    if (!slash || (bs && bs > slash)) slash = bs;
#endif
    if (slash) {
        strcpy(slash + 1, "mods/");
        make_dir(dir);
        mods_add_dir(dir);
    }
    char *base = SDL_GetBasePath();       /* mods next to the program count too */
    if (base) {
        snprintf(dir, sizeof(dir), "%smods/", base);
        SDL_free(base);
        mods_add_dir(dir);
    }
    mods_scan();
    SDL_Log("mods folder: %s (%d mods, %d enabled)", mods_main_dir(), mods_count(), mods_enabled_count());
}

/* (re)load the machine with the original ROM plus the enabled mods */
static void load_rom_with_mods(void)
{
    if (!rom_buf) return;
    if (!mod_rom && !(mod_rom = malloc(0x400000))) {
        md_load_rom(rom_buf, rom_buf_size);
        return;
    }
    size_t n = mods_apply(rom_buf, rom_buf_size, mod_rom, 0x400000);
    md_load_rom(mod_rom, n);
    if (mods_status()[0])
        SDL_Log("%s", mods_status());
}

/* Give a windowed desktop window the shape of the picture (4:3, 16:9, ...). */
static void fit_window(void)
{
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
    Uint32 flags = SDL_GetWindowFlags(window);
    if (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_MAXIMIZED))
        return;
    if (settings.screen == SCREEN_STRETCH)
        return;
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    int nw = (int)(h * settings_aspect() + 0.5);
    if (nw != w)
        SDL_SetWindowSize(window, nw, h);
#endif
}

static void save_settings(void)
{
    if (settings_path[0] && !settings_save(settings_path))
        SDL_Log("could not save %s", settings_path);
}

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

/* The game's sounds, recorded from the ROM (sndrec.c) by the build (sounds.bbs next to
 * the program) or else the first time the game starts, and played back (sndmix.c): no
 * sound hardware runs while the game plays. On the web the page fetches the build's
 * file or keeps the recorded one (IndexedDB) and puts it there before the game starts. */
static void start_sounds(void)
{
    if (snd_ready() || recording)
        return;
    if (!sounds_path[0]) {
#ifdef __EMSCRIPTEN__
        snprintf(sounds_path, sizeof(sounds_path), "/sounds.bbs");
#else
        /* made by the build, next to the program; else recorded once into the user
         * data folder */
        char *base = SDL_GetBasePath();
        if (base) {
            snprintf(sounds_path, sizeof(sounds_path), "%ssounds.bbs", base);
            SDL_free(base);
        }
        if (!sounds_path[0] || !file_exists(sounds_path) || !snd_open(sounds_path)) {
            char *pref = SDL_GetPrefPath(APP_ORG, APP_NAME);
            snprintf(sounds_path, sizeof(sounds_path), "%ssounds.bbs", pref ? pref : "");
            SDL_free(pref);
        }
#endif
    }
    if (snd_ready()) {
        SDL_Log("sounds: %s", sounds_path);
        return;
    }
    if (snd_open(sounds_path)) {
        SDL_Log("sounds: %s", sounds_path);
        return;
    }
    recording = snd_record_start(sounds_path);
    if (!recording)
        SDL_Log("could not write %s: the sound hardware is run instead", sounds_path);
}

static void start_stage(int entry);

/* a slice of the recording (about 25 ms); when it is done the game starts */
static void record_sounds(void)
{
    double t0 = (double)SDL_GetTicks();
    int r;
    do r = snd_record_step(recording, 10);
    while (r == 0 && SDL_GetTicks() - t0 < 25);
    if (r == 0)
        return;
    snd_record_free(recording);
    recording = NULL;
    md_audio_set_rate(out_rate);
    if (r > 0 && snd_open(sounds_path)) {
        SDL_Log("sounds recorded: %s", sounds_path);
#ifdef __EMSCRIPTEN__
        EM_ASM({ if (Module.bonanzaSoundsRecorded) Module.bonanzaSoundsRecorded(UTF8ToString($0)); }, sounds_path);
#endif
    } else
        SDL_Log("could not record the sounds: the sound hardware is run instead");
    md_reset();
    if (pending_extra) { xs_play(pending_extra - 1); start_stage(2); }
    else if (pending_stage) start_stage(pending_stage);
    pending_stage = pending_extra = 0;
}

static void present_recording(void)
{
    int done, total;
    snd_record_progress(recording, &done, &total);
    memset(menu_buf, 0, sizeof(menu_buf));
    for (int i = 0; i < MENU_W * MENU_H; i++) menu_buf[i] = 0xFF101018;
    menu_text(menu_buf, MENU_W, MENU_H, 24, 92, "Recording the game's sounds", 0xFFFFFFFF);
    menu_text(menu_buf, MENU_W, MENU_H, 24, 104, "from your ROM (first start only)", 0xFFB0B0C0);
    char t[64];
    snprintf(t, sizeof(t), "%d of %d", done, total);
    menu_text(menu_buf, MENU_W, MENU_H, 24, 124, t, 0xFFFFD040);
    for (int x = 24; x < MENU_W - 24; x++)
        for (int y = 140; y < 148; y++)
            menu_buf[y * MENU_W + x] = x - 24 < (MENU_W - 48) * done / (total ? total : 1) ? 0xFFFFD040 : 0xFF404050;
    int ww, wh;
    SDL_GetRendererOutputSize(renderer, &ww, &wh);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    int mh = wh, mw = wh * 4 / 3;
    if (mw > ww) { mw = ww; mh = ww * 3 / 4; }
    SDL_Rect r = { (ww - mw) / 2, (wh - mh) / 2, mw, mh };
    SDL_UpdateTexture(menu_tex, NULL, menu_buf, MENU_W * 4);
    SDL_RenderCopy(renderer, menu_tex, NULL, &r);
    SDL_RenderPresent(renderer);
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
    rom_buf_size = size;
    load_rom_with_mods();
    save_rom_copy(data, size);
    md_reset();
    have_rom = 1;
    SDL_Log("loaded ROM from %s", path);
    start_sounds();
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
    for (int i = 1; i < argc; i++) {
        if (i > 1 && (!strcmp(argv[i - 1], "--join") || !strcmp(argv[i - 1], "--play-stage") ||
                      !strcmp(argv[i - 1], "--play-extra") ||
                      (!strcmp(argv[i - 1], "--host") && atoi(argv[i]) > 0)))
            continue;                            /* an address or port, not a ROM */
        if (argv[i][0] != '-' && try_rom(argv[i], 1))
            return;
    }
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
    for (int i = 0; i < 2; i++) {
        if (pads[i]) {
            SDL_GameControllerClose(pads[i]);
            pads[i] = NULL;
        }
        if (joys[i]) {
            SDL_JoystickClose(joys[i]);
            joys[i] = NULL;
        }
    }
    /* first pad = player 1; pads with a known layout first, then any other joystick */
    int n = 0;
    for (int i = 0; i < SDL_NumJoysticks() && n < 2; i++)
        if (SDL_IsGameController(i) && (pads[n] = SDL_GameControllerOpen(i)) != NULL)
            n++;
    for (int i = 0; i < SDL_NumJoysticks() && n < 2; i++)
        if (!SDL_IsGameController(i) && (joys[n] = SDL_JoystickOpen(i)) != NULL) {
            if (SDL_JoystickNumButtons(joys[n]) < 2) {   /* not a gamepad (accelerometer...) */
                SDL_JoystickClose(joys[n]);
                joys[n] = NULL;
                continue;
            }
            n++;
        }
}

/* a joystick without a known layout: hat or first stick, buttons 1-3 = A B C, a
 * button further right (usually Start) = Start */
static uint8_t joystick_state(SDL_Joystick *j)
{
    uint8_t b = 0;
    int hat = SDL_JoystickNumHats(j) > 0 ? SDL_JoystickGetHat(j, 0) : 0;
    int ax = SDL_JoystickNumAxes(j) > 1 ? SDL_JoystickGetAxis(j, 0) : 0;
    int ay = SDL_JoystickNumAxes(j) > 1 ? SDL_JoystickGetAxis(j, 1) : 0;
    if ((hat & SDL_HAT_UP) || ay < -16000) b |= PAD_UP;
    if ((hat & SDL_HAT_DOWN) || ay > 16000) b |= PAD_DOWN;
    if ((hat & SDL_HAT_LEFT) || ax < -16000) b |= PAD_LEFT;
    if ((hat & SDL_HAT_RIGHT) || ax > 16000) b |= PAD_RIGHT;
    int nb = SDL_JoystickNumButtons(j);
    static const uint8_t abc[3] = {PAD_A, PAD_B, PAD_C};
    for (int i = 0; i < 3 && i < nb; i++)
        if (SDL_JoystickGetButton(j, i)) b |= abc[i];
    for (int i = 6; i < nb && i < 12; i++)
        if (SDL_JoystickGetButton(j, i)) b |= PAD_START;
    return b;
}

static uint8_t controller_state(int p)
{
    SDL_GameController *c = pads[p];
    if (!c)
        return joys[p] ? joystick_state(joys[p]) : 0;
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
    {0.01f, 0.02f, 0.08f, 0.09f, 0, "MENU"},
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

static void menu_action(int act);

static void touch_event(SDL_Event *e)
{
    show_touch = 1;
    if (e->type == SDL_FINGERDOWN) {
        int ww, wh;
        SDL_GetRendererOutputSize(renderer, &ww, &wh);
        if (menu_is_open()) {
            int mx = (int)((e->tfinger.x * ww - menu_rect.x) * MENU_W / (menu_rect.w ? menu_rect.w : 1));
            int my = (int)((e->tfinger.y * wh - menu_rect.y) * MENU_H / (menu_rect.h ? menu_rect.h : 1));
            menu_action(menu_pointer(mx, my));
            return;
        }
        touch_btn *mb = &tbtns[NTB - 1];
        if (e->tfinger.x >= mb->x && e->tfinger.x < mb->x + mb->w && e->tfinger.y >= mb->y && e->tfinger.y < mb->y + mb->h) {
            menu_open();
            return;
        }
    }
    if (menu_is_open())
        return;
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
        int on = b->bits && (touch_pad & b->bits) == b->bits;
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, on ? 110 : 45);
        SDL_RenderFillRect(renderer, &r);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 120);
        SDL_RenderDrawRect(renderer, &r);
        if (!b->bits) { /* menu button: three bars */
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
            for (int k = 0; k < 3; k++) {
                SDL_Rect bar = {r.x + r.w / 4, r.y + r.h * (k + 1) / 4 - r.h / 24, r.w / 2, r.h / 12 + 1};
                SDL_RenderFillRect(renderer, &bar);
            }
        }
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
        uint32_t *fb = (uint32_t *)md_framebuffer();
        int hl = menu_highlight();
        if (hl >= 0 && (SDL_GetTicks() / 200) % 2) {
            /* flash the colour being recoloured */
            const uint16_t *fm = md_framebuffer_md();
            for (int i = 0; i < MD_SCREEN_H * MD_FB_W; i++)
                if (VDP_COLOR(fm[i]) == hl)
                    fb[i] = 0xFFFFFFFFu ^ (fb[i] & 0x00FFFFFF);
        }
        SDL_UpdateTexture(texture, NULL, fb, MD_FB_W * 4);
#if SDL_VERSION_ATLEAST(2, 0, 12)
        SDL_SetTextureScaleMode(texture, settings.smooth ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
#endif
        SDL_Rect src = {0, 0, sw, MD_SCREEN_H};
        SDL_Rect dst = {0, 0, ww, wh};
        if (settings.screen != SCREEN_STRETCH) {
            /* the picture keeps its TV shape: 4:3, or wider in widescreen modes */
            double aspect = settings_aspect();
            int dw = ww, dh = (int)(ww / aspect);
            if (dh > wh) {
                dh = wh;
                dw = (int)(wh * aspect);
            }
            dst.x = (ww - dw) / 2;
            dst.y = (wh - dh) / 2;
            dst.w = dw;
            dst.h = dh;
        }
        SDL_RenderCopy(renderer, texture, &src, &dst);
        if (show_touch && !menu_is_open())
            draw_touch(ww, wh);
    }
    /* menu overlay: 4:3 box in the middle of the window */
    int mh = wh, mw = wh * 4 / 3;
    if (mw > ww) {
        mw = ww;
        mh = ww * 3 / 4;
    }
    menu_rect.x = (ww - mw) / 2;
    menu_rect.y = (wh - mh) / 2;
    menu_rect.w = mw;
    menu_rect.h = mh;
    if (menu_is_open()) {
        menu_draw(menu_buf);
        SDL_UpdateTexture(menu_tex, NULL, menu_buf, MENU_W * 4);
        SDL_RenderCopy(renderer, menu_tex, NULL, &menu_rect);
    }
    SDL_RenderPresent(renderer);
}

static void set_title(void)
{
    char t[256];
    if (!have_rom)
        SDL_SetWindowTitle(window, status_msg);
    else if (net_state() != NET_OFF) {
        snprintf(t, sizeof(t), "Bonanza Bros - %s", net_status());
        SDL_SetWindowTitle(window, t);
    } else
        SDL_SetWindowTitle(window, paused ? "Bonanza Bros (paused)" : "Bonanza Bros");
}

/* ------------------------------------------------------------------ loop */

static int last_screen = -1;

static void menu_action(int act)
{
    if (settings.screen != last_screen) {
        last_screen = settings.screen;
        fit_window();
    }
    if (act == MENU_ACT_NONE)
        return;
    if (menu_settings_dirty) {
        save_settings();
        menu_settings_dirty = 0;
    }
    if (act == MENU_ACT_MODS && have_rom && net_state() != NET_PLAYING) {
        load_rom_with_mods();
        md_reset();
    }
    if (act == MENU_ACT_RESET && have_rom)
        md_reset();
    else if (act == MENU_ACT_QUIT)
        running = 0;
    memset(key_pad, 0, sizeof(key_pad));
}

static void toggle_menu(void)
{
    if (menu_is_open())
        menu_action(menu_key(MK_BACK));
    else if (have_rom)
        menu_open();
}

static int menu_key_for(SDL_Keycode k)
{
    switch (k) {
    case SDLK_UP: case SDLK_w: return MK_UP;
    case SDLK_DOWN: case SDLK_s: return MK_DOWN;
    case SDLK_LEFT: case SDLK_a: return MK_LEFT;
    case SDLK_RIGHT: case SDLK_d: return MK_RIGHT;
    case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: case SDLK_z: return MK_OK;
    case SDLK_BACKSPACE: case SDLK_x: return MK_BACK;
    }
    return -1;
}

static int menu_button_for(int b)
{
    switch (b) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return MK_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return MK_DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return MK_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return MK_RIGHT;
    case SDL_CONTROLLER_BUTTON_A: case SDL_CONTROLLER_BUTTON_START: return MK_OK;
    case SDL_CONTROLLER_BUTTON_B: return MK_BACK;
    }
    return -1;
}

static void handle_events(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            running = 0;
            break;
        case SDL_TEXTINPUT:
            if (menu_text_editing())
                menu_text_typed(e.text.text);
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
            if (e.type == SDL_KEYDOWN) {
                if (!e.key.repeat && (k == SDLK_F11 || (k == SDLK_RETURN && (e.key.keysym.mod & KMOD_ALT)))) {
                    Uint32 fs = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                    SDL_SetWindowFullscreen(window, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                    break;
                }
                if (menu_text_editing()) {          /* typing an address: only these keys */
                    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) menu_action(menu_key(MK_OK));
                    else if (k == SDLK_ESCAPE || k == SDLK_AC_BACK) menu_action(menu_key(MK_BACK));
                    else if (k == SDLK_BACKSPACE) menu_text_backspace();
                    break;
                }
                if (!e.key.repeat && (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_F1 || k == SDLK_TAB)) {
                    toggle_menu();
                    break;
                }
                if (menu_is_open()) {
                    int mk = menu_key_for(k);
                    if (mk >= 0)
                        menu_action(menu_key(mk));
                    break;
                }
                if (!e.key.repeat && (k == SDLK_p || k == SDLK_PAUSE) && net_state() != NET_PLAYING) {
                    paused = !paused;
                    set_title();
                    break;
                }
                if (!e.key.repeat && k == SDLK_F3 && have_rom && net_state() != NET_PLAYING) {
                    md_reset();
                    break;
                }
            }
            if (menu_is_open())
                break;
            int pl;
            uint8_t bit = key_bit(k, &pl);
            if (e.type == SDL_KEYDOWN)
                key_pad[pl] |= bit;
            else
                key_pad[pl] &= ~bit;
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
            if (e.button.which == SDL_TOUCH_MOUSEID)
                break;
            if (menu_is_open() && e.button.button == SDL_BUTTON_LEFT) {
                int ww, wh, winw, winh;
                SDL_GetRendererOutputSize(renderer, &ww, &wh);
                SDL_GetWindowSize(window, &winw, &winh);
                /* window coordinates -> renderer pixels (high-DPI) -> menu coordinates */
                int px = e.button.x * ww / (winw ? winw : 1), py = e.button.y * wh / (winh ? winh : 1);
                menu_action(menu_pointer((px - menu_rect.x) * MENU_W / (menu_rect.w ? menu_rect.w : 1),
                                         (py - menu_rect.y) * MENU_H / (menu_rect.h ? menu_rect.h : 1)));
            } else if (e.button.button == SDL_BUTTON_RIGHT) {
                toggle_menu();
            }
            break;
        case SDL_JOYBUTTONDOWN:
        case SDL_JOYHATMOTION:
            show_touch = 0;      /* playing with a controller: hide the touch buttons */
            break;
        case SDL_CONTROLLERBUTTONDOWN:
            show_touch = 0;
            if (e.cbutton.button == SDL_CONTROLLER_BUTTON_BACK || e.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE) {
                toggle_menu();
            } else if (menu_is_open()) {
                int mk = menu_button_for(e.cbutton.button);
                if (mk >= 0)
                    menu_action(menu_key(mk));
            }
            break;
        case SDL_JOYDEVICEADDED:
        case SDL_JOYDEVICEREMOVED:
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
            if (menu_settings_dirty)
                save_settings();
            break;
        case SDL_APP_DIDENTERFOREGROUND:
            paused = 0;
            break;
        }
    }
}

static SDL_AudioSpec have;
static int vsync_on;              /* the renderer waits for the display's refresh */

/* The picture is shown at the display's refresh rate, which is rarely exactly the
 * Mega Drive's 59.92 Hz; the audio queue absorbs the difference by dropping or
 * repeating a few samples (inaudible) instead of making the picture wait. */
static void queue_audio(int16_t *buf, int n)
{
    Uint32 queued = SDL_GetQueuedAudioSize(audio_dev) / 4;
    Uint32 hi = (Uint32)have.freq * 90 / 1000, lo = (Uint32)have.freq * 35 / 1000;
    if (queued > hi * 3) {
        SDL_ClearQueuedAudio(audio_dev);
    } else if (queued > hi && n > 64) {
        n -= 4;
    } else if (queued < lo && n > 0) {
        for (int i = 0; i < 4; i++) {
            buf[(n + i) * 2] = buf[(n - 1) * 2];
            buf[(n + i) * 2 + 1] = buf[(n - 1) * 2 + 1];
        }
        n += 4;
    }
    SDL_QueueAudio(audio_dev, buf, (Uint32)n * 4);
}

/* Online games: the game sometimes has to wait a frame for the other player's buttons,
 * then runs two at once. The sound is kept further ahead (about 0.12 s) and played very
 * slightly faster or slower to stay there - never repeated or cut, which sounds like
 * an echo. */
static void queue_audio_net(const int16_t *buf, int n)
{
    static double pos;                   /* read position into the input, carried over */
    static int16_t last[2];
    Uint32 queued = SDL_GetQueuedAudioSize(audio_dev) / 4;
    Uint32 target = (Uint32)have.freq * 120 / 1000;
    if (queued > target * 4) {           /* far too much (after a long stall): start over */
        SDL_ClearQueuedAudio(audio_dev);
        queued = 0;
    }
    if (queued == 0) {                   /* ran dry: put some silence ahead of us */
        static int16_t zero[2048 * 2];
        Uint32 pad = target / 2 < 2048 ? target / 2 : 2048;
        SDL_QueueAudio(audio_dev, zero, pad * 4);
    }
    double err = ((double)target - (double)queued) / (double)target;   /* + = too little */
    if (err > 1) err = 1;
    if (err < -1) err = -1;
    double step = 1.0 - 0.04 * err;      /* input samples per output sample */
    int16_t out[4096 * 2];
    int m = 0;
    while (pos < n && m < 4096) {
        int i = (int)pos;
        double f = pos - i;
        const int16_t *a = i == 0 ? last : &buf[(i - 1) * 2], *b = &buf[i * 2];
        out[m * 2] = (int16_t)(a[0] + (b[0] - a[0]) * f);
        out[m * 2 + 1] = (int16_t)(a[1] + (b[1] - a[1]) * f);
        m++;
        pos += step;
    }
    pos -= n;
    if (pos < 0) pos = 0;
    last[0] = buf[(n - 1) * 2];
    last[1] = buf[(n - 1) * 2 + 1];
    SDL_QueueAudio(audio_dev, out, (Uint32)m * 4);
}

/* ---- online games (net.c): both machines run the same frames with both players' buttons */
static int saved_floor_alert = -1;

static void net_update(void)
{
    static int text_on;
    static Uint32 title_at;
    net_poll();
    if (net_take_start() && have_rom) {
        if (net_local_player() == 1) {           /* the host's game options count */
            if (saved_floor_alert < 0) saved_floor_alert = settings.floor_alert;
            settings.floor_alert = net_option_floor_alert();
            game_mods_apply();
        }
        md_reset();
        paused = 0;
        memset(key_pad, 0, sizeof(key_pad));
        SDL_Log("online game started: player %d", net_local_player() + 1);
    }
    static int last_state = NET_OFF;
    if (net_state() != last_state) {
        last_state = net_state();
        if (last_state == NET_OFF)
            SDL_Log("online: %s", net_status());
    }
    if (net_state() != NET_PLAYING && saved_floor_alert >= 0) {
        settings.floor_alert = saved_floor_alert;
        saved_floor_alert = -1;
        game_mods_apply();
    }
    vdp_local_player = net_state() == NET_PLAYING ? net_local_player() : -1;
    if (menu_text_editing() != text_on) {
        text_on = menu_text_editing();
        if (text_on) SDL_StartTextInput(); else SDL_StopTextInput();
    }
    if (SDL_GetTicks() - title_at > 1000) {
        title_at = SDL_GetTicks();
        set_title();
    }
}

/* --play-stage E (level maker test play): a 1-player game straight into stage entry E.
 * Start is pressed through the title as a player would, and the stage number is set
 * before the stage loads; the frames before that are run at once, without showing them. */
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

static int run_net_frame(void)
{
    uint8_t mine = 0, pads2[2];
    if (!menu_is_open()) {                      /* the game cannot wait for the menu */
        for (int p = 0; p < 2; p++)
            ctl_pad[p] = controller_state(p);
        mine = key_pad[0] | key_pad[1] | ctl_pad[0] | ctl_pad[1] | touch_pad;
    }
    net_local_input(mine);
    if (!net_inputs_for_next_frame(pads2))
        return 0;                               /* the other player's buttons are not here yet */
    md.pad[0] = pads2[0];
    md.pad[1] = pads2[1];
    md_run_frame();
    int16_t buf[2048 * 2 + 64];
    int n;
    while ((n = md_audio_take(buf, 2048)) > 0)
        if (audio_dev)
            queue_audio_net(buf, n);
    net_frame_done((net_frame() + 1) % 60 == 0 ? md_state_hash() : 0);
    return 1;
}

static void run_one(void)
{
    if (have_rom && net_state() == NET_PLAYING) {
        if (run_net_frame() && net_frames_waiting() > 2)
            run_net_frame();                    /* behind the other player: catch up */
        return;
    }
    if (have_rom && !paused && !menu_is_open()) {
        for (int p = 0; p < 2; p++) {
            ctl_pad[p] = controller_state(p);
            md.pad[p] = key_pad[p] | ctl_pad[p] | (p == 0 ? touch_pad : 0);
        }
        md_run_frame();
        cheat_frame();                          /* offline only: title-screen codes */
        int16_t buf[2048 * 2 + 64];
        int n;
        while ((n = md_audio_take(buf, 2048)) > 0)
            if (audio_dev)
                queue_audio(buf, n);
    } else {
        int16_t buf[2048 * 2];
        while (md_audio_take(buf, 2048) > 0) {
        }
    }
}

#define MD_FRAME_MS (1000.0 / 59.922743)

static double now_ms(void)
{
#ifdef __EMSCRIPTEN__
    return emscripten_get_now();
#else
    return (double)SDL_GetPerformanceCounter() * 1000.0 / (double)SDL_GetPerformanceFrequency();
#endif
}

/* One pass per displayed picture. On a ~60 Hz display with vsync the game runs exactly
 * one frame per refresh (smooth scrolling); otherwise frames are run by elapsed time. */
static void frame(void)
{
    static double last, acc, avg = MD_FRAME_MS;
    double now = now_ms();
    double dt = last ? now - last : MD_FRAME_MS;
    last = now;
    if (dt > 250) dt = MD_FRAME_MS;          /* after a pause or a dragged window */
    avg = avg * 0.95 + dt * 0.05;
    handle_events();
    if (recording) {                         /* first start: the sounds come first */
        record_sounds();
        if (recording) present_recording();
        return;
    }
    net_update();
    int locked = vsync_on && avg > 15.6 && avg < 17.6;
    if (locked) {
        acc = 0;
        run_one();
    } else {
        acc += dt;
        if (acc > 100) acc = 100;
        int n = 0;
        while (acc >= MD_FRAME_MS && n < 4) {
            acc -= MD_FRAME_MS;
            run_one();
            n++;
        }
    }
    present();
}

#ifdef __EMSCRIPTEN__
/* Called from the HTML builder page (Module.ccall) for its live preview. */
EMSCRIPTEN_KEEPALIVE int bonanza_set(const char *key, const char *value)
{
    int ok = settings_set(key, value);
    settings_apply();
    return ok;
}
EMSCRIPTEN_KEEPALIVE void bonanza_reset_settings(void)
{
    settings_defaults();
    settings_apply();
}
EMSCRIPTEN_KEEPALIVE void bonanza_save_settings(const char *path)
{
    settings_save(path);
}
EMSCRIPTEN_KEEPALIVE void bonanza_open_menu(void)
{
    if (have_rom && !menu_is_open())
        menu_open();
}
#endif

int main(int argc, char **argv)
{
    const char *host_port = NULL, *join_addr = NULL;
    int play_stage = 0, play_extra = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--play-stage") && i + 1 < argc)
            play_stage = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--play-extra") && i + 1 < argc)
            play_extra = atoi(argv[++i]);    /* extra stage K of the mods (1 = first) */
        else if (!strcmp(argv[i], "--host"))
            host_port = i + 1 < argc && argv[i + 1][0] != '-' ? argv[++i] : "";
        else if (!strcmp(argv[i], "--join") && i + 1 < argc)
            join_addr = argv[++i];
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1"); /* Back opens the menu */
    SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0"); /* the phone itself is not a pad */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
#ifdef __EMSCRIPTEN__
    const int win_w = 1280; /* the page shows a 16:9 canvas; 4:3 is pillarboxed inside */
#else
    const int win_w = 960;
#endif
    window = SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w, 720,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return 1;
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
        renderer = SDL_CreateRenderer(window, -1, 0);
    {
        SDL_RendererInfo ri;
        vsync_on = renderer && SDL_GetRendererInfo(renderer, &ri) == 0 && (ri.flags & SDL_RENDERER_PRESENTVSYNC);
#ifdef __EMSCRIPTEN__
        vsync_on = 1;   /* requestAnimationFrame */
#endif
    }
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, MD_FB_W, MD_SCREEN_H);
    menu_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, MENU_W, MENU_H);
    SDL_SetTextureBlendMode(menu_tex, SDL_BLENDMODE_BLEND);
#ifdef __EMSCRIPTEN__
    menu_allow_quit = 0;
#endif
#if defined(__ANDROID__)
    show_touch = 1;
#endif

    SDL_AudioSpec want = {0};
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    out_rate = audio_dev ? have.freq : 48000;
    md_audio_set_rate(out_rate);
    if (audio_dev)
        SDL_PauseAudioDevice(audio_dev, 0);
    open_pads();

    load_settings();
    setup_mods();
    last_screen = settings.screen;
    fit_window();
    find_rom(argc, argv);
    if (host_port)
        net_host(atoi(host_port));
    else if (join_addr)
        net_join(join_addr);
    if (recording) {                   /* test play waits for the sounds */
        if (play_extra > 0 && have_rom && play_extra <= xs_count()) pending_extra = play_extra;
        else if (play_stage > 0 && play_stage <= 13 && have_rom) pending_stage = play_stage;
    } else if (play_extra > 0 && have_rom && play_extra <= xs_count()) {
        xs_play(play_extra - 1);       /* the stage dispatcher plays it instead */
        start_stage(2);
    } else if (play_stage > 0 && play_stage <= 13 && have_rom)
        start_stage(play_stage);
    set_title();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(frame, 0, 1);
#else
    double next = now_ms();
    while (running) {
        double t0 = now_ms();
        frame();
        /* without a working vsync the loop would spin: wait for the next game frame */
        if (!vsync_on || now_ms() - t0 < 2.0) {
            next += MD_FRAME_MS;
            double wait = next - now_ms();
            if (wait > 0)
                SDL_Delay((Uint32)wait);
            else if (wait < -100)
                next = now_ms();
        } else {
            next = now_ms();
        }
    }
    if (menu_settings_dirty)
        save_settings();
    SDL_Quit();
#endif
    return 0;
}
