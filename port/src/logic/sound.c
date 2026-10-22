/* Sounds. */
#include "rt.h"

/* sub_02D54: ask for sound effect d0, unless a music command (0xE0-0xEF: fade, stop...)
 * is waiting to be sent: it goes first. */
int logic_sound_request(void)
{
    if (!start_io()) return 0;
    uint16_t waiting = ram16(SOUND_REQUEST);
    int music_waiting = (waiting & 0xFFF0) == 0x00E0;
    if (!music_waiting)
        set16(SOUND_REQUEST, lo(REG_D[0]));

    /* for the callers: the flags (of the compare, or of the word stored) */
    if (music_waiting) {
        FLAG_N = NFLAG_CLEAR; FLAG_Z = 0; FLAG_V = VFLAG_CLEAR; FLAG_C = CFLAG_CLEAR;
    } else {
        flags_of16(lo(REG_D[0]));
    }
    begin(0x02D54);
    if (music_waiting) branched(0x02D66, 0x02D70);
    ran_to(0x02D74);
    return done();
}

/* (the Z80's bus asked for and waited on: granted at once here, as on the console) */
static void z80_bus_take(uint32_t poll, uint32_t bne)
{
    WR16(0xA11100, 0x100);
    ran_to(poll);
    for (;;) {
        if (!(RD8(0xA11100) & 1)) { ran_to(bne + 2); return; }
        branched(bne, poll);
    }
}

/* sub_02D3A: the Z80 reset, a moment, and let run again */
static void z80_reset_pulse(void)
{
    WR16(0xA11200, 0);
    WR16(0xA11200, 0x100);
}
int logic_z80_reset(void)
{
    if (!start_io()) return 0;
    begin(0x02D3A);
    z80_reset_pulse();
    flags_of16(0x100);
    ran_to(0x02D52);
    return done();
}

/* sub_02D76 / sub_02D98: sound command d0 sent to the Z80 at once (sub_02D98: a song or
 * effect, then no more for 60 frames) */
int logic_sound_now(void)
{
    if (!start_io()) return 0;
    begin(0x02D76);
    z80_bus_take(0x02D7E, 0x02D86);
    WR8(0xA01C10, (uint8_t)REG_D[0]);
    WR16(0xA11100, 0);
    flags_of16(0);
    ran_to(0x02D96);
    return done();
}
int logic_sound_play(void)
{
    if (!start_io()) return 0;
    begin(0x02D98);
    z80_bus_take(0x02DA0, 0x02DA8);
    WR8(0xA01C0A, (uint8_t)REG_D[0]);
    WR16(0xA11100, 0);
    set16(0xFF566A, 0x3C);
    flags_of16(0x3C);
    ran_to(0x02DC0);
    return done();
}

/* sub_02DC2 (each frame): the sound asked for (SOUND_REQUEST) sent to the Z80, with the
 * interrupts held off, unless one was sent less than 60 frames ago */
int logic_sound_send(void)
{
    if (!start_io()) return 0;
    begin(0x02DC2);
    if (ram16(0xFF566A)) { flags_of16(ram16(0xFF566A)); branched(0x02DC8, 0x02E04); return done(); }
    set_lo(0, ram16(SOUND_REQUEST));
    if (!lo(REG_D[0])) { flags_of16(0); branched(0x02DD2, 0x02E04); return done(); }
    uint32_t sr = m68ki_get_sr();
    REG_A[7] -= 2; WR16(REG_A[7], (uint16_t)sr);
    m68ki_set_sr(sr | 0x700);
    z80_bus_take(0x02DE4, 0x02DEC);
    WR8(0xA01C0A, (uint8_t)REG_D[0]);
    WR16(0xA11100, 0);
    m68ki_set_sr(RD16(REG_A[7]));
    REG_A[7] += 2;
    set16(SOUND_REQUEST, 0);
    flags_of16(0);
    ran_to(0x02E04);
    return done();
}

/* sub_02C76 (long, like sub_00470: it always runs across frames): the sound driver into
 * the Z80's memory (its code from 0x33000, its data from 0x3418C at $1200), its work set
 * up, then the Z80 reset and let run */
int logic_sound_start(void)
{
    begin_long(0x02C76);
    WR16(0xA11100, 0x100);
    ran_to(0x02C82);                                  /* (bsr sub_02D3A, inline) */
    jumped_to(0x02D3A);
    z80_reset_pulse();
    ran_to(0x02D54);
    jumped_to(0x02C82);
    for (;;) {
        if (!(RD8(0xA11100) & 1)) { ran_to(0x02C8C); break; }
        branched(0x02C8A, 0x02C82);
    }
    static const struct { uint32_t from, to; uint16_t n; uint32_t loop, dbra; } copy[2] = {
        {0x33000, 0xA00000, 0x118A, 0x02C9C, 0x02C9E}, {0x3418C, 0xA01200, 0x6FD, 0x02CB2, 0x02CB4},
    };
    for (int k = 0; k < 2; k++) {
        REG_A[0] = copy[k].from;
        REG_A[1] = copy[k].to;
        set_lo(0, copy[k].n);
        ran_to(copy[k].loop);
        for (;;) {
            WR8(REG_A[1], RD8(REG_A[0]));
            REG_A[0]++; REG_A[1]++;
            uint16_t c = lo(REG_D[0]);
            set_lo(0, (uint16_t)(c - 1));
            if (!c) { ran_to(copy[k].dbra + 4); break; }
            branched(copy[k].dbra, copy[k].loop);
        }
    }
    static const uint8_t work[10] = {3, 0x80, 0, 0x80, 0, 0, 0xCD, 0x80, 0, 0};
    for (int i = 0; i < 10; i++) WR8(0xA01C00 + (uint32_t)i, work[i]);
    WR8(0xA01BFF, 0);
    WR8(0xA01C10, 0x80);
    z80_reset_pulse();
    WR16(0xA11100, 0);
    flags_of16(0);
    ran_to(0x02D38);
    return done_long();
}
