/* The game's logic, rewritten in readable C.
 *
 * Each function here does what one of the game's routines does, in plain C on the
 * game's work memory: the same values in the same places. The recompiler calls it where
 * the routine starts (tools/recomp.py LOGIC) and does the routine's rts after it; with
 * logic_off set (--translated) the game's own routine runs instead, so the two can be
 * compared (they must leave the same game, frame for frame).
 *
 * The 68000 is not quite gone yet: the game's clock is its cycle count, and the code
 * around a routine still runs as translated 68000 code. So each function also
 *   - takes the time the routine took (the time of each instruction it ran, from the
 *     table the recompiler makes: ran_to / branched);
 *   - leaves the registers the routine changed, the words it saved on the stack, and
 *     the condition flags, as the routine left them (at the end of each function,
 *     "left as the 68000 leaves them").
 * Once the code that calls a routine is in C too, those parts go. */
#include "recomp_rt.h"
#include "game.h"
#include "logic.h"
#include <stdio.h>
#include <stdlib.h>

uint64_t logic_stat_c;
int logic_off = -1;

/* ---------------------------------------------------------------- work memory */

static uint16_t ram16(uint32_t a) { a &= 0xFFFF; return (uint16_t)(md.ram[a] << 8 | md.ram[(a + 1) & 0xFFFF]); }
static void set16(uint32_t a, uint16_t v) { a &= 0xFFFF; md.ram[a] = (uint8_t)(v >> 8); md.ram[(a + 1) & 0xFFFF] = (uint8_t)v; }
static void set32(uint32_t a, uint32_t v) { set16(a, (uint16_t)(v >> 16)); set16(a + 2, (uint16_t)v); }
static uint32_t rom32(uint32_t a) { return (uint32_t)md.rom[a] << 24 | (uint32_t)md.rom[a + 1] << 16 | (uint32_t)md.rom[a + 2] << 8 | md.rom[a + 3]; }

/* the low word of a data register, and setting it (the high word kept) */
static uint16_t lo(uint32_t r) { return (uint16_t)r; }
static void set_lo(int d, uint16_t v) { REG_D[d] = (REG_D[d] & 0xFFFF0000u) | v; }

/* ---------------------------------------------------------------- time */

static int cyc;            /* the routine's time so far */
static uint32_t pc_at;     /* the next instruction whose time is not counted yet */

static int instr_time(uint32_t a) { return game.m68k_cost[a >> 1]; }

/* the routine starts at a */
static void begin(uint32_t a) { cyc = 0; pc_at = a; }

/* it ran on, one instruction after the other, up to (not including) b: branches on the
 * way were not taken */
static void ran_to(uint32_t b)
{
    for (; pc_at < b; pc_at += 2)
        cyc += instr_time(pc_at);
}

/* it ran up to the branch at a, which was taken, to b */
static void branched(uint32_t a, uint32_t b)
{
    ran_to(a);
    uint16_t op = (uint16_t)(md.rom[a] << 8 | md.rom[a + 1]);
    int t = instr_time(a);                 /* (the table: as when not taken) */
    if ((op & 0xF0F8) == 0x50C8) t -= 4;   /* dbcc looping: 10 */
    else if ((op & 0xF000) == 0x6000 && (op & 0x0F00) > 0x0100)
        t += (op & 0xFF) ? 2 : -2;          /* bcc taken: 10 */
    cyc += t;
    pc_at = b;
}

/* calls done in C, by routine (logic_report) */
static struct { const char *name; uint64_t n; } calls[64];

static int start_named(const char *name)
{
    if (logic_off < 0) logic_off = getenv("BONANZA_TRANSLATED") != 0;
    if (logic_off) return 0;
    logic_stat_c++;
    for (int i = 0; i < 64; i++)
        if (calls[i].name == name || !calls[i].name) { calls[i].name = name; calls[i].n++; break; }
    return 1;
}
#define start() start_named(__func__)

void logic_report(FILE *f)
{
    for (int i = 0; i < 64 && calls[i].name; i++)
        fprintf(f, "  %-24s %llu\n", calls[i].name, (unsigned long long)calls[i].n);
}

/* the routine done (up to its rts): its time used */
static int done(void)
{
    USE_CYCLES(cyc);
    return 1;
}

/* ---------------------------------------------------------------- left as the 68000 leaves them */

/* the flags of a move / tst of a word or long (N, Z; V and C clear; X kept) */
static void flags_of16(uint16_t v) { FLAG_N = NFLAG_16(v); FLAG_Z = v; FLAG_V = VFLAG_CLEAR; FLAG_C = CFLAG_CLEAR; }
static void flags_of32(uint32_t v) { FLAG_N = NFLAG_32(v); FLAG_Z = v; FLAG_V = VFLAG_CLEAR; FLAG_C = CFLAG_CLEAR; }

/* movem.l of registers to the stack (as saved, then taken back): the words stay below
 * the stack pointer */
static void saved_on_stack(const int *regs, int n)
{
    uint32_t a = REG_A[7] - 4u * n;
    for (int i = 0; i < n; i++)
        WR32(a + 4u * i, REG_DA[regs[i]]);
}

/* ================================================================ sounds */

#define SOUND_REQUEST 0xFF5668   /* the sound the next frame sends to the sound driver */

/* sub_02D54: ask for sound effect d0, unless a music command (0xE0-0xEF: fade, stop...)
 * is waiting to be sent: it goes first. */
int logic_sound_request(void)
{
    if (!start()) return 0;
    uint16_t waiting = ram16(SOUND_REQUEST);
    int music_waiting = (waiting & 0xFFF0) == 0x00E0;
    if (!music_waiting)
        set16(SOUND_REQUEST, lo(REG_D[0]));

    /* left as the 68000 leaves them: d7 saved and taken back; flags of the compare, or
     * of the word stored */
    static const int d7[] = {7};
    saved_on_stack(d7, 1);
    if (music_waiting) {
        FLAG_N = NFLAG_CLEAR; FLAG_Z = 0; FLAG_V = VFLAG_CLEAR; FLAG_C = CFLAG_CLEAR;   /* (0xE0 - 0xE0) */
    } else {
        flags_of16(lo(REG_D[0]));
    }
    begin(0x02D54);
    if (music_waiting) branched(0x02D66, 0x02D70);
    ran_to(0x02D74);
    return done();
}

/* ================================================================ the players */

/* A player who has just been hurt, has tripped or has landed is protected for a while:
 * guards and their bullets leave him alone. Player 1's protection is $56BE (1 blinking,
 * 2 lying / getting up), counted down from $56C0 frames; $56C2 keeps his picture of the
 * moment. Player 2's: $5768, $576A, $576C. */
#define P1_PROTECT      0xFF56BE
#define P2_PROTECT      0xFF5768
#define P1_FRAME        0xFF3252   /* player 1's sprite frame (object 0); player 2's: +2 */

static int protect(uint32_t base, uint32_t frame, uint16_t kind, uint32_t routine)
{
    if (!start()) return 0;
    uint16_t frames = lo(REG_D[7]);
    uint16_t picture = ram16(frame);
    set16(base + 2, frames);          /* how long */
    set16(base, kind);                /* what kind */
    set16(base + 4, picture);         /* the picture he had */
    flags_of16(picture);
    begin(routine);
    ran_to(routine + 0x18);
    return done();
}

/* sub_09EDA / sub_09EF4: protect player 1 / 2 for d7 frames (blinking) */
int logic_protect_p1(void)  { return protect(P1_PROTECT, P1_FRAME, 1, 0x09EDA); }
int logic_protect_p2(void)  { return protect(P2_PROTECT, P1_FRAME + 2, 1, 0x09EF4); }
/* sub_09F0E / sub_09F28: the same, while he lies or gets up */
int logic_protect2_p1(void) { return protect(P1_PROTECT, P1_FRAME, 2, 0x09F0E); }
int logic_protect2_p2(void) { return protect(P2_PROTECT, P1_FRAME + 2, 2, 0x09F28); }

/* sub_097F6 / sub_0981E: start player 1's / 2's animation d7: its number in $33AA (+2),
 * its step and time ($35AE, $365A: from the first step, at once), and its steps (a list
 * in the table at 0x36E00 / 0x37720, $3456 / $345A). */
static int player_anim(int p, uint32_t table, uint32_t routine)
{
    if (!start()) return 0;
    uint16_t anim = lo(REG_D[7]);
    uint32_t steps = rom32(table + (uint32_t)(int16_t)(anim << 2));
    set16(0xFF33AA + 2 * p, anim);
    set16(0xFF35AE + 2 * p, 0);
    set16(0xFF365A + 2 * p, 1);
    set32(0xFF3456 + 4 * p, steps);

    /* left as the 68000 leaves them: d7 times 4 (lsl.w #2), a6 the steps */
    FLAG_X = (uint)anim >> 6;                  /* (lsl's last bit out, in Musashi's place) */
    set_lo(7, (uint16_t)(anim << 2));
    REG_A[6] = steps;
    flags_of32(steps);
    begin(routine);
    ran_to(routine + 0x26);
    return done();
}

int logic_player_anim_p1(void) { return player_anim(0, 0x36E00, 0x097F6); }
int logic_player_anim_p2(void) { return player_anim(1, 0x37720, 0x0981E); }

/* sub_0986A / sub_098B8: player 1 / 2 goes into state d7 (standing, walking, shot,
 * tripped...): the state before and its routine kept ($568E, $5690), the new state's
 * routine from the table at 0xB492 / 0xD996 ($568A: run every frame). */
static int player_state(uint32_t state, uint32_t table, uint32_t routine)
{
    if (!start()) return 0;
    uint16_t next = lo(REG_D[7]);
    uint32_t handler = rom32(table + (uint32_t)(int16_t)(uint16_t)(next << 2));
    set16(state + 6, ram16(state));               /* the state before */
    set32(state + 8, ram16(state + 2) << 16 | ram16(state + 4));
    set16(state, next);
    set32(state + 2, handler);

    /* left as the 68000 leaves them: d7 times 4, a6 the table */
    FLAG_X = (uint)next >> 6;
    set_lo(7, (uint16_t)(next << 2));
    REG_A[6] = table;
    flags_of32(handler);
    begin(routine);
    ran_to(routine + 0x28);
    return done();
}

int logic_player_state_p1(void) { return player_state(0xFF5688, 0x0B492, 0x0986A); }
int logic_player_state_p2(void) { return player_state(0xFF5732, 0x0D996, 0x098B8); }

/* sub_098E2 / sub_09928: keep player 1's / 2's pose (animation, its steps, step, time
 * and picture: the picture he had when protection began, if protected) in $56D2.. /
 * $577C..; sub_0996E / sub_099A2 put it back. */
static int pose_keep(int p, uint32_t routine)
{
    if (!start()) return 0;
    uint32_t keep = p ? 0xFF577C : 0xFF56D2;
    uint32_t protect = p ? P2_PROTECT : P1_PROTECT;
    set16(keep, ram16(0xFF33AA + 2 * p));
    set32(keep + 2, (uint32_t)ram16(0xFF3456 + 4 * p) << 16 | ram16(0xFF3458 + 4 * p));
    set16(keep + 6, ram16(0xFF35AE + 2 * p));
    set16(keep + 8, ram16(0xFF365A + 2 * p));
    int protected_now = ram16(protect) != 0;
    uint16_t picture = protected_now ? ram16(protect + 4) : ram16(P1_FRAME + 2 * p);
    set16(keep + 10, picture);

    set_lo(7, picture);                           /* (left in d7) */
    flags_of16(picture);
    begin(routine);
    if (!protected_now) branched(routine + 0x34, routine + 0x3E);
    ran_to(routine + 0x44);
    return done();
}

static int pose_back(int p, uint32_t routine)
{
    if (!start()) return 0;
    uint32_t keep = p ? 0xFF577C : 0xFF56D2;
    set16(0xFF33AA + 2 * p, ram16(keep));
    set32(0xFF3456 + 4 * p, (uint32_t)ram16(keep + 2) << 16 | ram16(keep + 4));
    set16(0xFF35AE + 2 * p, ram16(keep + 6));
    set16(0xFF365A + 2 * p, ram16(keep + 8));
    set16(P1_FRAME + 2 * p, ram16(keep + 10));
    flags_of16(ram16(keep + 10));
    begin(routine);
    ran_to(routine + 0x32);
    return done();
}

int logic_pose_keep_p1(void) { return pose_keep(0, 0x098E2); }
int logic_pose_keep_p2(void) { return pose_keep(1, 0x09928); }
int logic_pose_back_p1(void) { return pose_back(0, 0x0996E); }
int logic_pose_back_p2(void) { return pose_back(1, 0x099A2); }

/* sub_0A51A / sub_0A542: player 1 / 2 starts moving along the path a6 (its step: x, y
 * and a third word), a5 kept in $6C12 / $6C16. */
static int path_start(int p, uint32_t routine)
{
    if (!start()) return 0;
    uint32_t path = REG_A[6];
    set32(p ? 0xFF5748 : 0xFF569E, path);
    set16(0xFF385E + 2 * p, (uint16_t)RD16(path));       /* x step */
    set16(0xFF390A + 2 * p, (uint16_t)RD16(path + 2));   /* y step */
    set16(p ? 0xFF575A : 0xFF56B0, (uint16_t)RD16(path + 4));
    set16(p ? 0xFF574C : 0xFF56A2, 1);
    set32(p ? 0xFF6C16 : 0xFF6C12, REG_A[5]);

    REG_A[6] = path + 4;                          /* (left past the two steps) */
    flags_of32(REG_A[5]);
    begin(routine);
    ran_to(routine + 0x26);
    return done();
}

int logic_path_start_p1(void) { return path_start(0, 0x0A51A); }
int logic_path_start_p2(void) { return path_start(1, 0x0A542); }

/* sub_0BF10 / sub_0E392: d7 = the word for player 1's / 2's height within its 8 pixels
 * (y & 7), from the table at 0xBF28 / 0xE3AA */
static int height_step(int p, uint32_t table, uint32_t routine)
{
    if (!start()) return 0;
    uint16_t sub = ram16(0xFF30FA + 2 * p) & 7;
    uint16_t v = (uint16_t)((md.rom[table + 2 * sub] << 8) | md.rom[table + 2 * sub + 1]);
    set_lo(7, v);
    REG_A[6] = table;
    FLAG_X = 0;                                   /* (add.w d7,d7: no carry) */
    flags_of16(v);
    begin(routine);
    ran_to(routine + 0x16);
    return done();
}

int logic_height_step_p1(void) { return height_step(0, 0x0BF28, 0x0BF10); }
int logic_height_step_p2(void) { return height_step(1, 0x0E3AA, 0x0E392); }

/* ================================================================ objects */

#define OBJ_ACTIVE 0xFF2EF6   /* per object (number * 2): nonzero while it is in play */

/* sub_0AE8A / sub_0AEA6: the object player 1 / 2 left to be taken away ($9DFE /
 * $9E00, an object number * 2; 0 none) goes */
static int take_away(uint32_t slot, uint32_t routine)
{
    if (!start()) return 0;
    uint16_t obj = ram16(slot);
    set_lo(7, obj);
    begin(routine);
    if (obj) {
        set16(slot, 0);
        set16(OBJ_ACTIVE + (int16_t)obj, 0);
        REG_A[6] = OBJ_ACTIVE;
        FLAG_N = NFLAG_CLEAR; FLAG_Z = 0; FLAG_V = VFLAG_CLEAR; FLAG_C = CFLAG_CLEAR;   /* (clr) */
        ran_to(routine + 0x1A);
    } else {
        flags_of16(0);
        branched(routine + 6, routine + 0x1A);
    }
    return done();
}

int logic_take_away_p1(void) { return take_away(0xFF9DFE, 0x0AE8A); }
int logic_take_away_p2(void) { return take_away(0xFF9E00, 0x0AEA6); }

/* sub_19C00 / sub_19C26: object d0 goes to object $9E02's place, 32 / 64 pixels above it */
static int place_above(int up, uint32_t routine)
{
    if (!start()) return 0;
    uint16_t from = ram16(0xFF9E02), obj = lo(REG_D[0]);
    set16(0xFF304E + (int16_t)obj, ram16(0xFF304E + (int16_t)from));          /* x */
    uint16_t y = ram16(0xFF30FA + (int16_t)from);
    uint16_t ny = (uint16_t)(y - up);
    set16(0xFF30FA + (int16_t)obj, ny);                                        /* y */

    /* left as the 68000 leaves them */
    set_lo(2, from);
    set_lo(7, ny);
    REG_A[6] = 0xFF30FA;
    FLAG_X = CFLAG_16((uint)y - (uint)up);
    flags_of16(ny);
    begin(routine);
    ran_to(routine + 0x24);
    return done();
}

int logic_place_above32(void) { return place_above(0x20, 0x19C00); }
int logic_place_above64(void) { return place_above(0x40, 0x19C26); }

/* ================================================================ screens */

/* sub_04460: a screen's wait: when its count ($5546) runs out, the screen's next routine
 * ($C072) becomes the one run every frame ($C068) */
int logic_screen_wait(void)
{
    if (!start()) return 0;
    uint16_t before = ram16(0xFF5546);
    uint16_t left = (uint16_t)(before - 1);
    set16(0xFF5546, left);
    uint sub = (uint)before - 1;
    FLAG_X = FLAG_C = CFLAG_16(sub);           /* (subq.w) */
    FLAG_N = NFLAG_16(sub); FLAG_Z = left; FLAG_V = VFLAG_SUB_16(1, before, sub);
    begin(0x04460);
    if (left) {
        branched(0x04466, 0x04474);
    } else {
        uint32_t next = (uint32_t)ram16(0xFFC072) << 16 | ram16(0xFFC074);
        set32(0xFFC068, next);
        flags_of32(next);
        ran_to(0x04474);
    }
    return done();
}

/* ================================================================ guards */

/* sub_17170: send guard d6 (object number * 2) event d7: 4 shot, 8 touched, $C a player
 * seen, ... (each a script the guard runs next). Unless the guard ignores the event now
 * (its mask $6DA8: bit d7/4 - 1), the guard's script goes back to its stack's start
 * ($7CB0: the stack at $7D48 + (d6 + 2) * 64) and the event waits in $6D5C. */
#define GUARD_EVENT_MASK  0xFF6DA8
#define GUARD_EVENT       0xFF6D5C
#define GUARD_SCRIPT_SP   0xFF7CB0
#define GUARD_STACKS      0xFF7D48
#define GUARD_SEEN        0xFF931C   /* the player a guard has seen: 2 player 1, 4 player 2 */

/* sub_09F42 / sub_09F66: every guard (objects 6-37) after player 1 (2) / player 2 (4)
 * forgets him ($931C: the player it has seen) */
static int guards_forget(uint16_t player, uint32_t routine)
{
    if (!start()) return 0;
    begin(routine);
    ran_to(routine + 0x0E);
    for (int o = 0xC; o < 0xC + 64; o += 2) {
        if (ram16(GUARD_SEEN + o) == player) {
            set16(GUARD_SEEN + o, 0);
            ran_to(routine + 0x1E);
        } else {
            branched(routine + 0x14, routine + 0x1C);
        }
        if (o < 0xC + 62) branched(routine + 0x1E, routine + 0x0E);
        else ran_to(routine + 0x22);
    }

    /* left as the 68000 leaves them */
    REG_A[6] = GUARD_SEEN;
    set_lo(7, 0x4C);
    set_lo(6, 0xFFFF);
    FLAG_N = NFLAG_CLEAR; FLAG_Z = 0x4C; FLAG_V = VFLAG_CLEAR; FLAG_X = FLAG_C = CFLAG_CLEAR;
    return done();
}

int logic_guards_forget_p1(void) { return guards_forget(2, 0x09F42); }
int logic_guards_forget_p2(void) { return guards_forget(4, 0x09F66); }

/* sub_171B6: the script guard d6 of kind d7 runs for the event waiting for it ($6D5C):
 * d7 = the word for that event in the kind's row of the table at 0x171D6 (16 words a
 * kind, by event / 4); 0 when no event waits */
int logic_guard_event_script(void)
{
    if (!start()) return 0;
    uint16_t kind = lo(REG_D[7]), guard = lo(REG_D[6]);
    uint16_t row_off = (uint16_t)(kind << 5);
    uint32_t row = 0x171D6 + (uint32_t)(int16_t)row_off;
    uint16_t event = ram16(GUARD_EVENT + (int16_t)guard);
    FLAG_X = (uint)kind >> 3;                         /* (lsl.w #5) */
    REG_A[6] = row;
    REG_A[5] = GUARD_EVENT;
    begin(0x171B6);
    if (!event) {
        set_lo(7, 0);
        flags_of16(0);
        branched(0x171C8, 0x171D4);
        return done();
    }
    uint16_t idx = (uint16_t)((event >> 2) * 2);
    uint16_t script = (uint16_t)(md.rom[row + (int16_t)idx] << 8 | md.rom[row + (int16_t)idx + 1]);
    set_lo(7, script);
    FLAG_X = CFLAG_16((uint)(event >> 2) + (event >> 2));   /* (add.w d7,d7) */
    flags_of16(script);
    ran_to(0x171D4);
    return done();
}

int logic_guard_event(void)
{
    if (!start()) return 0;
    uint16_t guard = lo(REG_D[6]), event = lo(REG_D[7]);
    uint16_t mask = ram16(GUARD_EVENT_MASK + (int16_t)guard);
    uint16_t bit = (uint16_t)((event >> 2) - 1);
    int ignored = (((REG_D[5] & 0xFFFF0000u) | mask) >> (bit & 31)) & 1;
    uint32_t stack = GUARD_STACKS + (uint32_t)(int16_t)(uint16_t)((guard + 2) << 6);
    if (!ignored) {
        set32(GUARD_SCRIPT_SP + (int16_t)(uint16_t)(guard * 2), stack);
        set16(GUARD_EVENT + (int16_t)guard, event);
    }

    /* left as the 68000 leaves them: a5/a6 saved and taken back; d4 the bit, d5 the
     * mask or the guard * 2; flags */
    static const int a5a6[] = {8 + 5, 8 + 6};
    saved_on_stack(a5a6, 2);
    set_lo(4, bit);
    if (ignored) {
        uint sub = (uint)(event >> 2) - 1;               /* (subq.w #1, unmasked) */
        set_lo(5, mask);
        FLAG_N = NFLAG_16(sub);
        FLAG_X = FLAG_C = CFLAG_16(sub);
        FLAG_V = VFLAG_SUB_16(1, event >> 2, sub);
        FLAG_Z = REG_D[5] & (1u << (bit & 31));          /* (btst) */
    } else {
        uint16_t twice = (uint16_t)(guard * 2);
        set_lo(5, twice);
        FLAG_X = CFLAG_16((uint)guard + guard);
        flags_of16(event);
    }
    begin(0x17170);
    if (ignored) branched(0x17186, 0x171B0);
    ran_to(0x171B4);
    return done();
}
