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
