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
#include "sndmix.h"

uint64_t draw_stat_c, draw_stat_translated;

static int cyc;     /* time of the routine so far */
static int dry;     /* only working out the time */

#define COST(n) (cyc += (n))

/* each instruction's time where it does not depend on the data (port/gen/m68k_costs.c) */
extern const unsigned char m68k_cost[0x40000];

/* the time of the instructions from a up to b, run one after the other */
static int span(uint32_t a, uint32_t b)
{
    int n = 0;
    for (uint32_t i = a >> 1; i < b >> 1; i++)
        n += m68k_cost[i];
    return n;
}

static int stops_z80;   /* the routine holds the Z80 (its bus): it must not run past a line */

/* may the routine, taking this long (its rts too), run in one go? */
static int may_run(int cost)
{
    cost += 20;                         /* its return (rts 16, rte 20) */
    if (cpu_irq_dirty)
        return 0;
    int left = GET_CYCLES();
    if (cost <= left)
        return 1;
    /* while the Z80 runs (no recorded sounds) it would run in the lines the game's
     * routine keeps it stopped */
    if (stops_z80 && !snd_ready())
        return 0;
    return !vdp_irq_within((cost - left) / MD_CYC68K_LINE + 2, (FLAG_INT_MASK >> 8) & 7);
}

/* runs body dry, then (if it may) for real */
static int run(void (*body)(void))
{
    dry = 1; cyc = 0; stops_z80 = 0;
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

/* sub_13318 / sub_1376A: the first of the 4 lines of text waiting at 0xFF6C82 / 0xFF6C92
 * (pointers; 0: none), as sub_011B8 draws them but + 0xF4 / + 0x174; one a frame */
static uint32_t queue_at;
static uint16_t queue_offset;

static void text_queue(void)
{
    uint32_t slot = queue_at;
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
            text_chars(&src, base, n + 1, queue_offset);
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
int draw_text_queue(void) { queue_at = 0xFF6C82u; queue_offset = 0xF4; return run(text_queue); }
int draw_text_queue2(void) { queue_at = 0xFF6C92u; queue_offset = 0x174; return run(text_queue); }

/* ---------------------------------------------------------------- registers */

/* sub_0070A: the video registers the game changed (bits of 0xFFC0A0; their values at
 * 0xFFC07A, as the command words) sent to the chip */
static void registers(void)
{
    uint32_t mask = RD32(0xFFC0A0u);
    COST(12 + 12 + 20);                                  /* lea, lea, move.l */
    if (!mask) {
        COST(10);                                        /* beq taken */
    } else {
        COST(10 + CYC_BCC_NOTAKE_W + 8);                 /* beq, move.w #$12,d1 */
        uint32_t d0 = mask;
        int x = 0;
        for (int r = 0; r < 19; r++) {
            x = d0 & 1;
            d0 >>= 1;
            COST(8 + (1 << CYC_SHIFT));                  /* lsr.l #1,d0 */
            if (x) {
                if (!dry) vdp_write_ctrl((uint16_t)RD16(0xFFC07Au + 2u * r));
                COST(10 + CYC_BCC_NOTAKE_W + 12);        /* bcc, move.w (a0),(a1) */
            } else {
                COST(10);
            }
            COST(4 + dbra_cost(r == 18));                /* addq.w #2,a0; dbra */
        }
        if (!dry) {
            REG_D[0] = d0;
            REG_D[1] |= 0xFFFF;
            FLAG_X = x ? XFLAG_SET : XFLAG_CLEAR;
        }
    }
    COST(28);                                            /* clr.l */
    if (dry) return;
    if (!mask) REG_D[0] = 0;
    REG_A[0] = mask ? 0xFFC0A0u : 0xFFC07Au;
    REG_A[1] = 0xC00004u;
    WR32(0xFFC0A0u, 0);
    FLAG_N = NFLAG_CLEAR; FLAG_Z = ZFLAG_SET; FLAG_V = VFLAG_CLEAR; FLAG_C = CFLAG_CLEAR;
}
int draw_registers(void) { return run(registers); }

/* ---------------------------------------------------------------- the split screen's bands
 *
 * The game's line effects (interrupt routines at sub_00C1E..sub_010E8, one after the
 * other through the RAM vector 0xFFC0B2): each sets the vertical scroll of both planes
 * (VSRAM 0/1) and the horizontal scroll (the first entry of the table at 0xB000) for the
 * band of lines that follows, how many lines on the next one comes (register 10), and
 * which one that is. Top to bottom: player 2's view (or the 2P panel), the HUD between
 * them, player 1's view, the bottom border. The rte is done by the caller. */

static void at_vscroll(void) { if (!dry) vdp_command(0x05, 0x0000); COST(28); }   /* move.l #$40000010 */
static void at_hscroll(void) { if (!dry) vdp_command(0x01, 0xB000); COST(28); }   /* move.l #$70000002 */
static void word(uint16_t v) { put(v); COST(20); }                                /* move.w #v */
static void word_ram(uint32_t a) { put((uint16_t)RD16(a)); COST(28); }            /* move.w (a).l */
static void lines_to_next(uint8_t n) { if (!dry) vdp_set_reg(10, n); COST(20); }
static void next_band(uint32_t stub) { if (!dry) WR32(0xFFC0B2u, stub); COST(28); }
static void views_split(void) { if (!dry) WR16(0xFF17B6u, 1); COST(20); }      /* move.w #1,$ff17b6 */

/* tst.w / cmpi.w #1 / cmpi.w #2 of a mode, each followed by a beq: 0-3 (3: none of them) */
static int mode_of(uint32_t a)
{
    uint16_t m = (uint16_t)RD16(a);
    COST(16);
    if (m == 0) { COST(10); return 0; }
    COST(10 + CYC_BCC_NOTAKE_W + 20);
    if (m == 1) { COST(10); return 1; }
    COST(10 + CYC_BCC_NOTAKE_W + 20);
    if (m == 2) { COST(10); return 2; }
    COST(10 + CYC_BCC_NOTAKE_W);
    return 3;
}

static void band_00C1E(void) { lines_to_next(0x04); next_band(0x00C32); }

/* the top border; then player 2's view as the 2P panel mode (0xFF6C6C) has it */
static void band_00C32(void)
{
    at_vscroll(); word(0x00F3); word(0x01CF);
    at_hscroll(); word(0x0000); word(0x00FF);
    switch (mode_of(0xFF6C6Cu)) {
    case 0:  lines_to_next(0x4F); next_band(0x00CD8); break;
    case 1:  lines_to_next(0x17); next_band(0x00D2C); break;
    case 2:  lines_to_next(0x17); next_band(0x00DEC); break;
    default: lines_to_next(0x17); next_band(0x00E34); break;
    }
}

/* player 2's view, following his camera */
static void band_00CD8(void)
{
    at_hscroll(); word_ram(0xFF228Au); word(0x0000);
    at_vscroll(); word_ram(0xFF2288u); word(0xFFF9);
    views_split();
    lines_to_next(0x17); next_band(0x00E7C);
}

static void band_00D2C(void)
{
    at_vscroll(); word(0x00EB); word(0xFFF9);
    at_hscroll(); word(0x0000); word(0x0100);
    lines_to_next(0x17); next_band(0x00D74);
}

static void band_00D74(void)
{
    at_vscroll(); word(0x00D3);
    at_hscroll(); word(0x0000);
    lines_to_next(0x1F); next_band(0x00DAC);
}

static void band_00DAC(void)
{
    at_vscroll(); word(0x00B9);
    at_hscroll(); word(0x0000);
    views_split();
    lines_to_next(0x17); next_band(0x00E7C);
}

static void band_00DEC(void)
{
    at_vscroll(); word(0x00EB); word(0x0049);
    at_hscroll(); word(0x0000); word(0x0100);
    lines_to_next(0x17); next_band(0x00D74);
}

static void band_00E34(void)
{
    at_vscroll(); word(0x00EB); word(0x0098);
    at_hscroll(); word(0x0000); word(0x0100);
    lines_to_next(0x17); next_band(0x00D74);
}

/* the HUD */
static void band_00E7C(void)
{
    at_vscroll(); word(0x009B); word(0x0179);
    at_hscroll(); word(0x0000); word(0x0000);
    lines_to_next(0x17); next_band(0x00EC4);
}

/* below the HUD: player 1's view as the mode (0xFF6C6A) has it */
static void band_00EC4(void)
{
    at_vscroll(); word(0x0084);
    at_hscroll(); word(0x0000);
    switch (mode_of(0xFF6C6Au)) {
    case 0:  lines_to_next(0x4F); next_band(0x00F5A); break;
    case 1:  lines_to_next(0x17); next_band(0x00FA0); break;
    case 2:  lines_to_next(0x17); next_band(0x01058); break;
    default: lines_to_next(0x17); next_band(0x010A0); break;
    }
}

/* player 1's view, following his camera */
static void band_00F5A(void)
{
    at_vscroll(); word_ram(0xFF1848u);
    at_hscroll(); word_ram(0xFF184Au);
    if (!dry) WR32(0xFF1848u, RD32(0xFF1844u));           /* move.l $ff1844,$ff1848 */
    COST(36);
    lines_to_next(0x0E); next_band(0x010E8);
}

static void band_00FA0(void)
{
    at_vscroll(); word(0x006B); word(0xFF78);
    at_hscroll(); word(0x0000); word(0x0100);
    lines_to_next(0x17); next_band(0x00FE8);
}

static void band_00FE8(void)
{
    at_vscroll(); word(0x0053);
    at_hscroll(); word(0x0000);
    lines_to_next(0x1F); next_band(0x01020);
}

static void band_01020(void)
{
    at_vscroll(); word(0x0039);
    at_hscroll(); word(0x0000);
    lines_to_next(0x0E); next_band(0x010E8);
}

static void band_01058(void)
{
    at_vscroll(); word(0x006B); word(0xFFC9);
    at_hscroll(); word(0x0000); word(0x0000);
    lines_to_next(0x17); next_band(0x00FE8);
}

static void band_010A0(void)
{
    at_vscroll(); word(0x006B); word(0x0018);
    at_hscroll(); word(0x0000); word(0x0000);
    lines_to_next(0x17); next_band(0x00FE8);
}

/* the bottom border; the line effects off until the next frame */
static void band_010E8(void)
{
    at_vscroll(); word(0x001D); word(0x0111);
    at_hscroll(); word(0x0000); word(0x00FF);
    lines_to_next(0x00); next_band(0x00C1E);
    uint16_t r0 = (uint16_t)(RD16(0xFFC07Au) & 0x80EF);    /* andi.w #$80ef,$ffc07a: */
    COST(24);
    if (!dry) {
        WR16(0xFFC07Au, r0);
        vdp_write_ctrl(r0);                                /* register 0: line interrupts off */
    }
    COST(28);
    if (!dry && FLAG_S)                                    /* andi.w #$fdff,sr */
        m68ki_set_sr(m68ki_get_sr() & 0xFDFF);
    COST(20);
}

int draw_band_00C1E(void) { return run(band_00C1E); }
int draw_band_00C32(void) { return run(band_00C32); }
int draw_band_00CD8(void) { return run(band_00CD8); }
int draw_band_00D2C(void) { return run(band_00D2C); }
int draw_band_00D74(void) { return run(band_00D74); }
int draw_band_00DAC(void) { return run(band_00DAC); }
int draw_band_00DEC(void) { return run(band_00DEC); }
int draw_band_00E34(void) { return run(band_00E34); }
int draw_band_00E7C(void) { return run(band_00E7C); }
int draw_band_00EC4(void) { return run(band_00EC4); }
int draw_band_00F5A(void) { return run(band_00F5A); }
int draw_band_00FA0(void) { return run(band_00FA0); }
int draw_band_00FE8(void) { return run(band_00FE8); }
int draw_band_01020(void) { return run(band_01020); }
int draw_band_01058(void) { return run(band_01058); }
int draw_band_010A0(void) { return run(band_010A0); }
int draw_band_010E8(void) { return run(band_010E8); }

/* ---------------------------------------------------------------- clearing video memory */

/* sub_2848C: where data goes: video memory at d7 (written) */
static void vram_at(void)
{
    uint16_t a = (uint16_t)REG_D[7];
    uint32_t hi = (uint32_t)((a & 0x3FFF) + 0x4000) << 16;
    uint32_t cmd = hi + ((a & 0xC000u) >> 14);
    COST(4 + 14 + 8 + 2 * (8 + (8 << CYC_SHIFT)) + 14 + (6 + (8 << CYC_SHIFT)) + (6 + (6 << CYC_SHIFT)) + 6 + 20);
    if (dry) return;
    vdp_command(0x01, a);
    REG_D[7] = hi;
    REG_D[6] = cmd;
    flags_move(cmd, 32);
    FLAG_X = XFLAG_CLEAR;
}
int draw_vram_at(void) { return run(vram_at); }

/* sub_006FA: no window plane (registers 17, 18) */
static void no_window(void)
{
    COST(12 + 12 + 12);
    if (dry) return;
    vdp_set_reg(17, 0x00);
    vdp_set_reg(18, 0x00);
    REG_A[0] = 0xC00004u;
    flags_move(0x9200, 16);
}
int draw_no_window(void) { return run(no_window); }

/* a fill of video memory by the chip (the game's DMA fill): register 15 set to 1 first
 * (if inc1), the length (registers 19, 20), the command, the fill value 0, then the
 * game waits while the chip is busy and puts register 1 back; register 15 to 2 after
 * (if inc2). Returns the last status read & 2 (what d7 is left with). */
static uint16_t dma_fill0(int inc1, uint8_t len_lo, uint8_t len_hi, uint8_t code, uint16_t addr, int inc2)
{
    uint16_t status = 0;
    if (inc1) { if (!dry) vdp_set_reg(15, 1); COST(12); }
    uint16_t r1 = (uint16_t)(RD16(0xFFC07Cu) | 0x10);    /* the DMA allowed */
    COST(16 + 8 + 8);                                    /* move.w, ori.b, move.w d7,(a6) */
    COST(20 + 12 + 20 + 20);                             /* length, mode, command, the value */
    if (!dry) {
        vdp_write_ctrl(r1);
        vdp_set_reg(19, len_lo);
        vdp_set_reg(20, len_hi);
        vdp_set_reg(23, 0x80);
        vdp_command(code, addr);
        vdp_write_data(0);
    }
    for (;;) {                                           /* move.w (a6),d7; andi.w #2; bne.s */
        if (!dry) status = vdp_read_ctrl() & 2;
        COST(8 + 8);
        if (!status) { COST(10 + CYC_BCC_NOTAKE_B); break; }
        COST(10);
    }
    if (!dry) vdp_write_ctrl((uint16_t)RD16(0xFFC07Cu));
    COST(20);
    if (inc2) { if (!dry) vdp_set_reg(15, 2); COST(12); }
    return status;
}

/* sub_00628: video memory 0x0000-0xFFFF? no: 0xFF bytes from 0 cleared */
static void clear_vram_00628(void)
{
    COST(12);                                            /* lea */
    uint16_t s = dma_fill0(1, 0xFF, 0x00, 0x21, 0x0000, 1);
    if (dry) return;
    REG_A[6] = 0xC00004u;
    REG_D[7] = (REG_D[7] & 0xFFFF0000u) | s;
    flags_move(0x8F02, 16);
}
int draw_clear_00628(void) { return run(clear_vram_00628); }

/* sub_0052E: the planes (0xC000, 0xE000: 0x1FFF bytes each) and the scroll table
 * (0xB000) cleared in video memory, the game's copies of the scroll (0xFF0280,
 * 0xFF0680) and the vertical scroll cleared */
static void clear_planes(void)
{
    static const int saved[] = {6, 7};
    push_regs(saved, 2);
    COST(12);
    dma_fill0(1, 0xFF, 0x1F, 0x21, 0xC000, 1);
    COST(12);
    dma_fill0(1, 0xFF, 0x1F, 0x21, 0xE000, 1);
    COST(12);
    dma_fill0(0, 0xFF, 0x01, 0x21, 0xB000, 0);
    COST(12 + 8 + 4);                                    /* lea, move.w, moveq */
    for (int i = 0; i < 0x200; i++) {
        if (!dry) WR16(0xFF0280u + 2u * i, 0);
        COST(8 + dbra_cost(i == 0x1FF));
    }
    COST(12 + 8 + 4);
    for (int i = 0; i < 0x50; i++) {
        if (!dry) WR16(0xFF0680u + 2u * i, 0);
        COST(8 + dbra_cost(i == 0x4F));
    }
    COST(28 + 28);                                       /* the vertical scroll: 0, 0 */
    pop_cost(2);
    if (dry) return;
    vdp_command(0x05, 0x0000);
    vdp_write_data(0);
    vdp_write_data(0);
    REG_A[6] = 0xC00004u;
    REG_A[0] = 0xFF0720u;
    REG_D[0] |= 0xFFFF;
    REG_D[1] = 0;
    flags_move(0, 32);
}
int draw_clear_planes(void) { return run(clear_planes); }

/* sub_00484: the video registers from the table at 0x4FC (also kept at 0xFFC07A), the
 * palette cleared, all of video memory cleared */
static void video_init(void)
{
    COST(12 + 12 + 8 + 12 + 4);                          /* lea x4, moveq */
    for (int i = 0; i < 19; i++) {
        uint16_t w = (uint16_t)RD16(0x4FCu + 2u * i);
        if (!dry) { vdp_write_ctrl(w); WR16(0xFFC07Au + 2u * i, w); }
        COST(12 + 12 + dbra_cost(i == 18));
    }
    COST(4 + 28 + 8);                                    /* moveq, the palette at 0, move.w */
    if (!dry) vdp_command(0x03, 0x0000);
    for (int i = 0; i < 64; i++) {
        if (!dry) vdp_write_data(0);
        COST(8 + dbra_cost(i == 63));
    }
    COST(12);
    uint16_t s = dma_fill0(1, 0xFF, 0xFF, 0x21, 0x0000, 1);
    if (dry) return;
    REG_A[0] = 0xC00004u;
    REG_A[1] = 0xC00000u;
    REG_A[2] = 0x522u;
    REG_A[3] = 0xFFC0A0u;
    REG_A[6] = 0xC00004u;
    REG_D[0] = 0;
    REG_D[7] = s;
    flags_move(0x8F02, 16);
}
int draw_video_init(void) { return run(video_init); }

/* ---------------------------------------------------------------- the planes' new columns
 *
 * As a view scrolls, the game builds the map entries that come into it in RAM, and in
 * the vertical blank has the chip copy them into plane A (DMA): two columns side by side
 * (0x1E entries each, one under the other: register 15 0x80, a plane row apart), or a
 * block of rows. Player 1: from 0xFF1884 / 0xFF1D84 to the address at 0xFFC114, done
 * flag 0xFFC10C; player 2: 0xFF22C8 / 0xFF27C8, 0xFFC118, 0xFFC110. The Z80 is held
 * off the bus while the chip copies. */

static void z80_bus(uint16_t v) { if (!dry) WR16(0xA11100u, v); }

/* a copy from RAM (src) to plane memory at v, n words: the chip told the length, the
 * source and the destination, register 1 with DMA allowed, then put back */
static void dma_to_plane(uint32_t src, uint16_t v, uint8_t len_hi, uint8_t len_lo)
{
    if (dry) return;
    vdp_write_ctrl((uint16_t)(RD16(0xFFC07Cu) | 0x10));
    vdp_set_reg(20, len_hi);
    vdp_set_reg(19, len_lo);
    vdp_set_reg(21, (uint8_t)(src >> 1));
    vdp_set_reg(22, (uint8_t)(src >> 9));
    vdp_set_reg(23, (uint8_t)((src >> 17) & 0x7F));
    vdp_write_ctrl((uint16_t)((v & 0x3FFF) | 0x4000));
    uint16_t second = (uint16_t)((v >> 14) | 0x80);      /* the copy starts with it */
    WR16(0xFF17CAu, second);
    vdp_write_ctrl((uint16_t)RD16(0xFF17CAu));
    vdp_write_ctrl((uint16_t)RD16(0xFFC07Cu));
}

static uint16_t entry_sr;

static void save_sr(void)                                /* move.w sr,-(a7) */
{
    stops_z80 = 1;
    if (!dry) { entry_sr = (uint16_t)m68ki_get_sr(); WR16(REG_A[7] - 2, entry_sr); }
}

static void restore_sr(void)                             /* move.w (a7)+,sr */
{
    if (!dry) m68ki_set_sr(entry_sr);
}

static void column_regs(uint16_t v)                      /* as the routines leave d6, d7 */
{
    REG_A[6] = 0xC00004u;
    REG_D[6] = (uint32_t)((v & 0x3FFF) | 0x4000);
    REG_D[7] = (uint32_t)((v >> 14) | 0x80);
}

static uint32_t col_src1, col_src2, col_at, col_done;
static uint32_t rows_test, rows_flag;
static uint16_t rows_v0, rows_v1;

/* sub_089C0 / sub_08C1A: two columns */
static void columns(void)
{
    COST(span(col_at == 0xFFC114u ? 0x89C0 : 0x8C1A, col_at == 0xFFC114u ? 0x8B06 : 0x8D60));
    save_sr();
    uint16_t v = (uint16_t)RD16(col_at);
    z80_bus(0x100);
    if (!dry) vdp_set_reg(15, 0x80);
    dma_to_plane(col_src1, v, 0x00, 0x1E);
    z80_bus(0x000);
    z80_bus(0x100);
    v += 2;
    dma_to_plane(col_src2, v, 0x00, 0x1E);
    if (!dry) vdp_set_reg(15, 0x02);
    z80_bus(0x000);
    if (dry) return;
    WR32(col_done, 0);
    restore_sr();
    column_regs(v);
}

int draw_columns_p1(void)
{
    col_src1 = 0xFF1884u; col_src2 = 0xFF1D84u; col_at = 0xFFC114u; col_done = 0xFFC10Cu;
    return run(columns);
}

int draw_columns_p2(void)
{
    col_src1 = 0xFF22C8u; col_src2 = 0xFF27C8u; col_at = 0xFFC118u; col_done = 0xFFC110u;
    return run(columns);
}

/* sub_08B50 / sub_08DAA: 0x280 entries (a block of rows) at one of two places */
static void rows(void)
{
    int p1 = col_at == 0xFFC114u;
    uint32_t start = p1 ? 0x8B50 : 0x8DAA, test = p1 ? 0x8BC4 : 0x8E1E, after = p1 ? 0x8BCC : 0x8E26;
    uint32_t end = p1 ? 0x8C18 : 0x8E72;
    COST(span(start, test));
    int second = RD16(rows_test) != 0;
    COST(second ? 10 + CYC_BCC_NOTAKE_W + 8 : 10);       /* beq; move.w #$ca00 / #$db00,d7 */
    COST(span(after, end));
    save_sr();
    uint16_t v = second ? rows_v1 : rows_v0;
    z80_bus(0x100);
    if (!dry) vdp_set_reg(15, 0x02);
    dma_to_plane(col_src1, v, 0x02, 0x80);
    z80_bus(0x000);
    if (dry) return;
    WR16(rows_flag, 0);
    WR32(col_done, 0);
    restore_sr();
    column_regs(v);
}

int draw_rows_p1(void)
{
    col_src1 = 0xFF1884u; col_at = 0xFFC114u; col_done = 0xFFC10Cu;
    rows_test = 0xFF186Au; rows_flag = 0xFF1878u; rows_v0 = 0xC000; rows_v1 = 0xCA00;
    return run(rows);
}

int draw_rows_p2(void)
{
    col_src1 = 0xFF22C8u; col_at = 0xFFC118u; col_done = 0xFFC110u;
    rows_test = 0xFF22AEu; rows_flag = 0xFF22BCu; rows_v0 = 0xD100; rows_v1 = 0xDB00;
    return run(rows);
}

/* ---------------------------------------------------------------- numbers, copies */

/* sub_11EA8: 4 bytes (a6) as 8 hexadecimal digits, each the tile in the table at a5
 * + 0x500, to where the video data goes */
static void hex_digits(void)
{
    uint32_t src = REG_A[6];
    uint16_t hi = 0, lo = 0;
    uint32_t sum = 0;
    COST(span(0x11EA8, 0x11EB2));
    for (int i = 0; i < 4; i++) {
        uint8_t b = (uint8_t)RD8(src++);
        hi = (uint16_t)(RD16(REG_A[5] + ((b & 0xF0) >> 3)) + 0x500);
        sum = (uint32_t)(uint16_t)RD16(REG_A[5] + ((b & 0x0F) << 1)) + 0x500;
        lo = (uint16_t)sum;
        put(hi);
        put(lo);
        COST(span(0x11EB2, 0x11ED6) + dbra_cost(i == 3));
    }
    if (dry) return;
    REG_A[4] = 0xC00000u;
    REG_A[6] = src;
    REG_D[5] = (REG_D[5] & 0xFFFF0000u) | 0xFFFF;
    REG_D[7] = hi;
    REG_D[6] = (REG_D[6] & 0xFFFF0000u) | lo;
    flags_move(lo, 16);
    FLAG_X = sum > 0xFFFF ? XFLAG_SET : XFLAG_CLEAR;
}
int draw_hex_digits(void) { return run(hex_digits); }

/* sub_28200: d5 bytes of video memory copied by the chip from d7 to d6 */
static void vram_copy(void)
{
    stops_z80 = 1;
    uint16_t len = (uint16_t)REG_D[5], from = (uint16_t)REG_D[7], to = (uint16_t)REG_D[6];
    uint16_t status = 0;
    COST(span(0x28200, 0x2827C));
    if (!dry) {
        uint32_t sp = REG_A[7];
        WR16(sp - 2, (uint16_t)m68ki_get_sr());              /* move.w sr,-(a7) */
        WR32(sp - 10, REG_D[4]);                              /* movem.l d4/a6,-(a7) */
        WR32(sp - 6, REG_A[6]);
        WR16(0xA11100u, 0x100);
        vdp_set_reg(15, 0x01);
        vdp_write_ctrl((uint16_t)(RD16(0xFFC07Cu) | 0x10));
        vdp_set_reg(19, (uint8_t)len);
        vdp_set_reg(20, (uint8_t)(len >> 8));
        vdp_set_reg(21, (uint8_t)from);
        vdp_set_reg(22, (uint8_t)(from >> 8));
        vdp_set_reg(23, 0xC0);                                /* a copy within video memory */
        vdp_write_ctrl(to & 0x3FFF);
        uint32_t d6 = (REG_D[6] >> 14) | 0xC0;
        WR16(0xFF17CAu, (uint16_t)d6);
        vdp_write_ctrl((uint16_t)RD16(0xFF17CAu));
        REG_D[6] = d6;
    }
    for (;;) {                                               /* lea; move.w (a6),d7; andi; bne.s */
        if (!dry) status = vdp_read_ctrl() & 2;
        COST(12 + 8 + 8);
        if (!status) { COST(10 + CYC_BCC_NOTAKE_B); break; }
        COST(10);
    }
    COST(span(0x2828A, 0x282A2));
    if (dry) return;
    vdp_write_ctrl((uint16_t)RD16(0xFFC07Cu));
    vdp_set_reg(15, 0x02);
    WR16(0xA11100u, 0);
    REG_D[5] = 0x9400u | (len >> 8);
    REG_D[7] = status;
    m68ki_set_sr(m68ki_get_sr());                            /* move.w (a7)+,sr: as it was */
}
int draw_vram_copy(void) { return run(vram_copy); }

/* the time of a loop run n times: its instructions from a to its dbra at b, and the dbra */
static int loop_cost(uint32_t a, uint32_t b, int n)
{
    return n * span(a, b) + (n - 1) * dbra_cost(0) + dbra_cost(1);
}

/* ---------------------------------------------------------------- the HUD */

/* words from the ROM (or RAM) at *src, each + 0x500, to where the video data goes; the
 * last one is left in d1 (low word) and its addition in X */
static uint16_t tiles_plus_500(uint32_t *src, int n)
{
    uint32_t sum = 0;
    for (int i = 0; i < n; i++) {
        sum = (uint32_t)(uint16_t)RD16(*src) + 0x500;
        *src += 2;
        put((uint16_t)sum);
    }
    if (!dry) FLAG_X = sum > 0xFFFF ? XFLAG_SET : XFLAG_CLEAR;
    return (uint16_t)sum;
}

static void set_low_word(int d, uint16_t v) { REG_D[d] = (REG_D[d] & 0xFFFF0000u) | v; }

/* sub_1050C: the stage's name (4 rows of 10 tiles from the table at 0x10598, by the
 * stage 0xFFC05A) in the HUD */
static void hud_stage_name(void)
{
    uint16_t d0 = (uint16_t)((RD16(0xFFC05Au) - 1) << 2);
    uint32_t src = RD32(0x10598u + (uint32_t)(int16_t)d0);
    uint16_t last = 0;
    static const uint16_t rows[4] = {0xFD96, 0xFE16, 0xFE96, 0xFF16};
    COST(span(0x1050C, 0x1052C));
    for (int r = 0; r < 4; r++) {
        if (r) COST(28 + 8);                             /* the next row's command, move.w #9 */
        if (!dry) vdp_command(0x01, rows[r]);
        last = tiles_plus_500(&src, 10);
        COST(loop_cost(0x1052C, 0x10538, 10));
    }
    if (dry) return;
    set_low_word(0, 0xFFFF);
    set_low_word(1, last);
    REG_A[0] = src;
    flags_move(last, 16);
}
int draw_hud_stage_name(void) { return run(hud_stage_name); }

/* sub_1044A: the HUD's frame (0x180 tiles at 0xFD00), the stage number, the time
 * (0xFF6C4A: set to 3:00 with the minutes by the difficulty), then the stage's name */
static void hud_frame(void)
{
    uint16_t d0 = (uint16_t)(RD16(0xFFC04Eu) << 1);
    COST(span(0x1044A, 0x10482));
    if (!dry) {
        WR32(0xFF6C4Au, 0x30000);
        WR8(0xFF6C4Au, RD8(0x110F4u + (uint32_t)(int16_t)d0));
        vdp_command(0x01, 0xFD00);
    }
    uint32_t src = 0x10DF4;
    tiles_plus_500(&src, 0x180);
    COST(loop_cost(0x10482, 0x1048A, 0x180));
    COST(span(0x1048E, 0x1050C));
    if (!dry) vdp_command(0x01, 0xFEB2);
    src = 0x10D84u + (uint16_t)(RD16(0xFFC05Au) << 2);   /* the stage number: 2 tiles */
    tiles_plus_500(&src, 2);
    uint8_t t[3];
    for (int i = 0; i < 3; i++) t[i] = dry ? 0 : (uint8_t)RD8(0xFF6C4Bu + i);
    if (!dry) {
        vdp_command(0x01, 0xFE8A);                       /* the time: minutes, then seconds */
        vdp_write_data((uint16_t)(t[0] + 0x8520));
        vdp_command(0x01, 0xFE8E);
        vdp_write_data((uint16_t)(t[1] + 0x8520));
        vdp_write_data((uint16_t)(t[2] + 0x8520));
        REG_A[1] = 0xC00000u;
        REG_D[0] = 0;                                    /* andi.l #$ffff: upper word 0 */
    }
    hud_stage_name();
}
int draw_hud_frame(void) { return run(hud_frame); }

/* ---------------------------------------------------------------- the HUD's numbers */

/* abcd: the decimal (BCD) bytes added with X, as the 68000 does it; sets X */
static uint8_t abcd(uint8_t src, uint8_t dst, int *x)
{
    uint32_t res = (src & 0x0F) + (dst & 0x0F) + (uint32_t)*x;
    if (res > 9) res += 6;
    res += (src & 0xF0) + (dst & 0xF0);
    *x = res > 0x99;
    if (*x) res -= 0xA0;
    return (uint8_t)res;
}

typedef struct {
    uint32_t start, bmi, loop, dbra, after, bne1, l_hi1, l_lo1, l_hi2, bne2, bne3, rts_short, rts;
    uint32_t count_at, store_at, done_flag, table, blank_at;
    uint16_t blank;
    uint16_t digit_at[4];   /* each digit's own place (0: they follow each other from blank_at) */
} number_layout;

static const number_layout *num;

#define W(r, v) ((r) = ((r) & 0xFFFF0000u) | (uint16_t)(v))
#define B(r, v) ((r) = ((r) & 0xFFFFFF00u) | (uint8_t)(v))

/* sub_11EDC / sub_11FB8: a player's count (0xFFC054 / 0xFFC056) - 1 in decimal (kept
 * at 0xFF6C26 / 0xFF6C3C), drawn as up to 4 digits without leading zeros after 4 blank
 * tiles: player 1's from the left, player 2's each at its own place */
static void hud_number(void)
{
    const number_layout *L = num;
    uint32_t d6 = 0, d7 = 0, a6 = L->store_at;
    int x = 0;
    int16_t n = (int16_t)(RD16(L->count_at) - 2);
    COST(span(L->start, L->bmi));
    if (n < 0) {
        COST(10);
    } else {
        COST(10 + CYC_BCC_NOTAKE_W);
        COST(loop_cost(L->loop, L->dbra, n + 1));
        for (int i = 0; i <= n; i++) {                    /* neg.w d4 (0: X clear); abcd x2 */
            x = 0;
            B(d7, abcd(1, (uint8_t)d7, &x));
            B(d6, abcd(0, (uint8_t)d6, &x));
        }
    }
    W(d6, (uint16_t)d6 << 8);
    W(d7, (uint16_t)d7 + (uint16_t)d6);
    COST(span(L->after, L->bne1));
    if (!dry) {
        WR16(L->store_at, (uint16_t)d7);
        vdp_command(0x01, L->blank_at);
        for (int i = 0; i < 4; i++) vdp_write_data(L->blank);
        if (!L->digit_at[0]) vdp_command(0x01, L->blank_at);
    }
    uint32_t sum = 0;
    int k;                                                /* the next digit's place */
#define DIGIT(r, place) do { sum = (uint32_t)(uint16_t)RD16(L->table + (uint32_t)(int16_t)(uint16_t)(r)) + 0x500; \
        W(r, sum); if (!dry) { if (L->digit_at[place]) vdp_command(0x01, L->digit_at[place]); \
        vdp_write_data((uint16_t)sum); } } while (0)
#define NEXT_BYTE() do { B(d7, RD8(a6)); a6++; B(d6, d7); W(d7, ((uint16_t)d7 & 0xF0) >> 3); } while (0)
    NEXT_BYTE();
    if ((uint16_t)d7) {                                   /* hundreds and thousands */
        COST(10 + span(L->l_hi1, L->l_lo1) + span(L->l_lo1, L->l_hi2) + span(L->l_hi2, L->rts));
        DIGIT(d7, 0);
        W(d6, ((uint16_t)d6 & 0x0F) << 1);
        k = 1;
        goto lo1;
    }
    COST(10 + CYC_BCC_NOTAKE_W);
    W(d6, ((uint16_t)d6 & 0x0F) << 1);
    if ((uint16_t)d6) {
        COST(span(L->bne1 + 4, L->bne2) + 10 + span(L->l_lo1, L->l_hi2) + span(L->l_hi2, L->rts));
        k = 1;
        goto lo1;
    }
    COST(span(L->bne1 + 4, L->bne2) + 10 + CYC_BCC_NOTAKE_W);
    NEXT_BYTE();
    if ((uint16_t)d7) {
        COST(span(L->bne2 + 4, L->bne3) + 10 + span(L->l_hi2, L->rts));
        goto hi2;
    }
    COST(span(L->bne2 + 4, L->bne3) + 10 + CYC_BCC_NOTAKE_W + span(L->bne3 + 4, L->rts_short));
    W(d6, ((uint16_t)d6 & 0x0F) << 1);
    DIGIT(d6, 3);
    goto done;
lo1:
    (void)k;
    DIGIT(d6, 1);
    NEXT_BYTE();
hi2:
    DIGIT(d7, 2);
    W(d6, ((uint16_t)d6 & 0x0F) << 1);
    DIGIT(d6, 3);
done:
#undef DIGIT
#undef NEXT_BYTE
    if (dry) return;
    WR16(L->done_flag, 0);
    REG_D[4] = 0;
    REG_D[5] = 1;
    REG_D[6] = d6;
    REG_D[7] = d7;
    W(REG_D[0], L->blank);
    REG_A[4] = 0xC00000u;
    REG_A[5] = L->table;
    REG_A[6] = a6;
    FLAG_N = NFLAG_CLEAR; FLAG_Z = ZFLAG_SET; FLAG_V = VFLAG_CLEAR; FLAG_C = CFLAG_CLEAR;
    FLAG_X = sum > 0xFFFF ? XFLAG_SET : XFLAG_CLEAR;
}

static const number_layout number_p1 = {
    0x11EDC, 0x11EEC, 0x11EF0, 0x11EF6, 0x11EFA, 0x11F3E, 0x11F72, 0x11F82, 0x11F96,
    0x11F48, 0x11F56, 0x11F70, 0x11FB6,
    0xFFC054, 0xFF6C26, 0xFF6C30, 0x120BC, 0xFF0A, 0x8501, {0, 0, 0, 0}};
static const number_layout number_p2 = {
    0x11FB8, 0x11FC8, 0x11FCC, 0x11FD2, 0x11FD6, 0x12010, 0x1204E, 0x12068, 0x12086,
    0x1201A, 0x12028, 0x1204C, 0x120BA,
    0xFFC056, 0xFF6C3C, 0xFF6C46, 0x120D0, 0xFDAE, 0x8502, {0xFDAE, 0xFDB0, 0xFDB2, 0xFDB4}};

int draw_hud_number_p1(void) { num = &number_p1; return run(hud_number); }
int draw_hud_number_p2(void) { num = &number_p2; return run(hud_number); }
