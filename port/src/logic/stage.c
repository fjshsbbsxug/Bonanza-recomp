/* Setting up a stage: its objects placed. */
#include "rt.h"

/* sub_015F2: object d1 placed from a0 (x, y, height, picture, animation from the
 * table at a5, facing, sprite attributes), kept as where it started; in play, not seen
 * yet, standing still */
static void place_object(void)
{
    uint16_t o = lo(REG_D[1]);
    uint32_t a0 = REG_A[0];
    set_obj16(OBJ_ACTIVE, o, 1);
    set_obj16(OBJ_ON_SCREEN, o, 0);
    static const uint32_t now[4] = {OBJ_X, OBJ_Y, OBJ_HEIGHT, OBJ_FRAME},
                          kept[4] = {0xFF508E, 0xFF513A, 0xFF51E6, OBJ_PICTURE};
    for (int k = 0; k < 4; k++, a0 += 2) {
        set_obj16(now[k], o, mem16(a0));
        set_obj16(kept[k], o, mem16(a0));
    }
    uint16_t anim = mem16(a0);
    a0 += 2;
    set_obj16(OBJ_ANIM, o, anim);
    set_obj16(0xFF533E, o, anim);
    set_lo(7, (uint16_t)(anim << 2));
    REG_A[5] = mem32(REG_A[5] + (int16_t)lo(REG_D[7]));
    set_lo(6, (uint16_t)(o * 2));
    set_obj32(OBJ_ANIM_STEPS, o, REG_A[5]);
    set_obj16(OBJ_ANIM_AT, o, 0);
    set_obj16(OBJ_ANIM_TIME, o, 1);
    static const uint32_t cleared[5] = {OBJ_SIZE_X, OBJ_SIZE_Y, OBJ_DX, OBJ_DY, 0xFF39B6};
    for (int k = 0; k < 5; k++) set_obj16(cleared[k], o, 0);
    set_lo(3, mem16(a0));
    a0 += 2;
    set8(P1_FACING + (int16_t)o, (uint8_t)lo(REG_D[3]));
    set8(0xFF53EA + (int16_t)o, (uint8_t)lo(REG_D[3]));
    set_obj16(OBJ_ATTR, o, mem16(a0));
    REG_A[0] = a0 + 2;
    REG_A[6] = OBJ_ATTR;
    ran_to(0x016EA);
}

int logic_place_object(void)
{
    if (!start()) return 0;
    begin(0x015F2);
    place_object();
    return done();
}

/* sub_0156C: the stage's objects (table 0x3E000 by stage): the players (animations
 * 0x36E00), objects 2.. and 6.. (0x17CDE), the items from $26 (0x1CCBC); each group's
 * count less one first (negative: none) */
int logic_stage_objects(void)
{
    if (!start()) return 0;
    begin(0x0156C);
    set_lo(0, (uint16_t)((ram16(STAGE_ENTRY) - 1) << 2));
    REG_A[0] = mem32(0x3E000 + (int16_t)lo(REG_D[0]));
    static const struct { uint16_t o, d2; uint32_t anims, loop, bmi, past; } G[4] = {
        {0x00, 0x00, 0x36E00, 0x01586, 0, 0x01598},
        {0x04, 0x08, 0x17CDE, 0x015A2, 0x0159A, 0x015B4},
        {0x0C, 0x18, 0x17CDE, 0x015BE, 0x015B6, 0x015D0},
        {0x4C, 0x98, 0x1CCBC, 0x015DE, 0x015D2, 0x015F0},
    };
    for (int g = 0; g < 4; g++) {
        set_lo(0, mem16(REG_A[0]));
        REG_A[0] += 2;
        if (g && (lo(REG_D[0]) & 0x8000)) { branched(G[g].bmi, G[g].past); continue; }
        if (g < 3) { REG_D[1] = G[g].o; REG_D[2] = G[g].d2; }
        else { set_lo(1, G[g].o); set_lo(2, G[g].d2); }
        ran_to(G[g].loop);
        for (;;) {
            REG_A[5] = G[g].anims;
            uint32_t bsr = G[g].loop + 6;
            ran_to(bsr + 4);
            stack_put(-4, bsr + 4);
            jumped_to(0x015F2);
            place_object();
            ran_to(0x016EC);                          /* (its rts) */
            jumped_to(bsr + 4);
            set_lo(1, (uint16_t)(lo(REG_D[1]) + 2));
            set_lo(2, (uint16_t)(lo(REG_D[2]) + 4));
            uint16_t c = lo(REG_D[0]);
            set_lo(0, (uint16_t)(c - 1));
            if (!c) { ran_to(G[g].past); break; }
            branched(bsr + 8, G[g].loop);
        }
    }
    return done();
}

/* sub_04484, to its load list: the stage's tiles (its load list, table 0x640C by stage:
 * run as the game's code, by a jsr of the C's own, so a frame may end between its
 * loads as it does in the game's code) */
int logic_stage_graphics(void)
{
    if (!start()) return 0;
    begin(0x04484);
    set_lo(0, (uint16_t)((ram16(STAGE_ENTRY) - 1) << 2));
    REG_A[0] = mem32(0x0640C + (int16_t)lo(REG_D[0]));
    ran_to(0x04498);                                  /* (jsr (a0)) */
    REG_A[7] -= 4;
    set32(REG_A[7], 0x04498);
    return jump_to(REG_A[0]);
}

/* sub_04484 from 0x04498: the stage's objects (sub_0156C); the views' starting scroll;
 * the map's size (table 0x54E2: columns, rows; each + 1 too); the stage's two longs at
 * $1838 (table 0x5516); its palette (preset, table 0x557E: sub_018FC); its map's
 * picture (table 0x5598: sub_28A90); its music, not in the attract demo (0x5644) */
int logic_stage_graphics2(void)
{
    if (!start()) return 0;
    begin(0x04498);
    call(0x0156C, 0x04498);
    set16(0xFF184C, 0xFF79);
    set16(0xFF184E, 0);
    set16(0xFF2290, 0xFFF9);
    set16(0xFF2292, 0);
    set16(0xFF0680, 0xF3);
    uint16_t s = (uint16_t)(ram16(STAGE_ENTRY) - 1);
    uint32_t a6 = 0x054E2 + (uint32_t)(int32_t)(int16_t)(uint16_t)(s << 2);
    set_lo(7, mem16(a6));
    set16(0xFF2CCC, lo(REG_D[7]));
    set16(0xFF2CD0, (uint16_t)(lo(REG_D[7]) + 1));
    set_lo(7, mem16(a6 + 2));
    set16(0xFF2CCE, lo(REG_D[7]));
    set_lo(7, (uint16_t)(lo(REG_D[7]) + 1));
    set16(0xFF2CD2, lo(REG_D[7]));
    a6 = 0x05516 + (uint32_t)(int32_t)(int16_t)(uint16_t)(s << 3);
    set32(0xFF1838, mem32(a6));
    set32(0xFF183C, mem32(a6 + 4));
    REG_A[6] = 0x0557E;
    set_lo(7, mem16(0x0557E + (int16_t)(uint16_t)(s * 2)));
    call(0x018FC, 0x04524);
    set_lo(7, (uint16_t)(s << 2));
    REG_A[6] = mem32(0x05598 + (int16_t)lo(REG_D[7]));
    call(0x28A90, 0x0453C);
    if (ram16(DEMO)) { branched(0x04548, 0x04564); return done(); }
    REG_A[6] = 0x05644;
    set_lo(0, mem16(0x05644 + (int16_t)(uint16_t)(s * 2)));
    call(0x02D54, 0x0455E);
    ran_to(0x04564);
    return done();
}

/* ---------------------------------------------------------------- setting up a stage */

#include "setup.h"

/* sub_0430A (screen mode 3): a stage set up: the screen off, the work memory cleared
 * (sub_00470: a frame ends inside it, so the game's code runs it; on at 0x4322) */
int logic_stage_setup(void)
{
    if (!start()) return 0;
    begin(0x0430A);
    screen_off();
    ran_to(0x04322);
    REG_A[7] -= 4; set32(REG_A[7], 0x04322);
    return jump_to(0x00470);
}

/* (0x4322, long: see begin_long) the sprites and planes cleared, the HUD, the palette
 * cycles, the map and the points, the stage's tiles, objects and views (sub_04484), the
 * stage's own start (table 0x45CE); each player in the game started, the others' game
 * over put up; the views' tasks, the guards, the items; then a second's wait and the
 * stage's start (0x442A) from its frame (0x4460) */
int logic_stage_setup2(void)
{
    begin_long(0x04322);
    call(0x00522, 0x04322);
    call(0x050C4, 0x04328);
    call(0x0FD90, 0x0432E);
    call(0x1044A, 0x04334);
    call(0x0FF22, 0x0433A);
    call(0x11D36, 0x04340);
    call(0x11D50, 0x04346);
    set16(0xFF17A2, 0);
    call(0x04484, 0x04352);
    set_lo(0, (uint16_t)((ram16(STAGE_ENTRY) - 1) << 2));
    REG_A[0] = mem32(0x45CE + (int16_t)lo(REG_D[0]));
    call(REG_A[0], 0x0436A);
    if (ram16(STAGE_ENTRY) != 6) branched(0x04374, 0x04380);
    else { set16(0xFFA5C6, 1); ran_to(0x04380); }
    set32(OBJ_ACTIVE, 0);
    static const struct { uint32_t beq, out, jsr_in, bra, jsr_over, next; } at[2] = {
        {0x04390, 0x043A8, 0x0439E, 0x043A4, 0x043B6, 0x043BC},
        {0x043C6, 0x043DE, 0x043D4, 0x043DA, 0x043EC, 0x043F2},
    };
    for (int p = 0; p < 2; p++) {
        set_lo(7, ram16(0xFFC076) & (p ? 2 : 1));
        if (lo(REG_D[7])) {
            set16(OBJ_ACTIVE + 2 * (uint32_t)p, 1);
            REG_D[0] = 0;
            call(p ? 0x08F26 : 0x08E74, at[p].jsr_in);
            branched(at[p].bra, at[p].next);
        } else {
            branched(at[p].beq, at[p].out);
            set16(p ? 0xFFC056 : 0xFFC054, 0);
            set8(0xFF6C6F + 2 * (uint32_t)p, 1);
            call(p ? 0x13ADC : 0x137A6, at[p].jsr_over);
        }
    }
    call(0x080F8, 0x043F2);
    call(0x141F8, 0x043F8);
    call(0x1AD74, 0x043FE);
    set16(0xFF5546, 0x3C);
    set32(SCREEN_RUN, 0x4460);
    set32(0xFFC072, 0x442A);
    set16(0xFF1816, 1);
    ran_to(0x04428);
    return done_long();
}
