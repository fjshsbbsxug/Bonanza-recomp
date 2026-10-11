Bonanza Bros - native port
==========================

This is a native port of Bonanza Bros. (Sega Mega Drive / Genesis, 1991) built from the
bonanza-recomp project: the game's 68000 code was statically recompiled to C and runs on
a small re-implementation of the Mega Drive hardware.

The game itself is NOT included. You need your own copy of the ROM:
    Bonanza Bros. (Japan, USA, Europe)  - 512 KiB
    SHA-1 f7313ecaf7143873597e3a6ebf58d542d06c3ef3

Running
-------
Windows : put the ROM next to bonanza.exe and name it baserom.md, or drag the ROM
          file onto the game window. Then run bonanza.exe.
Linux   : put the ROM next to the "bonanza" program as baserom.md (or drag it onto the
          window) and run ./bonanza
Android : install the APK, start "Bonanza Bros" and pick the ROM file once.

Packages made with the HTML builder already contain your ROM as baserom.md.
The ROM is remembered after the first start.

Controls
--------
             Player 1        Player 2
Move         Arrow keys      W A S D
A / B / C    Z / X / C       J / K / L
Start        Enter           Space

Game controllers are supported (first pad = player 1). On touch screens an on-screen
pad is shown.
F11 or Alt+Enter: full screen   P: pause   F3: reset   Esc: quit

Command line: bonanza [ROM file] [--interp]
  --interp  run the original code on the built-in 68000 interpreter instead of the
            recompiled code (for comparison / debugging)
