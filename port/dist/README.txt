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

Game controllers are supported, USB or Bluetooth, also on Android (first pad =
player 1, plug in any time). Xbox / PlayStation / Switch-style pads: d-pad or left
stick; Mega Drive A, B, C = the left, bottom and right face buttons (Xbox X, A, B);
Start = Start; Back/Select opens the menu. Other pads:
d-pad or stick, the first three buttons = A B C, Start = Start. On touch screens an
on-screen pad is shown; it hides while you use a controller and comes back when you
touch the screen.
F11 or Alt+Enter: full screen   P: pause   F3: reset   Esc: menu

Menu: screen shape & colours
----------------------------
Open the menu with Esc, F1, Tab or a right click; Back/Guide on a gamepad; the Back
button or the MENU button (top left) on Android.

  Online...    Play 2 players over the network (PC and Android; see below).
  Mods...      Turn mods from the mods folder on and off (see below).
  Screen       Original 4:3, Widescreen 16:9 / 16:10, Ultrawide 21:9, or Stretch.
               The widescreen modes show more of the level on the left and right
               instead of stretching the picture.
  Top (1P)     In a 1-player game the top of the screen shows the floor above
               Robo (follows his camera) instead of the "press 2P start" panel.
               On the top floor it shows the exit floor (roof); on the exit
               floor it stays still and you see Robo escaping in it, with the
               floor below on the bottom of the screen.
               Guards there only appear once the game puts them on screen.
  Cutscenes    Widescreen only: show title/story/stage intros 4:3 with black bars
               (game over and time-up screens stay wide), or keep them wide.
  Patrol Return  Widescreen only. The game sends a guard that leaves the 4:3
               picture straight back to its post. On: in the wider picture you see
               it get up and hurry back there (three times walking speed), opening
               and shutting the doors on its way, and guards you have not met yet stand at their posts. Off: they vanish at the edge of the 4:3 picture, as in the
               original. Either way the game itself plays exactly as the original.
  Floor Alert  Changes the game (off by default). Every guard on your floor keeps
               going even off screen: it keeps chasing you, never jumps back to its
               post and never disappears, until you change floors. Guards on other
               floors behave as in the original. Turns Patrol Return off.
               Semi: the original rules, but "on screen" means your whole picture
               for the screen shape you chose (4:3 as in the original, wider shapes
               include the side areas): guards you can see keep going, off screen
               they wait or go back to their posts as in the original.
  Scaling      Sharp pixels or smooth.
  Colors       Original, Vivid, Soft, Arcade, Grayscale, Sepia, Night, Game Boy,
               Inverted.
  Brightness, Contrast, Saturation, Hue
  Characters...  Shirt, pants and skin tone of Robo (player 1) and Mobo (player 2).
               Left/right picks a preset (skin tones, clothing colours), Enter opens
               Red/Green/Blue sliders. Only the two players change.
  Recolor...   Shows the colours on screen; pick one (it flashes in the game) and set
               any new colour with the Red/Green/Blue sliders.

Settings are saved in bonanza.ini (next to the program if a bonanza.ini is there,
otherwise in your user data folder). It is a plain text file, e.g.

    screen = 16:9
    guards = return         # Patrol Return on ("original" = off)
    floor_alert = on        # Floor Alert: guards on your floor stay after you (or semi)
    colors = vivid
    robo.shirt = 20B040     # player 1 shirt
    mobo.skin = 8D5524      # player 2 skin tone ("original" = unchanged)
    color.700 = 2040FF      # Mega Drive colour R7 G0 B0 becomes #2040FF everywhere

Command line: bonanza [ROM file] [--interp] [--host [port]] [--join address]
  --interp  run the original code on the built-in 68000 interpreter instead of the
            recompiled code (for comparison / debugging)
  --host    host an online game right away (port 7777 unless given)
  --join    join an online game, e.g. --join 192.168.1.20 or --join myhost.net:7777

Mods (.bbro files)
------------------
Put mods in the "mods" folder and turn them on in Menu > Mods... ("Restart the game
with these" applies them). The mods folder is next to bonanza.ini: next to the program
when a bonanza.ini is there (packages made with the HTML builder), otherwise in your
user data folder (Windows: %APPDATA%\bonanza-recomp\Bonanza Bros\mods, Linux:
~/.local/share/bonanza-recomp/Bonanza Bros/mods). Android:
Android/data/com.bonanzarecomp.port/files/mods on the phone's storage. The menu shows
the folder when no mods are there yet. Mods are turned off when you delete them.

Mods are applied in the order shown (newly turned on ones go last); where two change
the same thing, the later one wins. Both players of an online game need the same mods.

Making a mod: a .bbro file is a zip file (renamed), or use a plain folder with the same
contents:

    mod.ini            name = My Mod
                       author = You
                       version = 1.0
                       description = One line about it
    patches/           ROM patches, applied in file-name order:
      level.ips          IPS patches (the usual ROM-hacking format; Lunar IPS, Floating
                         IPS... make them). They may make the ROM bigger, up to 4 MiB.
      tweaks.txt         text patches, one change per line:  ADDRESS: XX XX XX
                         (hex ROM address and bytes; # starts a comment), e.g.
                         4652: 00 C4
    levels/            ignored by the game (the level maker keeps its levels there)

Bonanza Maker (levelmaker.html, next to the HTML builder on the project's site, or
the BonanzaMaker zip): build your own version of any of the 10 stages in your browser.
Pick parts from the tray - stairs, exit stairs, doors, walls and hallways taken from the
stage itself (they always bring their walls and floor with them), guards, loot and
gadgets from any stage (their pictures come along; gadgets whose pictures would cover
the stage's own are greyed out), single blocks to paint - and click to place them; the eraser removes things,
the hand moves them or selects an area to make your own stamp, add space or remove it.
Press Play to try the level right away, and save it as a .bbro. Open the .bbro again to
keep building. Blocks you paint can be solid (both lanes, front lane or back lane only: a low one is
jumped over, a stack is a wall) or walk-through; pick that above the blocks. The Stairs
part puts a staircase from the floor you click to the one above; Flip (F) turns it the
other way. Drag the robbers with the hand to move where they start; graphics, music
and the time limit stay as in the original stage. Flip (F) mirrors the stamp you are placing; Trace shows a picture (a map
of the stage, say) under the building so you can build over it.

New stages: the arcade had two stages the Mega Drive left out, the Department Store
(after Mint) and the Antique Shop (after Deluxe Liner). "Or make a new stage" on Bonanza
Maker's first page builds one with the look of a stage you pick; the game plays it
between the two stages, and the stages after it move up one number. LostLevels.bbro
(made from your ROM with tools/lostlevels/make.js) has the arcade's Department Store.

Patches that change the game's code work too: that code then runs on the built-in
68000 interpreter instead of the recompiled code. With no mods on, the game is the
original.

Online 2 players
----------------
Each player has his own screen: your own view at the bottom and the floor above you
at the top (like Top (1P)), instead of the other player's view. Both of you need
the same ROM.

  Host:  Menu > Online... > Host a game. You are player 1 (Robo). Your LAN address
         is shown on that page.
  Join:  Menu > Online... > Search LAN (games hosted on your network are listed -
         pick one), or type the host's address in "Join:" (Enter to edit, type,
         Enter to connect). You are player 2 (Mobo).

When the other player connects the game restarts on both machines. Then pick
"2 players start" on the title screen as usual (player 1 picks, player 2 just plays).

Over the internet the host forwards UDP port 7777 on his router to his PC (like
sm64coopdx / other self-hosted games) and gives the other player his public address.
VPN-style LANs (Tailscale, ZeroTier, Radmin, Hamachi) work as well, without
forwarding: join the host's address on that network.

How it works: both machines run the same game in lockstep and only exchange button
presses, so there is no server and nothing to install. The input delay adapts to
the ping (one frame on a LAN). The host's Floor Alert setting is used for both
players; display options (screen shape, colours...) are each player's own. Pause
and reset are off during an online game; opening the menu does not stop the game.
The title bar / menu shows the ping, and "out of sync" should the two games ever
differ.
