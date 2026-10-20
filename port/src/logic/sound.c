/* Sounds. */
#include "rt.h"

/* sub_02D54: ask for sound effect d0, unless a music command (0xE0-0xEF: fade, stop...)
 * is waiting to be sent: it goes first. */
int logic_sound_request(void)
{
    if (!start()) return 0;
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
