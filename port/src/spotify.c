/* Is Spotify playing music on this computer? (see spotify.h)
 *   Windows: Spotify's main window is titled "Artist - Song" while it plays, "Spotify",
 *            "Spotify Free" or "Spotify Premium" while it does not.
 *   Linux:   its playback status on the session bus (MPRIS), through dbus-send.
 *   macOS:   its player state, through osascript.
 * Elsewhere (Android, the web) it cannot be seen: never playing. */
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include "spotify.h"

#if defined(_WIN32) || ((defined(__linux__) || defined(__APPLE__)) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__))
#define SPOTIFY_CAN_SEE 1
#endif

static SDL_atomic_t playing, watching;
static SDL_Thread *thread;

#ifdef _WIN32
#include <windows.h>

static BOOL CALLBACK spotify_window(HWND w, LPARAM found)
{
    if (!IsWindowVisible(w) || GetWindow(w, GW_OWNER)) return TRUE;
    wchar_t title[256];
    if (GetWindowTextW(w, title, 256) <= 0) return TRUE;
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!p) return TRUE;
    wchar_t path[MAX_PATH];
    DWORD n = MAX_PATH;
    int spotify = 0;
    if (QueryFullProcessImageNameW(p, 0, path, &n)) {
        const wchar_t *base = wcsrchr(path, L'\\');
        spotify = _wcsicmp(base ? base + 1 : path, L"Spotify.exe") == 0;
    }
    CloseHandle(p);
    if (!spotify) return TRUE;
    /* not playing: its own name; playing: the song */
    if (wcsncmp(title, L"Spotify", 7) != 0 && wcschr(title, L'-')) {
        *(int *)found = 1;
        return FALSE;
    }
    return TRUE;
}

static int ask(void)
{
    int found = 0;
    EnumWindows(spotify_window, (LPARAM)&found);
    return found;
}
#elif defined(SPOTIFY_CAN_SEE)
static int ask(void)
{
#ifdef __APPLE__
    const char *cmd = "osascript -e 'if application \"Spotify\" is running then tell application "
                      "\"Spotify\" to player state as string' 2>/dev/null";
    const char *word = "playing";
#else
    const char *cmd = "dbus-send --print-reply --dest=org.mpris.MediaPlayer2.spotify "
                      "/org/mpris/MediaPlayer2 org.freedesktop.DBus.Properties.Get "
                      "string:org.mpris.MediaPlayer2.Player string:PlaybackStatus 2>/dev/null";
    const char *word = "\"Playing\"";
#endif
    FILE *f = popen(cmd, "r");
    if (!f) return 0;
    char line[256];
    int yes = 0;
    while (fgets(line, sizeof(line), f))
        if (strstr(line, word)) yes = 1;
    pclose(f);
    return yes;
}
#endif

#ifdef SPOTIFY_CAN_SEE
static int watcher(void *unused)
{
    (void)unused;
    while (SDL_AtomicGet(&watching)) {
        SDL_AtomicSet(&playing, ask());
        for (int i = 0; i < 20 && SDL_AtomicGet(&watching); i++)
            SDL_Delay(100);
    }
    SDL_AtomicSet(&playing, 0);
    return 0;
}
#endif

void spotify_watch(int on)
{
#ifdef SPOTIFY_CAN_SEE
    if (on && !thread) {
        SDL_AtomicSet(&watching, 1);
        thread = SDL_CreateThread(watcher, "spotify", NULL);
        if (!thread) SDL_AtomicSet(&watching, 0);
    } else if (!on && thread) {
        SDL_AtomicSet(&watching, 0);
        SDL_WaitThread(thread, NULL);
        thread = NULL;
    }
#else
    (void)on;
#endif
}

int spotify_playing(void)
{
    return SDL_AtomicGet(&playing);
}
