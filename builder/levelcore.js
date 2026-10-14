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
  LC.MAX_W = 127;
  LC.SLOT_SIZE = 0x20000;
  /* ROM region for a level's data: stage entries 1-13, then the extra stages */
  LC.slotBase = level => LC.ROM_SIZE + LC.SLOT_SIZE * (level.extra ? 13 + (level.extra.slot || 0) : level.stage - 1);                               /* 2 x W collision columns must fit 255 */
  LC.KIND = { TREASURE: 0x00, DOOR: 0x04, EXIT: 0x2C };
  LC.KIND_NAMES = { 0x00: 'Treasure', 0x04: 'Door', 0x08: 'Spring', 0x0C: 'Zip wire', 0x10: 'Trip hazard',
    0x14: 'Trip hazard 2', 0x18: 'Lever', 0x1C: 'Danger gate', 0x20: 'Floor hazard', 0x24: 'Machine',
    0x28: 'Pyramid trap', 0x2C: 'Exit' };
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
  LC.tileLoads = (rom, routine) => tileLoads(R(rom), routine);
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
    /* mirrored copies (for flipped stamps) at m + nmeta: tiles swapped left/right, hflip toggled */
    for (let i = 0; i < nmeta; i++) { const [a, b, c, d] = meta[i]; meta.push([b ^ 0x800, a ^ 0x800, d ^ 0x800, c ^ 0x800]); }
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
    return { vram, pal: palette(r, st), meta, nmeta, metaPtr, bg, guardPresets, itemPresets, descriptors: d1.concat(d2), orig };
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
  /* a clip mirrored left to right: mirrored metatiles (m <-> m + N, N = the stage's metatile
   * count), collision columns reversed with the stair direction swapped, objects mirrored */
  const swapDir = v => { const lo = v & 0xFF; return (lo & 0xF0) === 0x80 && (lo & 3) ? ((v & ~3) | (3 - (lo & 3))) >>> 0 : v; };
  LC.flipClip = function (clip, N) {
    const w = clip.slices * 16, mir = o => Object.assign({}, o, { x: 64 + w - o.x, dir: o.dir === 1 ? 2 : o.dir === 2 ? 1 : o.dir });
    /* own blocks m <-> m + N; blocks from other stages are pairs from 2N on: bit 0 */
    const fm = m => m < N ? m + N : m < 2 * N ? m - N : m ^ 1;
    return {
      floors: clip.floors, slices: clip.slices,
      map: clip.map.map(row => row.slice().reverse().map(fm)),
      col: clip.col.map(c => c.slice().reverse().map(swapDir)),
      guards: clip.guards.map(mir), items: clip.items.map(mir),
    };
  };
  LC.flipLevel = function (level, N) {
    const clip = LC.flipClip(LC.copyRegion(level, 0, level.floors - 1, 0, level.W - 1), N);
    LC.pasteRegion(level, clip, 0, 0, true);
  };

  /* add a floor above floor `at` (0 = a new top floor): its pictures and walls are a copy
   * of floor `like`; everything below moves down 80 px */
  LC.addFloor = function (level, at, like) {
    if (level.floors >= 5) throw new Error('A level can have at most 5 floors.');
    if (level.base) throw new Error('This stage cannot get more floors.');
    const W = level.W, src = like === undefined ? at : like;
    const rows = level.map.slice(src * 5 * W, (src + 1) * 5 * W);
    level.map.splice(at * 5 * W, 0, ...rows);
    level.col.splice(at, 0, level.col[src].map(v => LC.isDoorInfo(v & 0xFFFF) || LC.isStairInfo(v & 0xFFFF) || LC.isExitInfo(v & 0xFFFF) ? 0x01010000 : v));
    for (const o of level.guards.concat(level.items, level.players)) if (LC.floorOf(o.y, level.floors) >= at) o.y += 80;
    level.floors++;
  };
  LC.removeFloor = function (level, at) {
    if (level.floors <= 3) throw new Error('A level needs at least 3 floors.');
    const W = level.W;
    LC.removeObjects(level, at, at, 0, W - 1);
    for (const o of level.guards.concat(level.items, level.players)) if (LC.floorOf(o.y, level.floors) > at) o.y -= 80;
    level.map.splice(at * 5 * W, 5 * W);
    level.col.splice(at, 1);
    level.floors--;
  };

  /* insert n slices before slice s on every floor, copies of slice s-1 (widens rooms) */
  LC.insertSlices = function (level, s, n) {
    const W = level.W;
    if (W + n > LC.MAX_W) throw new Error('A level can be at most ' + LC.MAX_W + ' blocks wide.');
    /* never split a staircase: space goes in after it (or before, near the end) */
    const inStairs = k => level.col.some(c => [2 * k - 1, 2 * k].some(i => {
      const v = c[i];
      return v !== undefined && (LC.isStairInfo(v & 0xFFFF) || ((v >>> 16) & 255) === 0);
    }));
    let t = s;
    while (t < W && inStairs(t)) t++;
    if (t >= W) { t = s; while (t > 1 && inStairs(t)) t--; }
    s = t;
    /* per floor, the copied slice is the nearest one at the left (else right) that is not
     * part of a staircase, so a staircase is never stretched */
    const stairy = (f, k) => [level.col[f][2 * k], level.col[f][2 * k + 1]].some(v =>
      v === undefined || LC.isStairInfo(v & 0xFFFF) || ((v >>> 16) & 255) === 0);
    const srcOf = [];
    for (let f = 0; f < level.floors; f++) {
      let k = Math.max(0, s - 1);
      while (k > 0 && stairy(f, k)) k--;
      if (stairy(f, k)) { k = s; while (k < W - 1 && stairy(f, k)) k++; }
      srcOf.push(stairy(f, k) ? Math.max(0, s - 1) : k);
    }
    const map = [];
    for (let row = 0; row < level.floors * 5; row++) {
      const line = level.map.slice(row * W, row * W + W);
      line.splice(s, 0, ...new Array(n).fill(line[srcOf[Math.floor(row / 5)]]));
      map.push(...line);
    }
    level.map = map;
    for (let f = 0; f < level.floors; f++) {
      const src = srcOf[f], c = level.col[f], a = c[2 * src], b = c[2 * src + 1];
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
    level.players.forEach(shift);
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
    /* robbers in the removed part start where it was */
    level.players.forEach(o => { const k = LC.sliceOf(o.x); if (k >= s + n) o.x -= 16 * n; else if (k >= s) o.x -= 16 * (k - s); });
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

  /* ---------- blocks with physics ----------
   * A 16 px block at row k (0..4) of a floor covers two collision columns and mask rows
   * 2k, 2k+1 of that floor (bit7 = top row ... bit1 = row 6, bit0 = rows 7-9, the slab). */
  LC.BLOCK_KINDS = [
    { id: 'picture', name: 'Picture only', tip: 'only the picture changes' },
    { id: 'solid', name: 'Solid', tip: 'solid in both lanes: low ones are jumped over, a stack is a wall' },
    { id: 'front', name: 'Front lane', tip: 'solid in the front lane only: walk behind it' },
    { id: 'back', name: 'Back lane', tip: 'solid in the back lane only: walk in front of it' },
    { id: 'open', name: 'Walk-through', tip: 'nothing solid there' },
    { id: 'stairs-r', name: 'Stairs \u2197', tip: 'working stairs up to the right from here to the floor above (5 blocks wide)' },
    { id: 'stairs-l', name: 'Stairs \u2196', tip: 'working stairs up to the left from here to the floor above (5 blocks wide)' },
  ];
  const ROW_BITS = [0xC0, 0x30, 0x0C, 0x03, 0x01];
  /* the kind of the block at map row r, block column c (from its two columns' bits) */
  LC.blockKindAt = function (level, r, c) {
    const f = Math.floor(r / 5), bits = ROW_BITS[r % 5] & 0xFE || 0x01;
    const cols = [level.col[f][2 * c], level.col[f][2 * c + 1]];
    const has = sh => cols.every(v => ((v >>> sh) & 255 & bits) === bits);
    const none = sh => cols.every(v => !((v >>> sh) & 255 & bits));
    if (!(has(24) || none(24)) || !(has(16) || none(16))) return 'picture';   /* the two halves differ */
    const fr = has(24), bk = has(16);
    return fr && bk ? 'solid' : fr ? 'front' : bk ? 'back' : 'open';
  };
  /* the collision of an area of floor f, in 8 px cells: columns c0..c1 (two per block),
   * rows r0..r1 (0 = the ceiling, 7 to 9 = the floor itself, one bit) made solid in the
   * front lane, the back lane, both ('wall') or neither ('open'). Doors, stairs and the exit
   * keep their use; returns the number of columns changed. */
  LC.COLLISION_KINDS = [
    { id: 'front', name: 'Front lane', tip: 'solid in the front lane only: robbers and guards walk behind it' },
    { id: 'back', name: 'Back lane', tip: 'solid in the back lane only: walk in front of it' },
    { id: 'wall', name: 'Wall', tip: 'solid in both lanes: a wall (low ones can be jumped over)' },
    { id: 'open', name: 'Open', tip: 'nothing solid: walk through' },
  ];
  LC.cellBit = row => row < 8 ? 0x80 >> row : 0x01;
  LC.setCollision = function (level, f, c0, c1, r0, r1, kind) {
    const col = level.col[f];
    if (!col) return 0;
    let bits = 0;
    for (let r = Math.max(0, Math.min(r0, r1)); r <= Math.min(9, Math.max(r0, r1)); r++) bits |= LC.cellBit(r);
    const fr = kind === 'front' || kind === 'wall', bk = kind === 'back' || kind === 'wall';
    let n = 0;
    for (let c = Math.max(0, Math.min(c0, c1)); c <= Math.min(col.length - 1, Math.max(c0, c1)); c++) {
      const v = col[c];
      let front = v >>> 24, back = (v >>> 16) & 255;
      front = fr ? front | bits : front & ~bits;
      back = bk ? back | bits : back & ~bits;
      const nv = ((front << 24) | (back << 16) | (v & 0xFFFF)) >>> 0;
      if (nv !== v) { col[c] = nv; n++; }
    }
    return n;
  };
  /* what is solid in one 8 px cell: 'wall', 'front', 'back' or 'open' */
  LC.collisionAt = function (level, f, c, row) {
    const v = (level.col[f] || [])[c];
    if (v === undefined) return 'open';
    const b = LC.cellBit(row), fr = (v >>> 24) & b, bk = (v >>> 16) & b;
    return fr && bk ? 'wall' : fr ? 'front' : bk ? 'back' : 'open';
  };
  /* paint a block; returns false where the physics could not be changed (stairs, doors) */
  LC.paintBlock = function (level, r, c, meta, kind) {
    level.map[r * level.W + c] = meta;
    if (!kind || kind === 'picture' || /^stairs/.test(kind)) return true;
    const f = Math.floor(r / 5), bits = ROW_BITS[r % 5] & 0xFE;   /* the slab bit stays */
    let ok = true;
    for (const i of [2 * c, 2 * c + 1]) {
      const v = level.col[f][i];
      if (v === undefined) continue;
      if (v & 0xFFFF) { ok = false; continue; }
      let fr = (v >>> 24) & 255, bk = (v >>> 16) & 255;
      const set = (m, on) => on ? m | bits : m & ~bits;
      fr = set(fr, kind === 'solid' || kind === 'front') | 1;
      bk = set(bk, kind === 'solid' || kind === 'back') | 1;
      level.col[f][i] = ((fr << 24) | (bk << 16)) >>> 0;
    }
    return ok;
  };

  /* ---------- blocks from other stages ----------
   * VRAM tiles each stage leaves alone during play (measured: never written after the
   * stage starts, never shown, empty; the queued sprite loads stay below 0x200 and in the
   * font area). Borrowed treasures use 0x7A0-0x7CF, so these are for blocks. */
  LC.FREE_TILES = {1: [[0x338, 0x3d5], [0x3e0, 0x3ff], [0x41e, 0x41f], [0x466, 0x47c], [0x48f, 0x4da], [0x4ff, 0x4ff]], 2: [[0x4e0, 0x4ea], [0x4ff, 0x4ff]], 3: [[0x38d, 0x3d5], [0x3e0, 0x3ec], [0x3f0, 0x3ff], [0x41e, 0x42b], [0x4a7, 0x4de], [0x4ff, 0x4ff]], 5: [[0x3c8, 0x3ef], [0x428, 0x42b], [0x466, 0x47c], [0x4e0, 0x4e6], [0x4ff, 0x4ff]], 6: [[0x338, 0x38c], [0x3c8, 0x3df], [0x434, 0x440], [0x44d, 0x465], [0x480, 0x48e], [0x4b6, 0x4e6], [0x4ff, 0x4ff]], 7: [[0x38d, 0x3d5], [0x3e0, 0x3ec], [0x3f0, 0x3ff], [0x41e, 0x41f], [0x4e0, 0x4e6], [0x4ff, 0x4ff]], 9: [[0x38d, 0x3d5], [0x3e0, 0x3ec], [0x3f0, 0x3ff], [0x41e, 0x41f], [0x493, 0x4e2], [0x4ff, 0x4ff]], 10: [[0x338, 0x3b5], [0x4e0, 0x4e6], [0x4ff, 0x4ff]], 11: [[0x38d, 0x3d5], [0x3e0, 0x3ec], [0x3f0, 0x3ff], [0x41e, 0x41f], [0x4e0, 0x4e6], [0x4ff, 0x4ff]], 13: [[0x3c8, 0x3d5], [0x434, 0x440], [0x44d, 0x465], [0x480, 0x48e], [0x4a1, 0x4da], [0x4ff, 0x4ff]]};
  LC.freeTileCount = st => (LC.FREE_TILES[st] || []).reduce((n, [a, b]) => n + b - a + 1, 0);
  /* a block of stage src as it looks in stage st: its 4 cells, each a name-table word
   * (priority and flips kept, the palette line that fits best here) and the tile's
   * pixels with colours of that line */
  const lookCache = new Map();
  LC.blockLook = function (rom, st, src, meta) {
    const key = st + ':' + src + ':' + meta;
    if (lookCache.has(key)) return lookCache.get(key);
    const si = LC.info(rom, src), ti = LC.info(rom, st);
    const cells = si.meta[meta].map(w => {
      const tile = w & 0x7FF, pl = (w >> 13) & 3, px = [];
      for (let i = 0; i < 32; i++) { const b = si.vram[(tile * 32 + i) & 0xFFFF]; px.push(b >> 4, b & 15); }
      const used = [...new Set(px)].filter(c => c);
      const near = (L, c) => {
        const [r0, g0, b0] = si.pal[pl * 16 + c];
        let best = 1, bd = 1e9;
        for (let k = 1; k < 16; k++) {
          const [r1, g1, b1] = ti.pal[L * 16 + k], d = (r1 - r0) ** 2 + (g1 - g0) ** 2 + (b1 - b0) ** 2;
          if (d < bd) { bd = d; best = k; }
        }
        return [best, bd];
      };
      let L = pl, err = 1e18;
      for (let k = 0; k < 4; k++) {
        const e = used.reduce((n, c) => n + near(k, c)[1], 0) + (k === pl ? -1 : 0);   /* ties: keep the line */
        if (e < err) { err = e; L = k; }
      }
      const map = new Map(used.map(c => [c, near(L, c)[0]]));
      const bytes = [];
      for (let i = 0; i < 64; i += 2) bytes.push(((map.get(px[i]) || 0) << 4) | (map.get(px[i + 1]) || 0));
      return { word: (w & 0x9800) | (L << 13), bytes };
    });
    lookCache.set(key, cells);
    return cells;
  };
  /* the index of a borrowed block in this level (added if new): 2N + 2k, mirrored 2N + 2k + 1 */
  LC.borrowBlock = function (level, N, src, meta) {
    level.blocks = level.blocks || [];
    let k = level.blocks.findIndex(b => b.src === src && b.meta === meta);
    if (k < 0) { k = level.blocks.length; level.blocks.push({ src, meta }); }
    return 2 * N + 2 * k;
  };
  /* tiles and metatile words for the level's borrowed blocks that are used */
  function blockPlan(rom, level, N) {
    const st = level.stage, out = { meta: [], loads: [], tiles: 0, problems: [] };
    const used = new Set(level.map.filter(m => m >= 2 * N));
    const free = [];
    for (const [a, b] of LC.FREE_TILES[st] || []) for (let t = a; t <= b; t++) free.push(t);
    const placed = new Map();
    (level.blocks || []).forEach((bl, k) => {
      if (!used.has(2 * N + 2 * k) && !used.has(2 * N + 2 * k + 1)) {
        out.meta.push([0, 0, 0, 0], [0, 0, 0, 0]);
        return;
      }
      const words = LC.blockLook(rom, st, bl.src, bl.meta).map(c => {
        const key = c.bytes.join();
        if (!placed.has(key)) {
          const t = free[placed.size];
          placed.set(key, t);
          if (t !== undefined) out.loads.push({ tile: t, bytes: c.bytes });
        }
        const t = placed.get(key);
        return t === undefined ? 0 : (c.word | t) & 0xFFFF;
      });
      const [a, b, c, d] = words;
      out.meta.push(words, [b ^ 0x800, a ^ 0x800, d ^ 0x800, c ^ 0x800]);
    });
    out.tiles = placed.size;
    if (placed.size > free.length)
      out.problems.push('Too many blocks from other stages: ' + placed.size + ' different pieces (8x8), this stage has room for ' +
                        free.length + '.');
    return out;
  }
  LC.blockRoom = (rom, level) => {
    const N = LC.info(rom, level.stage).nmeta, p = blockPlan(rom, level, N);
    return { used: p.tiles, free: LC.freeTileCount(level.stage), problems: p.problems };
  };

  /* blocks of a stage sorted into groups by where the stage uses them (most used first) */
  LC.BLOCK_GROUPS = ['Stairs', 'Doors', 'Walls', 'Floors', 'Furniture', 'Background', 'Other'];
  const groupCache = new Map();
  LC.blockGroups = function (rom, e) {
    const key = e;
    if (groupCache.has(key)) return groupCache.get(key);
    const L = LC.readStage(rom, e), N = LC.info(rom, e).nmeta;
    const votes = Array.from({ length: N }, () => ({}));
    for (let r = 0; r < L.floors * 5; r++)
      for (let c = 0; c < L.W; c++) {
        const m = L.map[r * L.W + c], f = Math.floor(r / 5), k = r % 5;
        if (m >= N) continue;
        const cols = [L.col[f][2 * c], L.col[f][2 * c + 1]];
        const info = cols.map(v => v & 0xFFFF), bitsAt = [0xC0, 0x30, 0x0C, 0x02, 0][k];
        let g;
        if (info.some(i => LC.isStairInfo(i))) g = 'Stairs';
        else if (info.some(i => LC.isDoorInfo(i))) g = 'Doors';
        else if (cols.every(v => (v >>> 24) === 0xFF && ((v >>> 16) & 255) === 0xFF)) g = 'Walls';
        else if (k === 4) g = 'Floors';
        else if (cols.some(v => ((v >>> 24) & bitsAt) || (((v >>> 16) & 255) & bitsAt))) g = 'Furniture';
        else g = 'Background';
        votes[m][g] = (votes[m][g] || 0) + 1;
      }
    const groups = {};
    LC.BLOCK_GROUPS.forEach(g => { groups[g] = []; });
    votes.forEach((v, m) => {
      const best = Object.keys(v).sort((a, b) => v[b] - v[a])[0];
      groups[best || 'Other'].push({ m, n: best ? v[best] : 0 });
    });
    const out = LC.BLOCK_GROUPS.map(g => ({ name: g, blocks: groups[g].sort((a, b) => b.n - a.n).map(x => x.m) })).filter(g => g.blocks.length);
    groupCache.set(key, out);
    return out;
  };

  /* ---------- the stage's objects ----------
   * Blocks the stage uses together: on each floor, everything that is not plain hallway
   * (the blocks of the stage's hallway and wall stamps, row by row) is cut into connected
   * groups (at most 8 blocks wide). Identical groups are merged; each keeps its pictures,
   * the solid rows of its columns, and what it is in the game. */
  LC.OBJECT_KINDS = ['Stairs', 'Doors', 'Furniture', 'On the wall', 'Floor', 'Other'];
  const objCache = new Map();
  LC.stageObjects = function (rom, e) {
    if (objCache.has(e)) return objCache.get(e);
    const L = LC.readStage(rom, e), W = L.W, stamps = LC.makeStamps(L), srcN = LC.info(rom, e).nmeta;
    const hall = [0, 1, 2, 3, 4].map(() => new Set());
    for (const s of stamps)
      if (/^hall0$|^hall1$/.test(s.id)) s.clip.map.forEach((row, k) => row.forEach(m => hall[k % 5].add(m)));
    const seen = new Uint8Array(W * L.floors * 5), found = new Map();
    for (let f = 0; f < L.floors; f++) {
      const at = (r, c) => (f * 5 + r) * W + c;
      /* background on this floor: the hallway blocks and the 2 commonest blocks of each row */
      const plain = hall.map((h, r) => {
        const n = new Map();
        for (let c = 0; c < W; c++) n.set(L.map[at(r, c)], (n.get(L.map[at(r, c)]) || 0) + 1);
        const top = [...n.entries()].sort((a, b) => b[1] - a[1]).slice(0, 2).filter(x => x[1] >= 4).map(x => x[0]);
        return new Set([...h, ...top]);
      });
      /* doors, walls and pillars (a whole block column of non-background) are objects of their own */
      const alone = c => [2 * c, 2 * c + 1].some(i => LC.isDoorInfo(L.col[f][i] & 0xFFFF) ||
                                                       ((L.col[f][i] >>> 24) === 0xFF && ((L.col[f][i] >>> 16) & 255) === 0xFF)) ||
                         [0, 1, 2, 3, 4].every(r => !plain[r].has(L.map[at(r, c)]));
      for (let r0 = 0; r0 < 5; r0++)
        for (let c0 = 0; c0 < W; c0++) {
          if (seen[at(r0, c0)] || plain[r0].has(L.map[at(r0, c0)])) continue;
          /* flood fill on this floor */
          const cells = [], stack = [[r0, c0]];
          seen[at(r0, c0)] = 1;
          while (stack.length) {
            const [r, c] = stack.pop();
            cells.push([r, c]);
            for (const [dr, dc] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
              const rr = r + dr, cc = c + dc;
              if (rr < 0 || rr > 4 || cc < 0 || cc >= W || seen[at(rr, cc)] || plain[rr].has(L.map[at(rr, cc)])) continue;
              if (dc && (alone(c) || alone(cc))) continue;               /* never joined sideways */
              seen[at(rr, cc)] = 1;
              stack.push([rr, cc]);
            }
          }
          /* split wide groups into pieces of at most 8 blocks */
          const cmin = Math.min(...cells.map(x => x[1])), cmax = Math.max(...cells.map(x => x[1]));
          for (let x0 = cmin; x0 <= cmax; x0 += 8) {
            const part = cells.filter(([, c]) => c >= x0 && c < x0 + 8);
            if (!part.length) continue;
            const rs = part.map(x => x[0]), cs = part.map(x => x[1]);
            const top = Math.min(...rs), h = Math.max(...rs) - top + 1, left = Math.min(...cs), w = Math.max(...cs) - left + 1;
            if (part.length < 2 && !alone(left)) continue;               /* slivers */
            const grid = Array.from({ length: h }, () => new Array(w).fill(-1));
            for (const [r, c] of part) grid[r - top][c - left] = L.map[at(r, c)];
            const key = top + ':' + JSON.stringify(grid);
            if (found.has(key)) { found.get(key).count++; continue; }
            /* what the game does there */
            const cols = L.col[f].slice(2 * left, 2 * (left + w));
            let rowBits = 0;
            for (let k = top; k < top + h; k++) rowBits |= [0xC0, 0x30, 0x0C, 0x02, 0][k];
            const infos = cols.map(v => v & 0xFFFF);
            let kind;
            if (infos.some(i => LC.isExitInfo(i))) continue;            /* the exit stairs stamp has those */
            if (infos.some(i => LC.isStairInfo(i))) kind = 'Stairs';
            else if (infos.some(i => LC.isDoorInfo(i))) kind = 'Doors';
            else if (cols.some(v => ((v >>> 24) | (v >>> 16)) & rowBits)) kind = 'Furniture';
            else if (top + h <= 4) kind = 'On the wall';
            else if (top === 4) kind = 'Floor';
            else kind = 'Other';
            const ramp = infos.map((i, k) => LC.isStairInfo(i) && bits((cols[k] >>> 16) & 255) >= 2 && bits((cols[k] >>> 16) & 255) <= 7 ? k : -1).filter(k => k >= 0);
            const door = L.items.find(o => o.kind === LC.KIND.DOOR && LC.floorOf(o.y, L.floors) === f && LC.sliceOf(o.x) >= left && LC.sliceOf(o.x) < left + w);
            found.set(key, {
              src: e, srcN, kind, top, w, h, grid, cols, rowBits, count: 1,
              stairs: kind === 'Stairs' && ramp.length ? { dir: infos[ramp[0]] & 3, first: ramp[0], last: ramp[ramp.length - 1] } : null,
              door: kind === 'Doors' && door ? Object.assign({}, door, { x: door.x - 32 - 16 * left, y: door.y - 80 * f }) : null,
            });
          }
        }
    }
    const out = [...found.values()].filter(o => o.kind !== 'Stairs' || o.stairs)
      .sort((a, b) => LC.OBJECT_KINDS.indexOf(a.kind) - LC.OBJECT_KINDS.indexOf(b.kind) || b.count - a.count);
    objCache.set(e, out);
    return out;
  };
  /* an object mirrored left to right (blocks, columns, stair direction, door) */
  LC.mirrorObject = function (o, N) {
    const fm = m => m < 0 ? m : m < N ? m + N : m < 2 * N ? m - N : m ^ 1;
    const cols = o.cols.slice().reverse().map(swapDir), n = cols.length;
    return Object.assign({}, o, {
      grid: o.grid.map(row => row.slice().reverse().map(fm)), cols, mirrored: !o.mirrored,
      stairs: o.stairs ? { dir: 3 - o.stairs.dir, first: n - 1 - o.stairs.last, last: n - 1 - o.stairs.first } : null,
      door: o.door ? Object.assign({}, o.door, { x: 16 * o.w - o.door.x }) : null,
    });
  };
  /* place an object at block column c of floor f (its rows as in its stage). Blocks of another
   * stage are borrowed; its solid rows go with it; stairs and doors work. */
  LC.placeObject = function (level, o, f, c, mirror, N) {
    if (mirror) o = LC.mirrorObject(o, o.srcN);
    if (c < 1 || c + o.w > level.W - 1) throw new Error('It does not fit there.');
    const pick = m => m < 0 ? m : o.src === level.stage ? m
      : (m >= o.srcN ? LC.borrowBlock(level, N, o.src, m - o.srcN) + 1 : LC.borrowBlock(level, N, o.src, m));
    for (let r = 0; r < o.h; r++)
      for (let k = 0; k < o.w; k++) {
        const m = o.grid[r][k];
        if (m >= 0) level.map[(f * 5 + o.top + r) * level.W + c + k] = pick(m);
      }
    if (o.stairs) {
      const b = o.stairs.dir === 2 ? c + (o.stairs.first >> 1) : c + (o.stairs.last >> 1);
      LC.placeStairShape(level, f, b, o.stairs.dir);
      return;
    }
    const col = level.col[f];
    o.cols.forEach((v, i) => {
      const t = 2 * c + i, cur = col[t];
      if (cur === undefined || (cur & 0xFFFF)) return;               /* stairs/doors/exit stay */
      if (LC.isDoorInfo(v & 0xFFFF)) { col[t] = v >>> 0; return; }   /* the door column as it is */
      const fr = (((cur >>> 24) & ~o.rowBits) | ((v >>> 24) & o.rowBits) | 1) & 255;
      const bk = ((((cur >>> 16) & 255) & ~o.rowBits) | (((v >>> 16) & 255) & o.rowBits) | 1) & 255;
      col[t] = ((fr << 24) | (bk << 16)) >>> 0;
    });
    if (o.door) {
      LC.removeObjects(level, f, f, c, c + o.w - 1);
      level.items = level.items.filter(x => !(x.kind === LC.KIND.DOOR && LC.floorOf(x.y, level.floors) === f &&
                                              LC.sliceOf(x.x) >= c && LC.sliceOf(x.x) < c + o.w));
      level.items.push(Object.assign({}, o.door, { x: o.door.x + 32 + 16 * c, y: o.door.y + 80 * f }));
    }
  };

  /* ---------- a working staircase without its pictures ----------
   * The game's staircase (stage 1's, the plainest): on the lower floor a ramp of 7 columns,
   * above it a hole and a 3-column landing; 10 columns = 5 blocks. Rising right (dir 2)
   * from the block clicked; rising left (dir 1) is the mirror, ending at the block. */
  const STAIR_LOWER = [0x01030082, 0x01070082, 0x010F0082, 0x011F0082, 0x013F0282, 0x017F0282, 0x01FF0282];
  const STAIR_UPPER = { 2: 0x01FF0000, 3: 0x01000000, 4: 0x01000000, 5: 0x01000000, 6: 0x01000000,
                        7: 0x01004082, 8: 0x01008082, 9: 0x01010082 };
  /* the columns a staircase at block b of floor f takes: [{f, c, v}] */
  LC.stairShape = function (level, f, b, dir) {
    const out = [], first = dir === 1 ? 2 * b - 8 : 2 * b;
    const at = rel => dir === 1 ? first + 9 - rel : first + rel;
    STAIR_LOWER.forEach((v, rel) => out.push({ f, c: at(rel), v: dir === 1 ? swapDir(v) : v }));
    for (const rel in STAIR_UPPER) out.push({ f: f - 1, c: at(+rel), v: dir === 1 ? swapDir(STAIR_UPPER[rel]) : STAIR_UPPER[rel] });
    return out;
  };
  LC.placeStairShape = function (level, f, b, dir) {
    if (f < 1) throw new Error('Stairs go up to the floor above: start them on a lower floor.');
    const cols = LC.stairShape(level, f, b, dir);
    for (const { f: ff, c } of cols) {
      if (c < 2 || c > 2 * level.W - 3) throw new Error('The stairs do not fit there (too close to the end of the building).');
      const i = level.col[ff][c] & 0xFFFF;
      if (LC.isDoorInfo(i) || LC.isExitInfo(i)) throw new Error('The stairs would go through a door or the exit.');
    }
    for (const { f: ff, c, v } of cols) level.col[ff][c] = v >>> 0;
    return cols;
  };
  /* the Door piece: the middle block of the stage's door stamp (its pillar picture, the
   * door column, the door) */
  LC.doorPiece = function (stamps) {
    const d = stamps.find(s => s.id === 'door');
    if (!d) return null;
    const c = d.clip;
    return { floors: 1, slices: 1, map: c.map.map(row => [row[1]]), col: [c.col[0].slice(2, 4)], guards: [],
             items: c.items.map(o => Object.assign({}, o, { x: o.x - 16 })) };
  };

  /* a staircase in the stage's look (pictures of the Stairs piece) with the game's
   * staircase floor under it, centred on block `mid` of floor f (the lower floor) */
  LC.placeStairs = function (level, set, f, mid, dir) {
    if (f < 1) throw new Error('Stairs go up to the floor above: start them on a lower floor.');
    const clip = dir === 2 ? set.right : set.left, W = level.W, WALLV = 0xFFFF0000;
    const s0 = Math.max(3, Math.min(W - 3 - clip.slices, mid - (clip.slices >> 1)));
    LC.pasteRegion(level, clip, f - 1, s0, false);
    for (const ff of [f - 1, f]) for (let c = 2 * s0; c < 2 * (s0 + clip.slices); c++)
      if ((level.col[ff][c] >>> 0) !== WALLV) level.col[ff][c] = 0x01010000;
    LC.placeStairShape(level, f, dir === 2 ? s0 + 1 : s0 + 5, dir);
    return s0;
  };

  /* ---------- the layout of an arcade map picture ----------
   * img = {width, height, data (RGBA)}. Floors: bands between the light floor slabs that
   * cross the building; stairs: in a floor's band, the top edge of the white steps goes
   * up or down ~1 px per px over a long stretch. Same as tools/lostlevels/detect.py. */
  LC.detectLayout = function (img) {
    const W = img.width, H = img.height, d = img.data;
    const P = (x, y) => (y * W + x) * 4;
    const light = (x, y) => { const k = P(x, y), r = d[k], g = d[k + 1], b = d[k + 2];
      return r > 225 && g > 225 && b > 225 && Math.max(r, g, b) - Math.min(r, g, b) < 24; };
    const pale = (x, y) => { const k = P(x, y); return d[k] + d[k + 1] + d[k + 2] > 640; };
    const sum = (x, y) => { const k = P(x, y); return d[k] + d[k + 1] + d[k + 2]; };
    const slab = [];
    for (let y = 0; y < H; y++) { let n = 0; for (let x = 0; x < W; x += 2) if (pale(x, y)) n++; slab.push(n * 2 / W > 0.6); }
    const slabs = [];
    for (let y = 0; y < H; y++) if (slab[y]) { const y0 = y; while (y < H && slab[y]) y++; if (y - y0 >= 4) slabs.push([y0, y - 1]); }
    let ground = H - 1;
    for (let y = H - 1; y > 0; y--) { let n = 0; for (let x = 0; x < W; x += 4) if (sum(x, y) > 150) n++; if (n > W / 16) { ground = y; break; } }
    const tops = [0].concat(slabs.map(s => s[1] + 1)), bottoms = slabs.map(s => s[0] - 1).concat([ground]);
    const floors = [];
    tops.forEach((t, i) => {
      const b = bottoms[i];
      if (b - t <= 60) return;
      const h = b - t;
      let x0 = W, x1 = 0;
      for (let x = 0; x < W; x++) {
        let n = 0;
        for (let y = t + (h >> 1); y < b; y += 3) if (sum(x, y) > 120) n++;
        if (n > h / 8) { x0 = Math.min(x0, x); x1 = Math.max(x1, x); }
      }
      const top = [];
      for (let x = 0; x < W; x++) {
        let v = null;
        for (let y = t + 2; y < b - 8; y++) if (light(x, y) && light(x, y + 3) && light(x, y + 6)) { v = y; break; }
        top.push(v);
      }
      const stairs = [];
      for (let x = x0; x < x1;) {
        let best = null;
        for (const sgn of [-1, 1]) {
          if (top[x] === null) break;
          let k = x, last = top[x], first = x;
          while (k < x1 && top[k] !== null && Math.abs(top[k] - last) <= 18 && (top[k] - last) * sgn >= -2) { last = top[k]; k++; }
          if (k - first > 60) {
            const rise = (top[k - 1] - top[first]) * sgn, slope = rise / (k - 1 - first);
            if (rise > h * 0.45 && slope > 0.45 && slope < 1.8 && (!best || k - first > best.x1 - best.x0))
              best = { x0: first, x1: k - 1, dir: sgn > 0 ? 1 : 2 };
          }
        }
        if (best) { stairs.push(best); x = best.x1 + 1; } else x += 2;
      }
      floors.push({ top: t, bottom: b, x0, x1, stairs });
    });
    return { width: W, height: H, floors };
  };

  /* ---------- what stands on the floors of an arcade map picture ----------
   * For each floor of LC.detectLayout (adds to it):
   *   pillars   [{x0, x1}]: white columns from the ceiling to the floor;
   *   obstacles [{x0, x1, lane: 'front'|'back', h}]: things standing on the floor, by the
   *             floor strip they cover (the band's bottom 32 rows: the back lane's floor
   *             above, the front lane's below), h = how high they reach above the floor;
   *   people    [{x, foot, lane, kind: 'guard'|'robber'}]: by colours only they use.
   * The floor strip of a column counts as bare floor where it matches the column patterns
   * the floor shows most (the same floor runs on for many columns; things change from
   * column to column). Everything in picture pixels. */
  /* colours only people use in the arcade's maps: guards (navy helmets, the purple and the
   * orange ones, policemen, workers), the robbers (Mobo's blue; Robo's red, which a guard
   * shares, counts as a robber next to Mobo) */
  const PEOPLE = [
    { kind: 'guard', colors: [0x000031, 0x000021, 0x000010, 0x7B31E7, 0x2152B5, 0x082184, 0xF7B500, 0x00B5F7, 0x009CDE, 0x31C65A, 0xC60000] },
    { kind: 'robber', colors: [0x0000CE] },
  ];
  LC.detectContents = function (img, layout) {
    const W = img.width, d = img.data;
    const px = (x, y) => { const k = (y * W + x) * 4; return (d[k] << 16) | (d[k + 1] << 8) | d[k + 2]; };
    const light = c => (c >> 16) > 225 && ((c >> 8) & 255) > 225 && (c & 255) > 225;
    const near = (a, b, t) => Math.abs((a >> 16) - (b >> 16)) <= t && Math.abs(((a >> 8) & 255) - ((b >> 8) & 255)) <= t && Math.abs((a & 255) - (b & 255)) <= t;
    for (const f of layout.floors) {
      const t = f.top, b = f.bottom, x0 = Math.max(0, f.x0), x1 = Math.min(W - 1, f.x1), s0 = b - 31;
      /* people first: their pixels are not things on the floor */
      const person = new Uint8Array(W);
      f.people = [];
      for (const P of PEOPLE) {
        const xs = [];
        for (let x = x0; x <= x1; x++) {
          let n = 0;
          for (let y = t; y <= b; y++) if (P.colors.includes(px(x, y))) n++;
          if (n >= 2) xs.push(x);
        }
        for (let i = 0; i < xs.length;) {
          let j = i;
          while (j + 1 < xs.length && xs[j + 1] - xs[j] <= 8) j++;
          const a = xs[i], c = xs[j];
          let n = 0, ytop = b;
          for (let x = a; x <= c; x++) for (let y = t; y <= b; y++) if (P.colors.includes(px(x, y))) { n++; ytop = Math.min(ytop, y); }
          if (n >= 30 && c - a >= 6 && c - a <= 64) {
            /* the feet: the lowest dark pixels under them */
            let foot = ytop;
            for (let x = Math.max(x0, a - 6); x <= Math.min(x1, c + 6); x++)
              for (let y = ytop; y <= b; y++) { const v = px(x, y); if ((v >> 16) < 40 && ((v >> 8) & 255) < 40 && (v & 255) < 60) foot = Math.max(foot, y); }
            f.people.push({ x: (a + c) >> 1, x0: a, x1: c, top: ytop, foot, lane: foot > b - 16 ? 'front' : 'back', kind: P.kind });
            for (let x = Math.max(0, a - 10); x <= Math.min(W - 1, c + 10); x++) person[x] = 1;
          }
          i = j + 1;
        }
      }
      /* one person found by two colour sets, and Robo (red) next to Mobo */
      f.people.sort((p, q) => p.x - q.x);
      for (let i = 0; i + 1 < f.people.length; i++) {
        const p = f.people[i], q = f.people[i + 1];
        if (Math.abs(q.x - p.x) <= 24 && p.kind === q.kind) { p.x0 = Math.min(p.x0, q.x0); p.x1 = Math.max(p.x1, q.x1); p.x = (p.x0 + p.x1) >> 1;
          p.top = Math.min(p.top, q.top); p.foot = Math.max(p.foot, q.foot); p.lane = p.foot > b - 16 ? 'front' : 'back'; f.people.splice(i + 1, 1); i--; }
      }
      for (const r of f.people.filter(p => p.kind === 'robber'))
        for (const g of f.people) if (g.kind === 'guard' && Math.abs(g.x - r.x) < 48) g.kind = 'robber';
      /* pillars: light from near the ceiling to the floor, narrow */
      const tall = [];
      for (let x = x0; x <= x1; x++) {
        let n = 0, m = 0;
        for (let y = t + 2; y < s0; y += 2) { m++; if (light(px(x, y))) n++; }
        tall.push(n > m * 0.85);
      }
      f.pillars = [];
      for (let x = x0; x <= x1; x++) if (tall[x - x0]) {
        let e = x;
        while (e + 1 <= x1 && tall[e + 1 - x0]) e++;
        if (e - x >= 3 && e - x <= 48) f.pillars.push({ x0: x, x1: e });
        x = e;
      }
      /* the floor strip: column patterns, the common ones are bare floor */
      const sig = x => { const a = []; for (let y = s0; y <= b; y++) a.push(px(x, y)); return a; };
      const where = new Map(), cols = [];
      for (let x = x0; x <= x1; x++) { const k = sig(x).join(); cols.push(k); if (!where.has(k)) where.set(k, []); where.get(k).push(x); }
      /* bare floor repeats in several places across the floor (a bus's stripes only along
       * the bus): at least 8 columns in at least 3 stretches over a quarter of the width */
      const count = new Map();
      for (const [k, xs] of where) {
        let runs = 1;
        for (let i = 1; i < xs.length; i++) if (xs[i] - xs[i - 1] > 4) runs++;
        count.set(k, runs >= 3 && xs[xs.length - 1] - xs[0] >= (x1 - x0) / 4 ? xs.length : 0);
      }
      const floorAt = [];               /* per column: the nearest common pattern */
      const common = [];
      for (let x = x0; x <= x1; x++) if (count.get(cols[x - x0]) >= 8) common.push(x);
      let ci = 0;
      for (let x = x0; x <= x1; x++) {
        while (ci + 1 < common.length && Math.abs(common[ci + 1] - x) <= Math.abs(common[ci] - x)) ci++;
        floorAt.push(common.length ? sig(common[ci]) : null);
      }
      /* covered rows: not like the nearby bare floor (the pattern itself or its neighbours'
       * colours in that row, for the checkered floors) */
      const rowSet = [];
      for (let y = s0; y <= b; y++) rowSet.push(new Set());
      /* per column: which rows of the strip are covered; a thing in the back lane covers
       * the strip from its top down to its base (base in the back half), one in the front
       * lane most of the front half */
      const lanes = [];
      for (let x = x0; x <= x1; x++) {
        if (person[x] || tall[x - x0] || !floorAt[x - x0] || (f.stairs || []).some(st => x >= st.x0 - 8 && x <= st.x1 + 8)) { lanes.push(''); continue; }
        const ref = floorAt[x - x0], cov = [];
        for (let r = 0; r < 32; r++) {
          const v = px(x, s0 + r);
          let floorish = near(v, ref[r], 12);
          for (let dx = -40; !floorish && dx <= 40; dx += 8) {
            const xx = x + dx;
            if (xx >= x0 && xx <= x1 && count.get(cols[xx - x0]) >= 8 && near(v, px(xx, s0 + r), 12)) floorish = true;
          }
          cov.push(!floorish);
        }
        let front = 0, base = -1;
        for (let r = 16; r < 32; r++) if (cov[r]) front++;
        for (let r = 0, n = 0; r < 18; r++) { if (cov[r]) n++; if (cov[r] && n >= 0.6 * (r + 1)) base = r; }
        /* a thing in the front lane hides the back lane's floor behind it too */
        lanes.push(front >= 9 ? 'front' : base >= 3 ? 'back' : '');
      }
      f.obstacles = [];
      for (const lane of ['back', 'front']) {
        const on = lanes.map(l => l === lane);
        for (let x = x0; x <= x1; x++) if (on[x - x0]) {
          let e = x, gap = 0;
          while (e + 1 <= x1 && (on[e + 1 - x0] || (gap < 4 && e + 2 <= x1 && on[e + 2 - x0]))) e++;
          if (e - x >= 5) {
            /* how high: rows up from the strip where the thing differs from the floor's
             * surroundings left and right of it */
            const L = Math.max(x0, x - 3), R = Math.min(x1, e + 3);
            let h = 0;
            for (let y = s0 - 1; y > t; y--) {
              let n = 0, m = 0;
              for (let xx = x; xx <= e; xx += 2) { m++; const v = px(xx, y); if (!near(v, px(L, y), 20) && !near(v, px(R, y), 20)) n++; }
              if (n < m * 0.4) break;
              h++;
            }
            f.obstacles.push({ x0: x, x1: e, lane, h });
          }
          x = e;
        }
      }
    }
    return layout;
  };

  /* where a picture's floor is in the level: tr = {x, y, sx, sy} (picture px to building
   * px, as the Trace tab shows it) */
  const traceFloor = (level, fl, tr) => Math.floor((tr.y + (fl.top + fl.bottom) / 2 * tr.sy) / 80);
  /* the collision of the building as the picture shows it: on every floor of the picture
   * that is in the building, rows 0-6 (above the floor) of the columns the picture covers
   * become what stands there (things in the front / back lane as high as they are,
   * pillars a wall in the back lane). Columns of stairs, doors, the exit and the
   * building's outer walls are left as they are. Returns the number of columns changed. */
  LC.placeDetectedCollision = function (level, layout, tr) {
    const rowsOf = h => { let m = 0; const n = Math.max(1, Math.min(7, Math.ceil(h / 8))); for (let r = 7 - n; r <= 6; r++) m |= 0x80 >> r; return m; };
    const col8 = x => Math.floor((tr.x + x * tr.sx) / 8);
    let changed = 0;
    for (const fl of layout.floors) {
      const f = traceFloor(level, fl, tr), c = level.col[f];
      if (!c || !fl.obstacles) continue;
      const front = new Map(), back = new Map();
      const add = (m, a, b, bits) => { for (let k = Math.max(0, a); k <= Math.min(c.length - 1, b); k++) m.set(k, (m.get(k) || 0) | bits); };
      for (const o of fl.obstacles) add(o.lane === 'front' ? front : back, col8(o.x0), col8(o.x1), rowsOf(o.h * tr.sy + 8));
      for (const p of fl.pillars) add(back, col8(p.x0), col8(p.x1), 0xFE);
      for (let k = Math.max(0, col8(fl.x0)); k <= Math.min(c.length - 1, col8(fl.x1)); k++) {
        const v = c[k], inf = v & 0xFFFF;
        if (LC.isStairInfo(inf) || LC.isDoorInfo(inf) || LC.isExitInfo(inf) || (v >>> 16) === 0xFFFF) continue;
        const fr = ((v >>> 24) & 0x01) | (front.get(k) || 0), bk = (((v >>> 16) & 255) & 0x01) | (back.get(k) || 0);
        const nv = ((fr << 24) | (bk << 16) | inf) >>> 0;
        if (nv !== v) { c[k] = nv; changed++; }
      }
    }
    return changed;
  };
  /* guards where the picture has them (each a guard of the stage's, in turn, in the lane
   * it stands in) and the robbers' start where they are; returns {guards, start} */
  LC.placeDetectedPeople = function (level, layout, tr, presets) {
    const guards = [], robbers = [];
    for (const fl of layout.floors) {
      const f = traceFloor(level, fl, tr);
      if (f < 0 || f >= level.floors || !fl.people) continue;
      for (const p of fl.people) {
        const x = Math.round(tr.x + p.x * tr.sx) + 32;
        if (x < 48 || x > 16 * level.W) continue;
        (p.kind === 'robber' ? robbers : guards).push({ f, x, lane: p.lane });
      }
    }
    const out = { guards: 0, start: false };
    if (presets && presets.length && guards.length) {
      level.guards = guards.slice(0, 32).map((g, i) => {
        const pr = presets[i % presets.length];
        out.guards++;
        return { x: g.x, y: 80 * g.f + pr.yoff, z: g.lane === 'back' ? 16 : 0, frame: pr.frame, beh: pr.beh, dir: pr.dir, attr: pr.attr, template: pr.template };
      });
    }
    if (robbers.length) {
      const before = JSON.stringify(level.players);
      const r0 = robbers.sort((a, b) => b.f - a.f || a.x - b.x)[0];
      level.players.forEach((p, i) => {
        p.y = p.y - 80 * LC.floorOf(p.y, level.floors) + 80 * r0.f;
        p.x = Math.max(48, Math.min(16 * level.W - 16, r0.x + 8 * i));
      });
      if (LC.startProblem(level)) level.players = JSON.parse(before); else out.start = true;
    }
    return out;
  };

  /* stairs to the floor above in both directions, in the stage's look: the stage's own,
   * the missing direction mirrored */
  LC.stairStamps = function (stamps, N) {
    let up = stamps.find(s => s.id === 'stairs-up-right'), ul = stamps.find(s => s.id === 'stairs-up-left');
    if (!up && !ul) return null;
    const right = up ? up.clip : LC.flipClip(ul.clip, N), left = ul ? ul.clip : LC.flipClip(up.clip, N);
    return { right, left };
  };
  /* ramps with no landing on the floor above (cut off by later edits) */
  function brokenStairs(level) {
    const out = [];
    for (let f = 1; f < level.floors; f++) {
      const c = level.col[f], up = level.col[f - 1];
      for (let i = 0; i < c.length; i++) {
        const back = (c[i] >>> 16) & 255;
        if (!LC.isStairInfo(c[i] & 0xFFFF) || bits(back) < 2 || bits(back) > 7) continue;
        let j = i;
        while (j + 1 < c.length && LC.isStairInfo(c[j + 1] & 0xFFFF)) j++;
        let ok = false;
        for (let k = Math.max(0, i - 8); k <= Math.min(up.length - 1, j + 8); k++) if (LC.isStairInfo(up[k] & 0xFFFF)) ok = true;
        if (!ok) out.push('The stairs on floor ' + (f + 1) + ' at block ' + (i >> 1) + ' are broken (no landing above): place them again.');
        i = j;
      }
    }
    return out;
  }

  /* remove what is left of a cut staircase: a ramp with no landing above becomes plain
   * floor (its pictures are kept) */
  const PLAIN = 0x01010000;
  LC.fixStairs = function (level) {
    let n = 0;
    const isRamp = v => LC.isStairInfo(v & 0xFFFF) && bits((v >>> 16) & 255) >= 2 && bits((v >>> 16) & 255) <= 7;
    for (let f = 1; f < level.floors; f++) {
      const c = level.col[f], up = level.col[f - 1];
      const near = (arr, i, d, test) => { for (let k = Math.max(0, i - d); k <= Math.min(arr.length - 1, i + d); k++) if (test(arr[k])) return true; return false; };
      for (let i = 0; i < c.length; i++)
        if (LC.isStairInfo(c[i] & 0xFFFF) && !LC.isExitInfo(c[i] & 0xFFFF) && isRamp(c[i]) &&
            !near(up, i, 8, v => LC.isStairInfo(v & 0xFFFF))) {
          let j = i;
          while (j < c.length && LC.isStairInfo(c[j] & 0xFFFF)) j++;
          /* the exit stairs are left alone (the exit check reports a cut exit) */
          if (!c.slice(i, j).some(v => LC.isExitInfo(v & 0xFFFF))) for (let k = i; k < j; k++) { c[k] = PLAIN; n++; }
          i = j;
        }
    }
    return n;
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
    const others = level.items.filter(o => o.kind === LC.KIND.TREASURE).concat(
      level.items.filter(o => o.kind !== LC.KIND.DOOR && o.kind !== LC.KIND.TREASURE));
    const nlost = level.items.filter(o => o.kind === LC.KIND.DOOR && !used.has(o)).length;
    if (nlost) problems.push(nlost + ' door object(s) without a door in the wall were left out.');
    if (!exit) problems.push('There is no exit: keep (or paste) the stairs with the EXIT sign.');
    const exitObj = exit ? { kind: LC.KIND.EXIT, x: exit.c * 8 + 32, y: 80 * exit.f + 40, z: 16, frame: 0, beh: 0, dir: 0, attr: 0, floor: exit.f } : null;
    const items = doors.map(d => d.obj).concat(others, exitObj ? [exitObj] : []);
    if (items.length > 32) problems.push('Too many items: ' + items.length + ' (doors + treasures + others + exit, at most 32).');
    if (level.guards.length > 32) problems.push('Too many guards: ' + level.guards.length + ' (at most 32).');
    const treasures = others.filter(o => o.kind === LC.KIND.TREASURE).length;
    if (!treasures) problems.push('There are no treasures.');
    if (LC.startProblem(level)) problems.push(LC.startProblem(level));
    problems.push(...brokenStairs(level));
    for (const p of level.players) {
      const f = LC.floorOf(p.y, level.floors), c = Math.floor((p.x - 32) / 8);
      const v = level.col[f][c];
      if (v !== undefined && ((v >>> 24) & 0xFE)) problems.push('A player starts inside a wall (floor ' + (f + 1) + ').');
    }
    return { doors, items, exit: exitObj, treasures, problems };
  };

  /* ---------- guards: the picture of each guard type (its first frame on screen in that
   * stage; recorded from the game, as the level data only gives frame 0) ---------- */
  LC.GUARD_FRAMES = {1: {1: 0x185, 4: 0x185, 5: 0x330, 6: 0x330, 7: 0x330, 25: 0x275, 26: 0x254, 28: 0x254, 30: 0x324}, 2: {0: 0x185, 3: 0x185, 8: 0x1b3, 16: 0x217, 26: 0x256, 29: 0x19a, 30: 0x324, 31: 0x172, 32: 0x217, 33: 0x217, 34: 0x256}, 3: {3: 0x184, 5: 0x330, 8: 0x1b4, 10: 0x1b4, 25: 0x275, 26: 0x254, 27: 0x275, 34: 0x254, 35: 0x275, 36: 0x254, 38: 0x277}, 5: {11: 0x1b3, 25: 0x277, 26: 0x256, 28: 0x256, 31: 0x172, 34: 0x256, 39: 0x1f9}, 6: {0: 0x188, 1: 0x188, 3: 0x188, 4: 0x188, 20: 0x1fa, 21: 0x1fa, 22: 0x1fa, 23: 0x1ef, 24: 0x1f8, 25: 0x276, 26: 0x255, 28: 0x255, 38: 0x276}, 7: {0: 0x186, 1: 0x186, 3: 0x186, 8: 0x1b3, 12: 0x330, 13: 0x186, 14: 0x186, 25: 0x277, 28: 0x256, 29: 0x19a, 30: 0x325}, 9: {0: 0x186, 4: 0x186, 5: 0x330, 6: 0x330, 11: 0x1b3, 25: 0x277, 26: 0x256, 28: 0x256, 36: 0x256, 38: 0x277}, 10: {2: 0x186, 13: 0x186, 15: 0x217, 17: 0x217, 18: 0x217, 19: 0x217, 25: 0x277, 26: 0x256, 27: 0x277, 28: 0x256, 40: 0x186}, 11: {1: 0x186, 2: 0x186, 4: 0x186, 5: 0x330, 8: 0x1b3, 10: 0x1b3, 12: 0x330, 25: 0x277, 26: 0x256, 29: 0x19a, 30: 0x325, 35: 0x277}, 13: {1: 0x186, 3: 0x185, 4: 0x186, 10: 0x1b3, 25: 0x277, 26: 0x256, 34: 0x256, 36: 0x256, 37: 0x275, 42: 0x330}};
  LC.guardFrame = (e, template) => (LC.GUARD_FRAMES[e] || {})[template] || 0x186;

  /* ---------- items from any stage ---------- */
  const FRAME_SITES = [0x59E4, 0x5B32, 0x5DB2];   /* lea (frame table).l,a6 */
  const BORROW_TILE0 = 0x7A0, BORROW_TILES = 48;    /* VRAM tiles no stage uses */
  const DYNAMIC = [0x128, 0x200];                   /* tiles the game streams sprites into */
  const infoCache = new WeakMap();
  LC.info = function (rom, e) {
    let m = infoCache.get(rom);
    if (!m) { m = {}; infoCache.set(rom, m); }
    return m[e] || (m[e] = LC.stageInfo(rom, e));
  };
  /* every item kind of every stage, with the stage it comes from */
  LC.allItemPresets = function (rom) {
    const out = [], seen = new Set();
    for (const e of LC.STAGES)
      for (const p of LC.info(rom, e).itemPresets) {
        const k = [p.kind, p.frame, p.attr, p.beh, p.dir, p.z].join();
        if (seen.has(k)) continue;
        seen.add(k);
        out.push(Object.assign({ src: e }, p));
      }
    return out;
  };
  function frameTiles(rom, frame, attrOr) {
    const t = [];
    for (const p of LC.spritePieces(rom, frame))
      for (let k = 0; k < p.w * p.h; k++) t.push({ tile: ((p.attr | attrOr) & 0x7FF) + k, pal: ((p.attr | attrOr) >> 13) & 3 });
    return t;
  }
  /* the frame looks the same in stage b as in stage a (same tiles and colours) */
  function sameLook(rom, a, b, frame, attrOr) {
    const ia = LC.info(rom, a), ib = LC.info(rom, b);
    for (const { tile, pal } of frameTiles(rom, frame, attrOr))
      for (let i = 0; i < 32; i++) {
        const x = ia.vram[(tile * 32 + i) & 0xFFFF], y = ib.vram[(tile * 32 + i) & 0xFFFF];
        for (const [ca, cb] of [[x >> 4, y >> 4], [x & 15, y & 15]]) {
          if (!ca && !cb) continue;
          if (!ca || !cb || ia.pal[pal * 16 + ca].join() !== ib.pal[pal * 16 + cb].join()) return false;
        }
      }
    return true;
  }
  /* item animation scripts (tbl_1CCBC): (frame, duration) pairs, FFFF ends one */
  let scripts = null;
  function itemScripts(rom) {
    if (scripts) return scripts;
    const r = R(rom);
    scripts = [];
    for (let i = 0; i < 64; i++) {
      const p = r.u32(0x1CCBC + 4 * i);
      if (p < 0x1C000 || p > 0x1F000) break;
      const fr = [];
      for (let a = p; a < p + 200 && r.u16(a) !== 0xFFFF; a += 4) fr.push(r.u16(a));
      scripts.push(fr);
    }
    return scripts;
  }
  /* the frames an item shows: those of every animation that has its frame */
  function itemFrames(rom, frame) {
    const set = new Set([frame]);
    for (const sc of itemScripts(rom)) if (sc.includes(frame)) sc.forEach(f => set.add(f));
    return [...set];
  }
  function loadsOf(rom, e) { return tileLoads(R(rom), R(rom).u32(0x640C + 4 * (e - 1))); }
  /* How an item from stage o.src can be used in stage st:
   *   'own'      it is this stage's own (or looks the same here)
   *   'treasure' a treasure: its picture gets new frames and free tiles
   *   'block'    a gadget: its graphics block is loaded where it was in its own stage
   *              (those tiles are free here)
   *   'moved'    a gadget whose tiles are used here: they go to free tiles and its
   *              animation frames get new pictures (same frame numbers) in the level's
   *              own frame table
   *   'none'     it cannot be used here (no picture, or this stage uses its frames) */
  LC.itemUse = function (rom, st, o) {
    if (!o.src || o.src === st || sameLook(rom, o.src, st, o.frame, o.attr || 0)) return { mode: 'own' };
    if (o.kind === LC.KIND.TREASURE) return { mode: 'treasure', tiles: frameTiles(rom, o.frame, o.attr || 0).length };
    const first = frameTiles(rom, o.frame, o.attr || 0)[0];
    if (!first) return { mode: 'none' };
    /* the tiles of all its animation frames go where they were in its own stage, if this
     * stage leaves them free */
    const tiles = new Map(), frames = itemFrames(rom, o.frame);
    for (const f of frames)
      for (const t of frameTiles(rom, f, o.attr || 0)) tiles.set(t.tile, t.pal);
    const mine = loadsOf(rom, st);
    for (const t of tiles.keys())
      if ((t >= DYNAMIC[0] && t < DYNAMIC[1]) || mine.some(l => t >= l.vram / 32 && t < l.vram / 32 + l.n)) {
        /* moved: only if this stage's own things do not show these frames */
        const own = new Set();
        for (const p of LC.info(rom, st).itemPresets) for (const f of itemFrames(rom, p.frame)) own.add(f);
        if (frames.some(f => own.has(f)) || (o.attr || 0) & 0x7FF) return { mode: 'none' };
        return { mode: 'moved', tiles, frames };
      }
    return { mode: 'block', tiles };
  };
  /* what a level needs for its borrowed items: tile loads, new frames and frames drawn
   * with other tiles (moved gadgets; blockTiles: the stage's free tiles blocks use) */
  function borrowPlan(rom, st, items, blockTiles) {
    const r = R(rom), plan = { loads: [], frames: [], frameOf: new Map(), override: new Map(), problems: [] };
    const moved = new Map();
    const tinfo = LC.info(rom, st);
    let nextTile = BORROW_TILE0;
    const doneT = new Map(), doneB = new Set();
    /* the stage's colour for a colour of another stage's palette line */
    const remap = (sinfo, pal, c) => {
      if (!c) return 0;
      const [r0, g0, b0] = sinfo.pal[pal * 16 + c];
      let best = 1, bd = 1e9;
      for (let k = 1; k < 16; k++) {
        const [r1, g1, b1] = tinfo.pal[pal * 16 + k];
        const d = (r1 - r0) ** 2 + (g1 - g0) ** 2 + (b1 - b0) ** 2;
        if (d < bd) { bd = d; best = k; }
      }
      return best;
    };
    const tileBytes = (sinfo, tile, pal) => {
      const out = [];
      for (let i = 0; i < 32; i++) {
        const b = sinfo.vram[(tile * 32 + i) & 0xFFFF];
        out.push((remap(sinfo, pal, b >> 4) << 4) | remap(sinfo, pal, b & 15));
      }
      return out;
    };
    for (const o of items) {
      const use = LC.itemUse(rom, st, o);
      if (use.mode === 'own') continue;
      const key = o.src + ':' + o.frame + ':' + (o.attr || 0);
      const sinfo = LC.info(rom, o.src);
      if (use.mode === 'treasure') {
        if (!doneT.has(key)) {
          if (nextTile + use.tiles > BORROW_TILE0 + BORROW_TILES) {
            plan.problems.push('Too many different treasures from other stages (there is room for about 10).');
            doneT.set(key, o.frame);
          } else {
            /* the frame's pieces again, with the copied tiles */
            let p = r.u32(FRAMES_TBL + o.frame * 4) & 0xFFFFFF;
            const n = (r.u16(p) >> 8) + 1, def = [rom[p], rom[p + 1]], tiles = [];
            p += 2;
            for (let i = 0; i < n; i++, p += 6) {
              const sz = rom[p + 2], cells = (((sz >> 2) & 3) + 1) * ((sz & 3) + 1);
              const a = r.u16(p + 4), pal = ((a | (o.attr || 0)) >> 13) & 3;
              for (let k = 0; k < cells; k++) tiles.push(...tileBytes(sinfo, ((a | (o.attr || 0)) & 0x7FF) + k, pal));
              const na = (a & 0xF800) | (nextTile & 0x7FF);
              def.push(rom[p], rom[p + 1], rom[p + 2], rom[p + 3], (na >> 8) & 255, na & 255);
              nextTile += cells;
            }
            plan.loads.push({ tile: nextTile - tiles.length / 32, bytes: tiles });
            doneT.set(key, 0x10000 + plan.frames.length);
            plan.frames.push(def);
          }
        }
        plan.frameOf.set(o, doneT.get(key));
      } else if (use.mode === 'block') {
        for (const [t, pal] of use.tiles) {
          if (doneB.has(t)) continue;
          doneB.add(t);
          plan.loads.push({ tile: t, bytes: tileBytes(sinfo, t, pal) });
        }
      } else if (use.mode === 'moved') {
        if (!moved.has(key)) moved.set(key, { o, use, sinfo });
      } else {
        plan.problems.push((LC.KIND_NAMES[o.kind] || 'An item') + ' from stage ' + LC.stageNumber(o.src) +
                           ' cannot be used here (this stage shows its frames for its own things).');
      }
    }
    if (!moved.size) return plan;
    /* moved gadgets: free tiles in runs (what the treasures left of the borrow area, then
     * the stage's free tiles after the blocks'); each run of its tiles stays in one piece */
    const runs = [[nextTile, BORROW_TILE0 + BORROW_TILES - 1]];
    let skip = blockTiles || 0;
    for (const [a, b] of LC.FREE_TILES[st] || []) {
      const n = b - a + 1;
      if (skip >= n) { skip -= n; continue; }
      runs.push([a + skip, b]);
      skip = 0;
    }
    const newTile = new Map();
    for (const { o, use, sinfo } of moved.values()) {
      const ts = [...use.tiles.keys()].sort((a, b) => a - b);
      for (let i = 0; i < ts.length;) {
        if (newTile.has(ts[i])) { i++; continue; }
        let j = i;
        while (j + 1 < ts.length && ts[j + 1] === ts[j] + 1 && !newTile.has(ts[j + 1])) j++;
        const n = j - i + 1, run = runs.find(([a, b]) => b - a + 1 >= n);
        if (!run) {
          plan.problems.push('No room for the pictures of the ' + (LC.KIND_NAMES[o.kind] || 'gadget').toLowerCase() +
                             ' from stage ' + LC.stageNumber(o.src) + ' (too many things from other stages).');
          return plan;
        }
        const bytes = [];
        for (let k = 0; k < n; k++) { newTile.set(ts[i + k], run[0] + k); bytes.push(...tileBytes(sinfo, ts[i + k], use.tiles.get(ts[i + k]))); }
        plan.loads.push({ tile: run[0], bytes });
        run[0] += n;
        i = j + 1;
      }
      for (const f of use.frames) {
        if (plan.override.has(f)) continue;
        let p = r.u32(FRAMES_TBL + f * 4) & 0xFFFFFF;
        const n = (r.u16(p) >> 8) + 1, def = [rom[p], rom[p + 1]];
        p += 2;
        for (let i = 0; i < n; i++, p += 6) {
          const a = r.u16(p + 4), na = (a & 0xF800) | (newTile.get(a & 0x7FF) & 0x7FF);
          def.push(rom[p], rom[p + 1], rom[p + 2], rom[p + 3], (na >> 8) & 255, na & 255);
        }
        plan.override.set(f, def);
      }
    }
    return plan;
  }

  /* problems with the borrowed items of a level (for the editor's checks) */
  LC.borrowProblems = (rom, level) => {
    const items = level.items.filter(o => o.src && o.src !== level.stage);
    if (!items.length) return [];
    const blk = blockPlan(rom, level, LC.info(rom, level.stage).nmeta);
    return borrowPlan(rom, level.stage, items, blk.tiles).problems;
  };

  LC.exportLevel = function (rom, level) {
    level = JSON.parse(JSON.stringify(level));
    LC.fixStairs(level);
    const r = R(rom), st = level.stage, W = level.W, nf = level.floors, H = nf * 5;
    const res = LC.resolveItems(level);
    if (res.problems.some(p => /Too many|no exit|no treasures/.test(p))) throw new Error(res.problems.join('\n'));
    const out = [];
    /* each stage (and each extra stage) has its own region, so several level mods can be
     * turned on together */
    const base = LC.slotBase(level);
    const emit = bytes => { const a = base + out.length; out.push(...bytes); if (out.length & 1) out.push(0xFF); return a; };
    const words = ws => { const b = []; for (const w of ws) b.push((w >> 8) & 255, w & 255); return b; };
    const longs = ls => { const b = []; for (const l of ls) b.push((l >>> 24) & 255, (l >>> 16) & 255, (l >>> 8) & 255, l & 255); return b; };
    const yb = 80 * (level.base || 0);
    if (level.extra) emit(new Array(32).fill(0));          /* extra stage: its descriptor goes here */
    /* map (a stage with a floor above its map gets that floor's rows before it: the
     * game draws them at the start, out of sight) */
    if (level.map.length !== W * H) throw new Error('map size');
    const pre = [];
    for (let k = 0; k < 5 * (level.base || 0); k++) pre.push(...level.map.slice(0, W));
    const mapAddr = emit(words(pre.concat(level.map))) + 2 * pre.length;
    /* flipped stamps use mirrored metatiles: the table with them after the originals */
    const sinfo = LC.info(rom, st);
    const blk = blockPlan(rom, level, sinfo.nmeta);
    if (blk.problems.length) throw new Error(blk.problems.join('\n'));
    let metaAddr = 0;
    if (level.map.some(m => m >= sinfo.nmeta)) metaAddr = emit(words([].concat(...sinfo.meta, ...blk.meta)));
    /* collision, numbered door columns; buffers in the original RAM order */
    const col = level.col.map(c => c.slice());
    res.doors.forEach((d, k) => { col[d.f][d.c] = ((col[d.f][d.c] & 0xFFFFFF00) | (0x90 + 2 * k)) >>> 0; });
    const base_ = level.base || 0;
    /* a level with another number of floors than its stage gets the game's standard floor
     * tables for that count (buffers top floor first, as in the 3-, 4- and 5-floor stages) */
    const hostFloors = r.u16(0xFF86 + 2 * (st - 1)) + 1, nfc = nf + base_;
    if (nfc !== hostFloors && (base_ || nfc < 3 || nfc > 5)) throw new Error('A level can have 3 to 5 floors.');
    const orig = nfc === hostFloors ? readCollision(r, st, nfc, W)
      : { nbuf: nfc, floorBuf: Array.from({ length: nfc }, (_, f) => nfc - 1 - f) };
    const floorTables = nfc === hostFloors ? null
      : { list: { 3: 0xFFDA, 4: 0x1002E, 5: 0x10006 }[nfc], rows: { 3: 0x1035A, 4: 0x103FA, 5: 0x10396 }[nfc] };
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
    /* items from other stages: their pictures come along */
    const bp = borrowPlan(rom, st, res.items, blk.tiles);
    if (bp.problems.length) throw new Error(bp.problems.join('\n'));
    bp.loads.push(...blk.loads);
    let ntbl = 0, lowest = 0xFFFFFFFF;          /* entries: the table ends where the frames start */
    while (FRAMES_TBL + 4 * ntbl < lowest) { lowest = Math.min(lowest, r.u32(FRAMES_TBL + 4 * ntbl) & 0xFFFFFF); ntbl++; }
    const frameOf = o => { const f = bp.frameOf.get(o); return f === undefined ? o.frame : f >= 0x10000 ? ntbl + f - 0x10000 : f; };
    /* objects */
    const ent = o => words([o.x & 0xFFFF, (o.y + yb) & 0xFFFF, o.z & 0xFFFF, frameOf(o) & 0xFFFF, o.beh & 0xFFFF, o.dir & 0xFFFF, o.attr & 0xFFFF]);
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
    /* new frames: a copy of the frame table with them at its end */
    let tblAddr = 0;
    if (bp.frames.length || bp.override.size) {
      const defs = bp.frames.map(d => emit(d)), over = new Map([...bp.override].map(([f, d]) => [f, emit(d)]));
      const t = [];
      for (let i = 0; i < ntbl; i++) t.push(over.has(i) ? over.get(i) : r.u32(FRAMES_TBL + 4 * i));
      tblAddr = emit(longs(t.concat(defs)));
    }
    /* the stage's tile loader, then the borrowed tiles (raw copies with sub_0119C) */
    let loaderAddr = 0;
    if (bp.loads.length) {
      const blocks = bp.loads.map(l => ({ l, a: emit(l.bytes) }));
      const code = [0x4E, 0xB9, ...longs([r.u32(0x640C + 4 * (st - 1))])];           /* jsr (original).l */
      for (const { l, a } of blocks) {
        const v = l.tile * 32, cmd = (0x40000000 | ((v & 0x3FFF) << 16) | (v >>> 14)) >>> 0;
        code.push(0x4D, 0xF9, ...longs([a]));                                          /* lea (tiles).l,a6 */
        code.push(0x23, 0xFC, ...longs([cmd]), ...longs([0x00C00004]));               /* move.l #cmd,(VDP_CTRL).l */
        code.push(0x3E, 0x3C, ...words([l.bytes.length / 32 - 1]));                   /* move.w #n-1,d7 */
        code.push(0x4E, 0xB9, ...longs([0x119C]));                                     /* jsr (sub_0119C).l */
      }
      code.push(0x4E, 0x75);                                                           /* rts */
      loaderAddr = emit(code);
    }
    /* the "NEXT TARGETS ARE" screen: one icon per treasure (the stage's own icons, in
     * rows of up to 5 centred like the game's) */
    const ip = r.u32(0x11A2E + 4 * st), hn = r.u16(ip);
    let iconAddr = 0;
    if (hn && (hn !== res.treasures || level.extra)) {
      const n = Math.min(res.treasures, 10), first = n > 5 ? Math.ceil(n / 2) : n, list = [n];
      for (let i = 0; i < n; i++) {
        const row = i < first ? 0 : 1, inRow = row ? n - first : first, k = row ? i - first : i, q = ip + 2 + 10 * (i % hn);
        list.push(r.u16(q), 248 - 16 * (inRow - 1) + 32 * k, 192 + 24 * row, r.u16(q + 6), r.u16(q + 8));
      }
      iconAddr = emit(words(list));
    }
    /* a moved start: new start routines for the two views (camera, first screen) */
    const orig0 = LC.readStage(rom, st).players;
    let startAddr = null;
    if (level.players.some((o, i) => !orig0[i] || o.x !== orig0[i].x || o.y !== orig0[i].y))
      startAddr = startRoutines(r, level, mapAddr, metaAddr || r.u32(0x5516 + 8 * (st - 1) + 4), emit, words, longs);
    /* the new ROM */
    if (out.length > LC.SLOT_SIZE) throw new Error('This level is too big (' + out.length + ' bytes of data).');
    const size = base + out.length;
    const rom2 = new Uint8Array(size).fill(0xFF);
    rom2.set(rom.subarray(0, LC.ROM_SIZE));
    rom2.set(out, base);
    w16(rom2, 0x54E2 + 4 * (st - 1), W);
    w16(rom2, 0x54E2 + 4 * (st - 1) + 2, H);
    const oldMap = r.u32(0x5516 + 8 * (st - 1)), oldW = r.u16(0x54E2 + 4 * (st - 1));
    w32(rom2, 0x5516 + 8 * (st - 1), mapAddr);
    if (metaAddr) w32(rom2, 0x5516 + 8 * (st - 1) + 4, metaAddr);
    w32(rom2, 0x100FA + 8 * (st - 1), colAddr);
    w32(rom2, 0x100FA + 8 * (st - 1) + 4, orig.nbuf - 1);
    if (floorTables) {
      w16(rom2, 0xFF86 + 2 * (st - 1), nfc - 1);
      w32(rom2, 0xFFA2 + 4 * (st - 1), floorTables.list);
      w32(rom2, 0x10322 + 4 * (st - 1), floorTables.rows);
    }
    w32(rom2, 0x3E000 + 4 * (st - 1), placeAddr);
    w32(rom2, 0x1ADE2 + 4 * (st - 1), kindAddr);
    w32(rom2, 0x14AB8 + 4 * (st - 1), guardAddr);
    w16(rom2, 0x1AF60 + 2 * (st - 1), res.treasures);
    if (iconAddr) w32(rom2, 0x11A2E + 4 * st, iconAddr);
    /* the HUD's treasure blips (item slots after the doors) and where the escape starts */
    w16(rom2, 0x10DBC + 4 * st, res.doors.length);
    w16(rom2, 0x10DBC + 4 * st + 2, res.treasures - 1);
    if (res.exit) {
      /* some stages start the escape a few pixels past the exit column: keep that */
      const ox = LC.resolveItems(LC.readStage(rom, st)).exit, dx = ox ? r.u16(0x8FF2 + 4 * (st - 1)) - ox.x : 0;
      w16(rom2, 0x8FF2 + 4 * (st - 1), res.exit.x + dx);
      w16(rom2, 0x8FF2 + 4 * (st - 1) + 2, 80 * (res.exit.floor + (level.base || 0)) + 48);
    }
    if (tblAddr) for (const a of FRAME_SITES) w32(rom2, a + 2, tblAddr);
    if (loaderAddr) w32(rom2, 0x640C + 4 * (st - 1), loaderAddr);
    if (startAddr) {
      w32(rom2, 0x4566 + 4 * (st - 1), startAddr.p1);
      w32(rom2, 0x459A + 4 * (st - 1), startAddr.p2);
      w32(rom2, 0x45CE + 4 * (st - 1), startAddr.both);
    }
    /* first screen: the start routines' map pointers, same row and column in the new map */
    for (const d of LC.stageInfo(rom, st).descriptors) {
      const off = (r.u32(d) - oldMap) / 2;
      const row = Math.floor(off / oldW), c = off - row * oldW;
      w32(rom2, d, mapAddr + 2 * (row * W + c));
      if (metaAddr) w32(rom2, d + 4, metaAddr);
    }
    if (level.extra) return { rom: extraRom(rom, rom2, level, out, emit, base), problems: res.problems };
    return { rom: fixHeader(rom2), problems: res.problems };
  };
  /* The start routines (tbl_04566 P1 / tbl_0459A P2, both called by tbl_045CE) set each
   * view's camera with move-immediates and draw its first screen from the map (sub_017AC
   * with 14-byte descriptors). From the game's own: camera x = a view's left edge (32 at
   * the left end, at most 16 W - 224), first streamed block column C = (camX - 32) / 16 - 2,
   * plane address of block column c = base + (2c mod 64) * 2, camera y = 32 + 80 floor
   * (counting the floor above the map, if any), first map row drawn 5 (floor - 1). */
  const VIEW = [
    { base: 0xC000, vs: 0xFFC9, a: { c19: 0x187C, c19v: 0x187E, c: 0x1880, row: 0x187A, cv: 0x1882, camX: 0x1852, camY: 0x1850,
      camX2: 0x1856, camY2: 0x1854, vs: 0x1844, hs: 0x1846, vs2: 0x1848, hs2: 0x184A }, clr: 0x2284 },
    { base: 0xD100, vs: 0x159, a: { c19: 0x22C0, c19v: 0x22C2, c: 0x22C4, row: 0x22BE, cv: 0x22C6, camX: 0x2296, camY: 0x2294,
      camX2: 0x229A, camY2: 0x2298, vs: 0x2288, hs: 0x228A, vs2: 0x228C, hs2: 0x228E }, clr: 0x2CC8 },
  ];
  LC.startProblem = level => {
    const p = level.players[0];
    if (p && LC.floorOf(p.y, level.floors) === 0 && !level.base) return 'The robbers cannot start on the top floor.';
    return '';
  };
  function startRoutines(r, level, mapAddr, metaPtr, emit, words, longs) {
    const W = level.W, base = level.base || 0;
    if (LC.startProblem(level)) throw new Error(LC.startProblem(level));
    const views = VIEW.map((v, i) => {
      const pl = level.players[i] || level.players[0];
      const fm = LC.floorOf(pl.y, level.floors), fc = fm + base;
      const camX = Math.max(32, Math.min(16 * W - 224, pl.x - 96)), camY = 32 + 80 * fc;
      const C = Math.floor((camX - 32) / 16) - 2, row = 5 * (fm - 1);
      const col = c => v.base + ((((2 * c) % 64) + 64) % 64) * 2;
      /* the first screen: 32 block columns from c0, split where the plane wraps */
      const c0 = W >= 32 ? Math.max(0, Math.min(C, W - 32)) : 0, n1 = 32 - (c0 % 32);
      const desc = (c, n) => emit([...longs([mapAddr + 2 * (row * W + c), metaPtr]), ...words([col(c), n - 1, 15])]);
      const descs = n1 === 32 ? [desc(c0, 32)] : [desc(c0, n1), desc(c0 + n1, 32 - n1)];
      const hs = (-(camX - 32)) & 0xFFFF;
      const vals = { c19: C + 19, c19v: col(C + 19), c: C, row, cv: col(C), camX, camY, camX2: camX + 128, camY2: camY + 8,
                     vs: v.vs, hs, vs2: v.vs, hs2: hs };
      const code = [];
      for (const d of descs) {
        code.push(0x3E, 0x39, ...longs([0xFF2CCC]));                 /* move.w (FF2CCC).l,d7 */
        code.push(0x4D, 0xF9, ...longs([d]));                        /* lea (desc).l,a6 */
        code.push(0x4E, 0xB9, ...longs([0x17AC]));                   /* jsr (sub_017AC).l */
      }
      for (const k of Object.keys(v.a))
        code.push(0x33, 0xFC, ...words([vals[k] & 0xFFFF]), ...longs([0xFF0000 | v.a[k]]));   /* move.w #v,(a).l */
      code.push(0x42, 0x79, ...longs([0xFF0000 | v.clr]));            /* clr.w (a).l */
      code.push(0x4E, 0x75);                                          /* rts */
      return emit(code);
    });
    const both = emit([0x4E, 0xB9, ...longs([views[0]]), 0x4E, 0xB9, ...longs([views[1]]), 0x4E, 0x75]);
    return { p1: views[0], p2: views[1], both };
  }
  function fixHeader(rom2) {
    w32(rom2, 0x1A4, rom2.length - 1);
    let sum = 0;
    for (let a = 0x200; a + 1 < rom2.length; a += 2) sum = (sum + ((rom2[a] << 8) | rom2[a + 1])) & 0xFFFF;
    w16(rom2, 0x18E, sum);
    return rom2;
  }

  /* ---------- extra stages (the arcade's lost levels) ----------
   * An extra stage is played between two stages on a host entry: the game (port,
   * extrastages.c) writes the host's table values from the descriptor while it is on and
   * puts the originals back afterwards, so the cartridge's own stages are untouched.
   * Descriptor at the start of the stage's slot:
   *   'BBXSTAGE', word version 1, word after (entry it follows), word host, word 0,
   *   long name frame mapping, long "STAGE 11" mapping, long "STAGE 12" mapping,
   *   long patch list: {long addr, word len, bytes (even)}..., long -1 */
  LC.EXTRA_HOSTS = [2, 3, 5, 7, 9, 10, 11, 13];          /* entries without special code */
  LC.EXTRA_AFTER = { 5: 'after Mint (arcade stage 5)', 10: 'after Deluxe Liner (arcade stage 10)' };
  /* the intro's letters (8x16, tile pairs) */
  const LETTERS = { A: 0x188, B: 0x18A, C: 0x18C, D: 0x18E, E: 0x190, G: 0x192, I: 0x196, J: 0x198, L: 0x19C,
                    M: 0x19E, N: 0x1A0, O: 0x1A2, P: 0x1A4, R: 0x1A6, S: 0x1A8, T: 0x1AA, U: 0x1AC, W: 0x1AE,
                    X: 0x1B0, Y: 0x1B2, "'": 0x1B6 };
  LC.nameProblem = name => {
    const bad = [...new Set([...String(name).toUpperCase()].filter(c => c !== ' ' && !LETTERS[c]))];
    return bad.length ? 'The intro has no letter ' + bad.join(', ') + ' (it has A-E, G, I, J, L-P, R-U, W-Y and \').' : '';
  };
  function nameMapping(name) {
    const t = String(name).toUpperCase().slice(0, 24), x0 = -4 * t.length, b = [];
    let n = 0;
    [...t].forEach((c, i) => {
      if (c === ' ') return;
      const tile = LETTERS[c];
      if (!tile) throw new Error(LC.nameProblem(name));
      b.push(0xF8, (x0 + 8 * i) & 255, 0x01, 0, tile >> 8, tile & 255);
      n++;
    });
    return [(Math.max(n, 1) - 1), 0].concat(n ? b : [0xF8, 0, 1, 0, 0, 0]);
  }
  function stageMapping(rom, digit) {
    const r = R(rom), p = r.u32(0x24B80 + 4 * 0x3B5) & 0xFFFFFF, n = (r.u16(p) >> 8) + 1;
    const b = Array.from(rom.subarray(p, p + 2 + 6 * n));
    for (let i = 0; i < n; i++) if (((b[6 + 6 * i] << 8) | b[7 + 6 * i]) === 0x210) b[7 + 6 * i] = 0x10 + digit;
    return b;
  }
  function extraRom(rom, rom2, level, out, emit, base) {
    const x = level.extra, host = level.stage;
    if (!LC.EXTRA_HOSTS.includes(host)) throw new Error('An extra stage cannot use ' + LC.stageLabel(host) + ' as its host.');
    /* everything the export changed below the original ROM's end: the host's values */
    const patch = [];
    for (let a = 0x200; a < LC.ROM_SIZE;) {
      if (rom2[a] === rom[a]) { a++; continue; }
      let e = a + 1;
      for (;;) {
        let k = e;
        while (k < LC.ROM_SIZE && k < e + 8 && rom2[k] === rom[k]) k++;
        if (k < LC.ROM_SIZE && k < e + 8) e = k + 1; else break;
      }
      if (e - a > 0xFFFF) throw new Error('patch run too long');
      patch.push(...[(a >>> 24) & 255, (a >>> 16) & 255, (a >>> 8) & 255, a & 255, ((e - a) >> 8) & 255, (e - a) & 255]);
      for (let i = a; i < e; i++) patch.push(rom2[i]);
      if (patch.length & 1) patch.push(0);
      a = e;
    }
    patch.push(255, 255, 255, 255);
    const nameAddr = emit(nameMapping(x.name || ''));
    const s11 = emit(stageMapping(rom, 1)), s12 = emit(stageMapping(rom, 2));
    const listAddr = emit(patch);
    if (out.length > LC.SLOT_SIZE) throw new Error('This level is too big (' + out.length + ' bytes of data).');
    const hdr = [0x42, 0x42, 0x58, 0x53, 0x54, 0x41, 0x47, 0x45, 0, 1, 0, x.after, 0, host, 0, 0];
    for (const l of [nameAddr, s11, s12, listAddr]) hdr.push((l >>> 24) & 255, (l >>> 16) & 255, (l >>> 8) & 255, l & 255);
    hdr.forEach((v, i) => { out[i] = v; });
    const rom3 = new Uint8Array(base + out.length).fill(0xFF);
    rom3.set(rom.subarray(0, LC.ROM_SIZE));
    rom3.set(out, base);
    return fixHeader(rom3);
  }

  LC.makeIps = function (orig, patched) {
    const out = [0x50, 0x41, 0x54, 0x43, 0x48];
    const n = patched.length;
    /* past the original ROM, 0xFF is empty space (the mod loader fills gaps with it) */
    const same = i => i < orig.length ? orig[i] === patched[i] : patched[i] === 0xFF && patched[i + 1] === 0xFF && patched[i + 2] === 0xFF && patched[i + 3] === 0xFF;
    for (let a = 0; a < n;) {
      if (same(a)) { a++; continue; }
      let e = a;
      while (e < n && e - a < 0xFFFF && !(same(e) && same(e + 1))) e++;
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
