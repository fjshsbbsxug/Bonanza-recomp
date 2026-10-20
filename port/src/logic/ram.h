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

/* ---- the players */
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
#define GUARD_SEEN        0xFF931C   /* the player it has seen: 2 player 1, 4 player 2 */
#define GUARD_DOWN        0xFF8DC4   /* knocked down */
#define GUARD_POST_X      0xFF9108   /* where it stands guard */
#define GUARD_POST_Y      0xFF9154
#define GUARD_POST_HEIGHT 0xFF91A0
#define GUARD_POST_FACING 0xFF91EC
#define GUARD_POST_SCRIPT 0xFF8FD8   /* (long) */
#define GUARD_POST_SP     0xFF9070   /* (long) */
#define GUARD_PLACED      0xFF9238   /* put back at its post while out of view */
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
