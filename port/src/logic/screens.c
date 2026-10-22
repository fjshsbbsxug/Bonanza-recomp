/* Screens: the routine run every frame, and waits. */
#include "rt.h"
#include "../video.h"

/* sub_04460: a screen's wait: when it runs out, the screen's next routine runs */
int logic_screen_wait(void)
{
    if (!start()) return 0;
    uint16_t before = ram16(SCREEN_WAIT), left = (uint16_t)(before - 1);
    set16(SCREEN_WAIT, left);
    FLAG_X = CFLAG_16((uint)before - 1);              /* (for the callers: subq) */
    begin(0x04460);
    if (left) {
        branched(0x04466, 0x04474);
    } else {
        set32(SCREEN_RUN, ram32(SCREEN_NEXT));
        ran_to(0x04474);
    }
    return done();
}

/* sub_008AA: player d7's buttons as the game uses them (P1_PAD / P2_PAD: held, then
 * pressed): start and the directions as read, and the pad's A, B and C each as the
 * options set them (the bits at PAD_MAP, $C05C..) */
int logic_pad_map(void)
{
    if (!start()) return 0;
    begin(0x008AA);
    int p = lo(REG_D[7]) != 0;
    if (!p) branched(0x008BE, 0x008D4);
    else ran_to(0x008D4);
    uint32_t pad = P1_PAD + 2 * p, raw = PAD_RAW + 2 * p;
    REG_A[0] = pad;
    REG_A[1] = 0xFFC060 + 2 * p;
    set16(pad, ram16(raw) & 0x8F8F);
    static const uint8_t bit[3] = {0x40, 0x10, 0x20};
    for (int k = 0; k < 3; k++) {
        uint32_t b = 0x008E0 + 0x1E * k;
        uint8_t map = ram8(PAD_MAP + k);
        set_lo(1, (uint16_t)((lo(REG_D[1]) & 0xFF00) | map));
        for (int h = 0; h < 2; h++) {
            uint8_t v = ram8(raw + h) & bit[k];
            set_lo(0, (uint16_t)((lo(REG_D[0]) & 0xFF00) | v));
            uint32_t beq = b + (h ? 0x16 : 8), to = b + (h ? 0x1E : 0xE);
            if (!v) branched(beq, to);
            else { set8(pad + h, ram8(pad + h) | map); ran_to(to); }
        }
    }
    REG_A[2] = raw;
    REG_A[3] = PAD_MAP + 3;
    return done();
}

/* sub_13744 / sub_132F2: a6 into the first of 4 free slots at $6C92 / $6C82 (none
 * free: nothing) */
static int slot_add(uint32_t r, uint32_t slots)
{
    begin(r);
    stack_put(-8, REG_D[0]); stack_put(-4, REG_A[0]);   /* (movem.l d0/a0, -(a7)) */
    uint32_t a0 = slots;
    for (int k = 0;; k++, a0 += 4) {
        if (!ram32(a0)) {
            branched(r + 0xE, r + 0x1E);
            set32(a0, REG_A[6]);
            ran_to(r + 0x24);
            break;
        }
        if (k == 3) { ran_to(r + 0x1C); break; }
        branched(r + 0x14, r + 0xC);
    }
    return done();
}
int logic_slot_add(void)  { if (!start()) return 0; return slot_add(0x13744, 0xFF6C92); }
int logic_slot_add2(void) { if (!start()) return 0; return slot_add(0x132F2, 0xFF6C82); }

/* sub_187CE: objects $46..$49 (the messages' slots) out of play */
int logic_messages_clear(void)
{
    if (!start()) return 0;
    begin(0x187CE);
    set_lo(0, 0x8C);
    REG_A[6] = OBJ_ACTIVE;
    for (int k = 0; k < 4; k++) set_obj16(OBJ_ACTIVE, (uint16_t)(0x8C + 2 * k), 0);
    flags_of16(0);
    ran_to(0x187E8);
    return done();
}

/* sub_01318: a job into the first of 8 free slots at $5550 ($10 bytes each, from a0:
 * two longs, three words); d7 1 if there was room ($554E counts them), else 0 */
int logic_text_job(void)
{
    if (!start()) return 0;
    begin(0x01318);
    uint32_t a6 = 0xFF5550;
    set_lo(6, 7);
    ran_to(0x01322);
    for (;;) {
        if (!ram16(a6)) break;
        a6 += 0x10;
        uint16_t c = lo(REG_D[6]);
        set_lo(6, (uint16_t)(c - 1));
        if (!c) { REG_A[6] = a6; REG_D[7] = 0; ran_to(0x01332); return done(); }
        ran_to(0x0132C);
        branched(0x0132C, 0x01322);
    }
    branched(0x01324, 0x01334);
    uint32_t a0 = REG_A[0];
    set32(a6 + 2, mem32(a0));
    set32(a6 + 6, mem32(a0 + 4));
    set16(a6 + 0xA, mem16(a0 + 8));
    set16(a6 + 0xC, mem16(a0 + 10));
    set16(a6 + 0xE, mem16(a0 + 12));
    REG_A[0] = a0 + 14;
    set_lo(6, (uint16_t)((lo(REG_D[6]) + 1) & 7));
    REG_D[7] = 1;
    set16(a6, 1);
    set16(0xFF554E, (uint16_t)(ram16(0xFF554E) + 1));
    REG_A[6] = a6;
    ran_to(0x01358);
    return done();
}

/* sub_01468: the jobs at $55D6 ($20 bytes each; $55D4 of them in use): each in use runs
 * its kind's routine (its second byte: table 0x14A6) */
int logic_jobs(void)
{
    if (!start()) return 0;
    begin(0x01468);
    REG_A[0] = 0xFF55D6;
    uint16_t n = ram16(0xFF55D4);
    set_lo(0, n);
    if (!n) { branched(0x01474, 0x014A4); return done(); }
    set_lo(0, (uint16_t)(n - 1));
    ran_to(0x0147A);
    for (;;) {
        while (!ram8(REG_A[0])) {
            REG_A[0] += 0x20;
            ran_to(0x01484);
            branched(0x01484, 0x0147A);
        }
        branched(0x0147C, 0x01486);
        REG_D[7] = (uint32_t)ram8(REG_A[0] + 1) << 2;
        REG_A[6] = rom32(0x014A6 + (int16_t)lo(REG_D[7]));
        uint32_t d0 = REG_D[0], a0 = REG_A[0];
        stack_put(-8, d0); stack_put(-4, a0);         /* (movem.l d0/a0, -(a7)) */
        REG_A[7] -= 8;
        call(REG_A[6], 0x01496);
        REG_A[7] += 8;
        REG_D[0] = d0; REG_A[0] = a0 + 0x20;
        uint16_t c = lo(REG_D[0]);
        set_lo(0, (uint16_t)(c - 1));
        if (!c) { ran_to(0x014A4); break; }
        branched(0x014A0, 0x0147A);
    }
    return done();
}

/* ---------------------------------------------------------------- the pads */

/* sub_0087A: the pad at port a1 read (its two halves: start and A, then B, C and the
 * directions, a bit set for each button down) into a0: held, then newly pressed */
static void pad_read(void)
{
    uint32_t a1 = REG_A[1];
    WR8(a1, 0);
    uint8_t d0 = (uint8_t)RD8(a1);
    uint32_t sh = (uint32_t)d0 << 2;
    FLAG_X = FLAG_C = CFLAG_8(sh);                    /* (lsl.b #2: X left for the callers) */
    d0 = (uint8_t)(sh & 0xC0);
    WR8(a1, 0x40);
    uint8_t d1 = (uint8_t)(RD8(a1) & 0x3F);
    d0 = (uint8_t)~(d0 | d1);
    d1 = d0;
    uint8_t d2 = ram8(REG_A[0]);
    set8(REG_A[0], d1);
    d0 = (uint8_t)((d0 ^ d2) & d1);
    set8(REG_A[0] + 1, d0);
    REG_A[0] += 2;
    REG_D[0] = (REG_D[0] & 0xFFFFFF00u) | d0;
    REG_D[1] = (REG_D[1] & 0xFFFFFF00u) | d1;
    REG_D[2] = (REG_D[2] & 0xFFFFFF00u) | d2;
    ran_to(0x008A8);
}

/* sub_007EE: both pads read (ports $A10003, $A10005) into PAD_RAW; up with down, or left
 * with right, count as neither; then each mapped by the options (sub_008AA) */
static void pads_read(void)
{
    REG_A[0] = PAD_RAW;
    REG_A[1] = 0xA10003;
    for (int k = 0; k < 2; k++) {
        uint32_t bsr = k ? 0x00800 : 0x007FA;
        ran_to(bsr + 4);
        stack_put(-4, bsr + 4);                       /* (the bsr's return) */
        jumped_to(0x0087A);
        pad_read();
        ran_to(0x008AA);                              /* (its rts) */
        jumped_to(bsr + 4);
        if (!k) REG_A[1] += 2;
    }
    static const struct { uint32_t pad; uint8_t both; uint16_t clear; uint32_t bne, past; } F[4] = {
        {PAD_RAW, 3, 0xFCFC, 0x00812, 0x0081E}, {PAD_RAW, 0xC, 0xF3F3, 0x0082C, 0x00838},
        {PAD_RAW + 2, 3, 0xFCFC, 0x00846, 0x00852}, {PAD_RAW + 2, 0xC, 0xF3F3, 0x00860, 0x0086C},
    };
    for (int k = 0; k < 4; k++) {
        set_lo(0, (uint16_t)((lo(REG_D[0]) & 0xFF00) | (ram8(F[k].pad) & F[k].both)));
        if ((ram8(F[k].pad) & F[k].both) != F[k].both) branched(F[k].bne, F[k].past);
        else { set16(F[k].pad, ram16(F[k].pad) & F[k].clear); ran_to(F[k].past); }
    }
    REG_D[7] = 0;
    call(0x008AA, 0x0086E);
    REG_D[7] = 1;
    call(0x008AA, 0x00874);
    ran_to(0x00878);
}

int logic_pad_read(void)
{
    if (!start()) return 0;
    begin(0x0087A);
    pad_read();
    return done();
}

int logic_pads_read(void)
{
    if (!start()) return 0;
    begin(0x007EE);
    pads_read();
    return done();
}

/* sub_007BE: the pads read with the sound processor's bus held */
int logic_pads(void)
{
    if (!start()) return 0;
    begin(0x007BE);
    WR16(0xA11100, 0x100);
    ran_to(0x007C6);
    while (RD8(0xA11100) & 1) branched(0x007CE, 0x007C6);
    ran_to(0x007D4);
    stack_put(-4, 0x007D4);                           /* (the bsr's return) */
    REG_A[7] -= 4;
    jumped_to(0x007EE);
    pads_read();
    ran_to(0x0087A);                                  /* (its rts) */
    REG_A[7] += 4;
    jumped_to(0x007D4);
    WR16(0xA11100, 0);
    ran_to(0x007EC);
    return done();
}

/* sub_01550: text number d7 (table 0x5E86) put up (sub_01318), d7 and a0 kept */
int logic_text_number(void)
{
    if (!start()) return 0;
    begin(0x01550);
    uint32_t d7 = REG_D[7], a0 = REG_A[0];
    REG_A[7] -= 8; set32(REG_A[7], d7); set32(REG_A[7] + 4, a0);
    set_lo(7, (uint16_t)(lo(REG_D[7]) << 2));
    REG_A[0] = mem32(0x5E86 + (int16_t)lo(REG_D[7]));
    call(0x01318, 0x01560);
    REG_D[7] = d7; REG_A[0] = a0;
    REG_A[7] += 8;
    ran_to(0x0156A);
    return done();
}

/* sub_0181A: the map's column jobs cleared (the 8 slots, and the one drawn this frame) */
int logic_column_jobs_clear(void)
{
    if (!start()) return 0;
    begin(0x0181A);
    set16(0xFF2EF4, 0);
    REG_A[0] = 0xFF2CF4;
    set_lo(0, 7);
    ran_to(0x0182A);
    for (;;) {
        set16(REG_A[0], 0);
        REG_A[0] += 0x40;
        uint16_t c = lo(REG_D[0]);
        set_lo(0, (uint16_t)(c - 1));
        if (!c) { ran_to(0x01834); break; }
        branched(0x01830, 0x0182A);
    }
    return done();
}

/* sub_013FE: the jobs list cleared (its 8 slots of $10 bytes, and its count) */
int logic_jobs_clear(void)
{
    if (!start()) return 0;
    begin(0x013FE);
    REG_A[0] = 0xFF5550;
    set_lo(0, 7);
    ran_to(0x01408);
    for (;;) {
        set16(REG_A[0], 0);
        REG_A[0] += 0x10;
        uint16_t c = lo(REG_D[0]);
        set_lo(0, (uint16_t)(c - 1));
        if (!c) { ran_to(0x01412); break; }
        branched(0x0140E, 0x01408);
    }
    set16(0xFF554E, 0);
    ran_to(0x01418);
    return done();
}

/* sub_007A8: the three controller ports' TH lines made outputs */
int logic_pads_init(void)
{
    if (!start_io()) return 0;
    begin(0x007A8);
    REG_D[0] = 0x40;
    WR8(0xA10009, 0x40);
    WR8(0xA1000B, 0x40);
    WR8(0xA1000D, 0x40);
    ran_to(0x007BC);
    return done();
}

/* sub_0093C: what is in the controller port at a6, from its answers to the TH line set
 * and cleared (table 0x986: each write and the bits to test), as bits in d7; the Z80
 * held and the interrupts off meanwhile; d1, d2, a5 kept */
int logic_pad_detect(void)
{
    if (!start_io()) return 0;
    begin(0x0093C);
    uint32_t d1 = REG_D[1], d2 = REG_D[2], a5 = REG_A[5];
    REG_A[7] -= 12;
    WR32(REG_A[7], d1); WR32(REG_A[7] + 4, d2); WR32(REG_A[7] + 8, a5);
    uint32_t sr = m68ki_get_sr();
    REG_A[7] -= 2; WR16(REG_A[7], (uint16_t)sr);
    m68ki_set_sr(sr | 0x700);
    WR16(0xA11100, 0x100);
    ran_to(0x0094E);
    for (;;) {
        if (!(RD8(0xA11100) & 1)) { ran_to(0x00958); break; }
        branched(0x00956, 0x0094E);
    }
    REG_A[5] = 0x986;
    WR8(REG_A[6] + 6, RD8(0x986));
    REG_D[7] = 0;
    REG_D[1] = 8;
    ran_to(0x00964);
    for (;;) {
        WR8(REG_A[6], RD8(REG_A[5])); REG_A[5]++;
        REG_D[2] = (REG_D[2] & 0xFFFFFF00u) | (uint8_t)(RD8(REG_A[6]) & RD8(REG_A[5]));
        REG_A[5]++;
        if (!(uint8_t)REG_D[2]) branched(0x0096E, 0x00972);
        else { REG_D[7] |= (uint8_t)REG_D[1]; ran_to(0x00972); }
        REG_D[1] = (REG_D[1] & 0xFFFFFF00u) | (uint8_t)((uint8_t)REG_D[1] >> 1);
        if (!(uint8_t)REG_D[1]) { ran_to(0x00976); break; }
        branched(0x00974, 0x00964);
    }
    WR16(0xA11100, 0);
    m68ki_set_sr(RD16(REG_A[7]));
    REG_A[7] += 2;
    REG_D[1] = RD32(REG_A[7]); REG_D[2] = RD32(REG_A[7] + 4); REG_A[5] = RD32(REG_A[7] + 8);
    REG_A[7] += 12;
    ran_to(0x00984);
    return done();
}

/* (the vertical blank, 0xAA2 / 0xAB4: this frame's player's line routine for his new
 * columns (0xFFC10C / 0xFFC110) called; 0xABC: with none, the map's column jobs
 * (sub_017D8), then the tiles waiting unless a column was drawn (sub_0135A)) */
static int vblank_lines(uint32_t at)
{
    if (!start()) return 0;
    begin(at);
    REG_A[0] = REG_D[0];
    call(REG_A[0], at + 2);
    branched(at + 4, 0x00B58);
    return jump_to(0x00B58);
}
int logic_vblank_lines1(void) { return vblank_lines(0x00AA2); }
int logic_vblank_lines2(void) { return vblank_lines(0x00AB4); }
int logic_vblank_columns(void)
{
    if (!start()) return 0;
    begin(0x00ABC);
    call(0x017D8, 0x00ABC);
    if (ram16(0xFF2EF4)) branched(0x00AC8, 0x00AD2);
    else call(0x0135A, 0x00ACC);
    return jump_to(0x00AD2);
}

/* ---------------------------------------------------------------- the boot's set-up */

/* (0x384, after a cold start's checks) the options' defaults: level NORMAL, 3 lives, the
 * sound test at 0, CONTROL 0 (the buttons' bits, sub_03DFA); the work memory marked
 * "init"; then cleared (sub_00470: frames end inside it; on at 0x3BA) */
int logic_boot_options(void)
{
    if (!start()) return 0;
    begin(0x00384);
    set16(0xFFC04E, 1);
    set16(0xFFC050, 0);
    set16(0xFFC052, 3);
    set16(0xFFC058, 0);
    call(0x03DFA, 0x003A4);
    set32(0xFFC000, 0x696E6974);
    ran_to(0x003BA);
    REG_A[7] -= 4; set32(REG_A[7], 0x003BA);
    return jump_to(0x00470);
}

/* (0x3BA) stage 1, the split screen's line routine's words (table 0x46A), no columns
 * routines; the video set up (sub_00484); then the sound driver loaded (sub_02C76: frames
 * end inside it; on at 0x3F2) */
int logic_boot_video(void)
{
    if (!start()) return 0;
    begin(0x003BA);
    set16(STAGE_ENTRY, 1);
    REG_A[0] = 0x46A; REG_A[1] = 0xFFC0B0;
    for (int k = 0; k < 3; k++) set16(0xFFC0B0 + 2 * (uint32_t)k, mem16(0x46A + 2 * (uint32_t)k));
    REG_A[0] = 0x46E; REG_A[1] = 0xFFC0B4;
    set32(0xFFC10C, 0);
    set32(0xFFC110, 0);
    call(0x00484, 0x003E6);
    ran_to(0x003F2);
    REG_A[7] -= 4; set32(REG_A[7], 0x003F2);
    return jump_to(0x02C76);
}

/* (0x3F2) the controller ports: set up and what is in each (bits $C064, $C065, both
 * $C066); the sprites and planes cleared, the screen on, the screens from the logo (mode
 * 0); then the interrupts let in and the idle loop (0x458) */
int logic_boot_pads(void)
{
    if (!start()) return 0;
    begin(0x003F2);
    call(0x007A8, 0x003F2);
    REG_A[6] = 0xA10003;
    call(0x0093C, 0x003FE);
    set8(0xFFC064, (uint8_t)REG_D[7]);
    REG_A[6] = 0xA10005;
    call(0x0093C, 0x00410);
    set8(0xFFC065, (uint8_t)REG_D[7]);
    REG_D[7] = (REG_D[7] & 0xFFFFFF00u) | (uint8_t)(ram8(0xFFC064) + ram8(0xFFC065));
    set8(0xFFC066, (uint8_t)REG_D[7]);
    call(0x00522, 0x0042E);
    set16(0xFFC07C, ram16(0xFFC07C) | 0x8140);
    video_reg_word(ram16(0xFFC07C));
    set32(SCREEN_RUN, 0x2E06);
    set16(GAME_MODE, 0);
    ran_to(0x00458);
    return jump_to(0x00458);
}

/* (the vertical blank at 0xA22: the pads read (sub_007BE), the sound asked for sent
 * (sub_02DC2); then its sprites' part (0xA2C)) */
int logic_vblank_pads(void)
{
    if (!start()) return 0;
    begin(0x00A22);
    call(0x007BE, 0x00A22);
    call(0x02DC2, 0x00A26);
    return jump_to(0x00A2C);
}

/* (the vertical blank's end at 0xBE8: the screen's routine (SCREEN_RUN) called - it
 * waits for frames, so the game's code calls it; on at 0xBF0) */
int logic_vblank_screen(void)
{
    if (!start()) return 0;
    begin(0x00BE8);
    REG_A[0] = ram32(SCREEN_RUN);
    ran_to(0x00BF0);
    REG_A[7] -= 4; set32(REG_A[7], 0x00BF0);
    return jump_to(REG_A[0]);
}

/* (0xBF0) the video registers the game keeps written (sub_0070A), the jobs list run
 * (sub_01468) unless paused, the frame's flag cleared; then its rte (0xC0E) */
int logic_vblank_end(void)
{
    if (!start()) return 0;
    begin(0x00BF0);
    call(0x0070A, 0x00BF0);
    if (ram16(0xFF1806)) branched(0x00BFC, 0x00C06);
    else call(0x01468, 0x00C00);
    set16(0xFF17B4, 0);
    ran_to(0x00C0E);
    return jump_to(0x00C0E);
}

/* sub_00470 (long: it always runs across frames, so the game's code runs it, but for the
 * checks with LOGIC_LONG_KEEP / LOGIC_LONG_REF): the work memory cleared, $C000 bytes
 * from $FF0000 */
int logic_work_clear(void)
{
    begin_long(0x00470);
    REG_A[0] = 0xFF0000;
    set_lo(0, 0x2FFF);
    REG_D[1] = 0;
    ran_to(0x0047C);
    for (;;) {
        set32(REG_A[0], 0);
        REG_A[0] += 4;
        uint16_t c = lo(REG_D[0]);
        set_lo(0, (uint16_t)(c - 1));
        if (!c) { ran_to(0x00482); break; }
        branched(0x0047E, 0x0047C);
    }
    return done_long();
}

/* sub_00522: the screen cleared: the planes (sub_0052E), the scroll (sub_00628), the
 * sprites (sub_0066A) */
int logic_screen_clear(void)
{
    if (!start()) return 0;
    begin(0x00522);
    call(0x0052E, 0x00522);
    call(0x00628, 0x00524);
    call(0x0066A, 0x00528);
    ran_to(0x0052C);
    return done();
}
