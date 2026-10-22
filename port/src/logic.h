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
/* the views (logic/views.c) */
extern int  logic_view_tasks(void);           /* sub_080FA */
extern int  logic_views_frame(void);          /* sub_08576, to its wait */
extern int  logic_views_frame2(void);         /* sub_08576 from 0x087A4 */
extern int  logic_column_jobs(void);          /* sub_017D8 */
/* the frame during play (logic/play.c) */
extern int  logic_play_frame(void);           /* sub_05124, to its views' part */
extern int  logic_play_frame2(void);          /* sub_05124 from 0x05312 */
extern int  logic_lane_walls(void);           /* sub_1019E (logic/map.c) */
extern int  logic_lane_walls_low(void);       /* sub_10224 */
extern int  logic_lane_walls_faced(void);     /* sub_102A6 */
extern int  logic_lane_walls_p2(void);        /* sub_101C8 */
extern int  logic_lane_walls_low_p2(void);    /* sub_1024C */
extern int  logic_lane_walls_faced_p2(void);  /* sub_102E4 */
extern int  logic_guard_touching(void);       /* sub_1A764 */
extern int  logic_door_touching(void);        /* sub_1AD04 */
extern int  logic_sprite_add(void);           /* sub_12E64 (logic/sprites.c) */
extern int  logic_blink_sprite(void);         /* sub_13CEA */
extern int  logic_object_picture(void);       /* sub_05D88 */
extern int  logic_wall_fx(void);              /* sub_13F18 (logic/play.c) */
/* the palette's fades (logic/palette.c) */
extern int  logic_fade_up(void);              /* sub_02B62 */
extern int  logic_fade_down(void);            /* sub_02BD4 */
extern int  logic_fade_white(void);           /* sub_02C06 */
extern int  logic_palette_copy(void);         /* sub_02C52 */
extern int  logic_fade_screen_up(void);       /* sub_02AEA */
extern int  logic_fade_screen_down(void);     /* sub_02BB4 */
extern int  logic_fade_job_up(void);          /* sub_014B6 */
extern int  logic_fade_job_down(void);        /* sub_014F0 */
extern int  logic_fade_job_white(void);       /* sub_01520 */
extern int  logic_palette_preset(void);       /* sub_018FC */
extern int  logic_palette_cycles(void);       /* sub_0FD90 */
extern int  logic_palette_cycle(void);        /* sub_0FDE8 */
extern int  logic_add_points(void);           /* sub_11D6A (logic/hud.c) */
extern int  logic_add_points_p1(void);        /* sub_11D88 */
extern int  logic_add_points_p2(void);        /* sub_11E18 */
extern int  logic_wall_fx_start_p1(void);     /* sub_13E00 (logic/play.c) */
extern int  logic_wall_fx_start_p2(void);     /* sub_13E8C */
extern int  logic_column_job_add(void);       /* sub_017AC (logic/views.c) */
/* the views put back when a player goes out, each stage's (table 0x4566 / 0x459A) */
extern int  logic_view_reset0_p1(void);
extern int  logic_view_reset0_p2(void);
extern int  logic_view_reset1_p1(void);
extern int  logic_view_reset1_p2(void);
extern int  logic_view_reset2_p1(void);
extern int  logic_view_reset2_p2(void);
extern int  logic_view_reset3_p1(void);
extern int  logic_view_reset3_p2(void);
extern int  logic_view_reset4_p1(void);
extern int  logic_view_reset4_p2(void);
extern int  logic_view_reset5_p1(void);
extern int  logic_view_reset5_p2(void);
extern int  logic_view_reset6_p1(void);
extern int  logic_view_reset6_p2(void);
extern int  logic_view_reset7_p1(void);
extern int  logic_view_reset7_p2(void);
extern int  logic_view_reset8_p1(void);
extern int  logic_view_reset8_p2(void);
extern int  logic_view_reset9_p1(void);
extern int  logic_view_reset9_p2(void);
extern int  logic_demo_frame(void);           /* sub_0427E (logic/play.c) */
extern int  logic_stage_objects(void);        /* sub_0156C (logic/stage.c) */
extern int  logic_place_object(void);         /* sub_015F2 */
extern int  logic_stage_graphics(void);       /* sub_04484, to its load list */
extern int  logic_stage_graphics2(void);      /* sub_04484 from 0x04498 */
extern int  logic_blink_sprite2(void);        /* sub_139C6 */
extern int  logic_slot_add2(void);            /* sub_132F2 */
extern int  logic_messages_clear(void);       /* sub_187CE */
extern int  logic_text_job(void);             /* sub_01318 */
extern int  logic_fade_job_add(void);         /* sub_0141A */
extern int  logic_palettes_clear(void);       /* sub_01928 */
extern int  logic_floor_line(void);           /* sub_1798C */
extern int  logic_players_back(void);         /* sub_1A67E */
/* the screens around the game (logic/menu.c) */
extern int  logic_screen_mode(void);             /* sub_02E06 */
extern int  logic_logo_wait(void);               /* sub_02F4A */
extern int  logic_logo_fade_up(void);            /* sub_02F7A */
extern int  logic_logo_fade_out(void);           /* sub_02FC0 */
extern int  logic_logo_shine(void);              /* sub_03050 */
extern int  logic_logo_frame(void);              /* sub_03012 */
extern int  logic_title_colors(void);            /* sub_03622 */
extern int  logic_title_runner(void);            /* sub_03648 */
extern int  logic_title_start(void);             /* sub_03484 */
extern int  logic_title_fade_up(void);           /* sub_034BA */
extern int  logic_title_scroll(void);            /* sub_034E6 */
extern int  logic_title_push_start(void);        /* sub_0353A */
extern int  logic_title_fade_out(void);          /* sub_035A0 */
extern int  logic_menu_start(void);              /* sub_0372C */
extern int  logic_menu_frame(void);              /* sub_037C0 */
extern int  logic_menu_choose(void);             /* sub_037DE */
extern int  logic_menu_fade_out(void);           /* sub_0392C */
extern int  logic_options_fade_up(void);         /* sub_03BA6 */
extern int  logic_options_frame(void);           /* sub_03BCC */
extern int  logic_options_line(void);            /* sub_03CA0 */
extern int  logic_option_level(void);            /* sub_03CC8 */
extern int  logic_option_lives(void);            /* sub_03CF6 */
extern int  logic_option_sound(void);            /* sub_03D22 */
extern int  logic_option_control(void);          /* sub_03DD2 */
extern int  logic_option_exit(void);             /* sub_03E1E */
extern int  logic_option_stage(void);            /* sub_03E52 */
extern int  logic_options_draw_line(void);       /* sub_03E7A */
extern int  logic_options_draw_control(void);    /* sub_03F14 */
extern int  logic_options_draw_exit(void);       /* sub_03F3C */
extern int  logic_options_draw_stage(void);      /* sub_03F48 */
extern int  logic_options_fade_out(void);        /* sub_03F6A */
extern int  logic_demo_setup(void);              /* sub_041C4 */
extern int  logic_demo_setup2(void);             /* sub_041DA */
extern int  logic_stage_go(void);                /* sub_0442A */
extern int  logic_demo_go(void);                 /* sub_04200 */
extern int  logic_row_tiles(void);               /* sub_284E2 (logic/views.c) */
extern int  logic_row_tiles_p2(void);            /* sub_287D2 (logic/views.c) */
extern int  logic_row_request(void);             /* sub_285A6 (logic/views.c) */
extern int  logic_row_request_p2(void);          /* sub_28896 (logic/views.c) */
extern int  logic_split_copy(void);              /* sub_08B08 (logic/views.c) */
extern int  logic_split_copy_top(void);          /* sub_08B28 (logic/views.c) */
extern int  logic_split_copy_p2(void);           /* sub_08D62 (logic/views.c) */
extern int  logic_split_copy_top_p2(void);       /* sub_08D82 (logic/views.c) */
extern int  logic_column_right(void);            /* sub_28624 (logic/views.c) */
extern int  logic_column_left(void);             /* sub_286E2 (logic/views.c) */
extern int  logic_column_right_p2(void);         /* sub_28914 (logic/views.c) */
extern int  logic_column_left_p2(void);          /* sub_289D2 (logic/views.c) */
extern int  logic_item_touching(void);           /* sub_1AD5E (logic/items.c) */
extern int  logic_item_effect(void);             /* sub_1A108 (logic/items.c) */
extern int  logic_loot_sparkle(void);            /* sub_1B502 (logic/items.c) */
extern int  logic_text_number(void);             /* sub_01550 (logic/screens.c) */
extern int  logic_column_jobs_clear(void);       /* sub_0181A (logic/screens.c) */
extern int  logic_jobs_clear(void);              /* sub_013FE (logic/screens.c) */
extern int  logic_sprites_end(void);             /* sub_125EC (logic/sprites.c) */
extern int  logic_points_redraw(void);           /* sub_11D36 (logic/hud.c) */
extern int  logic_points_redraw_p2(void);        /* sub_11D50 (logic/hud.c) */
extern int  logic_players_on(void);              /* sub_1A8F2 (logic/map.c) */
extern int  logic_guard_clear(void);             /* sub_179B0 (logic/guards.c) */
extern int  logic_picture_draw(void);            /* sub_05DB0 (logic/sprites.c) */
extern int  logic_z80_reset(void);               /* sub_02D3A (logic/sound.c) */
extern int  logic_sound_now(void);               /* sub_02D76 (logic/sound.c) */
extern int  logic_sound_play(void);              /* sub_02D98 (logic/sound.c) */
extern int  logic_sound_send(void);              /* sub_02DC2 (logic/sound.c) */
extern int  logic_pads_init(void);               /* sub_007A8 (logic/screens.c) */
extern int  logic_pad_detect(void);              /* sub_0093C (logic/screens.c) */
extern int  logic_vblank_lines1(void);           /* sub_00AA2 (logic/screens.c) */
extern int  logic_vblank_lines2(void);           /* sub_00AB4 (logic/screens.c) */
extern int  logic_vblank_columns(void);          /* sub_00ABC (logic/screens.c) */
extern int  logic_hud_fixed(void);               /* sub_10912 (logic/hud.c) */
extern int  logic_games_over(void);              /* sub_05496 (logic/play.c) */
extern int  logic_player_start(void);            /* sub_08E74 (logic/play.c) */
extern int  logic_player_start_p2(void);         /* sub_08F26 (logic/play.c) */
extern int  logic_guards_setup(void);            /* sub_141F8 (logic/guards.c) */
extern int  logic_guards_place(void);            /* sub_147FA (logic/guards.c) */
extern int  logic_stage_setup(void);             /* sub_0430A (logic/stage.c) */
extern int  logic_stage_setup2(void);            /* sub_04322 (logic/stage.c) */
extern int  logic_boot_options(void);            /* (at 0x00384, logic/screens.c) */
extern int  logic_boot_video(void);              /* (at 0x003BA, logic/screens.c) */
extern int  logic_boot_pads(void);               /* (at 0x003F2, logic/screens.c) */
extern int  logic_stage_views1(void);            /* sub_04602 (logic/views.c) */
extern int  logic_stage_views2(void);            /* sub_04710 (logic/views.c) */
extern int  logic_stage_views3(void);            /* sub_0481E (logic/views.c) */
extern int  logic_stage_views5(void);            /* sub_0492E (logic/views.c) */
extern int  logic_stage_views6(void);            /* sub_04A64 (logic/views.c) */
extern int  logic_stage_views7(void);            /* sub_04B72 (logic/views.c) */
extern int  logic_stage_views9(void);            /* sub_04C82 (logic/views.c) */
extern int  logic_stage_views10(void);           /* sub_04D90 (logic/views.c) */
extern int  logic_stage_views11(void);           /* sub_04EA6 (logic/views.c) */
extern int  logic_stage_views13(void);           /* sub_04FB6 (logic/views.c) */
extern int  logic_options_end(void);             /* sub_03F8A (logic/menu.c) */
extern int  logic_vblank_pads(void);             /* sub_00A22 (logic/screens.c) */
extern int  logic_vblank_screen(void);           /* sub_00BE8 (logic/screens.c) */
extern int  logic_vblank_end(void);              /* sub_00BF0 (logic/screens.c) */
extern int  logic_work_clear(void);              /* sub_00470 (logic/screens.c) */
extern int  logic_sound_start(void);             /* sub_02C76 (logic/sound.c) */
/* the players' panels (logic/panel.c) */
extern int  logic_panel_step(void);              /* sub_12F02 */
extern int  logic_panel_step_p2(void);           /* sub_13354 */
extern int  logic_over_check(void);              /* sub_137A6 */
extern int  logic_over_check_p2(void);           /* sub_13ADC */
extern int  logic_over_check_after(void);        /* (at 0x13862) */
extern int  logic_over_check_after_p2(void);     /* (at 0x13B98) */
/* the attract mode's opening (logic/intro.c) */
extern int  logic_intro_frame(void);             /* sub_122FE */
extern int  logic_intro_walk(void);              /* sub_123A4 */
extern int  logic_intro_town(void);              /* sub_128C4 */
extern int  logic_intro_van(void);               /* sub_12A3A */
extern int  logic_intro_alone(void);             /* sub_12BAE */
extern int  logic_intro_rise(void);              /* sub_12D8A */
extern int  logic_intro_wait(void);              /* sub_12DC0 */
extern int  logic_intro_end(void);               /* sub_12DE0 */
extern int  logic_intro_scroll0(void);           /* sub_12E9C */
extern int  logic_intro_scroll1f8(void);         /* sub_12EBA */
extern int  logic_intro_scroll100(void);         /* sub_12ED8 */
/* the screen before a stage (logic/stagein.c) */
extern int  logic_stagein_building(void);        /* sub_11460 */
extern int  logic_stagein_targets(void);         /* sub_1180C */
extern int  logic_stagein_count(void);           /* sub_11842 */
extern int  logic_stagein_wait(void);            /* sub_118C4 */
extern int  logic_stagein_fade(void);            /* sub_11946 */
extern int  logic_stagein_choose(void);          /* sub_11106 */
extern int  logic_stagein_setup(void);           /* sub_11120 */
extern int  logic_stagein_targets_setup(void);   /* sub_11476 */
extern int  logic_coffee_frame(void);            /* sub_24938 */
extern int  logic_coffee_setup(void);            /* sub_24536 */
extern int  logic_coffee_setup2(void);           /* sub_2454E */
extern int  logic_intro_town_setup(void);        /* sub_12614 */
extern int  logic_intro_rise_setup(void);        /* sub_12C50 */
extern int  logic_intro_setup(void);             /* sub_120E4 (logic/intro.c) */
extern int  logic_intro_setup2(void);            /* (at 0x121D2) */
extern int  logic_coffee_tiles4(void);           /* sub_246B6 (logic/stagein.c) */
extern int  logic_coffee_tiles8(void);           /* sub_24720 (logic/stagein.c) */
extern int  logic_coffee_tiles12(void);          /* sub_2478A (logic/stagein.c) */
/* the story cutscene (logic/cutscene.c) */
extern int  logic_cutscene_setup(void);          /* sub_23948 */
extern int  logic_cutscene_setup0(void);         /* sub_23960 */
extern int  logic_cutscene_frame(void);          /* sub_23AB6 */
extern int  logic_cutscene_wait(void);           /* sub_23B28 */
extern int  logic_cutscene_fade_up(void);        /* sub_23C0C */
extern int  logic_cutscene_scroll(void);         /* sub_23C38 */
extern int  logic_cutscene_fade_out(void);       /* sub_23C7A */
extern int  logic_cutscene_scene(void);          /* sub_23D80 */
extern int  logic_cutscene_scene_draw(void);     /* (sub_23D80 at 0x23D96) */
extern int  logic_cutscene_script(void);         /* sub_23E74 */
extern int  logic_cutscene_scene_rest(void);       /* (sub_23D80 at 0x23D90) */
extern int  logic_cutscene_script_after(void);     /* (sub_23E74 at 0x23E98) */
extern int  logic_cutscene_end(void);            /* sub_23EC0 */
extern int  logic_cutscene_setup1(void);         /* (at 0x23BB6) */
extern int  logic_cutscene_setup2a(void);        /* (at 0x23D32) */
extern int  logic_cutscene_setup2(void);         /* (at 0x23D60) */
extern int  logic_script_rect1(void);            /* sub_3BC9A */
extern int  logic_script_rect2(void);            /* sub_3BCB0 */
extern int  logic_script_rect3(void);            /* sub_3BCC6 */
extern int  logic_script_rect4(void);            /* sub_3BCDC */
extern int  logic_script_rect5(void);            /* sub_3BCF2 */
extern int  logic_script_rect6(void);            /* sub_3BD08 */
extern int  logic_script_rect7(void);            /* sub_3BD1E */
extern int  logic_script_rect8(void);            /* sub_3BD34 */
extern int  logic_script_title(void);            /* sub_3BC56 */
extern int  logic_script_last(void);             /* sub_3C0A2 */
extern int  logic_script_scroll_go(void);        /* sub_3BC4C */
extern int  logic_script_scroll_stop(void);      /* sub_3BDBE */
extern int  logic_script_scroll_stop2(void);     /* sub_3C0C0 */
extern int  logic_script_figures(void);          /* sub_3BDC8 */
extern int  logic_script_none(void);             /* sub_3BBDE */
extern int  logic_pad_mapping(void);             /* sub_03DFA (logic/menu.c) */
extern int  logic_logo_setup(void);              /* sub_02E34 (logic/menu.c) */
extern int  logic_logo_setup2(void);             /* sub_02E4C (logic/menu.c) */
extern int  logic_logo_setup3(void);             /* sub_02EBC (logic/menu.c) */
extern int  logic_title_setup(void);             /* sub_0318A (logic/menu.c) */
extern int  logic_title_setup2(void);            /* (at 0x032EE) */
extern int  logic_title_end(void);               /* sub_035BE (logic/menu.c) */
extern int  logic_menu_go(void);                 /* sub_0394A (logic/menu.c) */
extern int  logic_options_setup(void);           /* sub_03A78 (logic/menu.c) */
extern int  logic_options_draw_level(void);      /* sub_03EAA */
extern int  logic_map_setup(void);            /* sub_0FF22 (logic/map.c) */
extern int  logic_map_expand(void);           /* sub_1009A */
extern int  logic_items_setup(void);          /* sub_1AD74 (logic/items.c) */
extern int  logic_doors_reset(void);          /* sub_1B80E */
extern int  logic_press_crushes_guards(void); /* sub_1A7D8 */
extern int  logic_pads(void);                 /* sub_007BE (logic/screens.c) */
extern int  logic_pads_read(void);            /* sub_007EE */
extern int  logic_pad_read(void);             /* sub_0087A */
extern int  logic_slot_add(void);             /* sub_13744 (logic/screens.c) */
extern int  logic_jobs(void);                 /* sub_01468 */
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
