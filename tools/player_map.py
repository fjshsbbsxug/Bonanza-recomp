#!/usr/bin/env python3
"""Where player 2's code is: the game has the players' code twice, player 2's a copy of
player 1's with its own work memory and, in a few places, its own values or steps. The
game's logic in C (port/src/logic/players*.c) is written once, with player 1's addresses;
for player 2 it takes the matching place in player 2's copy from the map this writes.

    python3 tools/player_map.py            writes port/src/logic/player2_map.h
    python3 tools/player_map.py --diff     lists where the two copies differ

Each pair of routines (player 1's, player 2's) is followed from its start (branches, not
calls) and the two instruction lists are lined up (difflib); every instruction that lines
up with one of the same kind is mapped. Addresses only: nothing of the ROM's contents."""
import difflib
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import disasm  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "port", "src", "logic", "player2_map.h")

# routine pairs (player 1, player 2) besides the state tables'
PAIRS = [
    (0x08E74, 0x08F26), (0x09026, 0x091A8), (0x0932A, 0x09408), (0x094E6, 0x0966E),
    (0x097F6, 0x0981E), (0x09846, 0x09894), (0x0986A, 0x098B8), (0x098E2, 0x09928),
    (0x0996E, 0x099A2), (0x099D6, 0x09A1C), (0x099DE, 0x09A24), (0x09A62, 0x09A8C),
    (0x09ADE, 0x09B00), (0x09B32, 0x09BE6), (0x09C9A, 0x09CAA), (0x09CBA, 0x09DC6),
    (0x09EDA, 0x09EF4), (0x09F0E, 0x09F28), (0x09F42, 0x09F66), (0x0A51A, 0x0A542),
    (0x0A56A, 0x0A72A), (0x0A8EA, 0x0A942), (0x0A99A, 0x0A9CE), (0x0AA02, 0x0AA72),
    (0x0AAE2, 0x0AB24), (0x0AB68, 0x0ABAA), (0x0ABF6, 0x0ACE4), (0x0AE8A, 0x0AEA6),
    (0x0AEC2, 0x0AEFA), (0x0AF32, 0x0AF58), (0x0AFE8, 0x0B050), (0x0B0B8, 0x0B17C),
    (0x0B23A, 0x0B2BC), (0x0B33E, 0x0B3C0), (0x0B442, 0x0B45A),
    (0x0BF10, 0x0E392), (0x0D7E4, 0x0FBFE),
]
STATES = (0x0B492, 0x0D996, 0x36)       # the state tables: player 1's, player 2's, entries
# whole blocks lined up as one (the states' code: a state's next part is set as a pointer,
# not branched to)
BLOCKS = [(0x0B56A, 0x0D660, 0x0DA6E, 0x0FA70)]


def mnem(i):
    """(a call by bsr or jsr is the same call)"""
    return "jsr" if i.mnem.startswith("bsr") else i.mnem


def norm(i):
    s = "%s %s" % (mnem(i), ",".join(i.ops))
    s = re.sub(r"\$ff[0-9a-f]{4}", "RAM", s)     # each player's own work memory
    return re.sub(r"\$[0-9a-f]+", "N", s)


def values(i):
    """an instruction's numbers other than work memory addresses and code addresses (its
    immediates and displacements): where the two copies may differ unseen"""
    if i.flow in ("cond", "jump", "call"):
        return ()
    s = re.sub(r"\$ff[0-9a-f]{4}", "", ",".join(i.ops))
    s = s.replace("(pc)", "")                 # (a table by the code: its address counts)
    return tuple(re.findall(r"\$[0-9a-f]+|#\d+|\b\d+\b", s))


def reach(tr, e):
    seen, work = set(), [e]
    while work:
        a = work.pop()
        while a in tr.ins and a not in seen:
            seen.add(a)
            i = tr.ins[a]
            if i.flow in ("branch", "jump", "cond") and i.target is not None and i.target in tr.ins:
                work.append(i.target)
            if i.flow in ("end", "jump", "ijump"):
                break
            a += i.size
    return sorted(seen)


def main():
    rom = open(os.path.join(ROOT, "baserom.md"), "rb").read()
    tr = disasm.Tracer(rom)
    tr.run()
    t1, t2, n = STATES
    pairs = list(PAIRS)
    for k in range(n):
        a, b = struct.unpack(">II", rom[t1 + 4 * k:t1 + 4 * k + 4] + rom[t2 + 4 * k:t2 + 4 * k + 4])
        pairs.append((a, b))
    m, diffs, clash, ram = {}, [], 0, {}
    vdiffs, seen_v = [], set()
    same_obj = set()          # (player 2's copy using player 1's object: object 0's word)
    lists = [(reach(tr, e1), reach(tr, e2)) for e1, e2 in pairs]
    lists += [([a for a in sorted(tr.ins) if a0 <= a < a1], [b for b in sorted(tr.ins) if b0 <= b < b1])
              for a0, a1, b0, b1 in BLOCKS]
    for x, y in lists:
        sm = difflib.SequenceMatcher(None, [norm(tr.ins[a]) for a in x], [norm(tr.ins[b]) for b in y],
                                     autojunk=False)
        for tag, i1, i2, j1, j2 in sm.get_opcodes():
            same = tag == "equal" or (tag == "replace" and i2 - i1 == j2 - j1)
            if tag != "equal":
                diffs.append((x[i1:i2], y[j1:j2]))
            if not same:
                continue
            for a, b in zip(x[i1:i2], y[j1:j2]):
                if mnem(tr.ins[a]) != mnem(tr.ins[b]):
                    continue
                if values(tr.ins[a]) != values(tr.ins[b]) and (a, b) not in seen_v:
                    seen_v.add((a, b))
                    vdiffs.append((a, b))
                if m.get(a, b) != b:          # (the routines' own lining up comes first)
                    clash += 1
                m.setdefault(a, b)
                ra = re.findall(r"\$(ff[0-9a-f]{4})", ",".join(tr.ins[a].ops))
                rb = re.findall(r"\$(ff[0-9a-f]{4})", ",".join(tr.ins[b].ops))
                for u, v in zip(ra, rb):
                    ram.setdefault(int(u, 16), {}).setdefault(int(v, 16), []).append(a)
                    if u == v and 0xFF2EF6 <= int(u, 16) < 0xFF5400:
                        same_obj.add((a, b, u))
    if "--diff" in sys.argv:
        print("==== the same instructions with other numbers")
        for a, b in sorted(vdiffs):
            print("  %05X %-30s %05X %s" % (a, tr.ins[a].mnem + " " + ",".join(tr.ins[a].ops),
                                             b, tr.ins[b].mnem + " " + ",".join(tr.ins[b].ops)))
        print("==== player 2's copy using player 1's object (the same per-object word)")
        for a, b, u in sorted(same_obj):
            print("  %05X %-30s %05X %s" % (a, tr.ins[a].mnem + " " + ",".join(tr.ins[a].ops),
                                             b, tr.ins[b].mnem + " " + ",".join(tr.ins[b].ops)))
        print("==== other instructions")
        seen = set()
        for x, y in diffs:
            k = (tuple(x), tuple(y))
            if k in seen:
                continue
            seen.add(k)
            print("----")
            for a in x:
                print("  1 %05X %s %s" % (a, tr.ins[a].mnem, ",".join(tr.ins[a].ops)))
            for b in y:
                print("  2 %05X %s %s" % (b, tr.ins[b].mnem, ",".join(tr.ins[b].ops)))
        return
    with open(OUT, "w") as fh:
        fh.write("/* Generated by tools/player_map.py: player 1's code address -> the same place in\n"
                 " * player 2's copy (addresses only). */\n")
        fh.write("static const uint32_t player2_places[][2] = {\n")
        for a in sorted(m):
            fh.write("    {0x%05X, 0x%05X},\n" % (a, m[a]))
        fh.write("};\n")
        tables = {}
        for a, b in vdiffs:
            ia, ib = tr.ins[a], tr.ins[b]
            if ia.mnem.startswith("lea"):
                u, v = (int(re.match(r"\$([0-9a-f]+)", i.ops[0]).group(1), 16) for i in (ia, ib))
                tables.setdefault(u, set()).add(v)
        fh.write("/* player 1's tables -> player 2's (as the two copies use them) */\n")
        fh.write("static const uint32_t player2_tables[][2] = {\n")
        for u in sorted(tables):
            if len(tables[u]) > 1:
                print("table %05X: %s" % (u, " ".join("%05X" % v for v in sorted(tables[u]))), file=sys.stderr)
                continue
            fh.write("    {0x%05X, 0x%05X},\n" % (u, min(tables[u])))
        fh.write("};\n")
        fh.write("/* player 1's work memory -> player 2's (as the two copies use them) */\n")
        fh.write("static const uint32_t player2_words[][2] = {\n")
        for u in sorted(ram):
            vs = ram[u]
            if len(vs) > 1:                   # (a per-object table: by the object, o = 2 * p)
                continue
            v = list(vs)[0]
            fh.write("    {0x%06X, 0x%06X},\n" % (u, v))
        fh.write("};\n")
    print("%d places mapped (%d kept from the routines' own lining up), %d work memory words"
          % (len(m), clash, sum(len(v) == 1 for v in ram.values())))


if __name__ == "__main__":
    main()
