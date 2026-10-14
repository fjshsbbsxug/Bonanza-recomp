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
    cost += 20;                         /* its return (rts 16, rte 20) */
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
