/* The game's sounds, recorded in the page from the player's ROM while it builds the
 * package (sounds.bbs: songs, effects and voices, lossless), so the game has them from
 * its first start instead of recording them then.
 *
 * The recorder is the game's own (port/src/sndrec_web.c, WebAssembly: bonanza-sounds.js
 * in the kit). It runs in one worker per core, each recording one sound at a time into
 * a bank of its own; merge() puts the banks together in the game's order, the same file
 * byte for byte as the game makes (sndbank.c: "BBSND001", u32 rate, u32 directory
 * offset, the coded blocks and block tables, the directory).
 *
 * BonanzaSoundBank.record({ rom, code, progress }) -> Uint8Array (sounds.bbs)
 *   code: bonanza-sounds.js (bytes); progress(done, total, cores)
 * BonanzaSoundBank.merge([bank, ...]) -> Uint8Array
 */
(function (root) {
  'use strict';

  const u32 = (b, o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;
  const put32 = (b, o, v) => { b[o] = v & 255; b[o + 1] = (v >>> 8) & 255; b[o + 2] = (v >>> 16) & 255; b[o + 3] = (v >>> 24) & 255; };
  const STEMS = 4, ENTRY = 12 + 12 * STEMS;

  /* banks of one or more sounds each (lossless) -> one bank with all of them, in order */
  function merge(banks) {
    let body = 0, count = 0;
    for (const b of banks) {
      if (String.fromCharCode(...b.subarray(0, 8)) !== 'BBSND001') throw new Error('not a lossless sound bank');
      body += u32(b, 12) - 16;
      count += u32(b, u32(b, 12));
    }
    const out = new Uint8Array(16 + body + 4 + ENTRY * count);
    out.set(banks[0].subarray(0, 12), 0);
    let pos = 16;
    const dir = [];
    for (const b of banks) {
      const d = u32(b, 12), delta = pos - 16;
      out.set(b.subarray(16, d), pos);
      for (let i = 0, n = u32(b, d); i < n; i++) {
        const e = b.slice(d + 4 + ENTRY * i, d + 4 + ENTRY * (i + 1));
        for (let k = 0; k < STEMS; k++) {
          const o = 12 + 12 * k, nb = u32(e, o + 4), tab = u32(e, o + 8);
          if (!nb) continue;
          put32(e, o + 8, tab + delta);                      /* its block table moved */
          for (let j = 0; j < nb; j++) {                       /* and so did its blocks */
            const t = pos + tab - 16 + 4 * j;
            put32(out, t, u32(out, t) + delta);
          }
        }
        dir.push(e);
      }
      pos += d - 16;
    }
    put32(out, 12, pos);
    put32(out, pos, dir.length);
    pos += 4;
    for (const e of dir) { out.set(e, pos); pos += ENTRY; }
    return out;
  }

  /* the worker: the recorder, then this */
  const WORKER = `
;(() => {
  let M = null;
  const enc = new TextEncoder();
  self.onmessage = async (e) => {
    const d = e.data;
    try {
      if (d.rom) {
        M = await BonanzaSounds();
        const p = M._malloc(d.rom.length);
        M.HEAPU8.set(d.rom, p);
        if (M._sndweb_rom(p, d.rom.length)) throw new Error('the ROM could not be loaded');
        M._free(p);
        const ids = M._malloc(64);
        const n = M._sndweb_ids(ids);
        const list = Array.from(M.HEAPU8.subarray(ids, ids + n));
        M._free(ids);
        self.postMessage({ ready: true, ids: list });
        return;
      }
      const path = enc.encode('/part.bbs\\0');
      const p = M._malloc(1 + path.length);
      M.HEAPU8[p] = d.id;
      M.HEAPU8.set(path, p + 1);
      const rc = M._sndweb_record(p, 1, p + 1);
      M._free(p);
      if (rc) throw new Error('recording sound ' + d.id.toString(16) + ' failed');
      const data = M.FS.readFile('/part.bbs');
      M.FS.unlink('/part.bbs');
      self.postMessage({ id: d.id, data }, [data.buffer]);
    } catch (err) {
      self.postMessage({ error: String(err && err.message || err) });
    }
  };
})();
`;

  function workers() {
    const n = (root.navigator && root.navigator.hardwareConcurrency) || 2;
    return Math.max(1, Math.min(n, 16));
  }

  async function record({ rom, code, progress }) {
    const url = URL.createObjectURL(new Blob([code, WORKER], { type: 'text/javascript' }));
    const count = workers();
    const pool = [];
    try {
      /* every worker gets the ROM; the first to be ready says which sounds there are */
      const ready = [];
      for (let i = 0; i < count; i++) {
        const w = new Worker(url);
        pool.push(w);
        ready.push(new Promise((ok, bad) => {
          w.onmessage = (e) => e.data.error ? bad(new Error(e.data.error)) : ok(e.data.ids);
          w.onerror = (e) => bad(new Error(e.message || 'the sound recorder could not start'));
        }));
        w.postMessage({ rom: rom.slice() });
      }
      const ids = (await Promise.all(ready))[0];
      if (progress) progress(0, ids.length, count);
      /* longest first (the songs come first in the list): every worker takes the next one */
      const parts = new Array(ids.length);
      let next = 0, done = 0;
      await Promise.all(pool.map(w => new Promise((ok, bad) => {
        const take = () => {
          if (next >= ids.length) { ok(); return; }
          const k = next++;
          w.onmessage = (e) => {
            if (e.data.error) { bad(new Error(e.data.error)); return; }
            parts[k] = new Uint8Array(e.data.data);
            done++;
            if (progress) progress(done, ids.length, count);
            take();
          };
          w.onerror = (e) => bad(new Error(e.message || 'the sound recorder stopped'));
          w.postMessage({ id: ids[k] });
        };
        take();
      })));
      return merge(parts);
    } finally {
      for (const w of pool) w.terminate();
      URL.revokeObjectURL(url);
    }
  }

  const api = { record, merge };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  root.BonanzaSoundBank = api;
})(typeof window !== 'undefined' ? window : globalThis);
