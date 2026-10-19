/* Insane mode: a fourth LEVEL in the game's own OPTIONS menu (EASY, NORMAL, HARD,
 * INSANE), offered once it is unlocked - by beating the whole game on Hard without the
 * stage select code, or by the code A B B A Down A B B A on the title menu (cheats.c).
 * Underneath the game plays on Hard ($C04E stays 2: the clock's tables have three
 * entries); the port keeps INSANE in its settings.
 *
 *   - every guard but the purple one takes two shots. The first one is taken the way the
 *     purple guard takes his: the guard flinches - turns to the shot, shows its hit frame
 *     for 16 frames, standing still - and then carries on
 *   - the purple guard (the big one who throws) goes down at his fourth shot as usual, but
 *     the shots before that no longer make him flinch or start his throwing over; one in a
 *     hundred bounces back at the player (with the spring's sound) as a thrown bullet. He
 *     throws 1.2 times as fast (107 frames a throw -> 89)
 *   - the shield guard sends the shots it stops back at the player (as a guard's bullet,
 *     which costs a life), and walks and runs 1.2 times as fast
 *   - green guards look for the players every frame (normally every 4th, the two
 *     players in turn): they notice you about four times as soon
 * Offline only (an online game runs the same game on both machines). The stage select
 * code does nothing while it is on (cheats.c).
 *
 * Guard kinds, per guard object (obj*2 = o): the class is the long at $6F24 + o*2 (the
 * shot event script): 0x211AC the normal guard (blue; green with attribute $3C66 & 0x6000
 * = 0x4000), 0x211B8 the light blue one, 0x22140 another blue one, 0x1FDBA the purple
 * guard, 0x2306C the shield guard, 0x1E9AA the small striped one, 0x1F40C the dog.
 *
 * A shot hitting guard d6 is `jsr sub_17170` with d7 = 4 at 0x18BC2 (player 1) /
 * 0x18EAE (player 2), followed by the shot's removal; skipping the call leaves the guard
 * as it was. The purple guard counts his shots in $7430 (4 when set; his script takes one
 * per shot, flinches and restarts his throwing, or at 0 goes down); $6DA8 bit 0 set: he
 * cannot be hit now. A flinch for the others: turned as the purple guard's script does
 * ($92D0 = $A3C0 + 1, the facing $7644, mirrored $32FE), their own hit animation (op B8:
 * $33AA, $3456 = the long at 0x17CDE + anim*4, $35AE, $365A) and, for 16 frames, the guard
 * loop skips their events and script (0x14416 -> 0x144AE: animation, gravity and contact
 * still run); then their animation is put back and their script goes on where it was.
 * Shots: player shots are objects whose $99F0 is 8 (player 1) or $C (player 2); a guard's
 * bullet is type $10 ($385E its step, $9BF4 the guard, $3252 frame 0x165), removed
 * without touching the players' shot counts $9DF8 / $9DFA. Sounds: requested through the
 * word $5668 (0xAD a purple guard hit, 0xAF the spring). Script data changed (in the ROM
 * buffer, while in a stage, put back otherwise): the purple guard's aiming count 0x2001C /
 * 0x20182 (30 -> 21 passes of 2 frames; his wind-up is left as it was), the shield
 * guard's patrol legs 0x22C9A / 0x22CD4 (80 -> 67 frames, so that with the faster walk it
 * covers the same ground).
 *
 * The LEVEL item (sub_03CC8, the value changed by d7 = -1 / +1) is done here when INSANE
 * can be chosen, and its value is drawn here when it is INSANE (the game draws
 * tbl_0412C[$C04E] with sub_011F2 at 0x3B22 and 0x3EC4: VDP command 0x42A00003, 6
 * characters, attribute d7 | (character + 0x500)).
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
#include "video.h"
#include "m68k.h"
#include "m68kcpu.h"

#define NOBJ 0x56
#define CLASS_NORMAL 0x211ACu
#define CLASS_LIGHT 0x211B8u
#define CLASS_BLUE2 0x22140u
#define CLASS_PURPLE 0x1FDBAu       /* the big guard who throws */
#define CLASS_SHIELD 0x2306Cu
#define CLASS_SMALL 0x1E9AAu
#define CLASS_DOG 0x1F40Cu
#define UNLOCK_SHOW (6 * 60)
#define FLINCH_FRAMES 16
#define SND_PURPLE_HIT 0xAD
#define SND_SPRING 0xAF

static int r16(int a) { return (md.ram[a & 0xFFFF] << 8) | md.ram[(a + 1) & 0xFFFF]; }
static void w16(int a, int v) { md.ram[a & 0xFFFF] = (uint8_t)(v >> 8); md.ram[(a + 1) & 0xFFFF] = (uint8_t)v; }
static uint32_t r32(int a) { return (uint32_t)r16(a) << 16 | (uint32_t)r16(a + 2); }
static void w32(int a, uint32_t v) { w16(a, (int)(v >> 16)); w16(a + 2, (int)(v & 0xFFFF)); }
static uint32_t rom32(uint32_t a) { return a + 3 < md.rom_size ? (uint32_t)md.rom[a] << 24 | md.rom[a + 1] << 16 | md.rom[a + 2] << 8 | md.rom[a + 3] : 0; }
/* a sound effect, as sub_02D54 asks for one (not over a waiting music command) */
static void sound(int id) { if ((r16(0x5668) & 0xFFF0) != 0x00E0) w16(0x5668, id); }

static struct { uint32_t addr; uint16_t orig, insane; } patch[] = {
    { 0x2001C, 0x001E, 0x0015 },     /* the purple guard: aiming between throws (both copies */
    { 0x20182, 0x001E, 0x0015 },     /* of his loop): 107 -> 89 frames a throw */
    { 0x22C9A, 0x0050, 0x0043 },     /* shield guard: patrol legs */
    { 0x22CD4, 0x0050, 0x0043 },
};
#define NPATCH ((int)(sizeof(patch) / sizeof(patch[0])))

static int patched;
static uint8_t hits[NOBJ];           /* shots a guard has taken (and shrugged off) */
static struct { int left; uint16_t anim, step, count; uint32_t ptr; } flinch[NOBJ];
static int16_t last_x[NOBJ], last_y[NOBJ];
static int8_t walk[NOBJ];            /* the shield guard's steps towards its extra pixel */
static int tainted, earned_show, level_changed;
static uint32_t rng = 0x1234567u;

static uint32_t guard_class(int o) { return r32(0x6F24 + o * 2); }
static int colour(int o) { return r16(0x3C66 + o) & 0x6000; }
static int is_guard(int o) { int i = o >> 1; return i >= 6 && i < 6 + r16(0x6CC2) + 1 && i < 38; }

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
    memset(flinch, 0, sizeof(flinch));
    memset(walk, 0, sizeof(walk));
    tainted = 0;
}

void insane_taint(void) { tainted = 1; }

int insane_frame(void)
{
    int save = level_changed;
    level_changed = 0;
    if (earned_show) earned_show--;
    if (cheat_stage_select()) tainted = 1;
    /* the ending, reached on Hard in a game played through */
    if (r32(0xC068) == 0x23AB6 && r16(0xC04E) == 2 && !tainted && !settings.insane_unlocked
        && net_state() != NET_PLAYING) {
        settings.insane_unlocked = 1;
        save = 1;
        earned_show = UNLOCK_SHOW;
    }
    if (insane_active() && r16(0xC04E) != 2)
        w16(0xC04E, 2);                       /* (the boot sets NORMAL) */
    int in_stage = r16(0xC070) == 0x0C;
    int on = insane_active() && in_stage;
    set_patches(on);
    game_mods_apply();
    if (!on) {
        memset(hits, 0, sizeof(hits));
        memset(flinch, 0, sizeof(flinch));
        return save;
    }
    for (int i = 6; i < 38; i++) {
        int o = i * 2;
        /* a flinch over: its own animation back, its script goes on */
        if (flinch[i].left && --flinch[i].left == 0) {
            w16(0x33AA + o, flinch[i].anim);
            w32(0x3456 + o * 2, flinch[i].ptr);
            w16(0x35AE + o, flinch[i].step);
            w16(0x365A + o, flinch[i].count);
        }
        /* the shield guard: a pixel more for every five it walks */
        int x = (int16_t)r16(0x304E + o), y = (int16_t)r16(0x30FA + o);
        if (!r16(0x2EF6 + o) || !is_guard(o) || guard_class(o) != CLASS_SHIELD) { walk[i] = 0; last_x[i] = (int16_t)x; continue; }
        int d = x - last_x[i];
        if (y == last_y[i] && d && d >= -3 && d <= 3 && !r16(0x8DC4 + o) && !flinch[i].left) {
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
    return save;
}

/* a player's shot d0 turned into the guard's bullet, flying back at the player */
static void bounce_back(int p, int shot, int guard)
{
    w16(0x99F0 + shot, 0x10);
    w16(0x385E + shot, -(int16_t)r16(0x385E + shot));
    w16(0x9BF4 + shot, guard);
    w16(0x3252 + shot, 0x165);
    w16(0x3BBA + shot, 0);
    w16(0x9A9C + shot, 0);
    int count = p ? 0x9DFA : 0x9DF8;          /* the player may shoot again */
    if (r16(count) > 0) w16(count, r16(count) - 1);
}

/* the guard's own hit animation for a flinch, 0 if it has none here */
static int hit_anim(uint32_t c)
{
    if (c == CLASS_NORMAL || c == CLASS_LIGHT || c == CLASS_BLUE2) return 0x14;
    if (c == CLASS_SHIELD) return 0x85;
    if (c == CLASS_SMALL) return 0x52;
    if (c == CLASS_DOG) return 0xB6;
    return 0;
}

static void start_flinch(int g)
{
    int i = g >> 1, anim = hit_anim(guard_class(g));
    /* turned to the shot, as the purple guard's script turns him (0x1F830) */
    int face = r16(0xA3C0 + g) + 1;
    w16(0x92D0 + g, face);
    w16(0x7644 + g, face);
    w16(0x32FE + g, face == 1);
    if (anim) {
        flinch[i].anim = (uint16_t)r16(0x33AA + g);
        flinch[i].ptr = r32(0x3456 + g * 2);
        flinch[i].step = (uint16_t)r16(0x35AE + g);
        flinch[i].count = (uint16_t)r16(0x365A + g);
        w16(0x33AA + g, anim);
        w32(0x3456 + g * 2, rom32(0x17CDE + (uint32_t)anim * 4));
        w16(0x35AE + g, 0);
        w16(0x365A + g, 1);
    } else {
        flinch[i].anim = (uint16_t)r16(0x33AA + g);
        flinch[i].ptr = r32(0x3456 + g * 2);
        flinch[i].step = (uint16_t)r16(0x35AE + g);
        flinch[i].count = (uint16_t)r16(0x365A + g);
    }
    flinch[i].left = FLINCH_FRAMES;
}

/* player p's shot d0 reached guard d6 (at the jsr that applies it; skip_to: past it) */
static int shot_hits(int p, uint32_t skip_to)
{
    int shot = REG_D[0] & 0xFFFF, g = REG_D[6] & 0xFFFF;
    if (!is_guard(g) || shot >= NOBJ * 2) return 0;
    int i = g >> 1;
    if (guard_class(g) == CLASS_PURPLE) {
        if (r16(0x6DA8 + g) & 1) return 0;    /* (he cannot be hit now) */
        rng = rng * 1103515245u + 12345u;
        if ((rng >> 16) % 100 == 0) {         /* one in a hundred bounces off him */
            bounce_back(p, shot, g);
            sound(SND_SPRING);
            return 1;                         /* (as the routine's rts: the shot flies on) */
        }
        if (r16(0x7430 + g) > 1) {            /* counted, but no flinch, no new throwing */
            w16(0x7430 + g, r16(0x7430 + g) - 1);
            sound(SND_PURPLE_HIT);
            REG_PC = skip_to;
            return 2;
        }
        return 0;                             /* his last one: down he goes */
    }
    if (hits[i] == 0) {
        hits[i] = 1;                          /* the first shot: a flinch */
        start_flinch(g);
        sound(SND_PURPLE_HIT);
        REG_PC = skip_to;                     /* (past the jsr; the shot is used up) */
        return 2;
    }
    hits[i] = 0;                              /* down it goes (the event takes over) */
    flinch[i].left = 0;
    return 0;
}

/* a shield stopped player p's shot d0: it flies back at him as the guard's bullet */
static int reflect(int p)
{
    int shot = REG_D[0] & 0xFFFF, guard = REG_D[6] & 0xFFFF;
    if (shot >= NOBJ * 2 || guard >= NOBJ * 2) return 0;
    bounce_back(p, shot, guard);
    return 1;                                 /* (as the routine's rts) */
}

/* ---- the OPTIONS menu's LEVEL: EASY, NORMAL, HARD and, unlocked, INSANE */
static int level_hook(void)
{
    int d = (int16_t)(REG_D[7] & 0xFFFF);
    if (!d) return 1;
    int v = insane_active() ? 3 : r16(0xC04E);
    v += d;
    if (v < 0) v = 3;
    if (v > 3) v = 0;
    w16(0xC04E, v > 2 ? 2 : v);
    settings.insane = v == 3;
    level_changed = 1;
    return 1;                                 /* (the routine done) */
}

static int draw_insane(uint32_t next)
{
    static const char word[] = "INSANE";
    uint16_t attr = (uint16_t)REG_D[7];
    for (int k = 0; k < 6; k++)               /* VDP command 0x42A00003: video memory 0xC2A0 */
        video_store(VIDEO_VRAM, (uint16_t)(0xC2A0 + 2 * k), (uint16_t)(attr | (word[k] + 0x500)));
    REG_PC = next;
    return 2;
}

int insane_owns(uint32_t pc)
{
    return pc == INSANE_HOOK_P1_HIT || pc == INSANE_HOOK_P2_HIT || pc == INSANE_HOOK_P1_SHIELD ||
           pc == INSANE_HOOK_P2_SHIELD || pc == INSANE_HOOK_LOOK || pc == INSANE_HOOK_GUARD ||
           pc == INSANE_HOOK_LEVEL || pc == INSANE_HOOK_LEVEL_DRAW1 || pc == INSANE_HOOK_LEVEL_DRAW2;
}

int insane_hook(uint32_t pc)
{
    if (net_state() == NET_PLAYING) return 0;
    switch (pc) {
    case INSANE_HOOK_LEVEL: return settings.insane_unlocked ? level_hook() : 0;
    case INSANE_HOOK_LEVEL_DRAW1: return insane_active() ? draw_insane(0x3B28u) : 0;
    case INSANE_HOOK_LEVEL_DRAW2: return insane_active() ? draw_insane(0x3ECAu) : 0;
    }
    if (!insane_active()) return 0;
    switch (pc) {
    case INSANE_HOOK_P1_HIT: return shot_hits(0, 0x18BC8u);
    case INSANE_HOOK_P2_HIT: return shot_hits(1, 0x18EB4u);
    case INSANE_HOOK_P1_SHIELD: return reflect(0);
    case INSANE_HOOK_P2_SHIELD: return reflect(1);
    case INSANE_HOOK_GUARD: {
        /* a flinching guard: no events, no script this frame */
        int o = REG_D[0] & 0xFFFF;
        if (o < NOBJ * 2 && flinch[o >> 1].left) { REG_PC = 0x144AEu; return 2; }
        return 0;
    }
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
    centred(fb, stride, width, height, 114, "OPTIONS > LEVEL > INSANE", 0xFFFFFFFFu);
}
