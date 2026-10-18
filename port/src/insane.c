/* Insane mode (Game tab; unlocked by beating the whole game on Hard without the stage
 * select code, or by the code A B B A Down A B B A on the title menu, cheats.c).
 *
 *   - every guard but the purple ones takes two shots: the first only clinks
 *   - the purple guard aims and shoots 1.2 times as fast (44 frames a shot -> 36)
 *   - the shield guard sends shots it stops back at the player (as a guard's bullet,
 *     which costs a life), and walks and runs 1.2 times as fast
 *   - green guards look for the players every frame (normally every 4th, the two
 *     players in turn): they notice you about four times as soon
 * Offline only (an online game runs the same game on both machines). The stage select
 * code does nothing while it is on (cheats.c).
 *
 * Guard kinds, per guard object (obj*2 = o): the class is the long at $6F24 + o*2 (the
 * shot event script): 0x211AC the normal guard (blue; green with attribute $3C66 & 0x6000
 * = 0x4000; one purple-coloured with 0x2000), 0x211B8 the purple guard, 0x1FDBA the big
 * purple guard, 0x2306C the shield guard. Shots: player shots are objects whose $99F0 is
 * 8 (player 1) or $C (player 2); a guard's bullet is type $10 ($385E its step, $9BF4 the
 * guard, $3252 frame 0x165), removed without touching the players' shot counts
 * $9DF8 / $9DFA. Script data changed (in the ROM buffer, while in a stage, put back
 * otherwise): the purple guard's aiming count 0x21674 (10 -> 8) and its wait after a
 * shot 0x21764 (32 -> 27); the shield guard's patrol legs 0x22C9A / 0x22CD4 (80 -> 67
 * frames, so that with the faster walk it covers the same ground).
 *
 * Earning it: the ending starts ($C068 = 0x23AB6) with the difficulty ($C04E) on Hard,
 * in a game that did not use the stage select code (or a test play) since the machine
 * was reset. */
#include <stdio.h>
#include <string.h>
#include "insane.h"
#include "md.h"
#include "net.h"
#include "settings.h"
#include "cheats.h"
#include "gamemods.h"
#include "menu.h"
#include "m68k.h"
#include "m68kcpu.h"

#define NOBJ 0x56
#define CLASS_NORMAL 0x211ACu
#define CLASS_PURPLE 0x211B8u
#define CLASS_BIG 0x1FDBAu
#define CLASS_SHIELD 0x2306Cu
#define UNLOCK_SHOW (6 * 60)

static int r16(int a) { return (md.ram[a & 0xFFFF] << 8) | md.ram[(a + 1) & 0xFFFF]; }
static void w16(int a, int v) { md.ram[a & 0xFFFF] = (uint8_t)(v >> 8); md.ram[(a + 1) & 0xFFFF] = (uint8_t)v; }
static uint32_t r32(int a) { return (uint32_t)r16(a) << 16 | (uint32_t)r16(a + 2); }

static struct { uint32_t addr; uint16_t orig, insane; } patch[] = {
    { 0x21674, 0x000A, 0x0008 },     /* purple guard: aiming */
    { 0x21764, 0x0020, 0x001B },     /* ... the wait after a shot */
    { 0x22C9A, 0x0050, 0x0043 },     /* shield guard: patrol legs */
    { 0x22CD4, 0x0050, 0x0043 },
};
#define NPATCH ((int)(sizeof(patch) / sizeof(patch[0])))

static int patched;
static uint8_t hits[NOBJ];           /* shots a guard has taken (and shrugged off) */
static int16_t last_x[NOBJ], last_y[NOBJ];
static int8_t walk[NOBJ];            /* the shield guard's steps towards its extra pixel */
static int tainted, earned_show;

static uint32_t guard_class(int o) { return r32(0x6F24 + o * 2); }
static int colour(int o) { return r16(0x3C66 + o) & 0x6000; }
static int is_guard(int o) { int i = o >> 1; return i >= 6 && i < 6 + r16(0x6CC2) + 1 && i < 38; }

static int purple(int o)
{
    uint32_t c = guard_class(o);
    return c == CLASS_PURPLE || c == CLASS_BIG || (c == CLASS_NORMAL && colour(o) == 0x2000);
}

int insane_active(void)
{
    return settings.insane && settings.insane_unlocked && net_state() != NET_PLAYING;
}

static void set_patches(int on)
{
    if (on == patched) return;
    for (int i = 0; i < NPATCH; i++) {
        uint32_t a = patch[i].addr;
        if (a + 1 >= md.rom_size) continue;
        uint16_t have = (uint16_t)(md.rom[a] << 8 | md.rom[a + 1]);
        uint16_t want = on ? patch[i].insane : patch[i].orig;
        if (have != (on ? patch[i].orig : patch[i].insane)) continue;   /* (a mod moved it) */
        md.rom[a] = (uint8_t)(want >> 8);
        md.rom[a + 1] = (uint8_t)want;
    }
    patched = on;
}

void insane_reset(void)
{
    set_patches(0);
    memset(hits, 0, sizeof(hits));
    memset(walk, 0, sizeof(walk));
    tainted = 0;
}

void insane_taint(void) { tainted = 1; }

int insane_frame(void)
{
    int earned = 0;
    if (earned_show) earned_show--;
    if (cheat_stage_select()) tainted = 1;
    /* the ending, reached on Hard in a game played through */
    if (r32(0xC068) == 0x23AB6 && r16(0xC04E) == 2 && !tainted && !settings.insane_unlocked
        && net_state() != NET_PLAYING) {
        settings.insane_unlocked = 1;
        earned = 1;
        earned_show = UNLOCK_SHOW;
    }
    int in_stage = r16(0xC070) == 0x0C;
    int on = insane_active() && in_stage;
    set_patches(on);
    game_mods_apply();
    if (!on) {
        memset(hits, 0, sizeof(hits));
        return earned;
    }
    /* the shield guard: a pixel more for every five it walks */
    for (int i = 6; i < 38; i++) {
        int o = i * 2, x = (int16_t)r16(0x304E + o), y = (int16_t)r16(0x30FA + o);
        if (!r16(0x2EF6 + o) || !is_guard(o) || guard_class(o) != CLASS_SHIELD) { walk[i] = 0; last_x[i] = (int16_t)x; continue; }
        int d = x - last_x[i];
        if (y == last_y[i] && d && d >= -3 && d <= 3 && !r16(0x8DC4 + o)) {   /* walking, not down */
            walk[i] = (int8_t)(walk[i] + d);
            if (walk[i] >= 5 || walk[i] <= -5) {
                int step = walk[i] > 0 ? 1 : -1;
                x += step;
                w16(0x304E + o, x);
                walk[i] = (int8_t)(walk[i] - 5 * step);
            }
        }
        last_x[i] = (int16_t)x;
        last_y[i] = (int16_t)y;
    }
    return earned;
}

/* a player's shot (object d0) reached guard d6 */
static int shot_hits(int guard, uint32_t skip_to)
{
    if (!is_guard(guard) || purple(guard)) return 0;
    if (hits[guard >> 1] == 0) {
        hits[guard >> 1] = 1;                 /* the first shot only clinks */
        w16(0xFF5668, 0x009B);
        REG_PC = skip_to;                     /* (past the jsr; the shot is used up) */
        return 2;
    }
    hits[guard >> 1] = 0;                     /* down it goes */
    return 0;
}

/* a shield stopped player p's shot d0: it flies back at him as the guard's bullet */
static int reflect(int p)
{
    int shot = REG_D[0] & 0xFFFF, guard = REG_D[6] & 0xFFFF;
    if (shot >= NOBJ * 2 || guard >= NOBJ * 2) return 0;
    w16(0x99F0 + shot, 0x10);
    w16(0x385E + shot, -(int16_t)r16(0x385E + shot));
    w16(0x9BF4 + shot, guard);
    w16(0x3252 + shot, 0x165);
    w16(0x3BBA + shot, 0);
    w16(0x9A9C + shot, 0);
    int count = p ? 0x9DFA : 0x9DF8;          /* the player may shoot again */
    if (r16(count) > 0) w16(count, r16(count) - 1);
    return 1;                                 /* (as the routine's rts) */
}

int insane_hook(uint32_t pc)
{
    if (!insane_active()) return 0;
    int guard = REG_D[6] & 0xFFFF;
    switch (pc) {
    case INSANE_HOOK_P1_HIT: return shot_hits(guard, 0x18BC8u);
    case INSANE_HOOK_P2_HIT: return shot_hits(guard, 0x18EB4u);
    case INSANE_HOOK_P1_SHIELD: return reflect(0);
    case INSANE_HOOK_P2_SHIELD: return reflect(1);
    case INSANE_HOOK_LOOK: {
        /* green guards look every frame, at the two players in turn */
        int o = REG_D[0] & 0xFFFF;
        if (!is_guard(o) || guard_class(o) != CLASS_NORMAL || colour(o) != 0x4000) return 0;
        w16(0xFF6CC0, 0);
        int other = r16(0x9400 + o) ^ 1;
        w16(0x9400 + o, other);
        REG_PC = other ? 0x16E8Cu : 0x16C68u; /* sub_16E8C (player 2) / sub_16C68 (player 1); its rts returns for us */
        return 2;
    }
    }
    return 0;
}

static void centred(uint32_t *fb, int stride, int width, int height, int y, const char *s, uint32_t c)
{
    int x = (width - 8 * (int)strlen(s)) / 2;
    menu_text(fb, stride, height, x < 0 ? 0 : x, y, s, c);
}

int insane_overlay_on(void) { return earned_show > 0; }

void insane_overlay(uint32_t *fb, int stride, int width, int height)
{
    if (!earned_show) return;
    for (int y = 96; y < 128 && y < height; y++)
        for (int x = 0; x < width; x++)
            fb[y * stride + x] = 0xE0100010u;
    centred(fb, stride, width, height, 100, "INSANE MODE UNLOCKED!", 0xFFFF5060u);
    centred(fb, stride, width, height, 114, "MENU > GAME > INSANE MODE", 0xFFFFFFFFu);
}
