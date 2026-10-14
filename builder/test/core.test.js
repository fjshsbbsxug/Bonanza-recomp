#!/usr/bin/env node
/* Tests for Bonanza Maker's core (builder/levelcore.js):  node builder/test/core.test.js ROM
 * Every stage: unedited export, stamps / stairs / doors / objects / painted blocks placed
 * at several places, flip, width changes, borrowed blocks and treasures, moved start,
 * extra-stage export, and the level surviving a JSON round trip (what undo and save do). */
const LC = require('../levelcore.js'), fs = require('fs');
const rom = new Uint8Array(fs.readFileSync(process.argv[2] || 'baserom.md'));
let failed = 0, passed = 0;
const ok = (cond, what) => { if (cond) passed++; else { failed++; console.log('FAIL', what); } };
const tryit = (what, fn) => { try { return fn(); } catch (e) { failed++; console.log('FAIL', what + ':', e.message.split('\n')[0]); } };
const clone = L => JSON.parse(JSON.stringify(L));
const exportOk = (L, what) => tryit(what, () => {
  L = clone(L);
  LC.fixStairs(L);                     /* the editor does this after every change */
  const r = LC.resolveItems(L);
  const bad = r.problems.filter(p => !/door object\(s\) without a door/.test(p));
  ok(!bad.length, what + ' problems: ' + bad.join(' | '));
  const out = LC.exportLevel(rom, clone(L));
  ok(out.rom.length > LC.ROM_SIZE, what + ' exported');
  return out;
});

for (const e of LC.STAGES) {
  const name = 'stage ' + LC.stageNumber(e);
  const inf = LC.info(rom, e), N = inf.nmeta;
  const base = LC.readStage(rom, e);
  const stamps = LC.makeStamps(base);
  /* unedited export: only the stage's own pointers change */
  exportOk(base, name + ' unedited');
  /* every stamp at 3 places on every floor it fits */
  for (const s of stamps)
    for (let f = 0; f + s.clip.floors <= base.floors; f++)
      for (const at of [4, Math.floor(base.W / 2), base.W - s.clip.slices - 4]) {
        const L = clone(base);
        tryit(name + ' paste ' + s.id, () => LC.pasteRegion(L, s.clip, f, at, true));
      }
  /* stairs piece both ways, door piece, painted stairs */
  const ss = LC.stairStamps(stamps, N);
  if (ss) for (const d of ['right', 'left']) {
    const L = clone(base);
    LC.pasteRegion(L, ss[d], base.floors - 2, 20, true);
    exportOk(L, name + ' stairs ' + d);
    const twice = LC.flipClip(LC.flipClip(ss[d], N), N);
    ok(JSON.stringify(twice.map) === JSON.stringify(ss[d].map) && JSON.stringify(twice.col) === JSON.stringify(ss[d].col),
       name + ' flip twice ' + d);
  }
  const dp = LC.doorPiece(stamps);
  if (dp) { const L = clone(base); LC.pasteRegion(L, dp, base.floors - 1, 25, true); exportOk(L, name + ' door piece'); }
  {
    const L = clone(base);
    const f = base.floors - 1;
    for (const b of [12, 30, base.W - 12])
      try { LC.placeStairShape(L, f, b, b === 30 ? 1 : 2); } catch (err) { /* refused there: fine */ }
    exportOk(L, name + ' painted stairs');
  }
  /* painted blocks with every kind */
  {
    const L = clone(base);
    LC.BLOCK_KINDS.forEach((k, i) => LC.paintBlock(L, (base.floors - 1) * 5 + 3, 10 + i, 5, k.id));
    exportOk(L, name + ' painted kinds');
    for (let i = 0; i < 5; i++) {
      const before = L.col[base.floors - 1][2 * (10 + i)];
      LC.paintBlock(L, (base.floors - 1) * 5 + 3, 10 + i, 5, LC.blockKindAt(L, (base.floors - 1) * 5 + 3, 10 + i));
      ok(L.col[base.floors - 1][2 * (10 + i)] === before, name + ' kind round trip ' + i);
    }
  }
  /* borrowed blocks (from two other stages) + a borrowed treasure + flip + width changes */
  {
    const L = clone(base);
    const others = LC.STAGES.filter(x => x !== e).slice(0, 2);
    others.forEach((o, k) => { for (let i = 0; i < 3; i++) L.map[((base.floors - 1) * 5 + 1) * L.W + 6 + 4 * k + i] = LC.borrowBlock(L, N, o, 20 + i); });
    const room = LC.blockRoom(rom, L);
    if (!room.problems.length) {
      const t = LC.allItemPresets(rom).find(p => p.kind === LC.KIND.TREASURE && p.src !== e);
      L.items.push({ kind: t.kind, x: 32 + 16 * 15 + 8, y: 80 * (base.floors - 1) + t.yoff, z: t.z, frame: t.frame, beh: t.beh, dir: t.dir, attr: t.attr, src: t.src });
      const cl = LC.flipClip(LC.copyRegion(L, base.floors - 1, base.floors - 1, 6, 9), N);
      ok(cl.map[1].every(m => m < 2 * N || (m - 2 * N) % 2 === 1), name + ' flipped borrowed = mirrored');
      LC.insertSlices(L, 8, 2);
      /* remove 2 blocks where there is no door or exit */
      let at = 30;
      const busy = k => L.col.some(c => [2 * k, 2 * k + 1, 2 * k + 2, 2 * k + 3].some(i => LC.isDoorInfo(c[i] & 0xFFFF) || LC.isExitInfo(c[i] & 0xFFFF)));
      while (busy(at) && at < L.W - 10) at++;
      LC.removeSlices(L, at, 2);
      exportOk(L, name + ' borrowed + width');
      const L2 = clone(L);
      ok(JSON.stringify(L2) === JSON.stringify(L), name + ' JSON round trip');
    }
  }
  /* moved start (to the middle of the bottom floor) + width changes keep it in the building */
  {
    const L = clone(base);
    const f = LC.floorOf(L.players[0].y, L.floors), mid = 32 + 16 * Math.floor(L.W / 2) + 8;
    L.players.forEach(p => { p.x = mid; });
    LC.insertSlices(L, 2, 3);
    LC.removeSlices(L, Math.floor(L.W / 2) - 1, 3);
    ok(L.players.every(p => p.x >= 32 && p.x < 32 + 16 * L.W), name + ' start inside after width change');
    if (!LC.startProblem(L)) tryit(name + ' moved start export', () => LC.exportLevel(rom, clone(L)));
    ok(f >= 0, name + ' floor');
  }
  /* cutting a staircase with width changes leaves no broken stairs at export */
  {
    const L = clone(base);
    LC.removeSlices(L, 20, 3);
    LC.insertSlices(L, 30, 2);
    LC.fixStairs(L);
    ok(!LC.resolveItems(L).problems.some(p => /broken/.test(p)), name + ' stairs fixed after cuts');
  }
  /* objects of the stage (if available) */
  if (LC.stageObjects) {
    const objs = LC.stageObjects(rom, e);
    ok(objs.length > 0, name + ' has objects');
    const L = clone(base);
    let placed = 0;
    let doors = 0;
    for (const o of objs.slice(0, 40).filter(o => o.kind !== 'Doors' || doors++ < 3))
      try { LC.placeObject(L, o, base.floors - 1, 6 + (placed * 7) % (base.W - 20), placed % 2 === 1, N); placed++; } catch (err) { /* refused: fine */ }
    ok(placed > 0, name + ' objects placed');
    exportOk(L, name + ' objects');
    /* objects of another stage (borrowed blocks) */
    const other = LC.STAGES.find(x => x !== e), L2 = clone(base);
    let n2 = 0;
    for (const o of LC.stageObjects(rom, other).filter(o => o.kind !== 'Stairs').slice(0, 3))
      try { LC.placeObject(L2, o, base.floors - 1, 8 + 10 * n2, n2 === 1, N); n2++; } catch (err) { /* refused */ }
    if (!LC.blockRoom(rom, L2).problems.length) exportOk(L2, name + ' objects from stage ' + LC.stageNumber(other));
    const kinds = {};
    objs.forEach(o => { kinds[o.kind] = (kinds[o.kind] || 0) + 1; });
    console.log(name, 'objects:', JSON.stringify(kinds));
  }
  /* more / fewer floors (3 to 5) */
  if (!base.base) {
    const L = clone(base);
    if (L.floors < 5) { LC.addFloor(L, 0, 0); exportOk(L, name + ' one more floor'); }
    const L2 = clone(base);
    if (L2.floors > 3) { LC.removeFloor(L2, 0); if (!LC.startProblem(L2)) tryit(name + ' one floor less', () => LC.exportLevel(rom, clone(L2))); }
  }
  /* collision areas: each kind, read back, and the level still exports */
  {
    const L = clone(base), f = base.floors - 1;
    const kinds = ['front', 'back', 'wall', 'open'];
    kinds.forEach((k, i) => LC.setCollision(L, f, 20 + 4 * i, 21 + 4 * i, 1, 5, k));
    kinds.forEach((k, i) => ok([1, 3, 5].every(r => LC.collisionAt(L, f, 20 + 4 * i, r) === k && LC.collisionAt(L, f, 21 + 4 * i, r) === k),
                               name + ' collision ' + k));
    ok(LC.setCollision(L, f, 20, 21, 1, 5, 'front') === 0, name + ' collision unchanged twice');
    exportOk(L, name + ' collision areas');
  }
  /* every gadget of every stage (moved to free tiles where its own are used here) */
  {
    const L = clone(base), f = base.floors - 1;
    const gadgets = LC.allItemPresets(rom).filter(o => o.kind !== LC.KIND.TREASURE && o.kind !== LC.KIND.DOOR && o.kind !== LC.KIND.EXIT);
    const usable = gadgets.filter(o => LC.itemUse(rom, e, o).mode !== 'none');
    const none = gadgets.filter(o => LC.itemUse(rom, e, o).mode === 'none');
    if (none.length) console.log(name, 'gadgets that cannot be used:', none.map(o => LC.KIND_NAMES[o.kind] + ' (stage ' + LC.stageNumber(o.src) + ')').join(', '));
    const seen = new Set();
    usable.forEach((o, i) => {
      if (seen.has(o.kind + ':' + o.src) || LC.resolveItems(L).items.length >= 31) return;
      seen.add(o.kind + ':' + o.src);
      L.items.push({ kind: o.kind, x: 32 + 16 * (6 + 4 * seen.size) + 8, y: 80 * f + o.yoff, z: o.z, frame: o.frame, beh: o.beh, dir: o.dir, attr: o.attr, src: o.src });
    });
    if (!LC.borrowProblems(rom, L).length) exportOk(L, name + ' every gadget');
    else console.log(name, 'every gadget:', LC.borrowProblems(rom, L).join(' | '));
  }
  /* extra stage on a host */
  if (LC.EXTRA_HOSTS.includes(e)) {
    const L = clone(base);
    L.extra = { slot: 0, after: 5, name: 'Test Stage' };
    exportOk(L, name + ' as extra stage');
  }
}
/* the Department Store */
tryit('department store', () => {
  const L = require('../../tools/lostlevels/department_store.js')(LC, rom);
  exportOk(L, 'department store');
});
console.log(passed + ' passed, ' + failed + ' failed');
process.exit(failed ? 1 : 0);
