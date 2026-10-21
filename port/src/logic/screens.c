/* Screens: the routine run every frame, and waits. */
#include "rt.h"

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
