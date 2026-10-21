/* The game's work memory, by name. Per-object tables hold a word per object at
 * table + o (o = the object's number * 2; objects 0 and 1 are the players, 6..37 the
 * guards); long tables a long at table + o * 2. P2 versions of a player's word are at +2
 * unless named. */
#ifndef LOGIC_RAM_H
#define LOGIC_RAM_H

/* ---- objects */
#define OBJ_ACTIVE        0xFF2EF6   /* nonzero while the object is in play */
#define OBJ_ON_SCREEN     0xFF2FA2   /* bits: seen in player 1's view (1), player 2's (2) */
#define OBJ_X             0xFF304E
#define OBJ_Y             0xFF30FA
#define OBJ_HEIGHT        0xFF31A6   /* (z: how high it stands, on a box or the floor) */
#define OBJ_FRAME         0xFF3252   /* sprite frame */
#define OBJ_ANIM          0xFF33AA   /* the animation running */
#define OBJ_ANIM_STEPS    0xFF3456   /* (long) its steps: frame, time pairs */
#define OBJ_ANIM_AT       0xFF35AE   /* the step it is at (bytes into the steps) */
#define OBJ_ANIM_TIME     0xFF365A   /* frames left on this step */
#define OBJ_DX            0xFF385E   /* step per frame (x2) */
#define OBJ_DY            0xFF390A
#define OBJ_ATTR          0xFF3C66   /* sprite attribute (palette line: & 0x6000) */
#define OBJ_PASS          0xFF406E   /* shots pass through it */
/* points around an object where walls are looked for: (dx, dy) per object, by o * 2 */
#define OBJ_POINTS_A      0xFF411A
#define OBJ_POINTS_B      0xFF4272
#define OBJ_POINTS_C      0xFF43CA
#define OBJ_POINTS_D      0xFF4522
#define OBJ_POINTS_E      0xFF467A
/* a moving path (sub_016EC / sub_0172E): steps of dx, dy, frames (8 bytes each) */
#define OBJ_MOVE          0xFF4D32   /* (long) the path */
#define OBJ_MOVE_LEFT     0xFF4E8A   /* frames left on this step */
#define OBJ_MOVE_AT       0xFF4F36   /* the next step (bytes into the path) */

/* ---- objects 0x46..0x55 (o $8C..$AA): shots, bullets, thrown things, puffs... */
#define OBJS_FIRST        0x8C
#define OBJ_KIND          0xFF99F0   /* its kind (times 4): its start and frame routines */
#define OBJ_OWNER         0xFF9BF4   /* the object that made it */
#define OBJ_TIMER         0xFF9B48
#define OBJ_FLICKER       0xFF3BBA   /* (shots: every other frame) */
#define OBJ_DEFLECTED     0xFF9A9C   /* (shots: stopped by a shield, flying off a path) */
#define GUARD_CARRIES     0xFF8D78   /* (guards: the object over it) */
#define SHOT_TOGGLE       0xFF9E04   /* (the next shot's flicker phase) */
#define OBJ_STATE         0xFF9A9C   /* (thrown things: 0 flying, 1 breaking, 2 broken) */
#define THROWN_BOUNCES    0xFF9CA0   /* (thrown things: bounces left) */
#define OBJ_END_Y         0xFF513A   /* (kinds $40 / $44: the y where it goes) */
#define PUFF_X            0xFFA39C   /* (kind $48: where it starts, its animation, its path) */
#define PUFF_Y            0xFFA39E
#define PUFF_HEIGHT       0xFFA3A0
#define PUFF_ANIM         0xFFA3A2
#define PUFF_PATH         0xFFA3A4   /* (long) */
#define THROWN_WAY        0xFF9D4C   /* (thrown things: 0..3, its path, table 0x19212) */
#define OBJ_PICTURE       0xFF5292   /* (messages: the big picture sub_05DB0 draws) */
#define OBJ_DELAY         0xFF49D6   /* (messages: frames before it shows) */
#define OBJ_LEFT          0xFF4A82   /* (messages: frames it shows; kind $28: which player's) */
#define MSG_20            0xFF181A   /* the message objects up (o; 0 none): kind $20 */
#define MSG_24            0xFF9E06   /*   kind $24 */
#define MSG_28_P1         0xFF181E   /*   kind $28, over player 1's / 2's view */
#define MSG_28_P2         0xFF1820
#define MSG_NONE          0xFF1822   /* the "time up" message (kind $50; o): no more others */
#define ESCAPE            0xFF9E02   /* the way out at the stage end (kind $30, o): parts placed above it */
#define ESCAPE_STEP       0xFF4B2E   /* (kind $30: the next step of its moves, table 0x19EAE) */
#define ESCAPE_WAIT       0xFF4BDA   /* (kind $30: frames to the next state) */
#define ESCAPE_PARTS      0xFF4C86   /* (kind $30: frames before part $38 comes) */

/* ---- the players */
#define P1_PAD            0xFF17C0   /* (byte) the buttons held: U D L R (bits 0-3), B C A */
#define P1_PAD_PRESSED    0xFF17C1   /* (byte) the buttons pressed this frame */
#define BUTTONS_SWAPPED   0xFFC058   /* the options: the jump button (C) as B, not A */
#define P1_FACING         0xFF3A62   /* (byte; o table) 1 left, 3 right... */
#define P1_FACED          0xFF3B0E   /* (byte; o table) the way he faced before turning */
#define P1_ON_STAIRS      0xFF3A63   /* (byte; o table) bits 0-1: on stairs */
#define P1_ANIM_DONE      0xFF56D0   /* 3: his animation came round to its start */
#define P1_PROTECT_PIC    0xFF56C2   /* (his picture, for blinking) */
#define P1_CELL           0xFF56BA   /* the map cell's bits where he is */
#define P1_EDGE           0xFF56BC   /* (nonzero: at a floor's edge, the view follows) */
#define P1_DOOR           0xFF3D12   /* the door he touches (o; 0 none) ... */
#define P1_DOOR_BEFORE    0xFF3DBE   /* ... and the one he touched last frame */
#define P1_VIEW_X         0xFF1852   /* his view: where it is */
#define P1_VIEW_Y         0xFF1854
#define P1_VIEW_SPEED_X   0xFF186E
#define P1_VIEW_SPEED_Y   0xFF1870
#define P1_VIEW_WANT      0xFF1876   /* the ways it should scroll: bits up, down, left, right */
#define P1_VIEW_LOOK      0xFF2284   /* the way he faced when the view began to follow */
#define ANIMS_HELD        0xFF1806   /* nonzero: the players' animations stand still */
#define P1_FALLING        0xFF56A4   /* (moving on a path: going down now) */
#define P1_STAIRS_WAY     0xFF56A7   /* (byte; jumping: 1 down, 2 up the stairs he jumped onto) */
#define P1_PATH_NEXT      0xFF56A8   /* (long) where a path goes on: its next command ... */
#define P1_PATH_AFTER     0xFF56AC   /* (long) ... and after each step */
#define P1_JUMP_PAD       0xFF17FA   /* the buttons as the jump takes them */
#define P1_WAIT           0xFF567C   /* frames to wait (sub_0B472) ... */
#define P1_WAIT_THEN      0xFF567E   /* (long) ... then this routine runs his state */
#define P1_SCRIPT_STEP    0xFF56B8
#define P1_SCRIPT_AT      0xFF6C06   /* (long) (what the script started with: a6) */
#define P1_LIFT           0xFF6C0E   /* how high his feet are looked for (16; player 2's 12) */
#define P1_TOUCHING       0xFFA3AE   /* (nonzero: he touches something, on side P1_CAUGHT_SIDE) */
#define P1_SCRIPT_ON      0xFF56B2   /* nonzero: a little script moves him (sub_09ADE) ... */
#define P1_SCRIPT         0xFF56B4   /* (long) ... where it is */
#define P1_LEAVING        0xFF56E4   /* (nonzero: he is going out already) */
#define LOOT_ALL          0xFF1818   /* all the loot is taken: the way out is open */
#define P1_TAKEN_OVER     0xFF5682   /* nonzero: the routine at +2 (long) runs his frame */
#define P1_STATE          0xFF5688   /* the state (standing, walking, shot...) */
#define P1_STATE_RUN      0xFF568A   /* (long) the state's routine, run every frame */
#define P1_STATE_BEFORE   0xFF568E
#define P1_STATE_RUN_BEFORE 0xFF5690
#define P2_STATE          0xFF5732
#define P2_STATE_RUN      0xFF5734
#define P1_PROTECT        0xFF56BE   /* protected: 1 blinking, 2 lying / getting up */
#define P1_PROTECT_TIME   0xFF56C0   /* frames left */
#define P1_PROTECT_FRAME  0xFF56C2   /* his picture when it began */
#define P2_PROTECT        0xFF5768
#define P1_POSE_KEPT      0xFF56D2   /* a kept pose: animation, steps, step, time, picture */
#define P2_POSE_KEPT      0xFF577C
#define P1_PATH           0xFF569E   /* (long) the path being walked */
#define P2_PATH           0xFF5748
#define P1_SHOTS          0xFF9DF8   /* shots of his in flight */
#define P2_SHOTS          0xFF9DFA
#define P1_TAKE_AWAY      0xFF9DFE   /* an object to be taken away (o; 0 none) */
#define P2_TAKE_AWAY      0xFF9E00
#define P1_HELD_DX        0xFF56E6   /* what he holds up (that object, kinds $18 / $1C): */
#define P1_HELD_DY        0xFF56E8   /*   where, from him */
#define P1_HELD_ACTIVE    0xFF56EA   /*   its OBJ_ACTIVE */
#define P1_HELD_BIG       0xFF56EC   /*   nonzero: frame $3E, else $38 */
#define P2_HELD_DX        0xFF5790

/* ---- guards (objects 6..37) */
#define GUARD_COUNT       0xFF6CC2   /* the stage's guards - 1 */
#define GUARD_CLASS       0xFF6F24   /* (long) its kind: the script it runs when shot */
#define GUARD_EVENT       0xFF6D5C   /* the event waiting: 4 shot, 8 touched, $C a player seen */
#define GUARD_EVENT_MASK  0xFF6DA8   /* events it ignores now: bit event / 4 - 1 */
#define GUARD_SCRIPT      0xFF7774   /* (long) where its script is */
#define GUARD_WAIT        0xFF7858   /* frames its script waits */
#define GUARD_FACING      0xFF7644   /* 1 left, 3 right, 0 / 2 towards / away from the view */
#define GUARD_SCRIPT_SP   0xFF7CB0   /* (long) its script's call stack */
#define GUARD_STACKS      0xFF7D48   /* the stacks: 64 bytes each, from (o + 2) * 64 */
#define GUARD_LOOKS       0xFF9284   /* it looks for players */
#define GUARD_LOOK_COUNT  0xFF93B4   /* it looks every 4th frame */
#define GUARD_LOOK_WHICH  0xFF9400   /* ... at players 1 and 2 in turn */
#define LOOK_FOUND        0xFF6CC0   /* the player found by a look: 2 player 1, 4 player 2 */
#define LOOK_CELLS        0xFF6CBE   /* how many cells away he is */
#define GUARD_SEEN        0xFF931C   /* the player it has seen: 2 player 1, 4 player 2 */
#define GUARD_DOWN        0xFF8DC4   /* knocked down */
#define GUARD_POST_X      0xFF9108   /* where it stands guard */
#define GUARD_POST_Y      0xFF9154
#define GUARD_POST_HEIGHT 0xFF91A0
#define GUARD_POST_FACING 0xFF91EC
#define GUARD_POST_SCRIPT 0xFF8FD8   /* (long) */
#define GUARD_POST_SP     0xFF9070   /* (long) */
#define GUARD_PLACED      0xFF9238   /* put back at its post while out of view */
#define GUARD_WANT_FACING 0xFF92D0   /* the way it is to turn to (8: none) */
#define GUARD_TURN_SEQ    0xFF8E10   /* (long) the facings it turns through */
#define GUARD_TURN_TIME   0xFF8F40   /* frames left on this facing */
#define GUARD_TURN_STEP   0xFF8F8C   /* frames each facing lasts */
#define GUARD_FACING_ANIMS 0xFF8EA8  /* (long) its animations by facing: (animation, extra) */
#define GUARD_FACING_SHOWN 0xFF76DC  /* the facing its animation shows */
#define GUARD_PATH        0xFF47D2   /* (long) a path being followed: (dx, dy, frames)... */
#define GUARD_PATH_LEFT   0xFF492A   /* frames left on the path's step */
#define GUARD_FLAG_LESS   0xFF6CC4   /* the script's flags: a compare found less (or a
                                        player seen by op 18) ... */
#define GUARD_FLAG_EQUAL  0xFF6D10   /* ... and equal */
#define GUARD_ON_STAIRS   0xFF944C   /* climbing stairs: 1 up, 2 down */
#define GUARD_STOPPED     0xFF9498   /* why a run stopped: 1 a wall, 2 a step, 3 a player
                                        held, 4 a wall at its feet, 5 a player its way, 6
                                        stairs, 7 a wall beside it */
#define GUARD_DZ          0xFF39B6   /* height step per frame */
#define GUARD_STILL       0xFF9744   /* (no falling, no touching this frame) */
#define GUARD_FALLING     0xFF9790   /* falling: its steps from a list */
#define GUARD_FALL_DY     0xFF98C0   /* the step down per frame */
#define GUARD_FALL_LEFT   0xFF9874   /* frames left at that step */
#define GUARD_FALL_NEXT   0xFF97DC   /* (long) the next step in the list */
#define GUARD_99A4        0xFF99A4   /* (cleared every frame) */
#define TOUCHED_BY        0xFF3F16   /* the player touching it: 2 player 1, 4 player 2 */
#define TOUCH_SIDE        0xFF3FC2   /* ... on which side (1 left, 2 right) */
#define TOUCH_PLAYER      0xFF3D12   /* (players: the object touching them) */
#define TOUCH_WHO         0xFFA518   /* the player touching it: 0 player 1, 2 player 2 */
#define P1_TOUCH_SIDE     0xFFA3BC   /* the side a guard touched player 1 / 2 on */
#define P2_TOUCH_SIDE     0xFFA3BE
#define P1_CAUGHT         0xFFA3AE   /* player 1 / 2 caught by a guard */
#define P2_CAUGHT         0xFFA3B0
#define P1_CAUGHT_SIDE    0xFFA3B2
#define P2_CAUGHT_SIDE    0xFFA3B4
#define P1_PUSH           0xFF56EE   /* the way player 1 / 2 is pushed: -1 left, 1 right */
#define P2_PUSH           0xFF5798

/* ---- the stage's map (sub_0FF22 sets it up): the building in floors 80 pixels high; each
 * floor a row of cells 8 pixels wide, 4 bytes each: the walls met standing on the floor
 * (bits, by lane), the walls met standing on something (height 8 and up), and a word of
 * what is there (& $80 stairs, & $F their way, & $F00 the kind) */
#define MAP_FLOORS        0xFF6BD0   /* (longs) each floor's cells */
#define MAP_FLOOR_START   0xFF6BF0   /* (words) ... where its first cell is, x / 8 * 4 */
#define MAP_LANES         0xFF6C00   /* (long) the lanes: by depth (y / 8), two bytes: the
                                        lane's bit, and its stairs bits */
#define MAP_CELL_INFO     0xFF99A4   /* per object: the last cell's word it met */

/* ---- the game */
#define FRAME_COUNT       0xFF17A0
#define SOUND_REQUEST     0xFF5668   /* the sound the next frame sends to the sound driver */
#define SCREEN_WAIT       0xFF5546   /* a screen's wait, in frames */
#define SCREEN_RUN        0xFFC068   /* (long) the screen's routine, run every frame */
#define SCREEN_NEXT       0xFFC072   /* (long) the one after its wait */
#define STAGE_ENTRY       0xFFC05A
#define DIFFICULTY        0xFFC04E   /* 0 easy, 1 normal, 2 hard */
#define GAME_MODE         0xFFC070

#endif
