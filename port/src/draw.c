/* The game's drawing routines, written in C.
 *
 * Each one does what the game's own routine does, straight to the video stores: the
 * same tiles, maps and text in the same places, the same work memory and registers
 * left behind, and the same time taken (the game's clock is the 68000's cycle count, so
 * a routine finishing sooner would move every frame's work). The recompiler calls them
 * where the routines start (tools/recomp.py DRAW); the rts is done there.
 *
 * A routine runs in one go, so it is used only when nothing could have stopped the
 * game's routine part way: an interrupt the game lets in (the split screen's line
 * effects change where the video data goes, so a tile copy they cut into is sent partly
 * elsewhere, as on the console). Each routine first works out its time without doing
 * anything (dry); when an interrupt would come within it, it returns 0 and the game's
 * routine runs as translated, to be stopped where the console stops it. */
#include "recomp_rt.h"
#include "draw.h"

uint64_t draw_stat_c, draw_stat_translated;

static int cyc;     /* time of the routine so far */
static int dry;     /* only working out the time */

#define COST(n) (cyc += (n))

/* may the routine, taking this long (its rts too), run in one go? */
static int may_run(int cost)
{
    cost += 16;
    if (cpu_irq_dirty)
        return 0;
    int left = GET_CYCLES();
    if (cost <= left)
        return 1;
    return !vdp_irq_within((cost - left) / MD_CYC68K_LINE + 2, (FLAG_INT_MASK >> 8) & 7);
}

/* runs body dry, then (if it may) for real */
static int run(void (*body)(void))
{
    dry = 1; cyc = 0;
    body();
    if (!may_run(cyc)) {
        draw_stat_translated++;
        return 0;
    }
    draw_stat_c++;
    dry = 0; cyc = 0;
    body();
    USE_CYCLES(cyc);
    return 1;
}

static void put(uint16_t w) { if (!dry) vdp_write_data(w); }

/* move.l to the control port: two command words */
static void ctrl_long(uint32_t v)
{
    if (dry) return;
    vdp_write_ctrl((uint16_t)(v >> 16));
    vdp_write_ctrl((uint16_t)v);
}

/* movem.l of n registers to / from the stack, as the routines save and restore them */
static void push_regs(const int *regs, int n)
{
    if (!dry) {
        uint32_t a = REG_A[7] - 4u * n;
        for (int i = 0; i < n; i++)
            WR32(a + 4u * i, REG_DA[regs[i]]);
    }
    COST(8 + (n << CYC_MOVEM_L));
}

static void pop_cost(int n) { COST(12 + (n << CYC_MOVEM_L)); }

static int dbra_cost(int last) { return 12 + (last ? CYC_DBCC_F_EXP : CYC_DBCC_F_NOEXP); }

static void flags_move(uint32_t v, int bits)
{
    if (dry) return;
    FLAG_N = bits == 32 ? NFLAG_32(v) : NFLAG_16(v);
    FLAG_Z = v;
    FLAG_V = VFLAG_CLEAR;
    FLAG_C = CFLAG_CLEAR;
}

/* ---------------------------------------------------------------- tiles */

/* sub_01146: tiles packed by rows (a6), d7+1 of them, to where the video data goes.
 * Each tile: 4 bytes of flags, one per byte of the tile (a set flag: the next byte
 * follows, else 0); then every row (long) is XORed with the row above. Work buffer
 * 0xFF17DA. */
static void tiles_packed(void)
{
    static const int saved[] = {1, 2, 3, 4, 8 + 1, 8 + 2};   /* d1-d4/a1-a2 */
    push_regs(saved, 6);
    uint32_t src = REG_A[6];
    uint8_t tile[32];
    uint16_t last = 0;
    int xbit = 0;
    int count = (int)(REG_D[7] & 0xFFFF) + 1;
    for (int t = 0; t < count; t++) {
        COST(4 + 4 + 12);                                /* moveq, moveq, lea */
        int k = 0;
        for (int g = 0; g < 4; g++) {
            uint8_t flags = (uint8_t)RD8(src++);
            COST(8 + 4);                                 /* move.b (a6)+,d2; moveq */
            for (int bit = 7; bit >= 0; bit--) {
                int set = (flags >> bit) & 1;
                COST(6 + (1 << CYC_SHIFT));              /* lsl.b #1,d2 */
                if (set) {
                    tile[k++] = (uint8_t)RD8(src++);
                    COST(10 + CYC_BCC_NOTAKE_W + 12 + 10);   /* bcc, move.b, bra */
                } else {
                    tile[k++] = 0;
                    COST(10 + 12);                       /* bcc taken, move.b #0 */
                }
                COST(dbra_cost(bit == 0));
                xbit = set;
            }
            COST(dbra_cost(g == 3));
        }
        COST(12 + 4);                                    /* lea, moveq */
        uint32_t prev = 0;
        for (int r = 0; r < 8; r++) {
            uint32_t row = ((uint32_t)tile[r * 4] << 24 | (uint32_t)tile[r * 4 + 1] << 16 |
                            (uint32_t)tile[r * 4 + 2] << 8 | tile[r * 4 + 3]) ^ prev;
            tile[r * 4] = (uint8_t)(row >> 24); tile[r * 4 + 1] = (uint8_t)(row >> 16);
            tile[r * 4 + 2] = (uint8_t)(row >> 8); tile[r * 4 + 3] = (uint8_t)row;
            prev = row;
            COST(20 + 12 + dbra_cost(r == 7));           /* eor.l, move.l, dbra */
        }
        COST(12 + 12 + 4);                               /* lea, lea, moveq */
        for (int w = 0; w < 16; w++) {
            last = (uint16_t)(tile[w * 2] << 8 | tile[w * 2 + 1]);
            put(last);
            COST(12 + dbra_cost(w == 15));               /* move.w (a2)+,(a1); dbra */
        }
        COST(dbra_cost(t == count - 1));
    }
    pop_cost(6);
    if (dry) return;
    for (int i = 0; i < 32; i++)                         /* the work buffer as left */
        WR8(0xFF17DAu + i, tile[i]);
    REG_A[6] = src;
    REG_D[7] |= 0xFFFF;
    flags_move(last, 16);
    FLAG_X = xbit ? XFLAG_SET : XFLAG_CLEAR;
}
int draw_tiles_packed(void) { return run(tiles_packed); }

/* sub_0119C: d7+1 tiles (a6) as they are, to where the video data goes */
static void tiles_raw(void)
{
    uint32_t src = REG_A[6], v = 0;
    int count = (int)(REG_D[7] & 0xFFFF) + 1;
    COST(12);                                            /* lea */
    for (int t = 0; t < count; t++) {
        for (int i = 0; i < 8; i++) {
            if (!dry) {
                v = RD32(src);
                put((uint16_t)(v >> 16));
                put((uint16_t)v);
            }
            src += 4;
            COST(20);                                    /* move.l (a6)+,(a1) */
        }
        COST(dbra_cost(t == count - 1));
    }
    if (dry) return;
    REG_A[1] = 0xC00000u;
    REG_A[6] = src;
    REG_D[7] |= 0xFFFF;
    flags_move(v, 32);
}
int draw_tiles_raw(void) { return run(tiles_raw); }

/* ---------------------------------------------------------------- text */

/* the characters of a line of text, each (base | character) + offset, to where the
 * video data goes; flags as the routines leave them (X from the last addi, the rest
 * from moving the base back into d6) */
static void text_chars(uint32_t *src, uint16_t base, int count, uint16_t offset)
{
    uint32_t sum = 0;
    for (int i = 0; i < count; i++) {
        uint16_t c = (uint16_t)(base | (uint8_t)RD8((*src)++));
        sum = (uint32_t)c + offset;
        put((uint16_t)sum);
        COST(8 + 8 + 8 + 4 + dbra_cost(i == count - 1));  /* or.b, addi.w, move.w, move.w, dbra */
    }
    if (dry) return;
    flags_move(base, 16);
    FLAG_X = sum > 0xFFFF ? XFLAG_SET : XFLAG_CLEAR;
}

/* sub_011B8: a line of text (a6): where it goes (a command), its base tile, the number
 * of characters - 1, the characters; each is drawn as tile (base | character) + 0x500 */
static void text(void)
{
    static const int saved[] = {5, 6, 7, 8 + 5};         /* d5-d7/a5 */
    push_regs(saved, 4);
    uint32_t src = REG_A[6];
    ctrl_long(RD32(src)); src += 4;
    uint16_t base = (uint16_t)RD16(src); src += 2;
    int count = (int)(RD16(src) & 0xFFFF) + 1; src += 2;
    COST(28 + 12 + 8 + 4 + 8);                           /* move.l, lea, move.w, move.w, move.w */
    text_chars(&src, base, count, 0x500);
    pop_cost(4);
    if (!dry) REG_A[6] = src;
}
int draw_text(void) { return run(text); }

/* sub_011F2: the same with the base tile in d7 */
static void text_d7(void)
{
    static const int saved[] = {5, 6, 7, 8 + 5};
    push_regs(saved, 4);
    uint32_t src = REG_A[6];
    ctrl_long(RD32(src)); src += 4;
    int count = (int)(RD16(src) & 0xFFFF) + 1; src += 2;
    COST(28 + 12 + 4 + 8);                               /* move.l, lea, move.w, move.w */
    text_chars(&src, (uint16_t)REG_D[7], count, 0x500);
    pop_cost(4);
    if (!dry) REG_A[6] = src;
}
int draw_text_d7(void) { return run(text_d7); }

/* sub_13318: the first of the 4 lines of text waiting at 0xFF6C82 (pointers; 0: none),
 * as sub_011B8 draws them but + 0xF4; one a frame */
static void text_queue(void)
{
    uint32_t slot = 0xFF6C82u;
    COST(12 + 4);                                        /* lea, moveq */
    for (int i = 0; i < 4; i++, slot += 4) {
        uint32_t p = RD32(slot);
        if (p) {
            COST(12 + 10);                               /* tst.l, bne taken */
            uint32_t src = p;
            ctrl_long(RD32(src)); src += 4;
            uint16_t base = (uint16_t)RD16(src); src += 2;
            uint16_t n = (uint16_t)RD16(src); src += 2;
            COST(12 + 28 + 12 + 8 + 4 + 8);              /* movea, move.l, lea, move.w x3 */
            text_chars(&src, base, n + 1, 0xF4);
            COST(20);                                    /* clr.l (a0) */
            if (dry) return;
            REG_A[0] = slot;
            REG_D[0] = (REG_D[0] & 0xFFFF0000u) | (uint16_t)(3 - i);
            REG_A[6] = src;
            REG_A[5] = 0xC00000u;
            REG_D[7] = (REG_D[7] & 0xFFFF0000u) | base;
            REG_D[6] = (REG_D[6] & 0xFFFF0000u) | base;
            REG_D[5] |= 0xFFFF;
            WR32(slot, 0);
            FLAG_N = NFLAG_CLEAR; FLAG_Z = ZFLAG_SET; FLAG_V = VFLAG_CLEAR; FLAG_C = CFLAG_CLEAR;
            return;
        }
        COST(12 + 10 + CYC_BCC_NOTAKE_W + 8 + dbra_cost(i == 3));
    }
    if (dry) return;
    REG_A[0] = slot;
    REG_D[0] |= 0xFFFF;
    FLAG_N = NFLAG_CLEAR; FLAG_Z = ZFLAG_SET; FLAG_V = VFLAG_CLEAR; FLAG_C = CFLAG_CLEAR;
}
int draw_text_queue(void) { return run(text_queue); }
