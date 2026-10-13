/* The arcade's Department Store (arcade stage 5, between Mint and Underground Gold Bars),
 * rebuilt from its map (vgmaps.de, compiled by DarkWolf) with the Jewelry Store's graphics:
 * 1F street level with the start bottom left, 2F and 3F shops, stairs where the arcade has
 * them (the Jewelry Store's two stair stacks, one mirrored), the exit up to the roof on the
 * right, guards and treasures at the arcade's places (in 16 px blocks; floors 0 roof,
 * 1 = 3F, 2 = 2F, 3 = 1F). Returns a Bonanza Maker level. */
module.exports = function departmentStore(LC, rom) {
  const E = 7, N = LC.info(rom, E).nmeta;
  const L = LC.readStage(rom, E);
  const g0 = L.guards.slice();
  L.guards = [];
  L.items = L.items.filter(o => o.kind === LC.KIND.DOOR);
  const strip = c => (c.guards = [], c.items = c.items.filter(o => o.kind === LC.KIND.DOOR), c);
  // the two stair stacks
  const stackL = strip(LC.copyRegion(L, 0, 2, 4, 11));    // 2F->3F rising right, exit 3F->roof
  const stackR = strip(LC.copyRegion(L, 1, 3, 60, 66));   // 1F->2F->3F rising left (a car in front)
  const carEnd = strip(LC.copyRegion(L, 3, 3, 59, 59));   // the rest of that car
  // plain slices per floor
  const plain = (f, s) => { const h = LC.copyRegion(L, f, f, s, s); h.guards = []; h.items = []; return h; };
  const hall = { 0: plain(0, 13), 1: plain(1, 11), 2: plain(2, 10), 3: plain(3, 21) };
  const clear = (f0, f1, s0, s1) => {
    for (let f = f0; f <= f1; f++) for (let s = s0; s <= s1; s++) LC.pasteRegion(L, hall[f], f, s, false);
    LC.removeObjects(L, f0, f1, s0, s1);
  };
  clear(0, 0, 5, 11);
  clear(1, 2, 4, 11);
  clear(3, 3, 59, 66);
  clear(3, 3, 12, 12);
  // left: 1F->2F->3F rising right
  LC.pasteRegion(L, LC.flipClip(stackR, N), 1, 13, true);
  LC.pasteRegion(L, LC.flipClip(carEnd, N), 3, 20, true);
  for (const s of [13, 14]) L.map[(3 * 5 + 1) * L.W + s] = L.map[(3 * 5 + 1) * L.W + 21];   // no mirrored JEWEL sign
  // middle: 1F->2F->3F rising left
  LC.pasteRegion(L, stackR, 1, 44, true);
  LC.pasteRegion(L, carEnd, 3, 43, true);
  // right: 2F->3F and the exit stairs up to the roof
  LC.pasteRegion(L, stackL, 0, 59, true);
  L.map[3 * L.W + 67] = 0x1E; L.map[4 * L.W + 67] = 0x1F;          // the roof's right edge
  // a slice where something in lane z can stand: plain floor in that lane, no stairs/doors
  const used = new Set();
  const free = (f, s, z) => [2 * s, 2 * s + 1].every(c => {
    const v = L.col[f][c]; if (v === undefined || (v & 0xFFFF)) return false;
    const m = z >= 8 ? (v >>> 16) & 255 : v >>> 24;
    return m === 1 && !used.has(f + ':' + s);
  });
  const spot = (f, s, z) => {
    for (let d = 0; d < 12; d++) for (const t of [s + d, s - d]) if (t > 3 && t < L.W - 4 && free(f, t, z)) {
      for (let k = -1; k <= 1; k++) used.add(f + ':' + (t + k));
      return t;
    }
    throw new Error('no room near ' + f + ':' + s);
  };
  // guards at the arcade's places; floors: 0 roof, 1 = 3F, 2 = 2F, 3 = 1F
  const F = f => g0.filter(g => LC.floorOf(g.y, 4) === f);
  const pool = { 3: F(3).slice(0, 3), 2: F(2).concat(F(3).slice(3), F(1).slice(5)), 1: F(1).slice(0, 5) };
  const gpos = { 3: [47, 51, 66], 2: [11, 27, 42, 44, 48, 57, 64], 1: [10, 28, 43, 47, 66] };
  for (const f of [3, 2, 1]) gpos[f].forEach((s, i) => {
    const o = pool[f][i], of = LC.floorOf(o.y, 4), t = spot(f, s, o.z);
    L.guards.push(Object.assign({}, o, { x: 32 + 16 * t + 8, y: o.y - 80 * of + 80 * f }));
  });
  used.clear();
  // treasures: this stage's own and some from other stages (a department store sells everything)
  const P = LC.allItemPresets(rom).filter(p => p.kind === LC.KIND.TREASURE);
  const pick = (src, frame) => P.find(p => p.src === src && p.frame === frame);
  const tpos = [[3, 40, pick(7, 0x2e3)], [3, 60, pick(2, 0x2c8)], [2, 56, pick(7, 0x2e2)], [2, 67, pick(3, 0x2d0)],
                [1, 10, pick(2, 0x2ca)], [1, 38, pick(7, 0x2e0)], [1, 55, pick(11, 0x2f0)], [1, 58, pick(7, 0x2df)]];
  for (const [f, s, p] of tpos) {
    const t = spot(f, s, p.z);
    const o = { kind: p.kind, x: 32 + 16 * t + 8, y: 80 * f + p.yoff, z: p.z, frame: p.frame, beh: p.beh, dir: p.dir, attr: p.attr };
    if (p.src !== E) o.src = p.src;
    L.items.push(o);
  }
  L.extra = { slot: 0, after: 5, name: 'Department Store' };
  return L;
};
