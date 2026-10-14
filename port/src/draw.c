/* The game's drawing routines, written in C.
 *
 * Each one does what the game's own routine does, straight to the video stores: the
 * same tiles, maps and scroll in the same places, the same work memory and registers
 * left behind, and the same time taken (the game's clock is the 68000's cycle count, so
 * a routine finishing sooner would move every frame's work). The recompiler calls them
 * where the routines start (tools/recomp.py DRAW); the rts is done there. */
#include "recomp_rt.h"
#include "draw.h"

/* movem.l of n registers to / from the stack, as the routines save and restore them */
static void push_regs(const int *regs, int n)
{
    uint32_t a = REG_A[7] - 4u * n;
    for (int i = 0; i < n; i++)
        WR32(a + 4u * i, REG_DA[regs[i]]);
    USE_CYCLES(8 + (n << CYC_MOVEM_L));
}

static void pop_cost(int n) { USE_CYCLES(12 + (n << CYC_MOVEM_L)); }

static int dbra_cost(int last) { return 12 + (last ? CYC_DBCC_F_EXP : CYC_DBCC_F_NOEXP); }

/* sub_01146: tiles packed by rows (a6), d7+1 of them, to where the video data goes.
 * Each tile: 4 bytes of flags, one per byte of the tile (a set flag: the next byte
 * follows, else 0); then every row (long) is XORed with the row above. Work buffer
 * 0xFF17DA. */
void draw_tiles_packed(void)
{
    static const int saved[] = {1, 2, 3, 4, 8 + 1, 8 + 2};   /* d1-d4/a1-a2 */
    push_regs(saved, 6);
    uint32_t src = REG_A[6];
    uint8_t tile[32];
    uint16_t last = 0;
    int xbit = 0;
    int count = (int)(REG_D[7] & 0xFFFF) + 1;
    for (int t = 0; t < count; t++) {
        USE_CYCLES(4 + 4 + 12);                          /* moveq, moveq, lea */
        int k = 0;
        for (int g = 0; g < 4; g++) {
            uint8_t flags = (uint8_t)RD8(src++);
            USE_CYCLES(8 + 4);                           /* move.b (a6)+,d2; moveq */
            for (int bit = 7; bit >= 0; bit--) {
                int set = (flags >> bit) & 1;
                USE_CYCLES(6 + (1 << CYC_SHIFT));        /* lsl.b #1,d2 */
                if (set) {
                    tile[k++] = (uint8_t)RD8(src++);
                    USE_CYCLES(10 + CYC_BCC_NOTAKE_W + 12 + 10);  /* bcc, move.b, bra */
                } else {
                    tile[k++] = 0;
                    USE_CYCLES(10 + 12);                 /* bcc taken, move.b #0 */
                }
                USE_CYCLES(dbra_cost(bit == 0));
                xbit = set;
            }
            USE_CYCLES(dbra_cost(g == 3));
        }
        USE_CYCLES(12 + 4);                              /* lea, moveq */
        uint32_t prev = 0;
        for (int r = 0; r < 8; r++) {
            uint32_t row = ((uint32_t)tile[r * 4] << 24 | (uint32_t)tile[r * 4 + 1] << 16 |
                            (uint32_t)tile[r * 4 + 2] << 8 | tile[r * 4 + 3]) ^ prev;
            tile[r * 4] = (uint8_t)(row >> 24); tile[r * 4 + 1] = (uint8_t)(row >> 16);
            tile[r * 4 + 2] = (uint8_t)(row >> 8); tile[r * 4 + 3] = (uint8_t)row;
            prev = row;
            USE_CYCLES(20 + 12 + dbra_cost(r == 7));     /* eor.l, move.l, dbra */
        }
        USE_CYCLES(12 + 12 + 4);                         /* lea, lea, moveq */
        for (int w = 0; w < 16; w++) {
            last = (uint16_t)(tile[w * 2] << 8 | tile[w * 2 + 1]);
            vdp_write_data(last);
            USE_CYCLES(12 + dbra_cost(w == 15));         /* move.w (a2)+,(a1); dbra */
        }
        USE_CYCLES(dbra_cost(t == count - 1));
    }
    for (int i = 0; i < 32; i++)                         /* the work buffer as left */
        WR8(0xFF17DAu + i, tile[i]);
    REG_A[6] = src;
    REG_D[7] |= 0xFFFF;
    FLAG_N = NFLAG_16(last);
    FLAG_Z = last;
    FLAG_V = VFLAG_CLEAR;
    FLAG_C = CFLAG_CLEAR;
    FLAG_X = xbit ? XFLAG_SET : XFLAG_CLEAR;
    pop_cost(6);
}
