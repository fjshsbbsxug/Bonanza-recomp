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
extern int  logic_state_jump_p1(void);        /* sub_0B934 */
extern int  logic_state_jump_p2(void);        /* sub_0DE28 */
extern int  logic_jump_go_p1(void);           /* sub_0B970 */
extern int  logic_jump_go_p2(void);           /* sub_0DE5A */
extern int  logic_jump_frame_p1(void);        /* sub_0B9C8 */
extern int  logic_jump_frame_p2(void);        /* sub_0DEB2 */
extern int  logic_path_next_p1(void);         /* sub_0BA26 */
extern int  logic_path_next_p2(void);         /* sub_0DF10 */
extern int  logic_jump_after_p1(void);        /* sub_0BA3C */
extern int  logic_jump_after_p2(void);        /* sub_0DF26 */
extern int  logic_path_step_p1(void);         /* sub_0BC52 */
extern int  logic_path_step_p2(void);         /* sub_0E13C */
extern int  logic_path_switch_p1(void);       /* sub_0BE9A */
extern int  logic_path_switch_p2(void);       /* sub_0E31C */
extern int  logic_player_anim_set_p1(void);   /* sub_0BEB2 */
extern int  logic_player_anim_set_p2(void);   /* sub_0E334 */
extern int  logic_state_against_p1(void);     /* sub_0BFBE */
extern int  logic_state_against_p2(void);     /* sub_0E440 */
extern int  logic_against_go_p1(void);        /* sub_0BFE6 */
extern int  logic_against_go_p2(void);        /* sub_0E468 */
extern int  logic_against_frame_p1(void);     /* sub_0C01A */
extern int  logic_against_frame_p2(void);     /* sub_0E49C */
extern int  logic_state_fire_p1(void);        /* sub_0C1FE */
extern int  logic_state_fire_p2(void);        /* sub_0E67E */
extern int  logic_fire_go_p1(void);           /* sub_0C220 */
extern int  logic_fire_go_p2(void);           /* sub_0E6A0 */
extern int  logic_fire_frame_p1(void);        /* sub_0C24E */
extern int  logic_fire_frame_p2(void);        /* sub_0E6CE */
extern int  logic_state_trip_p1(void);        /* sub_0C260 */
extern int  logic_state_trip_p2(void);        /* sub_0E6E0 */
extern int  logic_trip_go_p1(void);           /* sub_0C292 */
extern int  logic_trip_go_p2(void);           /* sub_0E712 */
extern int  logic_trip_frame_p1(void);        /* sub_0C2D4 */
extern int  logic_trip_frame_p2(void);        /* sub_0E754 */
extern int  logic_trip_up_p1(void);           /* sub_0C366 */
extern int  logic_trip_up_p2(void);           /* sub_0E7DE */
extern int  logic_state_6_p1(void);           /* sub_0C3BA */
extern int  logic_state_6_p2(void);           /* sub_0E830 */
extern int  logic_state_6_go_p1(void);        /* sub_0C3DC */
extern int  logic_state_6_go_p2(void);        /* sub_0E852 */
extern int  logic_state_door_p1(void);        /* sub_0C404 */
extern int  logic_state_door_p2(void);        /* sub_0E87A */
extern int  logic_door_go_p1(void);           /* sub_0C45C */
extern int  logic_door_go_p2(void);           /* sub_0E8D4 */
extern int  logic_door_wait_p1(void);         /* sub_0C49E */
extern int  logic_door_wait_p2(void);         /* sub_0E916 */
extern int  logic_door_end_p1(void);          /* sub_0C4D4 */
extern int  logic_door_end_p2(void);          /* sub_0E94C */
extern int  logic_state_loot_p1(void);        /* sub_0C4E8 */
extern int  logic_state_loot_p2(void);        /* sub_0E960 */
extern int  logic_loot_go_p1(void);           /* sub_0C522 */
extern int  logic_loot_go_p2(void);           /* sub_0E99A */
extern int  logic_loot_frame_p1(void);        /* sub_0C55A */
extern int  logic_loot_frame_p2(void);        /* sub_0E9D2 */
extern int  logic_state_fall_p1(void);        /* sub_0C56C */
extern int  logic_state_fall_p2(void);        /* sub_0E9E4 */
extern int  logic_fall_frame_p1(void);        /* sub_0C5BA */
extern int  logic_fall_frame_p2(void);        /* sub_0EA32 */
extern int  logic_state_land_p1(void);        /* sub_0C87C */
extern int  logic_state_land_p2(void);        /* sub_0ECB4 */
extern int  logic_land_frame_p1(void);        /* sub_0C8B4 */
extern int  logic_land_frame_p2(void);        /* sub_0ECEC */
extern int  logic_state_a_p1(void);           /* sub_0C910 */
extern int  logic_state_a_p2(void);           /* sub_0ED40 */
extern int  logic_state_a_go_p1(void);        /* sub_0C932 */
extern int  logic_state_a_go_p2(void);        /* sub_0ED62 */
extern int  logic_climb_p1(void);             /* sub_0C952 */
extern int  logic_climb_p2(void);             /* sub_0ED82 */
extern int  logic_climb_go_p1(void);          /* sub_0C97A */
extern int  logic_climb_go_p2(void);          /* sub_0EDAA */
extern int  logic_climb_frame_p1(void);       /* sub_0C9B2 */
extern int  logic_climb_frame_p2(void);       /* sub_0EDE2 */
extern int  logic_state_hit_p1(void);         /* sub_0C9F4 */
extern int  logic_state_hit_p2(void);         /* sub_0EE26 */
extern int  logic_hit_go_p1(void);            /* sub_0CA82 */
extern int  logic_hit_go_p2(void);            /* sub_0EEB4 */
extern int  logic_hit_frame_p1(void);         /* sub_0CB1C */
extern int  logic_hit_frame_p2(void);         /* sub_0EF4E */
extern int  logic_hit_script_p1(void);        /* sub_0CB94 */
extern int  logic_hit_script_p2(void);        /* sub_0EFC6 */
extern int  logic_hit_down_p1(void);          /* sub_0CBE2 */
extern int  logic_hit_down_p2(void);          /* sub_0F014 */
extern int  logic_state_hide_p1(void);        /* sub_0CC86 */
extern int  logic_state_hide_p2(void);        /* sub_0F0A0 */
extern int  logic_hide_go_p1(void);           /* sub_0CCB4 */
extern int  logic_hide_go_p2(void);           /* sub_0F0CE */
extern int  logic_hide_wait_p1(void);         /* sub_0CD0E */
extern int  logic_hide_wait_p2(void);         /* sub_0F128 */
extern int  logic_hide_end_p1(void);          /* sub_0CD46 */
extern int  logic_hide_end_p2(void);          /* sub_0F160 */
extern int  logic_hide_out_p1(void);          /* sub_0CD62 */
extern int  logic_hide_out_p2(void);          /* sub_0F17C */
extern int  logic_state_open_p1(void);        /* sub_0CD9A */
extern int  logic_state_open_p2(void);        /* sub_0F1B4 */
extern int  logic_open_go_p1(void);           /* sub_0CDC2 */
extern int  logic_open_go_p2(void);           /* sub_0F1DC */
extern int  logic_open_frame_p1(void);        /* sub_0CE0E */
extern int  logic_open_frame_p2(void);        /* sub_0F228 */
extern int  logic_state_flat_p1(void);        /* sub_0CE20 */
extern int  logic_state_flat_p2(void);        /* sub_0F23A */
extern int  logic_flat_go_p1(void);           /* sub_0CE50 */
extern int  logic_flat_go_p2(void);           /* sub_0F26A */
extern int  logic_flat_frame_p1(void);        /* sub_0CE8E */
extern int  logic_flat_frame_p2(void);        /* sub_0F2A8 */
extern int  logic_state_carry_p1(void);       /* sub_0CEF0 */
extern int  logic_state_carry_p2(void);       /* sub_0F30A */
extern int  logic_carry_go_p1(void);          /* sub_0CF12 */
extern int  logic_carry_go_p2(void);          /* sub_0F32C */
extern int  logic_carry_frame_p1(void);       /* sub_0CF40 */
extern int  logic_carry_frame_p2(void);       /* sub_0F35A */
extern int  logic_carry_drop_p1(void);        /* sub_0CFA4 */
extern int  logic_carry_drop_p2(void);        /* sub_0F3BE */
extern int  logic_carry_used_p1(void);        /* sub_0CFD4 */
extern int  logic_carry_used_p2(void);        /* sub_0F3EE */
extern int  logic_state_drop_p1(void);        /* sub_0CFF4 */
extern int  logic_state_drop_p2(void);        /* sub_0F40E */
extern int  logic_drop_go_p1(void);           /* sub_0D016 */
extern int  logic_drop_go_p2(void);           /* sub_0F430 */
extern int  logic_drop_next_p1(void);         /* sub_0D07A */
extern int  logic_drop_next_p2(void);         /* sub_0F494 */
extern int  logic_drop_after_p1(void);        /* sub_0D090 */
extern int  logic_drop_after_p2(void);        /* sub_0F4AA */
extern int  logic_state_off_edge_p1(void);    /* sub_0D246 */
extern int  logic_state_off_edge_p2(void);    /* sub_0F660 */
extern int  logic_off_edge_go_p1(void);       /* sub_0D26E */
extern int  logic_off_edge_go_p2(void);       /* sub_0F688 */
extern int  logic_off_edge_frame_p1(void);    /* sub_0D2B4 */
extern int  logic_off_edge_frame_p2(void);    /* sub_0F6CE */
extern int  logic_state_out_p1(void);         /* sub_0D342 */
extern int  logic_state_out_p2(void);         /* sub_0F75C */
extern int  logic_state_exit_p1(void);        /* sub_0D3A0 */
extern int  logic_state_exit_p2(void);        /* sub_0F7BA */
extern int  logic_exit_go_p1(void);           /* sub_0D3C8 */
extern int  logic_exit_go_p2(void);           /* sub_0F7E2 */
extern int  logic_exit_frame_p1(void);        /* sub_0D3FC */
extern int  logic_exit_frame_p2(void);        /* sub_0F816 */
extern int  logic_exit_wave_p1(void);         /* sub_0D47E */
extern int  logic_exit_wave_p2(void);         /* sub_0F898 */
extern int  logic_exit_back_p1(void);         /* sub_0D4F4 */
extern int  logic_exit_back_p2(void);         /* sub_0F90E */
extern int  logic_state_18_p1(void);          /* sub_0D524 */
extern int  logic_state_18_p2(void);          /* sub_0F93E */
extern int  logic_state_18_go_p1(void);       /* sub_0D558 */
extern int  logic_state_18_go_p2(void);       /* sub_0F972 */
extern int  logic_state_18_frame_p1(void);    /* sub_0D59E */
extern int  logic_state_18_frame_p2(void);    /* sub_0F9B8 */
extern int  logic_state_stairs_on_p1(void);   /* sub_0D5B0 */
extern int  logic_state_stairs_on_p2(void);   /* sub_0F9CA */
extern int  logic_state_stairs_p1(void);      /* sub_0D5B4 */
extern int  logic_state_stairs_p2(void);      /* sub_0F9CE */
extern int  logic_stairs_go_p1(void);         /* sub_0D5FE */
extern int  logic_stairs_go_p2(void);         /* sub_0FA18 */
extern int  logic_stairs_frame_p1(void);      /* sub_0D654 */
extern int  logic_stairs_frame_p2(void);      /* sub_0FA6E */
extern int  logic_player_request_p1(void);    /* sub_09846 */
extern int  logic_player_request_p2(void);    /* sub_09894 */
extern int  logic_player_walk_ask_p1(void);   /* sub_0AF32 */
extern int  logic_player_walk_ask_p2(void);   /* sub_0AF58 */
extern int  logic_player_no_stairs_p1(void);  /* sub_0AFE8 */
extern int  logic_player_no_stairs_p2(void);  /* sub_0B050 */
extern int  logic_player_side_blocked_p1(void);/* sub_0B0B8 */
extern int  logic_player_side_blocked_p2(void);/* sub_0B17C */
extern int  logic_player_points_standing_p1(void);/* sub_0B23A */
extern int  logic_player_points_standing_p2(void);/* sub_0B2BC */
extern int  logic_player_points_lying_p1(void);/* sub_0B33E */
extern int  logic_player_points_lying_p2(void);/* sub_0B3C0 */
extern int  logic_player_recover_p1(void);    /* sub_0AEC2 */
extern int  logic_player_recover_p2(void);    /* sub_0AEFA */
extern int  logic_player_drop_loot_p1(void);  /* sub_0ABF6 */
extern int  logic_player_drop_loot_p2(void);  /* sub_0ACE4 */
extern int  logic_loot_back(void);            /* sub_0ADD0 */
extern int  logic_tell_guards(void);          /* sub_0AF7E */
extern int  logic_player_point_p1(void);      /* sub_0B442 */
extern int  logic_player_point_p2(void);      /* sub_0B45A */
extern int  logic_path_move_free_p1(void);    /* sub_0A8EA */
extern int  logic_path_move_free_p2(void);    /* sub_0A942 */
extern int  logic_path_move_p1(void);         /* sub_0A56A */
extern int  logic_path_move_p2(void);         /* sub_0A72A */
extern int  logic_script_fall_p1(void);       /* sub_09B32 */
extern int  logic_script_fall_p2(void);       /* sub_09BE6 */
extern int  logic_script_goto_p1(void);       /* sub_09C9A */
extern int  logic_script_goto_p2(void);       /* sub_09CAA */
extern int  logic_player_head_room_p1(void);  /* sub_0D7E4 */
extern int  logic_player_head_room_p2(void);  /* sub_0FBFE */
/* the objects' sprites (logic/sprites.c) */
extern int  logic_sprites_frame(void);        /* sub_0582E */
extern int  logic_object_near_views(void);    /* sub_058B4 */
extern int  logic_object_sprites(void);       /* sub_059C6 */
extern int  logic_sprites_merge(void);        /* sub_05C98 */
/* the time and the map's marks (logic/hud.c) */
extern int  logic_hud_frame(void);            /* sub_105CC */
/* the items (logic/items.c) */
extern int  logic_items_frame(void);          /* sub_1AFDA */
extern int  logic_pad_map(void);              /* sub_008AA (logic/screens.c) */
extern int  logic_player_door_across_p1(void);/* sub_0AB68 */
extern int  logic_player_door_across_p2(void);/* sub_0ABAA */
extern int  logic_loot_reset(void);           /* sub_0AE02 */
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
