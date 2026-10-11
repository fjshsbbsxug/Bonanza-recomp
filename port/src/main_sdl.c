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
#include "menu.h"
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
static SDL_Texture *menu_tex;
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
                if (fm[i] == hl)
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
    if (!have_rom)
        SDL_SetWindowTitle(window, status_msg);
    else
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
                if (!e.key.repeat && (k == SDLK_p || k == SDLK_PAUSE)) {
                    paused = !paused;
                    set_title();
                    break;
                }
                if (!e.key.repeat && k == SDLK_F3 && have_rom) {
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
        case SDL_CONTROLLERBUTTONDOWN:
            if (e.cbutton.button == SDL_CONTROLLER_BUTTON_BACK || e.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE) {
                toggle_menu();
            } else if (menu_is_open()) {
                int mk = menu_button_for(e.cbutton.button);
                if (mk >= 0)
                    menu_action(menu_key(mk));
            }
            break;
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

static Uint64 next_frame;

static void run_one(void)
{
    if (have_rom && !paused && !menu_is_open()) {
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
}

static void frame(void)
{
    handle_events();
#ifdef __EMSCRIPTEN__
    /* the browser calls us at the display refresh rate; run the game at 59.92 Hz */
    static double acc, last;
    double now = emscripten_get_now();
    if (last == 0)
        last = now - 1000.0 / 59.922743;
    acc += now - last;
    last = now;
    if (acc > 100)
        acc = 100;
    int n = 0;
    while (acc >= 1000.0 / 59.922743 && n < 4) {
        acc -= 1000.0 / 59.922743;
        run_one();
        n++;
    }
    /* drop audio backlog so latency stays low */
    if (audio_dev && SDL_GetQueuedAudioSize(audio_dev) > 48000u * 4 / 5)
        SDL_ClearQueuedAudio(audio_dev);
#else
    run_one();
#endif
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
    int use_recomp = 1;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--interp"))
            use_recomp = 0;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1"); /* Back opens the menu */
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
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, MD_FB_W, MD_SCREEN_H);
    menu_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, MENU_W, MENU_H);
    SDL_SetTextureBlendMode(menu_tex, SDL_BLENDMODE_BLEND);
#ifdef __EMSCRIPTEN__
    menu_allow_quit = 0;
#endif
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

    load_settings();
    last_screen = settings.screen;
    fit_window();
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
        if (audio_dev && have_rom && !paused && !menu_is_open()) {
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
    if (menu_settings_dirty)
        save_settings();
    SDL_Quit();
#endif
    return 0;
}
