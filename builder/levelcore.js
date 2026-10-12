/* Bonanza Bros level maker - core: reads a stage from the ROM, edits it, writes it back as
 * an IPS patch (for a .bbro mod). Works in the browser (window.LevelCore) and in node.
 *
 * Level data (see the port's docs): the map (16 px blocks, 5 rows per 80 px floor) only
 * draws the level; walls, stairs, doors and the exit come from a per-floor collision table
 * with one 4-byte record per 8 px column: front-lane solid mask, back-lane solid mask,
 * info word (stairs 0x80|dir, door 0x90+2k = k-th item, exit 0xF0xx). The editor works in
 * "slices": one floor tall and 16 px wide = 5 map blocks + 2 collision columns, so the
 * picture and the walls always stay together. New data is put after the original 512 KiB. */
(function (root) {
  'use strict';
  const LC = {};
  LC.ROM_SIZE = 0x80000;
  LC.SHA1 = 'f7313ecaf7143873597e3a6ebf58d542d06c3ef3';
  /* The ROM has 13 stage entries: the game's 10 stages plus the scenes between them
   * (entries 4, 8 and 12). Tables are indexed by the entry; the game shows its own number. */
  LC.STAGE_NAMES = { 1: [1, 'Company'], 2: [2, "Millionaire's Mansion"], 3: [3, 'Casino'], 5: [4, 'Mint'],
    6: [5, 'Underground Gold Bars'], 7: [6, 'Jewelry Store'], 9: [7, 'Laboratory'], 10: [8, 'Deluxe Liner'],
    11: [9, 'Art Museum'], 13: [10, 'Pyramid'] };
  LC.stageNumber = e => (LC.STAGE_NAMES[e] || [e])[0];
  LC.stageLabel = e => LC.STAGE_NAMES[e] ? 'Stage ' + LC.STAGE_NAMES[e][0] + ': ' + LC.STAGE_NAMES[e][1] : 'Entry ' + e;
  LC.STAGES = [1, 2, 3, 5, 6, 7, 9, 10, 11, 13];
  LC.MAX_W = 127;                               /* 2 x W collision columns must fit 255 */
  LC.KIND = { TREASURE: 0x00, DOOR: 0x04, EXIT: 0x2C };
  LC.KIND_NAMES = { 0x00: 'Treasure', 0x04: 'Door', 0x08: 'Spring', 0x0C: 'Zip wire', 0x10: 'Trip hazard',
    0x14: 'Trip hazard 2', 0x18: 'Lever', 0x1C: 'Danger gate', 0x20: 'Floor hazard', 0x24: 'Machine',
    0x28: 'Stage 13 object', 0x2C: 'Exit' };
  const FRAMES_TBL = 0x24B80;
  const WALL = 0xFFFF0000 >>> 0;                /* solid in both lanes */

  /* ---------- ROM access ---------- */
  function R(rom) {
    return {
      rom,
      u8: a => rom[a],
      u16: a => (rom[a] << 8) | rom[a + 1],
      s16: a => { const v = (rom[a] << 8) | rom[a + 1]; return v & 0x8000 ? v - 0x10000 : v; },
      u32: a => ((rom[a] << 24) | (rom[a + 1] << 16) | (rom[a + 2] << 8) | rom[a + 3]) >>> 0,
    };
  }
  function w16(b, a, v) { b[a] = (v >> 8) & 255; b[a + 1] = v & 255; }
  function w32(b, a, v) { b[a] = (v >>> 24) & 255; b[a + 1] = (v >>> 16) & 255; b[a + 2] = (v >>> 8) & 255; b[a + 3] = v & 255; }

  LC.floorOf = (y, nf) => Math.max(0, Math.min(nf - 1, Math.floor((y - 8) / 80)));
  LC.sliceOf = x => Math.floor((x - 32) / 16);
  LC.isDoorInfo = i => (i & 0xFF) >= 0x90 && (i & 0xFF) <= 0xAE && !((i & 0xFF) & 1);
  LC.isExitInfo = i => (i & 0xF000) === 0xF000;
  LC.isStairInfo = i => (i & 0xF0) === 0x80;

  /* ---------- graphics (for drawing) ---------- */
  /* the stage's tile loads: lea (src).l,a6 / move.l #cmd,(VDP_CTRL).l / move.w #n,d7 / jsr packed|raw */
  function tileLoads(r, routine) {
    const loads = [];
    let p = routine, cur = {};
    for (let guard = 0; guard < 200; guard++) {
      const op = r.u16(p);
      if (op === 0x4E75) break;                                  /* rts */
      if (op === 0x4DF9) { cur = { src: r.u32(p + 2) }; p += 6; continue; }
      if (op === 0x23FC && r.u32(p + 6) === 0x00C00004) {
        const c = r.u32(p + 2);
        cur.vram = ((c >>> 16) & 0x3FFF) | ((c & 3) << 14);
        p += 10; continue;
      }
      if (op === 0x3E3C) { cur.n = r.u16(p + 2) + 1; p += 4; continue; }
      if (op === 0x4EB9) {
        const t = r.u32(p + 2);
        if ((t === 0x1146 || t === 0x119C) && cur.src !== undefined) { cur.packed = t === 0x1146; loads.push(cur); }
        cur = {}; p += 6; continue;
      }
      break;                                                      /* something else: stop */
    }
    return loads;
  }
  function decodeLoad(r, ld, vram) {
    let p = ld.src, v = ld.vram;
    for (let t = 0; t < ld.n; t++) {
      const buf = new Uint8Array(32);
      if (!ld.packed) { for (let i = 0; i < 32; i++) buf[i] = r.u8(p + i); p += 32; }
      else {
        let k = 0;
        for (let m = 0; m < 4; m++) {
          const mask = r.u8(p++);
          for (let b = 0; b < 8; b++) buf[k++] = mask & (0x80 >> b) ? r.u8(p++) : 0;
        }
        let prev = [0, 0, 0, 0];
        for (let row = 0; row < 8; row++) {
          for (let i = 0; i < 4; i++) buf[row * 4 + i] ^= prev[i];
          prev = [buf[row * 4], buf[row * 4 + 1], buf[row * 4 + 2], buf[row * 4 + 3]];
        }
      }
      for (let i = 0; i < 32; i++) vram[(v + i) & 0xFFFF] = buf[i];
      v += 32;
    }
  }
  function palette(r, st) {
    const id = r.u16(0x557E + 2 * (st - 1));
    const e = 0x1960 + id * 10;
    const n = r.u16(e) + 1, src = r.u32(e + 2);
    const pal = [];
    for (let i = 0; i < 64; i++) {
      const c = i < n ? r.u16(src + 2 * i) : 0;
      pal.push([((c >> 1) & 7) * 36, ((c >> 5) & 7) * 36, ((c >> 9) & 7) * 36]);
    }
    return pal;
  }

  /* ---------- collision ---------- */
  function readCollision(r, st, nf, W) {
    const e = 0x100FA + (st - 1) * 8;
    let p = r.u32(e);
    const nbuf = r.u32(e + 4) + 1;
    const bufs = [];
    for (let b = 0; b < nbuf; b++) {
      const first = r.u16(p); p += 2;
      const recs = [];
      for (;;) {
        let v = r.u32(p); p += 4;
        if (v === 0xFFFFFFFF) break;
        if (v & 0x100) { v = (v & ~0x100) >>> 0; const c = r.u16(p) >> 8; p += 2; for (let i = 0; i < c; i++) recs.push(v); }
        else recs.push(v);
      }
      bufs.push({ first, recs });
    }
    const list = r.u32(0xFFA2 + 4 * (st - 1));
    const floorBuf = [], col = [];
    for (let f = 0; f < nf; f++) {
      const b = ((r.u32(list + 4 * f) & 0xFFFFFF) - 0xFF57D0) / 0x400;
      floorBuf.push(b);
      const a = new Array(2 * W).fill(WALL);
      const { first, recs } = bufs[b];
      recs.forEach((v, i) => { const c = first / 4 + i; if (c < a.length) a[c] = v; });
      col.push(a);
    }
    return { col, floorBuf, nbuf };
  }

  /* start routines draw the first screen from the map with a 14-byte descriptor:
   * lea desc(pc),a6 / jsr (sub_017AC).l */
  function startDescriptors(r, routine) {
    const out = [];
    for (let p = routine; p < routine + 0x200; p += 2) {
      if (r.u16(p) === 0x4DFA && r.u16(p + 4) === 0x4EB9 && r.u32(p + 6) === 0x17AC) out.push(p + 2 + r.s16(p + 2));
      if (r.u16(p) === 0x4E75) break;
    }
    return out;
  }

  /* ---------- reading a stage ---------- */
  LC.readStage = function (rom, st) {
    const r = R(rom);
    const W = r.u16(0x54E2 + 4 * (st - 1)), H = r.u16(0x54E2 + 4 * (st - 1) + 2);
    const mapPtr = r.u32(0x5516 + 8 * (st - 1)), metaPtr = r.u32(0x5516 + 8 * (st - 1) + 4);
    /* Floors with collision; normally one per 5 map rows. Deluxe Liner has one more on top
     * than its map (a copy of the next one): its objects are 80 px lower than the map. */
    const nfCol = r.u16(0xFF86 + 2 * (st - 1)) + 1, nf = H / 5, base = nfCol - nf;
    const all = mapPtr && nf === Math.floor(nf) ? readCollision(r, st, nfCol, W) : null;
    if (!all || base < 0 || base > 1 || (base && all.floorBuf[0] !== all.floorBuf[1]))
      throw new Error(LC.stageLabel(st) + ' cannot be edited (its floors are set up differently).');
    const map = [];
    for (let i = 0; i < W * H; i++) map.push(r.u16(mapPtr + 2 * i));
    const col = all.col.slice(base);
    /* objects */
    let p = r.u32(0x3E000 + 4 * (st - 1));
    const groups = [];
    for (let g = 0; g < 4; g++) {
      const n = r.s16(p); p += 2;
      const ents = [];
      if (n >= 0 || g === 0)
        for (let i = 0; i <= n; i++, p += 14)
          ents.push({ x: r.s16(p), y: r.s16(p + 2), z: r.s16(p + 4), frame: r.u16(p + 6), beh: r.s16(p + 8),
                      dir: r.u16(p + 10), attr: r.u16(p + 12) });
      groups.push(ents);
    }
    const kp = r.u32(0x1ADE2 + 4 * (st - 1)), kn = r.s16(kp) + 1;
    groups[3].forEach((o, i) => { o.kind = i < kn ? r.u16(kp + 2 + 2 * i) : 0; });
    const gp = r.u32(0x14AB8 + 4 * (st - 1));
    groups[2].forEach((o, i) => { o.template = r.u16(gp + 2 * i); });
    for (const g of groups) for (const o of g) o.y -= 80 * base;     /* map coordinates */
    const level = {
      format: 'bbro-level', version: 1, stage: st, W, floors: nf, base, map,
      col, players: groups[0], guards: groups[2], items: groups[3].filter(o => o.kind !== LC.KIND.EXIT),
    };
    return level;
  };

  /* things that stay as in the original stage: graphics, presets, start screen */
  LC.stageInfo = function (rom, st) {
    const r = R(rom);
    const vram = new Uint8Array(65536);
    const routine = r.u32(0x640C + 4 * (st - 1));
    for (const ld of tileLoads(r, routine)) decodeLoad(r, ld, vram);
    const orig = LC.readStage(rom, st);
    const metaPtr = r.u32(0x5516 + 8 * (st - 1) + 4);
    let nmeta = 0;
    for (const m of orig.map) nmeta = Math.max(nmeta, m + 1);
    const meta = [];
    for (let i = 0; i < nmeta; i++) meta.push([0, 1, 2, 3].map(k => r.u16(metaPtr + i * 8 + k * 2)));
    /* background plane: {vram addr, cols-1, rows-1, tile base, src} */
    const bgp = r.u32(0x5598 + 4 * (st - 1));
    const bg = { cols: r.u16(bgp + 2) + 1, rows: r.u16(bgp + 4) + 1, base: r.u16(bgp + 6), src: r.u32(bgp + 8) };
    bg.cells = [];
    for (let i = 0; i < bg.cols * bg.rows; i++) bg.cells.push((r.u16(bg.src + 2 * i) + bg.base) & 0xFFFF);
    /* presets: the kinds of guards and items this stage has */
    const nf = orig.floors, guardPresets = [], itemPresets = [], seen = new Set();
    for (const g of orig.guards) {
      const k = [g.template, g.attr, g.z].join();
      if (seen.has(k)) continue;
      seen.add(k);
      const f = LC.floorOf(g.y, nf);
      guardPresets.push({ template: g.template, attr: g.attr, z: g.z, yoff: g.y - 80 * f, frame: g.frame, beh: g.beh, dir: g.dir });
    }
    seen.clear();
    for (const o of orig.items) {
      if (o.kind === LC.KIND.DOOR) continue;
      const k = [o.kind, o.frame, o.z, o.beh, o.dir, o.attr].join();
      if (seen.has(k)) continue;
      seen.add(k);
      const f = LC.floorOf(o.y, nf);
      itemPresets.push({ kind: o.kind, frame: o.frame, z: o.z, yoff: o.y - 80 * f, beh: o.beh, dir: o.dir, attr: o.attr });
    }
    const d1 = startDescriptors(r, r.u32(0x4566 + 4 * (st - 1)));
    const d2 = startDescriptors(r, r.u32(0x459A + 4 * (st - 1)));
    return { vram, pal: palette(r, st), meta, bg, guardPresets, itemPresets, descriptors: d1.concat(d2), orig };
  };

  /* a sprite frame's pieces: {dx, dy, w, h (cells), attr} */
  LC.spritePieces = function (rom, frame) {
    const r = R(rom);
    if (!frame) return [];
    let p = r.u32(FRAMES_TBL + frame * 4) & 0xFFFFFF;
    if (p + 2 > rom.length) return [];
    const n = (r.u16(p) >> 8) + 1;
    p += 2;
    const out = [];
    for (let i = 0; i < n && p + 6 <= rom.length; i++, p += 6) {
      const dy = (r.u8(p) << 24) >> 24, dx = (r.u8(p + 1) << 24) >> 24, sz = r.u8(p + 2);
      out.push({ dx, dy, w: ((sz >> 2) & 3) + 1, h: (sz & 3) + 1, attr: r.u16(p + 4) });
    }
    return out;
  };

  /* ---------- editing: slices ---------- */
  function objectsIn(level, f0, f1, s0, s1) {
    const nf = level.floors, inR = o => {
      const f = LC.floorOf(o.y, nf), s = LC.sliceOf(o.x);
      return f >= f0 && f <= f1 && s >= s0 && s <= s1;
    };
    return { guards: level.guards.filter(inR), items: level.items.filter(inR) };
  }
  LC.copyRegion = function (level, f0, f1, s0, s1) {
    const W = level.W, clip = { floors: f1 - f0 + 1, slices: s1 - s0 + 1, map: [], col: [], guards: [], items: [] };
    for (let f = f0; f <= f1; f++) {
      for (let row = 0; row < 5; row++)
        clip.map.push(level.map.slice((f * 5 + row) * W + s0, (f * 5 + row) * W + s1 + 1));
      clip.col.push(level.col[f].slice(2 * s0, 2 * s1 + 2));
    }
    const o = objectsIn(level, f0, f1, s0, s1);
    const rel = x => Object.assign({}, x, { x: x.x - s0 * 16, y: x.y - f0 * 80 });
    clip.guards = o.guards.map(rel);
    clip.items = o.items.map(rel);
    return clip;
  };
  LC.removeObjects = function (level, f0, f1, s0, s1) {
    const o = objectsIn(level, f0, f1, s0, s1);
    level.guards = level.guards.filter(g => !o.guards.includes(g));
    level.items = level.items.filter(g => !o.items.includes(g));
  };
  LC.pasteRegion = function (level, clip, f, s, withObjects) {
    const W = level.W;
    const nfl = Math.min(clip.floors, level.floors - f), ns = Math.min(clip.slices, W - s);
    if (nfl <= 0 || ns <= 0) return;
    LC.removeObjects(level, f, f + nfl - 1, s, s + ns - 1);
    for (let k = 0; k < nfl; k++) {
      for (let row = 0; row < 5; row++)
        for (let i = 0; i < ns; i++) level.map[((f + k) * 5 + row) * W + s + i] = clip.map[k * 5 + row][i];
      for (let i = 0; i < 2 * ns; i++) level.col[f + k][2 * s + i] = clip.col[k][i];
    }
    if (withObjects === false) return;
    const abs = x => Object.assign({}, x, { x: x.x + s * 16, y: x.y + f * 80 });
    const fits = o => LC.floorOf(o.y, level.floors) < f + nfl && LC.sliceOf(o.x) < s + ns;
    level.guards.push(...clip.guards.map(abs).filter(fits));
    level.items.push(...clip.items.map(abs).filter(fits));
  };
  /* insert n slices before slice s on every floor, copies of slice s-1 (widens rooms) */
  LC.insertSlices = function (level, s, n) {
    const W = level.W, src = Math.max(0, s - 1);
    if (W + n > LC.MAX_W) throw new Error('A level can be at most ' + LC.MAX_W + ' blocks wide.');
    const map = [];
    for (let row = 0; row < level.floors * 5; row++) {
      const line = level.map.slice(row * W, row * W + W);
      line.splice(s, 0, ...new Array(n).fill(line[src]));
      map.push(...line);
    }
    level.map = map;
    for (let f = 0; f < level.floors; f++) {
      const c = level.col[f], a = c[2 * src], b = c[2 * src + 1];
      /* a copied door/exit column would make a second door/exit: copy it as plain floor */
      const plain = v => LC.isDoorInfo(v & 0xFFFF) ? ((v & 0xFF000000) | 0x00010000) >>> 0
                       : LC.isExitInfo(v & 0xFFFF) ? (v & ~0xF000) >>> 0 : v;
      const ins = [];
      for (let i = 0; i < n; i++) ins.push(plain(a), plain(b));
      c.splice(2 * s, 0, ...ins);
    }
    const shift = o => { if (LC.sliceOf(o.x) >= s) o.x += 16 * n; };
    level.guards.forEach(shift);
    level.items.forEach(shift);
    level.W = W + n;
  };
  LC.removeSlices = function (level, s, n) {
    const W = level.W;
    n = Math.min(n, W - s);
    if (W - n < 20) throw new Error('A level must be at least 20 blocks wide.');
    LC.removeObjects(level, 0, level.floors - 1, s, s + n - 1);
    const map = [];
    for (let row = 0; row < level.floors * 5; row++) {
      const line = level.map.slice(row * W, row * W + W);
      line.splice(s, n);
      map.push(...line);
    }
    level.map = map;
    for (let f = 0; f < level.floors; f++) level.col[f].splice(2 * s, 2 * n);
    const shift = o => { if (LC.sliceOf(o.x) >= s + n) o.x -= 16 * n; };
    level.guards.forEach(shift);
    level.items.forEach(shift);
    level.W = W - n;
  };

  /* ---------- stamps: ready-made parts taken from the stage itself ---------- */
  const bits = v => { let n = 0; for (; v; v >>= 1) n += v & 1; return n; };
  const plainCol = v => (v >>> 16) === 0x0101 && !(v & 0xFFFF);
  const wallCol = v => (v >>> 16) === 0xFFFF && !(v & 0xFFFF);
  LC.makeStamps = function (level) {
    const out = [], nf = level.floors, W = level.W;
    const clean = (clip, keepDoors) => {
      clip.guards = [];
      clip.items = keepDoors ? clip.items.filter(o => o.kind === LC.KIND.DOOR) : [];
      return clip;
    };
    /* stairs: the ramp is on the lower floor (back-lane masks 03 07 0F ...), the opening and
     * landing on the floor above; a stamp covers both floors */
    const seenStairs = {};
    for (let f = 1; f < nf; f++) {
      const c = level.col[f];
      for (let i = 0; i < c.length; i++) {
        const back = (c[i] >>> 16) & 255;
        if (!LC.isStairInfo(c[i] & 0xFFFF) || bits(back) < 2 || bits(back) > 7) continue;
        let j = i;
        while (j + 1 < c.length && LC.isStairInfo(c[j + 1] & 0xFFFF)) j++;
        /* the part on the floor above */
        let lo = i, hi = j;
        const up = level.col[f - 1];
        for (let k = Math.max(0, i - 6); k <= Math.min(up.length - 1, j + 6); k++)
          if (LC.isStairInfo(up[k] & 0xFFFF)) { lo = Math.min(lo, k); hi = Math.max(hi, k); }
        let exit = false;
        for (let k = lo; k <= hi; k++) if (LC.isExitInfo(c[k] & 0xFFFF) || LC.isExitInfo(up[k] & 0xFFFF)) exit = true;
        const dir = c[i] & 3;
        const kind = exit ? 'exit' : dir === 2 ? 'up-right' : 'up-left';
        if (!seenStairs[kind]) {
          seenStairs[kind] = 1;
          const s0 = Math.max(0, (lo >> 1) - 1), s1 = Math.min(W - 1, (hi >> 1) + 1);
          out.push({ id: 'stairs-' + kind, group: 'build', floors: 2,
                     name: exit ? 'Exit stairs' : dir === 2 ? 'Stairs up \u2197' : 'Stairs up \u2196',
                     clip: clean(LC.copyRegion(level, f - 1, f, s0, s1), false) });
        }
        i = j;
      }
    }
    /* a door, a wall, and the hallway pieces the stage uses most */
    let door = null, wall = null;
    const hall = new Map();
    for (let f = 0; f < nf; f++) {
      const c = level.col[f];
      for (let s = 1; s < W - 1; s++) {
        const a = c[2 * s], b = c[2 * s + 1];
        if (!door && (LC.isDoorInfo(a & 0xFFFF) || LC.isDoorInfo(b & 0xFFFF)))
          door = { name: 'Door', id: 'door', group: 'build', floors: 1, clip: clean(LC.copyRegion(level, f, f, s - 1, s + 1), true) };
        if (!wall && wallCol(a) && wallCol(b) && wallCol(c[2 * s + 2]) && wallCol(c[2 * s - 1]))
          wall = { name: 'Wall', id: 'wall', group: 'build', floors: 1, clip: clean(LC.copyRegion(level, f, f, s, s), false) };
        if (plainCol(a) && plainCol(b)) {
          const key = [0, 1, 2, 3, 4].map(r => level.map[(f * 5 + r) * W + s]).join();
          const h = hall.get(key) || { n: 0, f, s };
          h.n++;
          hall.set(key, h);
        }
      }
    }
    if (door) out.push(door);
    if (wall) out.push(wall);
    [...hall.values()].sort((x, y) => y.n - x.n).slice(0, 8).forEach((h, k) =>
      out.push({ name: k ? 'Hallway ' + (k + 1) : 'Hallway', id: 'hall' + k, group: 'build', floors: 1,
                 clip: clean(LC.copyRegion(level, h.f, h.f, h.s, h.s), false) }));
    return out;
  };

  /* ---------- checking and writing ---------- */
  /* doors (paired with their door columns, numbered) first, then the other items, then the exit */
  LC.resolveItems = function (level) {
    const problems = [], doors = [];
    const used = new Set();
    let exit = null;
    for (let f = 0; f < level.floors; f++)
      level.col[f].forEach((v, c) => {
        const info = v & 0xFFFF;
        if (LC.isExitInfo(info) && !exit) exit = { f, c };
        if (!LC.isDoorInfo(info)) return;
        const cx = c * 8 + 32;
        let best = null;
        for (const o of level.items)
          if (o.kind === LC.KIND.DOOR && !used.has(o) && LC.floorOf(o.y, level.floors) === f && Math.abs(o.x - cx) <= 14 &&
              (!best || Math.abs(o.x - cx) < Math.abs(best.x - cx))) best = o;
        if (best) used.add(best);
        doors.push({ f, c, obj: best || { kind: LC.KIND.DOOR, x: cx + 4, y: 80 * f + 66, z: 0, frame: 0x300, beh: 1, dir: 1, attr: 0 } });
      });
    if (doors.length > 16) problems.push('Too many doors: ' + doors.length + ' (at most 16).');
    const others = level.items.filter(o => o.kind !== LC.KIND.DOOR);
    const nlost = level.items.filter(o => o.kind === LC.KIND.DOOR && !used.has(o)).length;
    if (nlost) problems.push(nlost + ' door object(s) without a door in the wall were left out.');
    if (!exit) problems.push('There is no exit: keep (or paste) the stairs with the EXIT sign.');
    const exitObj = exit ? { kind: LC.KIND.EXIT, x: exit.c * 8 + 32, y: 80 * exit.f + 40, z: 16, frame: 0, beh: 0, dir: 0, attr: 0 } : null;
    const items = doors.map(d => d.obj).concat(others, exitObj ? [exitObj] : []);
    if (items.length > 32) problems.push('Too many items: ' + items.length + ' (doors + treasures + others + exit, at most 32).');
    if (level.guards.length > 32) problems.push('Too many guards: ' + level.guards.length + ' (at most 32).');
    const treasures = others.filter(o => o.kind === LC.KIND.TREASURE).length;
    if (!treasures) problems.push('There are no treasures.');
    for (const p of level.players) {
      const f = LC.floorOf(p.y, level.floors), c = Math.floor((p.x - 32) / 8);
      const v = level.col[f][c];
      if (v !== undefined && ((v >>> 24) & 0xFE)) problems.push('A player starts inside a wall (floor ' + (f + 1) + ').');
    }
    return { doors, items, exit: exitObj, treasures, problems };
  };

  LC.exportLevel = function (rom, level) {
    const r = R(rom), st = level.stage, W = level.W, nf = level.floors, H = nf * 5;
    const res = LC.resolveItems(level);
    if (res.problems.some(p => /Too many|no exit|no treasures/.test(p))) throw new Error(res.problems.join('\n'));
    const out = [];
    const base = LC.ROM_SIZE;
    const emit = bytes => { const a = base + out.length; out.push(...bytes); if (out.length & 1) out.push(0xFF); return a; };
    const words = ws => { const b = []; for (const w of ws) b.push((w >> 8) & 255, w & 255); return b; };
    const longs = ls => { const b = []; for (const l of ls) b.push((l >>> 24) & 255, (l >>> 16) & 255, (l >>> 8) & 255, l & 255); return b; };
    const yb = 80 * (level.base || 0);
    /* map (a stage with a floor above its map gets that floor's rows before it: the
     * game draws them at the start, out of sight) */
    if (level.map.length !== W * H) throw new Error('map size');
    const pre = [];
    for (let k = 0; k < 5 * (level.base || 0); k++) pre.push(...level.map.slice(0, W));
    const mapAddr = emit(words(pre.concat(level.map))) + 2 * pre.length;
    /* collision, numbered door columns; buffers in the original RAM order */
    const col = level.col.map(c => c.slice());
    res.doors.forEach((d, k) => { col[d.f][d.c] = ((col[d.f][d.c] & 0xFFFFFF00) | (0x90 + 2 * k)) >>> 0; });
    const base_ = level.base || 0;
    const orig = readCollision(r, st, nf + base_, W);
    const bufFloor = [];
    orig.floorBuf.forEach((b, f) => { if (f >= base_) bufFloor[b] = f - base_; });
    const cb = [];
    for (let b = 0; b < orig.nbuf; b++) {
      const c = bufFloor[b] !== undefined ? col[bufFloor[b]] : new Array(2 * W).fill(WALL);
      cb.push(...words([0]));
      for (let i = 0; i < c.length;) {
        let n = 1;
        while (i + n < c.length && c[i + n] === c[i] && n < 255) n++;
        if (c[i] & 0x100) throw new Error('collision record with bit 8 set');
        if (n >= 2) { cb.push(...longs([(c[i] | 0x100) >>> 0]), n, 0); }
        else cb.push(...longs([c[i]]));
        i += n;
      }
      cb.push(...longs([0xFFFFFFFF]));
    }
    const colAddr = emit(cb);
    /* objects */
    const ent = o => words([o.x & 0xFFFF, (o.y + yb) & 0xFFFF, o.z & 0xFFFF, o.frame & 0xFFFF, o.beh & 0xFFFF, o.dir & 0xFFFF, o.attr & 0xFFFF]);
    const pl = [];
    pl.push(...words([level.players.length - 1]));
    level.players.forEach(o => pl.push(...ent(o)));
    pl.push(...words([0xFFFF]));
    pl.push(...words([level.guards.length ? level.guards.length - 1 : 0xFFFF]));
    level.guards.forEach(o => pl.push(...ent(o)));
    pl.push(...words([res.items.length - 1]));
    res.items.forEach(o => pl.push(...ent(o)));
    const placeAddr = emit(pl);
    const kindAddr = emit(words([res.items.length - 1, ...res.items.map(o => o.kind)]));
    const gl = level.guards.map(g => g.template);
    while (gl.length < 32) gl.push(gl.length ? gl[0] : 0);
    const guardAddr = emit(words(gl));
    /* the new ROM */
    const size = base + out.length;
    const rom2 = new Uint8Array(size);
    rom2.set(rom.subarray(0, base));
    rom2.set(out, base);
    w16(rom2, 0x54E2 + 4 * (st - 1), W);
    w16(rom2, 0x54E2 + 4 * (st - 1) + 2, H);
    const oldMap = r.u32(0x5516 + 8 * (st - 1)), oldW = r.u16(0x54E2 + 4 * (st - 1));
    w32(rom2, 0x5516 + 8 * (st - 1), mapAddr);
    w32(rom2, 0x100FA + 8 * (st - 1), colAddr);
    w32(rom2, 0x100FA + 8 * (st - 1) + 4, orig.nbuf - 1);
    w32(rom2, 0x3E000 + 4 * (st - 1), placeAddr);
    w32(rom2, 0x1ADE2 + 4 * (st - 1), kindAddr);
    w32(rom2, 0x14AB8 + 4 * (st - 1), guardAddr);
    w16(rom2, 0x1AF60 + 2 * (st - 1), res.treasures);
    /* first screen: the start routines' map pointers, same row and column in the new map */
    for (const d of LC.stageInfo(rom, st).descriptors) {
      const off = (r.u32(d) - oldMap) / 2;
      const row = Math.floor(off / oldW), c = off - row * oldW;
      w32(rom2, d, mapAddr + 2 * (row * W + c));
    }
    /* header: ROM end and checksum */
    w32(rom2, 0x1A4, size - 1);
    let sum = 0;
    for (let a = 0x200; a + 1 < size; a += 2) sum = (sum + ((rom2[a] << 8) | rom2[a + 1])) & 0xFFFF;
    w16(rom2, 0x18E, sum);
    return { rom: rom2, problems: res.problems };
  };

  LC.makeIps = function (orig, patched) {
    const out = [0x50, 0x41, 0x54, 0x43, 0x48];
    const n = patched.length;
    for (let a = 0; a < n;) {
      if (a < orig.length && orig[a] === patched[a]) { a++; continue; }
      let e = a;
      while (e < n && e - a < 0xFFFF && !(e < orig.length && orig[e] === patched[e] && e + 1 < orig.length && orig[e + 1] === patched[e + 1])) e++;
      out.push((a >> 16) & 255, (a >> 8) & 255, a & 255, ((e - a) >> 8) & 255, (e - a) & 255);
      for (let i = a; i < e; i++) out.push(patched[i]);
      a = e;
    }
    out.push(0x45, 0x4F, 0x46);
    return new Uint8Array(out);
  };

  LC.modIni = function (info) {
    const clean = s => String(s || '').replace(/[\r\n]+/g, ' ').trim();
    return 'name = ' + clean(info.name) + '\nauthor = ' + clean(info.author) + '\nversion = ' + clean(info.version || '1.0') +
      '\ndescription = ' + clean(info.description) + '\n';
  };

  if (typeof module !== 'undefined' && module.exports) module.exports = LC;
  else root.LevelCore = LC;
})(typeof window !== 'undefined' ? window : this);
