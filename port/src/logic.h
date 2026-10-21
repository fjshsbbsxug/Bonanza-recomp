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
extern int  logic_player_frame_p1(void);      /* sub_09026 */
extern int  logic_player_frame_p2(void);      /* sub_091A8 */
extern int  logic_player_after_p1(void);      /* sub_0932A */
extern int  logic_player_after_p2(void);      /* sub_09408 */
extern int  logic_player_view_p1(void);       /* sub_094E6 */
extern int  logic_player_view_p2(void);       /* sub_0966E */
extern int  logic_player_script_p1(void);     /* sub_09ADE */
extern int  logic_player_script_p2(void);     /* sub_09B00 */
extern int  logic_player_ground_p1(void);     /* sub_09CBA */
extern int  logic_player_ground_p2(void);     /* sub_09DC6 */
extern int  logic_player_stop_p1(void);       /* sub_099D6 */
extern int  logic_player_stop_p2(void);       /* sub_09A1C */
extern int  logic_player_stand_pose_p1(void); /* sub_099DE */
extern int  logic_player_stand_pose_p2(void); /* sub_09A24 */
extern int  logic_player_script_start_p1(void);/* sub_09A62 */
extern int  logic_player_script_start_p2(void);/* sub_09A8C */
extern int  logic_player_door_ok_p1(void);    /* sub_0AAE2 */
extern int  logic_player_door_ok_p2(void);    /* sub_0AB24 */
extern int  logic_state_stand_p1(void);       /* sub_0B56A */
extern int  logic_state_stand_p2(void);       /* sub_0DA6E */
extern int  logic_state_walk_p1(void);        /* sub_0B5A6 */
extern int  logic_state_walk_p2(void);        /* sub_0DA9A */
extern int  logic_state_walk_frame_p1(void);  /* sub_0B5FC */
extern int  logic_state_walk_frame_p2(void);  /* sub_0DAF0 */
extern int  logic_player_wait_p1(void);       /* sub_0B472 */
extern int  logic_player_wait_p2(void);       /* sub_0D976 */
extern int  logic_player_waiting_p1(void);    /* sub_0B47C */
extern int  logic_player_waiting_p2(void);    /* sub_0D980 */
extern int  logic_player_foot_p1(void);       /* sub_0B8BC */
extern int  logic_player_foot_p2(void);       /* sub_0DDB0 */
extern int  logic_take_away_p1(void);         /* sub_0AE8A */
extern int  logic_take_away_p2(void);         /* sub_0AEA6 */
extern int  logic_guards_forget_p1(void);     /* sub_09F42 */
extern int  logic_guards_forget_p2(void);     /* sub_09F66 */
extern int  logic_guard_event(void);          /* sub_17170 */
extern int  logic_guard_event_script(void);   /* sub_171B6 */
extern int  logic_place_above32(void);        /* sub_19C00 */
extern int  logic_place_above64(void);        /* sub_19C26 */
extern int  logic_screen_wait(void);          /* sub_04460 */
extern int  logic_guards_update(void);        /* sub_14232 */
extern int  logic_map_cell(void);             /* sub_1016A */
extern int  logic_wall_test(void);            /* sub_101F2 */
extern int  logic_wall_test2(void);           /* sub_10274 */
extern int  logic_probe_176B8(void);
extern int  logic_probe_176F8(void);
extern int  logic_probe_17738(void);
extern int  logic_probe_17778(void);
extern int  logic_probe_1794A(void);
extern int  logic_probe_177B8(void);
extern int  logic_probe_177EC(void);
extern int  logic_probe_17820(void);
extern int  logic_probe_178AA(void);
extern int  logic_probe_17934(void);
extern int  logic_wall_near(void);            /* sub_17A8A */
extern int  logic_wall_feet(void);            /* sub_17AFC */
extern int  logic_floor_under(void);          /* sub_17C54 */
extern int  logic_stairs_ahead(void);         /* sub_17B82 */
extern int  logic_contact_free(void);         /* sub_17BD8 */
extern int  logic_contact_up(void);           /* sub_17C1A */
extern int  logic_guard_told(void);           /* sub_17A44 */
extern int  logic_guard_anim_start(void);     /* sub_1767C */
extern int  logic_guard_turn_begin(void);     /* sub_173D6 */
extern int  logic_guard_turn_begin2(void);    /* sub_173DC */
extern int  logic_guard_turn_step(void);      /* sub_1763C */
extern int  logic_touching(void);             /* sub_1A95C */
extern int  logic_touching2(void);            /* sub_1A9E8 */
extern int  logic_make_object(void);          /* sub_18736 */
extern int  logic_objects_frame(void);        /* sub_187EA */

#endif
