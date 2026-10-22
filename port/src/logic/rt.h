/* What the game's logic in C (port/src/logic/) is written with: the work memory by
 * name (ram.h), the time a routine takes, and calls to the game's routines not yet
 * written in C.
 *
 * Each function does what one of the game's routines does, in plain C on the game's
 * work memory: the same values in the same places. The recompiler calls it where the
 * routine starts (tools/recomp.py LOGIC) and does the routine's rts after it; with
 * BONANZA_TRANSLATED=1 the game's own routine runs instead, so the two can be compared
 * (they must leave the same game, frame for frame).
 *
 * While the code around a routine is still the game's translated code, a function also
 *   - takes the routine's time: the game's frame has a 68000 frame's time, and a frame
 *     whose work takes longer goes on in the next (a stage being set up). begin(),
 *     ran_to() and branched() count the time of the instructions the routine ran (from
 *     the recompiler's table port/gen/m68k_costs.c);
 *   - leaves what its callers read afterwards: the registers and condition flags
 *     tools/liveness.py lists for it ("leaves for its callers"), at its end. */
#ifndef LOGIC_RT_H
#define LOGIC_RT_H
#include "../recomp_rt.h"
#include "../game.h"
#include "../logic.h"
#include "ram.h"

/* ---- work memory (addresses 0xFFxxxx; ram.h names them) and the ROM */
static inline uint16_t ram16(uint32_t a) { a &= 0xFFFF; return (uint16_t)(md.ram[a] << 8 | md.ram[(a + 1) & 0xFFFF]); }
static inline uint32_t ram32(uint32_t a) { return (uint32_t)ram16(a) << 16 | ram16(a + 2); }
static inline void set16(uint32_t a, uint16_t v) { a &= 0xFFFF; md.ram[a] = (uint8_t)(v >> 8); md.ram[(a + 1) & 0xFFFF] = (uint8_t)v; }
static inline void set32(uint32_t a, uint32_t v) { set16(a, (uint16_t)(v >> 16)); set16(a + 2, (uint16_t)v); }
static inline uint8_t ram8(uint32_t a) { return md.ram[a & 0xFFFF]; }
static inline void set8(uint32_t a, uint8_t v) { md.ram[a & 0xFFFF] = v; }
/* what a movem / jsr writes onto the stack, `off` bytes from a7 (the C keeps the values
 * itself; a later routine may find them there, unwritten, above the stack pointer) */
static inline void stack_put(int32_t off, uint32_t v) { set32(REG_A[7] + (uint32_t)off, v); }
static inline uint16_t rom16(uint32_t a) { return (uint16_t)(md.rom[a] << 8 | md.rom[a + 1]); }
static inline uint32_t rom32(uint32_t a) { return (uint32_t)rom16(a) << 16 | rom16(a + 2); }
/* any address (ROM or RAM: script data is in both) */
static inline uint16_t mem16(uint32_t a) { return (uint16_t)RD16(a); }
static inline uint32_t mem32(uint32_t a) { return (uint32_t)RD32(a); }

/* per-object tables: a word per object, indexed by the object's number * 2 (the game's
 * "o"); long tables by o * 2 */
static inline uint16_t obj16(uint32_t table, uint16_t o) { return ram16(table + (int16_t)o); }
static inline void set_obj16(uint32_t table, uint16_t o, uint16_t v) { set16(table + (int16_t)o, v); }
static inline uint32_t obj32(uint32_t table, uint16_t o) { return ram32(table + 2 * (int16_t)o); }
static inline void set_obj32(uint32_t table, uint16_t o, uint32_t v) { set32(table + 2 * (int16_t)o, v); }

/* ---- the players' code: written once, with player 1's addresses (code and work memory);
 * player 2's copy is elsewhere, with its own work memory (tools/player_map.py lines the
 * two up): P(p, a) is the place a in player p's copy, PR(p, a) player p's word a, PT(p, a) his copy of
 * the table at a */
uint32_t player2_at(uint32_t a);
uint32_t player2_ram(uint32_t a);
uint32_t player2_table(uint32_t a);
static inline uint32_t P(int p, uint32_t a) { return p ? player2_at(a) : a; }
static inline uint32_t PR(int p, uint32_t a) { return p ? player2_ram(a) : a; }
static inline uint32_t PT(int p, uint32_t a) { return p ? player2_table(a) : a; }   /* a table */

/* the routine the jsr / bsr at `at` calls; and a call from the players' code: player p's
 * copy calls what its own jsr there calls (player 2's code calls its own copies, and
 * some routines of player 1's own; `a` is player 1's, checked) */
uint32_t jsr_target(uint32_t at);
void player_call(int p, uint32_t a, uint32_t jsr);
int  player_jump(int p, uint32_t a, uint32_t jmp);   /* ... the same for a jmp abs.l (its time too) */
/* the long (a routine's address) the move.l #imm at instr sets: player 2's copy's own */
uint32_t player_imm32(int p, uint32_t a, uint32_t instr);

/* ---- registers: the low word of a data register, and setting it (the high word kept) */
static inline uint16_t lo(uint32_t r) { return (uint16_t)r; }
static inline void set_lo(int d, uint16_t v) { REG_D[d] = (REG_D[d] & 0xFFFF0000u) | v; }

/* ---- the condition flags (Musashi's: FLAG_Z holds the result, so Z is set when it is 0;
 * a bne is taken when FLAG_Z != 0) */
static inline int z_clear(void) { return FLAG_Z != 0; }

/* ---- the condition flags a move / tst leaves (N, Z; V and C clear; X kept) */
static inline void flags_of16(uint16_t v) { FLAG_N = NFLAG_16(v); FLAG_Z = v; FLAG_V = VFLAG_CLEAR; FLAG_C = CFLAG_CLEAR; }
static inline void flags_of32(uint32_t v) { FLAG_N = NFLAG_32(v); FLAG_Z = v; FLAG_V = VFLAG_CLEAR; FLAG_C = CFLAG_CLEAR; }

/* ---- time */
void begin(uint32_t routine);        /* the routine starts here */
void ran_to(uint32_t b);             /* it ran on up to (not including) b, no branch taken */
void branched(uint32_t a, uint32_t b);   /* it ran up to the branch at a, taken, to b */
int  done(void);                     /* the routine done (up to its rts): its time used; 1 */
int  jump_to(uint32_t addr);         /* ... it goes on in the game's code at addr (a jmp): 2 */
void jumped_to(uint32_t a);          /* it went on at a (after a jmp counted with ran_to) */
void cyc_back(int n);                /* (time counted twice: an rts the caller counts) */
uint32_t pc_now(void);               /* where the time counted has got to */

/* the game's routine at addr (not in C yet), called by the jsr / bsr at jsr_at */
void call(uint32_t addr, uint32_t jsr_at);
/* the rest of a routine not in C yet: the game's code from addr (the time up to there
 * counted) until it reaches one of stops (tools/recomp.py STOPS); the address reached,
 * where the time counted goes on from */
uint32_t run_part(uint32_t addr, const uint32_t *stops, int n);

/* ---- the port's own changes to the game (gamemods.c: Insane mode...) at a place in a
 * routine now in C: as the recompiler's hook there (tools/recomp.py HOOKS) would; only
 * hooks that go on in the routine (0) are expected at these places */
int  hook_at(uint32_t pc, uint32_t ret);  /* a port hook within a routine: 2 when it ran it */
static inline void hook(uint32_t pc) { if (game_hooks) (void)game_hook(pc); }

/* ---- checks: registers (bits 0-7 d0-d7, 8-14 a0-a6) and flags a part written in C
 * does not leave as the game's code did: with LOGIC_SCRAMBLE=1 they are filled with
 * junk, so a check over every scene shows nothing reads them afterwards */
void scramble(uint32_t regs);
#define SCRAMBLE_FLAGS 0x10000u

/* ---- each function starts with: if (!start()) return 0; */
int start_named(const char *name);
void count_named(const char *name);     /* (LOGICSTATS: a part's calls, by name) */
#define start() start_named(__func__)

#endif
