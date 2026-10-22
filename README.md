# Bonanza Bros. (Mega Drive) disassembly

Tools that make a matching disassembly of **Bonanza Bros.** for the Sega Mega Drive /
Genesis (`GM G-4046 -01`, regions JUE) from your own ROM, and a native port built from
it. Running `make` disassembles the ROM and reassembles the source into a ROM that is
byte-identical to the original. **Nothing of the game is in this repository:** the
disassembly, the game's code translated to C, its graphics and its sounds are all made
from the player's ROM.

| | |
|---|---|
| ROM | `BONANZA BROS.` (C)SEGA 1991.JUL, 512 KiB |
| SHA-1 | `f7313ecaf7143873597e3a6ebf58d542d06c3ef3` |
| MD5 | `acf4aa6aea84be72afaa1a86954b85cb` |

Mega Drive games of this era were written directly in 68000 assembly, so there is
no original C to recover. This project is therefore a labelled, re-assemblable
disassembly. That's the usual base for documenting, modding, or porting a Mega
Drive game.

## Native port and HTML builder

Besides the matching disassembly, the repository contains a **native port**. The game's
68000 code is statically recompiled to C, and it runs on a small reimplementation of the
Mega Drive hardware. It builds for **Windows, Linux, Android and the web**.

**Easiest way to get a port: a builder page.** The `Port` GitHub Actions workflow makes
one page for each platform, published on the Releases page (release `builder`):
`BonanzaBuilder-Windows.html`, `BonanzaBuilder-Linux.html`, `BonanzaBuilder-Android.html`.
Open the one for your platform in a browser (straight from disk, offline), drop in your
ROM, and press *Build*:

| Page | What you get |
|---|---|
| Windows | `BonanzaBros-Windows.zip`: `bonanza.exe`, the game's code (`bonanza_game.dll`) and your ROM, ready to run |
| Linux | `BonanzaBros-Linux.zip`: `bonanza` (x86-64), `libbonanza_game.so` and your ROM |
| Android | `BonanzaBros.apk` (Android 7.0 and later; arm64-v8a, armeabi-v7a, x86_64): the app asks for the ROM once |

A page carries the platform's **engine**, which holds no game code, and makes the
game's code from your ROM in the browser, in about ten seconds, and records its sounds:

1. the project's recompiler (`tools/recomp.py`, `tools/z80recomp.py`) runs in Python
   compiled to WebAssembly ([Pyodide](https://pyodide.org)) and translates the game's
   68000 and Z80 code from the ROM to C;
2. [TinyCC](https://github.com/TinyCC/tinycc), compiled to WebAssembly
   (`port/dist/build_tcc.sh`), builds that into the library the engine loads at start
   (`port/src/gamelib.h`);
3. at the same time the page records the game's songs, effects and voices from the ROM,
   losslessly (`sounds.bbs`, 104 MB): the game's own recorder compiled to WebAssembly
   (`port/src/sndrec_web.c`; it plays the sound driver from the ROM, so nothing of the
   game is in it) runs on every core at once, one sound per core
   (`builder/sounds.js`), and the parts are put together into the same file, byte for
   byte, as the game records itself. About 40 seconds on four cores;
4. the page puts the library, the sounds, the ROM and your settings into the engine's
   package. An Android app is then signed in the page (`builder/apk.js`, APK Signature
   Scheme v2) with a key it makes the first time and keeps in the browser, so later
   builds install over earlier ones.

Nothing is uploaded. The game has its sounds from its first start (on Android it copies
them once out of the app). If a browser cannot record them, the game records them itself
on its first start instead (a minute or two, shown on screen) and keeps them in its
settings folder. The settings start at the best ones: widescreen 16:9, the
room above Robo, guards walking back to their posts and active anywhere on the wide
screen, original colors, sharp pixels.

### How the port works

```
port/
  src/        runtime: memory map + I/O (md.c), VDP renderer (vdp.c), PSG,
              Z80 + YM2612 sound, SDL2 frontend (main_sdl.c), headless test runner
  gen/        the game's code translated to C from the ROM by tools/recomp.py (made by
              the build or a builder page, not in the repository; do not edit)
  gamelib/    the few C library headers the game's code is built with as a library
  ext/        third-party cores: Musashi (68000), superzazu/z80, Nuked-OPN2 (YM2612)
  android/    Gradle project (SDL2 Java glue + ROM picker)
  dist/       packaging script, README and licence notes shipped with packages
builder/      the HTML builder page
```

* `tools/recomp.py` finds the code with the same tracer as the disassembler, then turns
  each instruction into C. It does this by partially evaluating the reference
  interpreter's (Musashi) handler for that opcode, so the opcode, registers, extension
  words and cycle counts become constants. Branches, calls and returns become C control
  flow. All 19,580 traced instructions are translated: `rte`, the status register
  writes, `stop` and `move usp` are written out with the interpreter handler's steps
  and cycles. The HBlank vector's `jmp` that the game keeps in RAM (0xFFC0B0) runs
  natively too. Code that the port changes while running is not interpreted either:
  only bytes of traced instructions switch recompiled code off (data changes don't),
  operands that the port changes (a level mod's frame table, the stage select code's
  menu) are read from the ROM buffer (`LIVE` in recomp.py), and a level mod's own
  routines past the cartridge's end (Bonanza Maker's tile loaders and start routines)
  are filled into instruction forms recomp.py generates with open operands
  (`port/gen/recomp_slot.c`, `port/src/slotcode.c`). Only code in other shapes (a
  hand-made mod) has no native code: the CPU takes it as an instruction it cannot run.
* **There is no CPU interpreter in the port.** `port/src/m68k_rt.c` holds the 68000's
  state, reset, interrupt line and the data the native code uses (from Musashi's
  `m68kcpu.c`; the header `m68kcpu.h` gives the register layout and the inline helpers
  for memory, stack, status register and exception entry). Musashi's opcode handlers,
  execution loop and disassembler, and the Z80 core's interpreter, are not built:
  `tools/z80recomp.py` copies only the Z80 core's operations. A PC with no native code
  takes the 68000's line-1111 exception (what the hardware does for the 0xFFFF bytes
  past the code: the game's error handler); for the Z80 the line is waited out.
* `tools/z80recomp.py` does the same for the Z80: the sound driver the 68000 copies at
  boot (0x118B bytes from ROM 0x33000, traced from the reset and interrupt entries,
  its RST vectors and three jump tables: 1,803 instructions) and the boot stub before
  it become C (`port/gen/z80_driver.c`). Each instruction is its opcode's case from the
  Z80 core (port/ext/z80/z80.c) with its operand bytes built in; calls and returns use
  the real stack in Z80 RAM and a dispatch switch; after each instruction the core's
  interrupt check runs and the line's cycle count is updated as in md.c's step loop.
  md.c runs the program it finds in Z80 RAM (`z80r_program`); writing over its code
  bytes makes it look again. The sound chips (YM2612, PSG) are simulated only to record
  the sounds, or to play them when there is no recording (below).
* **Game logic in readable C** (`port/src/logic/`): the decompilation of the game's own
  rules, written as plain C on the game's work memory by name (`ram.h`), one file per part:
  * `guards.c`: the guards' frame (out of view: back to their posts; forgetting players
    out of view; looking for players, walls and closed doors in between; events starting
    scripts; the script interpreter; animation steps; falling; touching and catching the
    players); `guard_ops.c`: all 65 script ops (waits, walks, runs and lane changes with
    their wall, stairs and contact checks, jumps, stairs, turning, firing, the script's
    jumps, calls, variables and compares); `guard_anim.c`: turning and animations;
  * `map.c`: the stage's map (the cell at a place, wall tests by lane, the probes around an
    object and the checks built on them, objects touching);
  * `objects.c`: the objects players and guards make (all 23 kinds, made and run each
    frame: the players' shots, the guards' bullets, bombs and thrown things, marks over a
    guard's head, the loot held up, the messages, the way out at the stage's end and its
    parts, puffs), with the small routines they share (animation steps, moving along a
    path, hitting the players);
  * `players.c`, `player_states.c`: the players: each one's frame (the buttons, the state
    they choose, protection), what runs after his state (animation, blinking, the view's
    scrolling, what is under him, the way out), and every state in the state table:
    standing, walking, jumping (its path commands), against a wall, firing, tripping,
    taking loot, going through a door, falling down a hole, shot, hiding, opening a door,
    knocked flat, carrying, the stairs, off an edge, at the way out and going out; and
    their helpers (moving along a path through walls or not, the little scripts' falls,
    the checks for stairs, doors, walls to the side and room above the head, the state
    requests, dropping and resetting loot, telling the guards). Kept as the game's code:
    the stage start (it runs while a stage is set up, when a frame may run out of time
    inside it) and the demo-input takeover (player 2's copy runs on into player 1's). The game has the
    players' code twice; the C is written once with player 1's addresses, and
    `tools/player_map.py` lines player 2's copy up with it (`player2_map.h`: the places,
    tables and work memory of player 2's copy; `--diff` lists where the two differ, e.g.
    in values, or player 2's copy using player 1's object). A call, a `jmp` or a routine's
    address the code sets comes from that player's own copy;
  * `items.c`: the items each frame (objects $26..$45, every kind): the loot (taken,
    points, thrown out of a shot player's hands and falling), doors (opened by guards,
    knocking over whoever is behind them, shutting when the doorway is clear),
    springboards, rides, things to trip on, sticky traps, presses, holes, hiding places,
    trapdoors and the way out;
  * `sprites.c`: the objects' sprites each frame (which views each object is near, its
    picture's pieces into each view's lists, the lists merged into the sprite table; a
    level mod's own picture table is read where the game keeps its address); a picture
    straight on the screen (the messages), and single sprites (the HUD's);
  * `hud.c`: the time (counted down and drawn, the hurry sound, "time up", the players
    out and back in, the time left turned into points at the way out), the map's marks
    for the players and guards, and points (decimal, an extra life at each threshold);
  * `views.c`: the players' views: scrolling them a pixel at a time by the moves the
    players' code asks for (stopping at the map's edges), and the drawing that asks for,
    a piece a frame (a column of the map as each 16 pixels go by, the rows, the HUD's
    split, tiles left by a player's code); the map's column jobs in the frame's
    interrupt; each stage's views put back when a player goes out (one table for the
    stages' twenty routines);
  * `play.c`: the frame during play: the pause (start, "PAUSE", the music held), every
    part of the frame in turn, then the stage done, players out of lives and the game
    over; the effect over a player stuck against a wall; the attract demo's frame (its
    recorded pads). The views' part waits for the frame's end inside, so these routines are
    each written as two parts around that wait (the first goes into the game's wait
    loop with a `jsr` of its own; the second is called where that returns);
  * `palette.c`: the palette's fades (up from black a level at a time, darker, towards
    white), as the screens run them and as jobs, the stages' palette cycles, and the
    palette copied and preset;
  * `stage.c` (with `map.c`, `items.c`): setting up a stage: its objects placed, its map
    (the cells expanded, its floors and lanes), its items' kinds and sizes and its loot;
  * `logic/menu.c`: the screens around the game: the screen dispatcher, the SEGA logo,
    the title (its scroll, colours and running figure, "push start"), the start menu,
    the options screen (every line: level, lives, the sound test, the pad, exit, stage)
    and setting up the attract demo and a stage's start;
  * `panel.c`: the players' panels during play: a player's game over put up and its
    count down, "continue" counting down, the credits, joining in again with start (the
    game has it twice, player 2's copy differing in a few places, e.g. a `bne` after
    the game over's check where player 1's has a `bra`, so the C returns the flag that
    check leaves);
  * `intro.c`: the attract mode's opening (the brothers walking with the police cars
    behind them along their paths, the town, the van, the view rising; start skips it);
    its steps' set-ups, which load tiles and maps with the screen off, stay the game's
    code;
  * `stagein.c`: the screen before each stage (the building, then the things to take
    counted up one at a time with a sound; A, B or C hurries it; the fade out);
  * `cutscene.c`: the story scene at the game's end (the van and the guards): its steps,
    fades and scrolls, and its script, whose commands run as the game's code (some
    run past a frame's end, so the game's code calls them and the C goes on where they
    return);
  * `screens.c`, `sound.c`: small routines (a screen's wait, the pad's buttons as the
    options map them, the jobs list). Where C stands for game code that pushes registers
    or return addresses, it writes them to the stack as the game does (`stack_put`): a
    later, deeper routine can find them there, above the stack pointer, before it writes
    its own. Routines that take longer than a frame (clearing the work memory, setting
    up a stage) stay the game's code: C runs a routine whole, which would move the
    frame's end.
  `tools/recomp.py` (`LOGIC`) calls each where its routine starts. While the code around a
  part is still translated 68000 code, the part also leaves the registers and flags its
  callers read (`tools/liveness.py` works them out: registers by byte, word and long,
  handler pointers and tables followed) and takes the routine's time (a frame that runs out
  of time goes on in the next). Parts of a routine not written yet run as the game's code
  up to stop points the recompiler marks (`STOPS`), and C calls routines not written yet
  with `cpu_call`. The port's own changes at places now in C (Insane mode's hooks) are
  called from the C. Checking: `BONANZA_TRANSLATED=1` runs the game's own routines;
  `LOGIC_SCRAMBLE=1` fills the registers C leaves alone with junk; every recorded scene must
  give the same pictures and game state both ways (`LOGICSTATS=1` lists the calls by part
  and by script op; `LOGIC_OPS_OFF=48,F0` runs those ops as the game's code;
  `LOGIC_SKIP=state_walk,...` runs those parts as the game's code; `LOGIC_TRACE=1` lists
  each part as it starts, with the frame's time left, both ways). When C calls the game's
  code, the generated code's `rts` goes back to the C as soon as it reaches the place
  `cpu_call` waits for, even within the same translated region. One frame of
  the ending differs: the game's frame runs out of time inside the guards' frame there, and
  the C version finishes it at once (the next frame is the same again).
* **Drawing** (`port/src/draw.c`, `video.c`, `vdp.c`):
  * The game's drawing routines are written in C: tile loading (raw and packed), lines
    of text and the queued text, the HUD (frame, stage name, time, scores, counters),
    the split screen's line effects (one function per band), the planes' new columns
    and rows, map columns, blocks and rectangles, the targets screen's picture, clearing
    video memory and the sprite table, and the video parts of the vertical blank (sprite
    table and palette copies, scroll). `tools/recomp.py` (`DRAW`) calls them where the
    routines (or the parts of a routine) start.
  * Each does what the game's routine does (the same video memory, registers, palette
    and scroll, the same RAM, stack, registers and flags left behind) and takes the same
    time: `tools/recomp.py` writes each instruction's time to `port/gen/m68k_costs.c`
    (the game's frame routine gets a frame's time; a stage being set up takes several).
    A routine runs in one go, between the frame's lines. The one routine that lets
    interrupts in part way (a map column) runs as translated when one is waiting then,
    so it is stopped at the same instruction.
  * They write the video stores directly (`port/src/video.h`): a word to video memory,
    the palette or the vertical scroll at a cursor, a register by number, copies from RAM,
    fills and copies within video memory as plain loops. No console port, command words,
    status polls or copies set up through registers. The game's own translated code (and
    a mod's routines, when they are translated) make the same calls; the console's port
    is kept only for a mod's code in other shapes (`video port words` in the headless
    runner counts what goes through it: none for the game and the maker's mods).
  * The split screen's band routines run at the lines a band schedule gives (each band
    says, in register 10, how many lines later the next one comes).
  * The game runs a frame at a time (`md_run_frame`), not line by line: the split
    screen's band routines (its line interrupts) run as calls at their lines, the
    picture is drawn at once, then the frame routine (the vertical blank, where the whole
    game runs) goes on until the game is idle. The play screen's routine waits for the
    next picture's top band and goes on from that line. No time is kept within a line.
  * Each frame's picture is the video memory as it is when it is drawn, plus its bands:
    runs of lines with the same registers, palette and scroll, as the band routines set
    them (the game also turns the display off part way down once in a while). It is
    drawn with shadow / highlight (the spotlights of the story and targets screens).
  * Checked against the build with no routines in C: the same game state, sound and
    picture in every frame of the reference runs (`--picture-hash` in the headless
    runner).
  * The picture is drawn on the graphics card (`port/src/render_gl.c`: OpenGL 3.3 on
    desktops, OpenGL ES 3 on Android, WebGL 2 on the web). `render.c` plans each line
    (planes, window, which sprites it shows, the port's extra pieces, widescreen sides
    from the level map) and lists it all as the frame's scene (`port/src/scene.h`); a
    shader works out every pixel's colour code from it as the CPU drawing does, and a
    second pass turns codes into colours. `bonanza-headless --gpu` draws each frame both
    ways and compares them (no pixel differs in any of the reference runs). The CRT
    filter, the menu's recolouring and `--cpu-drawing` (or no OpenGL) use the CPU's
    drawing.
* **Insane mode** (`port/src/insane.c`): a fourth LEVEL in the game's own OPTIONS menu
  (EASY, NORMAL, HARD, INSANE; underneath the game plays on Hard), offered once unlocked:
  by beating the whole game on Hard in a game that did not use the stage select code since
  the machine was reset, or by the code A, B, B, A, Down, A, B, B, A on the title menu
  (`cheats.c`). Every guard but the purple one takes two shots: the first does not stun it,
  it knows at once where the player is and goes for him (the game's "player seen" event).
  The players' guns fire one shot at a time. The purple guard (the big one who throws) goes down at his
  fourth shot as usual, but the shots before no longer make him flinch or start his
  throwing over, one in a hundred bounces back at the player with the spring's sound, and
  he throws 1.2 times as fast. The shield guard sends the shots it stops back at the
  player (as a guard's bullet, which costs a life) and walks and runs 1.2 times as fast;
  green guards look for the players every frame instead of every fourth. A waiter carrying
  plates who sees a player throws them at him at once instead of dropping them and raising
  the alarm. Dogs that wake up run straight at the player (a bark, no "!" pause, no
  sniffing) and run 1.33 times as fast (2 pixels a frame for 1.5). Tripping on a can no
  longer makes the player invincible while he gets up (the minecart's landing still does;
  the rake never did). The stage select code does nothing in Insane mode. Offline only.
  Game hooks: the shot-hits-guard calls (0x18BC2, 0x18EAE), the shield block (0x18B5C,
  0x18E40), the guards' look (0x16C2E), the dogs' running steps (0x156C0, 0x16752), the
  can's protection and getting up (0xC330, 0xE7AA, 0xC366, 0xE7DE), the LEVEL item
  (0x3CC8, drawn at 0x3B22 / 0x3EC4); some guard script data words are changed in the ROM
  buffer while in a stage.
* **Sprite limit** (menu: Screen tab, `sprite_limit =` in bonanza.ini, default off): the
  console shows at most 20 sprites and 320 sprite pixels on a line and drops the rest;
  with the limit off every sprite is drawn (the game's own masking sprites, which hide
  characters behind the HUD, still work). On: as the console.
* **Sound** (menu: Sound tab; `volume =`, `mute_music =`, `mute_all =` in bonanza.ini):
  the volume (0-100), the game's music off (sound effects and voices stay), or no sound.
* **Spotify** (`port/src/spotify.c`; menu: Sound tab, `spotify =` in bonanza.ini: mute,
  off): while Spotify plays music on the same computer, the game's music is eased out and
  its sound effects and voices stay; when Spotify stops, the music comes back. Spotify is
  asked every two seconds (Windows: its window title, Linux: its MPRIS status through
  `dbus-send`, macOS: `osascript`); not on Android or the web.
* **Options menu** (`port/src/menu.c`): tabs for Game (online, mods, guards, reset, quit),
  Screen, Colors and Sound; left / right on the tab bar (or on an item without a value)
  changes the tab.
* **CRT filter** (`port/src/crt.c`; menu: CRT filter, `crt =` in bonanza.ini: off,
  scanlines, tv, monitor): the picture as a CRT shows it, worked out for every screen pixel
  it covers, in linear light (the tube's gamma 2.4). Across a line each pixel is a flat
  step blurred at its edges by the signal's bandwidth (nearest neighbour, blended only at
  the edges); down the screen the beam draws each line as a Gaussian spot that grows with
  its brightness, for red, green and blue apart, carrying the line's light; then the
  phosphor mask on the screen's own pixels (slot mask for the TV, aperture grille for the
  monitor), halation in the glass, and for the TV a curved face, rounded corners and
  darker edges. The row passes are vectorised and shared among the CPU's cores (about
  6-10 ms a frame at 1280x800 on four cores of a slow machine).
* **Recorded sound** (`port/src/sndrec.c`, `sndbank.c`, `sndmix.c`): on first start the
  port plays every sound the game can ask for (songs 0x81-0x8C, effects 0x98-0xBD,
  voices E4/E5) alone through the game's own driver and sound chips (`md_snd_*`: the
  driver loaded as sub_02C76 does it) and stores it losslessly (FLAC-style fixed
  predictors and Rice codes, 44.1 kHz; the build's and new recordings in Opus: 48 kHz,
  songs 96 kbit/s, effects and voices 160 kbit/s per part, about 22 MB in all) in four parts: FM5, FM6, PSG3 + noise (the
  channels the game's effects take over) and the rest. Where a song repeats is found from
  the driver's state in Z80 RAM (the same tracks, timers and loops as at an earlier
  moment). While the game plays, the 68000's writes to the driver's mailbox (Z80 RAM
  0x1C0A, pause 0x1C10) go to a player that follows the driver's rules: a song stops the
  song and effects before it; an effect needs a priority (table at ROM 0x381EF) not below
  the playing one's, replaces the effect on its channels and leaves out the song's part
  on them until it ends; E0 fades 0.75 dB every 6 music ticks for 40 steps, E1 stops,
  E4/E5 (voices) cut the effects and hold the song; pause holds everything. A sound
  starts at the line in the frame where it was asked for. The Z80, YM2612 and PSG are not
  run while the game plays (`--sounds FILE` in the headless runner). Without a recording
  (when it cannot be written), they run for one frame after each of the game's frames:
  the game itself is the same either way.
* The recompiled code yields to the scheduler at exactly the points the interpreter
  does. A recompiled run is therefore **bit-identical** to an interpreted one; this was
  checked frame by frame over the attract mode, menus, and gameplay on x86-64, Windows
  (MinGW build under Wine), ARM64 and ARMv7, and for the Z80 also by comparing the
  audio output byte for byte (music, effects, the DAC voice samples). The headless
  runner prints how many instructions ran recompiled and interpreted on each CPU
  (0 interpreted for the game, its mods made with Bonanza Maker and the Lost Levels).
* **Widescreen and custom colours.** An in-game menu (Esc, gamepad Back, or MENU on
  touch screens) and the builder's *Screen & colors* step offer:
  * 4:3, 16:9, 16:10 and 21:9 output. Widescreen draws more of the room from video
    memory instead of stretching... except that the game only keeps ~20 map columns
    around the camera there, so the extra columns come straight from the level map in
    ROM (map $FF1838, metatiles $FF183C, width $FF2CCC, cameras $FF1844/$FF1846 and
    $FF2288/$FF228A, floor rows $FF187A/$FF22BE). Each frame the map is checked against
    what the game drew (allowing for the floor number changing a few frames before the
    scroll on stairs); outside the building the backdrop shows. The outer 16 pixels of
    the picture also come from the map: the game may leave the edge column half
    updated, which a TV hid in its overscan. Cutscenes and the title are shown 4:3
    (option), game over / time-up screens stay wide.
  * The game only turns objects within 144 pixels of a view's centre into sprites
    (sub_058B4), so in widescreen the renderer builds the missing ones itself from the
    object tables ($2EF6.. state, $304E x, $30FA y, $3252 frame, frames in tbl_24B80),
    the way sub_059C6 does: props, levers, presses and trap doors stay visible.
  * *Patrol Return* (option): a guard leaving the 4:3 picture is put back at its post
    by the game at once (sub_14232; posts at $9108/$9154). In the wider picture a copy
    of it is shown hurrying back there (3x walking speed, with the walk cycle it was
    last seen with; a knocked-down guard gets up first), opening the doors it passes
    and shutting them right behind it.
    Like everything above this only reads the game's memory, so the game plays exactly
    as the original.
  * In a 1-player game the top of the screen shows the floor above Robo, following
    his camera, instead of the "press 2P start" panel (option: Top (1P)). It is
    drawn from the level map, so it stays correct while he walks up or down stairs.
    It never goes above the top of the level: on the top floor it shows the exit
    floor (roof), and once Robo is up there it stays still and shows him escaping,
    while the lower view shows the floor below instead of the same floor twice
    (drawn from the map, characters where they really are).
  * Colour styles (vivid, soft, arcade, grayscale, sepia, night, Game Boy, inverted),
    plus brightness, contrast, saturation and hue.
  * Player characters: separate shirt, pants and skin-tone colours for Robo and Mobo.
    The renderer recognises the players' sprites (their animation frames always sit
    in sprite tiles 0x100-0x1FF) and recolours only those, so guards and the HUD
    keep their colours.
  * A recolour tool that replaces any Mega Drive colour with one you choose.

  * *Floor Alert* (option, off by default, the only one that changes the game): every
    guard on a player's floor keeps running off screen - it chases you and never jumps
    back to its post until you change floors. `tools/recomp.py` puts hooks at the
    start of sub_058B4 (visibility) and sub_059C6 (sprites); `port/src/gamemods.c`
    widens the visibility for guards on the player's floor and still only makes
    sprites for those near the picture. *Semi* instead widens the game's visibility
    tests (also the "is the post on screen" one) by the side areas of the chosen
    screen shape: guards you can see keep going, off screen the original rules apply. With the option off the hooks do nothing and
    runs stay bit-identical to the original.

  * *Online 2 players* (PC and Android, `port/src/net.c`): delay-based lockstep over
    UDP like most emulator netplay - both machines run the same game and exchange
    only pad states (with redundant resends), the input delay adapts to the ping,
    and state hashes every 60 frames detect a desync. Host or join from the menu
    (`Online...`) or with `--host [port]` / `--join address`; LAN games are found by
    broadcast, internet games need UDP 7777 forwarded on the host (self-hosted, no
    server). Each player gets his own screen: `vdp_local_player` makes the renderer
    put that player's view at the bottom and the floor above him at the top.
    `bonanza-headless --net-host PORT` / `--net-join ADDR` play scripted online games
    for testing (both sides' `--hash` output must be identical).

  * *Prop Hunt* (`port/src/prophunt.c`, online only): the host chooses it in
    `Online...` (Mode: Prop Hunt, Round: 1-9 minutes; sent to the joiner with the
    welcome). One player hides as one of the stage's guards, treasures or gadgets (A
    changes the disguise), the other has to find and shoot him; if the time runs out
    the hider wins. Roles swap every round and the stage starts again. The first 10
    seconds only the hider moves (the seeker's screen is dark). Guards ignore the
    hider, who never attacks; as a guard he moves as guards do (half a pixel per frame,
    lanes a step a frame, no jumping; the disguise walks with the real guards' walking
    frames), as a treasure or gadget he runs 1.1 times as fast as a player, jumps 1.1
    times sooner and changes lanes 1.1 times slower. The seeker's HUD map does not show
    the hider. Guards leave the seeker alone while he waits; afterwards a caught seeker
    is only knocked dizzy for a moment. Nobody loses lives and the exit stays shut. The rules run in lockstep on both machines around every frame
    (they read only the pads and RAM); the disguise and the banner are drawn by each
    machine for itself. `PH_OFFLINE=minutes` runs the rules offline in the headless
    runner (tests).

  * *Race* (`port/src/race.c`, online only): the host chooses it in `Online...`
    (Mode: Race). Each player plays the same stage in a 1-player game of their own -
    no lockstep, only a small race status goes over the network (round, vote, frames
    played, cleared and when, treasures, wins). Rounds: both vote for a stage (if the
    votes differ, the host's pick counts in odd rounds and the other player's in even
    ones), a 3-second countdown, then the race: lives never run out, a game that ends
    without clearing the stage (time up) starts it again with the clock still running,
    and whoever clears it in fewer game frames wins the round (a tie goes to the host).
    During the race the top line shows the other player's treasures. Then the result
    and the next vote. `bonanza-headless --net-host/--net-join --set race=1` plays it
    scripted (`--input` is this player's pad).

  * *Mods* (`port/src/mods.c`): `.bbro` files (zip archives: `mod.ini`, IPS and text
    ROM patches in `patches/`) or folders in a `mods` folder next to `bonanza.ini`,
    turned on in the menu (`Mods...`, saved as `mods = a.bbro|b.bbro`) and applied to the
    ROM at load time, in order. The zip reader has its own inflate. Recompiled blocks
    whose bytes a mod changes are handed to the interpreter
    (`recomp_invalidate()`, generated into `gen/recomp_index.c`). Online play compares a
    hash of the loaded (modded) ROM. `bonanza-headless --mods-dir DIR --set mods=X.bbro`
    for tests; the HTML builder adds mods to packages.

  * *Bonanza Maker* (`builder/levelmaker.html` + `builder/levelcore.js`, test play in
    `builder/play.html`): a Mario Maker style stage editor. `levelcore.js` reads a stage
    from the ROM (map, metatiles, the per-floor collision columns, object placement, item
    kinds, guard templates; tiles decoded from the stage's VRAM loads) and edits it in
    "slices" (one floor tall, 16 px wide = 5 map blocks + 2 collision columns), so
    pictures, walls, stairs and doors stay together; `makeStamps()` cuts ready-made parts
    (stairs, exit stairs, door, wall, hallways) out of the stage. Export writes new
    tables after 0x80000, repoints the stage's entries, renumbers door columns (info
    0x90+2k = k-th item), places the exit at the EXIT column, fixes the first-screen
    descriptors, ROM end and checksum, and saves an IPS patch plus the level source in a
    `.bbro`. Deluxe Liner has one more collision floor than map floors (objects 80 px
    lower): `level.base`. Play runs the browser runtime (`web/bonanza-single.js`, wasm
    inside, so it also works from disk) with `--play-stage ENTRY`, which presses Start
    through the title and sets the stage number. The browser runtime has the game's
    code in it, so it is not published: test play needs one built with your ROM
    (`emcmake cmake -S port -B build-web ...`, `bonanza-single.js` next to the page). `levelcore.js` also runs in node.
    The ROM's 13 stage entries are the 10 stages plus the scenes between them (4, 8, 12).
    Loot and gadgets can come from any stage: a borrowed treasure gets new frames (the
    frame table, read at 0x59E4/0x5B32/0x5DB2, is copied after 0x80000 with them at its
    end) and its tiles in VRAM 0x7A0-0x7CF, free in every stage; a gadget's animation
    tiles (frames from its tbl_1CCBC scripts) go where they were in its own stage when
    this stage leaves them free; colours of palette lines 1-3 are matched to the stage.
    The stage's tile loader is wrapped by a new routine that adds those loads. Guards are
    drawn with the frame each guard type shows first in that stage (LC.GUARD_FRAMES,
    recorded from the game). Stamps and selected areas can be flipped: mirrored
    metatiles (tiles swapped, hflip toggled) are added after the stage's own, collision
    columns reversed and the stair direction (info 0x81/0x82) swapped. The Trace tab shows
    a picture (e.g. an arcade map) under the building to build over. The robbers' start
    can be moved: the export then writes new start routines for both views (tbl_04566 /
    tbl_0459A / tbl_045CE): camera x = the view's left edge (32 to 16 W - 224), first
    streamed column C = (camX - 32) / 16 - 2, plane address of block column c =
    base + (2c mod 64) * 2, camera y = 32 + 80 floor, first map row 5 (floor - 1), and
    first-screen descriptors split where the plane wraps (values checked against all ten
    stages' own routines). Painted blocks can carry physics (`LC.paintBlock`): a 16 px
    block at row k of a floor sets mask rows 2k, 2k+1 of its two collision columns in the
    front lane, back lane, both, or clears them (the slab bit stays; stair/door/exit
    columns are left alone). Blocks can come from any stage: their 8x8 pieces are
    recoloured to the palette line of this stage that fits best (`LC.blockLook`) and
    loaded by the wrapped tile loader into VRAM the stage leaves alone (`LC.FREE_TILES`,
    measured: never written or shown during play, empty, and outside the queued sprite
    loads of tbl_05E86); they get metatile pairs after the stage's own and mirrored ones.
    A block from the Stairs group can lay a working staircase where it is painted
    (`LC.placeStairShape`: the game's ramp, hole and landing columns, stage 1's values,
    mirrored for the other direction; pictures stay as painted). The Door piece is the
    door stamp's middle block (pillar picture, door column, door item). The Paint tab
    shows each stage's objects (`LC.stageObjects`: on every floor, what is not hallway is
    cut into connected groups, doors and pillars on their own, identical ones merged and
    sorted by kind: stairs, doors, furniture, on the wall, floor); placing one writes its
    blocks and the solid rows of its columns, and stairs/doors work. Ramps left without a
    landing by edits are removed (`LC.fixStairs`). Tests:
    `node builder/test/core.test.js baserom.md` (core, every stage) and
    `node builder/test/ui.test.js baserom.md` (browser sweep, needs Playwright).
    Levels can have 3 to 5 floors (`LC.addFloor`/`removeFloor`; the export writes the
    game's standard floor tables for that count: dat_0FF86, tbl_0FFA2, buffer count,
    tbl_10322). Arcade map pictures: `LC.detectLayout` (and
    `tools/lostlevels/detect.py`) finds the floors (the light slabs across the building) and
    the staircases (the white steps' top edge rising ~1 px per px) with their direction;
    the Trace tab lines the picture up with the building, matches the floor count and
    places the staircases. The Stairs tool places the stage's staircase going either
    way (the missing direction mirrored, `LC.stairStamps`); cut-off staircases are
    reported.
    The Collision tab edits the masks directly (`LC.setCollision`: 8 px columns, rows
    0-6 one bit each, 7-9 the floor bit 0x01; front, back, both or neither, the info
    word kept). Gadgets whose tiles a stage uses itself are "moved" (`LC.itemUse`): their
    animation frames' tiles go to free VRAM (what treasures leave of 0x7A0-0x7CF, then
    the stage's free tiles after borrowed blocks) and the level's own frame table gets
    new pictures under the same frame numbers, so the game's code and animation
    scripts are unchanged.
    `LC.detectContents` finds the rest of an arcade map's layout per floor: pillars (light
    from ceiling to floor), things standing on the floor (the band's bottom 32 rows are the
    floor strip, back lane above front lane; a column's strip is bare floor where it
    matches the strip patterns that repeat across the floor, a thing in the back lane
    covers the strip from the top down to its base, one in the front lane most of the
    front half; height from where it differs from the columns beside it) and people (by
    colours only they use; feet give the lane). `LC.placeDetectedCollision` and
    `LC.placeDetectedPeople` put them into the level through the Trace tab's mapping.

  * *Stage select code* (`port/src/cheats.c`): the Options menu's sixth item STAGE
    (`sub_03E52` / `sub_03F48`, writes the stage number 0xFFC05A, which a new game keeps)
    is in the cartridge but unreachable: the cursor wraps after EXIT and the menu never
    draws it. Left, Left, Right, Up, Shoot, Jump on the title menu (0xFFC068 ==
    `sub_037C0`, 0xFF6C5C == 0; buttons from the remapped newly-pressed bytes 0xFF17C1/C3)
    patches the wrap immediates at 0x3C24 and 0x3C44 to 5 and the "draw EXIT" call at
    0x3B08 to a helper in the unused debug text at 0x121C (`jsr 0x11F2` / `jmp 0x3F48`).
    Only called from frames the front end runs on its own (not online); undone at
    `md_reset` (the boot code checksums the ROM).
  * *Extra stages* (`port/src/extrastages.c`, "New stage" in Bonanza Maker): new stages
    between the cartridge's own, e.g. the two arcade stages the Mega Drive left out
    (`tools/lostlevels/make.js ROM` builds `LostLevels.bbro` from your ROM: the arcade's
    Department Store after Mint, on the Jewelry Store's graphics). Every way into a stage
    passes the entry dispatcher `sub_11106` once, and everything the game knows about an
    entry comes from tables (intro name and "STAGE n" sprites, targets screen, setup,
    HUD number, minimap, spawn/escape; only entry 6 has special code). An extra stage's
    ROM slot starts with a `BBXSTAGE` descriptor (follows entry, host entry, name sprite,
    patch list = the host's table values); at the hooked dispatcher the port writes them
    into the ROM buffer when the game moves on from the entry it follows, plays the host,
    and puts them back when it is cleared. Later stages are numbered one higher. Nothing
    is hooked without one. `--play-extra K` (`--extra K` headless) starts one directly.

  Settings live in `bonanza.ini` (see `port/dist/README.txt`). Apart from Floor Alert
  they only change the picture, so recompiled-vs-interpreted runs stay bit-identical.
* Frame pacing: on a ~60 Hz display with vsync the game runs exactly one frame per
  refresh (the audio queue absorbs the 59.92 / 60 Hz difference by dropping or
  repeating a few samples); on other displays frames are run by elapsed time.
* Controls: arrows / Z X C / Enter (player 1), W A S D / J K L / Space (player 2),
  gamepads, and an on-screen touch pad on phones. F11 toggles full screen, P pauses.

### Building the port yourself

With your ROM as `baserom.md` in the repository root (or `-DBONANZA_ROM=...`), CMake
translates the game's code from it (Python 3) and links it into the program:

```sh
make port                          # Linux/macOS with SDL2 installed -> port/build/bonanza
port/build/bonanza baserom.md

# self-contained builds (SDL2 is downloaded and linked statically)
cmake -S port -B build -DBONANZA_FETCH_SDL=ON                       # Linux
cmake -S port -B build-win -DBONANZA_FETCH_SDL=ON \
      -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake           # Windows (cross)
emcmake cmake -S port -B build-web -DBONANZA_FETCH_SDL=ON           # Web
```

`-DBONANZA_GAME_LIB=ON` builds the **engine alone** (what the workflow ships; no ROM
needed): it loads the game's code at start from `bonanza_game.dll` / `libbonanza_game.so`
next to the program (on Android: in the app's libraries). With a ROM, that library is
built too, the way the builder pages build it. The Android app (`port/android`, Gradle)
is always the engine alone.

A native build with the ROM also records the game's sounds into `sounds.bbs` next to the
program (`-DBONANZA_SOUNDS_OPUS=ON`: Opus, about 22 MB, instead of lossless); ship it with
the game and the game does not record them on its first start. The file depends only on
the ROM, so the cross builds take the native build's: `-DBONANZA_SOUNDS=build/sounds.bbs`.
`bonanza-headless ROM --make-sounds FILE` does the recording alone.

```sh
# regression check: state hashes and pictures (and --wav audio) must stay as recorded
# before a change (the same with or without --sounds FILE)
build/bonanza-headless baserom.md --frames 1500 --hash --picture-hash > a.txt
```

After changing the tracer or `tools/config.py`, the build translates the game's code
again (or `make recomp` for `port/gen/`).

## Building the disassembly

Requirements:

* GNU binutils for m68k (`m68k-linux-gnu-as`, `-ld`, `-objcopy`).
  On Debian/Ubuntu: `apt install binutils-m68k-linux-gnu`.
* Python 3 (nothing else: `tools/m68kdec.py` decodes the 68000 the way capstone does,
  `tools/m68kdec_check.py` compares the two).
* Your own copy of the ROM, saved as `baserom.md` in the repository root.
  The ROM, the disassembly made from it and the graphics/sound data taken from it are
  **not** included.

```sh
make            # disassembles baserom.md into src/ (once), extracts assets/,
                # builds build/bonanza.md, checks SHA-1
make clean
```

A successful build ends with `build/bonanza.md: OK`.

## Layout

```
src/bonanza.s        vector table, cartridge header, includes the rest  (made from the
src/rom_XXXXX.s      code and small data, starting at ROM offset XXXXX  ROM by make)
include/md.inc       Mega Drive hardware register names
assets.txt           large binary blobs (graphics, sound, maps), extracted
                     from baserom.md into assets/ at build time
bonanza.ld           linker script (everything at 0x000000)
tools/disasm.py      the disassembler that generated src/
tools/m68k.py        68000 decoding helpers (on tools/m68kdec.py)
tools/m68kdec.py     a 68000 decoder giving capstone's text, in plain Python
tools/config.py      manual hints: extra entry points, jump tables, data ranges
tools/extract_assets.py
```

Labels follow the address they sit at:

* `sub_XXXXX`: called routines (`bsr`/`jsr` targets, function pointers, vectors)
* `loc_XXXXX`: branch targets inside routines
* `tbl_XXXXX`: jump tables and pointer tables
* `dat_XXXXX`: data referenced from code

Rename these as the code is understood. GNU `as` syntax is Motorola-style with
`--register-prefix-optional`, and comments start with `|`.

## How the disassembly was produced

`tools/disasm.py` starts from the 68000 vector table and follows code
recursively. It also follows these:

* `move.l #routine, ...` immediates (the game stores its mode and VBlank handlers
  in RAM pointers such as `$FFC068`)
* jump tables of the forms `movea.l tbl(pc,dN.w),aN / jsr (aN)`,
  `lea tbl,aN / movea.l (aN,dN.w),aN / jmp (aN)`, and
  `movea.l #tbl,aN / ...`
* pointer tables whose every entry decodes as plausible code
* unreferenced code that directly follows the end of a known routine

Anything it doesn't reach is emitted as data. Small chunks are written inline as
`.byte`, and chunks of 1 KiB or more go to `.incbin "assets/..."`.

Every instruction is checked by reassembling it at its original address. Any
form GNU `as` would encode differently is emitted as raw `.short` words, with the
disassembly kept as a comment. Currently that applies to one instruction.

Current coverage: about 19,600 instructions, 94 KB of 68000 code. The rest of
the ROM is data: graphics, maps, sound, and tables.

## Regenerating / improving

```sh
make disasm      # overwrites src/ and assets.txt
make
```

Regenerating discards hand edits to `src/`. While the disassembly is still
mostly auto-labelled, improve it through `tools/config.py`: add entry points
for code that is still shown as data, jump tables the tracer misses, and
ranges that must stay data. Once hand-documentation of `src/` begins, stop
regenerating and edit the sources directly.

## Known limitations / next steps

* Pointers inside the binary blobs in `assets/` (sprite mappings, level
  layouts, etc.) are still absolute numbers. The ROM therefore only rebuilds
  correctly while code stays the same size ("non-shiftable"). Splitting those
  blobs into labelled tables is the next step towards a shiftable build.
* RAM (`$FFxxxx`) addresses are still raw numbers. Naming the game's variables
  is the main documentation work left.
* Some indirect calls go through RAM pointers that are set at runtime. Code
  reached only that way may still be shown as data. Add it to
  `tools/config.py` when found.
