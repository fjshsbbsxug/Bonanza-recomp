/* The palette's fades: colors brought up from black a level at a time, darkened, or
 * brightened towards white, a step a frame, and the palette copied. Colors are the
 * console's: 3 bits each of blue, green and red (0x0BGR, even values). The palette
 * kept in work memory (0xFF0200) goes to the chip when PALETTE_CHANGED is set. */
#include "rt.h"

#define PALETTE          0xFF0200
#define PALETTE_CHANGED  0xFF17C4
#define FADE             0xFF17AE   /* frames to the next step, the frames per step, the level */

/* (the flags a subq.w #1 leaves, from the value before it) */
static void subq1_flags(uint16_t before)
{
    uint res = (uint)before - 1;
    FLAG_N = NFLAG_16(res);
    FLAG_X = FLAG_C = CFLAG_16(res);
    FLAG_V = VFLAG_SUB_16(1, before, res);
    FLAG_Z = MASK_OUT_ABOVE_16(res);
}

/* (the loop's dbra on d7: 1 while it goes round) */
static int next_color(uint32_t dbra, uint32_t loop)
{
    uint16_t c = lo(REG_D[7]);
    set_lo(7, (uint16_t)(c - 1));
    if (!c) { ran_to(dbra + 4); return 0; }
    branched(dbra, loop);
    return 1;
}

/* sub_02B62: d7 + 1 colors at a6 brought up a step from black towards those at a5: each
 * part (blue, green, red) of the target at least level d4 goes up by one */
static void fade_up(int32_t sp)
{
    uint32_t d1 = REG_D[1];
    stack_put(sp - 4, d1);                            /* (movem.l d1, -(a7)) */
    do {
        uint16_t level = (uint16_t)(lo(REG_D[4]) << 1 | lo(REG_D[4]) >> 15);
        uint16_t target = mem16(REG_A[5]);
        REG_A[5] += 2;
        set_lo(5, target);
        uint16_t c = ram16(REG_A[6]);
        static const uint16_t part[3] = {0x000E, 0x00E0, 0x0E00}, one[3] = {2, 0x20, 0x200};
        static const uint32_t bcs[3] = {0x02B76, 0x02B86, 0x02B98}, past[3] = {0x02B7C, 0x02B8E, 0x02BA0};
        for (int k = 0; k < 3; k++) {
            if (k) level = (uint16_t)(level << 4 | level >> 12);
            set_lo(3, target & part[k]);
            if (lo(REG_D[3]) < level) branched(bcs[k], past[k]);
            else { c = (uint16_t)(c + one[k]); ran_to(past[k]); }
        }
        set_lo(1, level);
        set_lo(6, c);
        set16(REG_A[6], c);
        REG_A[6] += 2;
    } while (next_color(0x02BA2, 0x02B66));
    set16(PALETTE_CHANGED, 1);
    REG_D[1] = d1;
    ran_to(0x02BB2);
}

/* sub_02BD4: d7 + 1 colors at a6 darkened a step (each part down by one, black kept) */
static void fade_down(void)
{
    do {
        uint16_t c = ram16(REG_A[6]);
        set_lo(6, c);
        if (!c) branched(0x02BD6, 0x02BF6);
        else {
            c = (uint16_t)((c | 0x1110) - 0x222);
            uint16_t carry = (uint16_t)((c & 0x1110) >> 1);
            uint16_t keep = carry;
            carry >>= 1; keep = (uint16_t)(keep + carry);
            carry >>= 1; keep = (uint16_t)(keep + carry);
            c &= keep;
            set_lo(5, carry);
            set_lo(4, keep);
            set_lo(6, c);
            ran_to(0x02BF6);
        }
        set16(REG_A[6], lo(REG_D[6]));
        REG_A[6] += 2;
    } while (next_color(0x02BF8, 0x02BD4));
    set16(PALETTE_CHANGED, 1);
    ran_to(0x02C04);
}

/* sub_02C06: d7 + 1 colors at a6 brightened a step (each part up by one, white kept) */
static void fade_white(void)
{
    do {
        uint16_t c = ram16(REG_A[6]);
        if (c == 0xEEE) branched(0x02C0C, 0x02C42);
        else {
            static const uint16_t one[3] = {2, 0x20, 0x200}, over[3] = {0x10, 0x100, 0x1000};
            static const uint32_t bne[3] = {0x02C18, 0x02C28, 0x02C3A}, past[3] = {0x02C1E, 0x02C30, 0x02C42};
            for (int k = 0; k < 3; k++) {
                set_lo(3, (uint16_t)(c + one[k]));
                if (lo(REG_D[3]) & over[k]) branched(bne[k], past[k]);
                else { c = (uint16_t)(c + one[k]); ran_to(past[k]); }
            }
        }
        set_lo(6, c);
        set16(REG_A[6], c);
        REG_A[6] += 2;
    } while (next_color(0x02C44, 0x02C06));
    set16(PALETTE_CHANGED, 1);
    ran_to(0x02C50);
}

int logic_fade_up(void)    { if (!start()) return 0; begin(0x02B62); fade_up(0);   return done(); }
int logic_fade_down(void)  { if (!start()) return 0; begin(0x02BD4); fade_down();  return done(); }
int logic_fade_white(void) { if (!start()) return 0; begin(0x02C06); fade_white(); return done(); }

/* sub_02C52: d1 colors from a0 into the palette from color d0 */
int logic_palette_copy(void)
{
    if (!start()) return 0;
    begin(0x02C52);
    stack_put(-16, REG_D[0]); stack_put(-12, REG_D[1]);   /* (movem.l d0-d1/a0-a1, -(a7)) */
    stack_put(-8, REG_A[0]); stack_put(-4, REG_A[1]);
    uint32_t a1 = PALETTE + (uint32_t)(int32_t)(int16_t)(uint16_t)(lo(REG_D[0]) << 1), a0 = REG_A[0];
    ran_to(0x02C62);
    for (uint16_t n = (uint16_t)(lo(REG_D[1]) - 1);; n--) {
        set16(a1, mem16(a0));
        a0 += 2; a1 += 2;
        if (!n) { ran_to(0x02C68); break; }
        branched(0x02C64, 0x02C62);
    }
    set16(PALETTE_CHANGED, 1);
    ran_to(0x02C74);
    return done();
}

/* sub_02AEA / sub_02BB4: the screen's fade (FADE), every so many frames a step: up from
 * black (from the colors at a5 into a6, by level) or darker (d7 + 1 colors at a6); the
 * flags from the last count taken down (zero: the fade done), the registers kept */
static int fade_screen(int down)
{
    uint32_t r = down ? 0x02BB4 : 0x02AEA, b = down ? 0xCA : 0;
    begin(r);
    uint32_t d[8], a[7];
    for (int k = 0; k < 8; k++) { d[k] = REG_D[k]; stack_put(-60 + 4 * k, d[k]); }   /* (movem) */
    for (int k = 0; k < 7; k++) { a[k] = REG_A[k]; stack_put(-28 + 4 * k, a[k]); }
    REG_A[down ? 5 : 4] = FADE;
    uint16_t t = (uint16_t)(ram16(FADE) - 1);
    subq1_flags(ram16(FADE));
    set16(FADE, t);
    if (t) branched(0x02AF6 + b, down ? 0x02BCE : 0x02B08);
    else {
        set16(FADE, ram16(FADE + 2));
        if (!down) set_lo(4, ram16(FADE + 4));
        uint32_t bsr = down ? 0x02BC8 : 0x02B02;
        ran_to(bsr + 2);
        stack_put(-64, bsr + 2);                      /* (the bsr's return) */
        jumped_to(down ? 0x02BD4 : 0x02B62);
        if (down) fade_down(); else fade_up(-64);
        ran_to(down ? 0x02C06 : 0x02BB4);             /* (its rts) */
        jumped_to(bsr + 2);
        subq1_flags(ram16(FADE + 4));                 /* (the caller tests them: the fade done) */
        set16(FADE + 4, (uint16_t)(ram16(FADE + 4) - 1));
        ran_to(down ? 0x02BCE : 0x02B08);
    }
    for (int k = 0; k < 8; k++) REG_D[k] = d[k];
    for (int k = 0; k < 7; k++) REG_A[k] = a[k];
    ran_to(down ? 0x02BD2 : 0x02B0C);
    return done();
}
int logic_fade_screen_up(void)   { if (!start()) return 0; return fade_screen(0); }
int logic_fade_screen_down(void) { if (!start()) return 0; return fade_screen(1); }

/* sub_014B6 / sub_014F0 / sub_01520 (jobs, a0 the job: +2 frames to its next step, +4
 * the frames per step, +6 the colors less one, +7 the steps left, +8 the colors to come
 * up to, +$C the palette's colors): a step of a fade up from black, down, or to white;
 * the last step ends the job */
static void fade_job(int kind)
{
    static const uint32_t at[3] = {0x014B6, 0x014F0, 0x01520};
    uint32_t r = at[kind], o = kind ? 0xA : 0;        /* (no a5 to load: its code shorter) */
    uint32_t j = REG_A[0];
    begin(r);
    uint16_t t = (uint16_t)(ram16(j + 2) - 1);
    set16(j + 2, t);
    uint32_t end = kind ? r + 0x2E : 0x014EE;
    if (t) { branched(r + 4, end); return; }
    set16(j + 2, ram16(j + 4));
    if (!kind) REG_A[5] = ram32(j + 8);
    REG_A[6] = ram32(j + 0xC);
    REG_D[7] = ram8(j + 6);
    if (!kind) REG_D[4] = ram8(j + 7);
    uint32_t jsr = r + 0x22 - o;
    ran_to(jsr + 6);
    stack_put(-4, jsr + 6);                           /* (the jsr's return) */
    jumped_to(kind == 0 ? 0x02B62 : kind == 1 ? 0x02BD4 : 0x02C06);
    if (kind == 0) { fade_up(-4); ran_to(0x02BB4); }
    else if (kind == 1) { fade_down(); ran_to(0x02C06); }
    else { fade_white(); ran_to(0x02C52); }
    jumped_to(jsr + 6);
    REG_A[0] = j;
    uint8_t left = (uint8_t)(ram8(j + 7) - 1);
    set8(j + 7, left);
    if (left) { branched(jsr + 0xA, end); return; }
    set8(j, 0);
    set16(0xFF55D4, (uint16_t)(ram16(0xFF55D4) - 1));
    ran_to(end);
}
int logic_fade_job_up(void)    { if (!start()) return 0; fade_job(0); return done(); }
int logic_fade_job_down(void)  { if (!start()) return 0; fade_job(1); return done(); }
int logic_fade_job_white(void) { if (!start()) return 0; fade_job(2); return done(); }

/* sub_018FC: palette preset d7 (table 0x1960: colors less one, from, to) copied */
int logic_palette_preset(void)
{
    if (!start()) return 0;
    begin(0x018FC);
    uint32_t d6 = REG_D[6], d7 = REG_D[7], a5 = REG_A[5], a6 = REG_A[6];
    stack_put(-16, d6); stack_put(-12, d7); stack_put(-8, a5); stack_put(-4, a6);   /* (movem) */
    uint16_t i = (uint16_t)(lo(REG_D[7]) * 2);
    set_lo(6, i);
    i = (uint16_t)(i * 5);
    uint32_t t = 0x01960 + (uint32_t)(int32_t)(int16_t)i;
    uint16_t n = rom16(t);
    uint32_t from = rom32(t + 2), to = rom32(t + 6);
    ran_to(0x01914);
    for (;; n--) {
        WR16(to, mem16(from));
        from += 2; to += 2;
        if (!n) { ran_to(0x0191A); break; }
        branched(0x01916, 0x01914);
    }
    set16(PALETTE_CHANGED, 1);
    REG_D[6] = d6; REG_D[7] = d7; REG_A[5] = a5; REG_A[6] = a6;
    ran_to(0x01926);
    return done();
}

/* sub_0FD90: the stage's palette cycles (table 0xFE38 by stage) as jobs of kind 3: each
 * its frames per step, the colors from (+8) to (+$C), how many (+6), the steps round
 * (+7) */
int logic_palette_cycles(void)
{
    if (!start()) return 0;
    begin(0x0FD90);
    set_lo(7, (uint16_t)((ram16(STAGE_ENTRY) - 1) << 2));
    uint32_t a6 = rom32(0x0FE38 + (int16_t)lo(REG_D[7]));
    set_lo(7, mem16(a6));
    a6 += 2;
    if (lo(REG_D[7]) & 0x8000) { branched(0x0FDA4, 0x0FDE6); REG_A[6] = a6; return done(); }
    uint32_t a0 = 0xFF55D6;
    ran_to(0x0FDAE);
    for (;;) {
        set16(a0 + 2, mem16(a6));
        set16(a0 + 4, mem16(a6 + 2));
        set32(a0 + 8, mem32(a6 + 4));
        set32(a0 + 0xC, mem32(a6 + 8));
        set8(a0 + 6, (uint8_t)RD8(a6 + 12));
        set8(a0 + 7, (uint8_t)RD8(a6 + 13));
        set8(a0 + 0x10, (uint8_t)RD8(a6 + 13));
        a6 += 14;
        set32(a0 + 0x12, 0);
        set8(a0 + 1, 3);
        set8(a0, 1);
        set16(0xFF55D4, (uint16_t)(ram16(0xFF55D4) + 1));
        a0 += 0x20;
        uint16_t c = lo(REG_D[7]);
        set_lo(7, (uint16_t)(c - 1));
        if (!c) { ran_to(0x0FDE6); break; }
        branched(0x0FDE2, 0x0FDAE);
    }
    REG_A[0] = a0; REG_A[6] = a6;
    return done();
}

/* sub_0FDE8 (job kind 3, a0 the job): a palette cycle's step: the colors from the
 * source, so many on (+$12), into the palette; round again after its steps */
int logic_palette_cycle(void)
{
    if (!start()) return 0;
    uint32_t j = REG_A[0];
    begin(0x0FDE8);
    uint16_t t = (uint16_t)(ram16(j + 2) - 1);
    set16(j + 2, t);
    if (t) { branched(0x0FDEC, 0x0FE36); return done(); }
    set16(j + 2, ram16(j + 4));
    uint32_t a6 = ram32(j + 8) + (uint32_t)(int32_t)(int16_t)ram16(j + 0x12), a5 = ram32(j + 0xC);
    uint16_t n = ram8(j + 6);
    set_lo(6, n);
    ran_to(0x0FE0C);
    for (uint16_t k = (uint16_t)(n - 1);; k--) {
        WR16(a5, mem16(a6));
        a5 += 2; a6 += 2;
        if (!k) { ran_to(0x0FE12); break; }
        branched(0x0FE0E, 0x0FE0C);
    }
    REG_D[7] = 0xFFFF;
    REG_A[5] = a5; REG_A[6] = a6;
    set16(PALETTE_CHANGED, 1);
    uint8_t left = (uint8_t)(ram8(j + 0x10) - 1);
    set8(j + 0x10, left);
    if (!left) {
        set8(j + 0x10, ram8(j + 7));
        REG_D[6] = 0;
        ran_to(0x0FE2A);
        branched(0x0FE2A, 0x0FE32);
    } else {
        branched(0x0FE1E, 0x0FE2C);
        set_lo(6, (uint16_t)(n * 2 + ram16(j + 0x12)));
        ran_to(0x0FE32);
    }
    set16(j + 0x12, lo(REG_D[6]));
    ran_to(0x0FE36);
    return done();
}
