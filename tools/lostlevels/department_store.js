/* The arcade's Department Store (arcade stage 5, between Mint and Underground Gold Bars),
 * rebuilt with the Jewelry Store's graphics from its map (vgmaps.de, compiled by
 * DarkWolf), measured with tools/lostlevels/detect.py (department_store.layout.json):
 *   floors as in the arcade: 1F, 2F, 3F and the walkable roof, the exit on the roof's right
 *   ("Fantasy Land") going up; every staircase where the map has it, going the same way.
 * Map floors in this level: 0 = above the roof (only the exit), 1 = roof, 2 = 3F, 3 = 2F,
 * 4 = 1F (arcade floor index + 1). Arcade x maps to blocks across the building.
 * Returns a Bonanza Maker level. */
const LAYOUT = require('./department_store.layout.json');

module.exports = function departmentStore(LC, rom) {
  const E = 7, N = LC.info(rom, E).nmeta;
  const L = LC.readStage(rom, E);
  const g0 = L.guards.slice();
  const stamps = LC.makeStamps(L);
  const plain = (f, s) => { const h = LC.copyRegion(L, f, f, s, s); h.guards = []; h.items = []; return h; };
  const [bx0, bx1] = LAYOUT.building;
  const W = L.W, inner = W - 6;
  const slice = x => 3 + Math.round((x - bx0) / (bx1 - bx0) * inner);
  const PLAIN = 0x01010000, WALLV = 0xFFFF0000;

  /* 1. remove the Jewelry Store's stairs and exit: their slices become hallway */
  const hall = { 1: plain(1, 11), 2: plain(2, 10), 3: plain(3, 21) };
  const clear = (f, s0, s1) => { for (let s = s0; s <= s1; s++) LC.pasteRegion(L, hall[f], f, s, false); };
  clear(1, 4, 11); clear(2, 4, 11);            /* stairs up-right and the exit, left */
  clear(1, 60, 67); clear(2, 60, 67); clear(3, 59, 67);   /* stairs up-left, right */
  clear(3, 4, 36);                             /* 1F from the start to the shop: no cars or posts */
  for (const c of L.col) for (let i = 0; i < c.length; i++)
    if (LC.isStairInfo(c[i] & 0xFFFF) || LC.isExitInfo(c[i] & 0xFFFF) || ((c[i] >>> 16) & 255) === 0) c[i] = PLAIN;
  L.guards = [];
  L.items = L.items.filter(o => o.kind === LC.KIND.DOOR);

  /* 2. one more floor on top: the old top floor (the roof seen from outside) becomes the
   * walkable roof, the new top floor is sky with only the exit */
  LC.addFloor(L, 0, 0);
  const at = (f, r, s) => (f * 5 + r) * W + s;
  for (let s = 0; s < W; s++) {
    for (let r = 0; r < 5; r++) L.map[at(0, r, s)] = 0;
    for (let r = 0; r < 3; r++) L.map[at(1, r, s)] = 0;
    const edge = s === 2 ? [0x18, 0x16] : s === 3 ? [0x19, 0x17] : s === W - 3 ? [0x1E, 0x1F] : s > 3 && s < W - 3 ? [0x1A, 0x05] : [0, 0];
    L.map[at(1, 3, s)] = edge[0];
    L.map[at(1, 4, s)] = edge[1];
  }
  for (let c = 0; c < 2 * W; c++) {
    L.col[0][c] = WALLV;
    L.col[1][c] = c >= 6 && c < 2 * W - 6 ? PLAIN : WALLV;
  }

  /* 3. stairs where the arcade has them: the stage's staircase pictures, the game's
   * staircase floor (placeStairShape) under them */
  const ss = LC.stairStamps(stamps, N);
  const placeStairs = (f, x0, x1, dir) => LC.placeStairs(L, ss, f, slice((x0 + x1) / 2), dir);
  LAYOUT.floors.forEach((fl, a) => fl.stairs.forEach(s => placeStairs(a + 1, s.x0, s.x1, s.dir)));

  /* 4. the exit: the stage's exit stairs on the roof's right, going up */
  const exit = stamps.find(s => s.id === 'stairs-exit').clip;
  const es = slice(1600) - 3;
  LC.pasteRegion(L, exit, 0, es, false);
  for (let c = 2 * es; c < 2 * (es + exit.slices); c++) {
    const v = L.col[0][c];
    if (!LC.isStairInfo(v & 0xFFFF) && !LC.isExitInfo(v & 0xFFFF) && ((v >>> 16) & 255) !== 0) L.col[0][c] = WALLV;
  }
  for (let s = es; s < es + exit.slices; s++) for (let r = 0; r < 5; r++)
    if (!L.map[at(0, r, s)]) L.map[at(0, r, s)] = 0;

  /* 5. guards and treasures at the arcade's places (arcade x; floors 1 roof ... 4 1F) */
  const used = new Set();
  const free = (f, s, z) => [2 * s, 2 * s + 1].every(c => {
    const v = L.col[f][c]; if (v === undefined || (v & 0xFFFF)) return false;
    const m = z >= 8 ? (v >>> 16) & 255 : v >>> 24;
    return m === 1 && !used.has(f + ':' + s);
  });
  const spot = (f, s, z) => {
    for (let d = 0; d < 14; d++) for (const t of [s + d, s - d]) if (t > 3 && t < W - 4 && free(f, t, z)) {
      for (let k = -1; k <= 1; k++) used.add(f + ':' + (t + k));
      return t;
    }
    return null;
  };
  const guardsAt = { 1: [490, 700, 1040, 1250, 1770], 2: [255, 735, 1135, 1235, 1720], 3: [295, 720, 1110, 1265, 1500, 1660], 4: [370, 1225, 1340, 1730] };
  let gi = 0;
  for (const f of [4, 3, 2, 1]) for (const x of guardsAt[f]) {
    const o = g0[gi++ % g0.length], of = LC.floorOf(o.y, 4) + 1, t = spot(f, slice(x), o.z);
    if (t !== null) L.guards.push(Object.assign({}, o, { x: 32 + 16 * t + 8, y: o.y + 80 - 80 * of + 80 * f }));
  }
  used.clear();
  const P = LC.allItemPresets(rom).filter(p => p.kind === LC.KIND.TREASURE);
  const pick = (src, frame) => P.find(p => p.src === src && p.frame === frame);
  const loot = [[4, 1010, pick(7, 0x2e3)], [4, 1500, pick(2, 0x2c8)], [3, 1460, pick(7, 0x2e2)], [3, 1600, pick(3, 0x2d0)],
                [2, 960, pick(2, 0x2ca)], [2, 1400, pick(7, 0x2e0)], [1, 600, pick(11, 0x2f0)], [1, 1180, pick(7, 0x2df)]];
  for (const [f, x, p] of loot) {
    const t = spot(f, slice(x), p.z);
    if (t === null) continue;
    const o = { kind: p.kind, x: 32 + 16 * t + 8, y: 80 * f + p.yoff, z: p.z, frame: p.frame, beh: p.beh, dir: p.dir, attr: p.attr };
    if (p.src !== E) o.src = p.src;
    L.items.push(o);
  }
  /* the robbers start bottom left, by the bus (already there, one floor lower now) */
  L.extra = { slot: 0, after: 5, name: 'Department Store' };
  LC.fixStairs(L);
  return L;
};
