/* The game's logic, rewritten in readable C (logic.c): 1 done (the caller does the
 * return), 0 not run (the game's routine runs as translated). */
#ifndef LOGIC_H
#define LOGIC_H
#include <stdint.h>
#include <stdio.h>

extern uint64_t logic_stat_c;            /* calls done in C */
extern int logic_off;                    /* 1: every routine runs as translated (checks) */
void logic_report(FILE *f);              /* the calls done in C, by routine */

extern int  logic_sound_request(void);        /* sub_02D54 */
extern int  logic_protect_p1(void);           /* sub_09EDA */
extern int  logic_protect_p2(void);           /* sub_09EF4 */
extern int  logic_protect2_p1(void);          /* sub_09F0E */
extern int  logic_protect2_p2(void);          /* sub_09F28 */
extern int  logic_player_anim_p1(void);       /* sub_097F6 */
extern int  logic_player_anim_p2(void);       /* sub_0981E */
extern int  logic_player_state_p1(void);      /* sub_0986A */
extern int  logic_player_state_p2(void);      /* sub_098B8 */
extern int  logic_pose_keep_p1(void);         /* sub_098E2 */
extern int  logic_pose_keep_p2(void);         /* sub_09928 */
extern int  logic_pose_back_p1(void);         /* sub_0996E */
extern int  logic_pose_back_p2(void);         /* sub_099A2 */
extern int  logic_path_start_p1(void);        /* sub_0A51A */
extern int  logic_path_start_p2(void);        /* sub_0A542 */
extern int  logic_height_step_p1(void);       /* sub_0BF10 */
extern int  logic_height_step_p2(void);       /* sub_0E392 */
extern int  logic_take_away_p1(void);         /* sub_0AE8A */
extern int  logic_take_away_p2(void);         /* sub_0AEA6 */
extern int  logic_guards_forget_p1(void);     /* sub_09F42 */
extern int  logic_guards_forget_p2(void);     /* sub_09F66 */
extern int  logic_guard_event(void);          /* sub_17170 */
extern int  logic_guard_event_script(void);   /* sub_171B6 */
extern int  logic_place_above32(void);        /* sub_19C00 */
extern int  logic_place_above64(void);        /* sub_19C26 */
extern int  logic_screen_wait(void);          /* sub_04460 */

#endif
