# Bonanza Bros. (Mega Drive) disassembly

A matching disassembly of **Bonanza Bros.** for the Sega Mega Drive / Genesis
(`GM G-4046 -01`, regions JUE). Running `make` reassembles the source into a ROM
that is byte-identical to the original.

| | |
|---|---|
| ROM | `BONANZA BROS.` (C)SEGA 1991.JUL, 512 KiB |
| SHA-1 | `f7313ecaf7143873597e3a6ebf58d542d06c3ef3` |
| MD5 | `acf4aa6aea84be72afaa1a86954b85cb` |

Mega Drive games of this era were written directly in 68000 assembly, so there is
no original C to recover. This project is therefore a labelled, re-assemblable
disassembly. That's the usual base for documenting, modding, or porting a Mega
Drive game.

## Building

Requirements:

* GNU binutils for m68k (`m68k-linux-gnu-as`, `-ld`, `-objcopy`).
  On Debian/Ubuntu: `apt install binutils-m68k-linux-gnu`.
* Python 3 (only `tools/disasm.py` also needs `pip install capstone`).
* Your own copy of the ROM, saved as `baserom.md` in the repository root.
  The ROM and the graphics/sound data taken from it are **not** included.

```sh
make            # extracts assets/ from baserom.md, builds build/bonanza.md, checks SHA-1
make clean
```

A successful build ends with `build/bonanza.md: OK`.

## Layout

```
src/bonanza.s        vector table, cartridge header, includes the rest
src/rom_XXXXX.s      code and small data, starting at ROM offset XXXXX
include/md.inc       Mega Drive hardware register names
assets.txt           large binary blobs (graphics, sound, maps), extracted
                     from baserom.md into assets/ at build time
bonanza.ld           linker script (everything at 0x000000)
tools/disasm.py      the disassembler that generated src/
tools/m68k.py        68000 decoding helpers (capstone wrapper)
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
pip install capstone
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
