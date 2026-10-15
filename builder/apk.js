/* The Android package: the game's code (built in the page from the player's ROM) added
 * to the engine's APK, which is then signed (APK Signature Scheme v2, RSA 2048 /
 * SHA-256). The key is made in the browser the first time and kept there (so later
 * builds install over earlier ones); nothing leaves the page.
 *
 * Works in a page and in Node (tests: port/dist/apk_test.mjs): WebCrypto,
 * CompressionStream. */
(function (root) {
  'use strict';
  const subtle = (root.crypto || globalThis.crypto).subtle;

  /* ---------------------------------------------------------------- bytes */
  const u16 = (b, o) => b[o] | (b[o + 1] << 8);
  const u32 = (b, o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;
  function cat(parts) {
    let n = 0;
    for (const p of parts) n += p.length;
    const out = new Uint8Array(n);
    let o = 0;
    for (const p of parts) { out.set(p, o); o += p.length; }
    return out;
  }
  function le(n, size) {                  /* little-endian integer of size bytes */
    const b = new Uint8Array(size);
    for (let i = 0; i < size; i++) { b[i] = n % 256; n = Math.floor(n / 256); }
    return b;
  }
  const lp = (b) => cat([le(b.length, 4), b]);          /* length-prefixed (uint32) */
  const utf8 = (s) => new TextEncoder().encode(s);

  let crcTable = null;
  function crc32(b) {
    if (!crcTable) {
      crcTable = new Uint32Array(256);
      for (let n = 0; n < 256; n++) {
        let c = n;
        for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1;
        crcTable[n] = c >>> 0;
      }
    }
    let c = 0xFFFFFFFF;
    for (let i = 0; i < b.length; i++) c = crcTable[(c ^ b[i]) & 0xFF] ^ (c >>> 8);
    return (c ^ 0xFFFFFFFF) >>> 0;
  }
  async function deflateRaw(b) {
    if (typeof CompressionStream === 'undefined') return null;
    const cs = new CompressionStream('deflate-raw');
    const out = new Response(new Blob([b]).stream().pipeThrough(cs)).arrayBuffer();
    return new Uint8Array(await out);
  }
  async function sha256(b) { return new Uint8Array(await subtle.digest('SHA-256', b)); }

  /* ---------------------------------------------------------------- zip */
  /* the APK's parts: its entries (local headers and data), its central directory
   * records, without an earlier signing block */
  function parseZip(z) {
    let e = z.length - 22;
    while (e >= 0 && u32(z, e) !== 0x06054b50) e--;
    if (e < 0) throw new Error('not a zip file');
    const count = u16(z, e + 10), cdSize = u32(z, e + 12), cdOff = u32(z, e + 16);
    let entriesEnd = cdOff;
    if (cdOff >= 32 && new TextDecoder().decode(z.subarray(cdOff - 16, cdOff)) === 'APK Sig Block 42') {
      const size = u32(z, cdOff - 24) + u32(z, cdOff - 20) * 4294967296;
      entriesEnd = cdOff - size - 8;     /* an earlier signature: dropped */
    }
    const recs = [];
    let o = cdOff;
    for (let i = 0; i < count; i++) {
      if (u32(z, o) !== 0x02014b50) throw new Error('bad central directory');
      const n = u16(z, o + 28), x = u16(z, o + 30), c = u16(z, o + 32);
      const len = 46 + n + x + c;
      const name = new TextDecoder().decode(z.subarray(o + 46, o + 46 + n));
      recs.push({ name, rec: z.subarray(o, o + len) });
      o += len;
    }
    return { entries: z.subarray(0, entriesEnd), recs };
  }

  const DOS_TIME = 0, DOS_DATE = (2026 - 1980) << 9 | 1 << 5 | 1;
  async function localEntry(name, data, at, store) {
    const nm = utf8(name);
    const crc = crc32(data);
    let body = store ? null : await deflateRaw(data), method = 8;
    if (!body || body.length >= data.length) { body = data; method = 0; }
    const head = cat([le(0x04034b50, 4), le(20, 2), le(0x0800, 2), le(method, 2), le(DOS_TIME, 2), le(DOS_DATE, 2),
                      le(crc, 4), le(body.length, 4), le(data.length, 4), le(nm.length, 2), le(0, 2), nm]);
    const rec = cat([le(0x02014b50, 4), le(20, 2), le(20, 2), le(0x0800, 2), le(method, 2), le(DOS_TIME, 2), le(DOS_DATE, 2),
                     le(crc, 4), le(body.length, 4), le(data.length, 4), le(nm.length, 2), le(0, 2), le(0, 2),
                     le(0, 2), le(0, 2), le(0, 4), le(at, 4), nm]);
    return { local: cat([head, body]), rec };
  }

  /* the APK with these entries added (replacing ones of the same name) and no signature:
   * { entries, cd, eocd } */
  async function addEntries(apk, add) {
    const z = parseZip(apk);
    const names = new Set(add.map(a => a.name));
    const keep = z.recs.filter(r => !names.has(r.name) &&
      !/^META-INF\/([^/]+\.(SF|RSA|DSA|EC)|MANIFEST\.MF)$/i.test(r.name));
    const locals = [], recs = keep.map(r => r.rec);
    let at = z.entries.length;
    for (const a of add) {
      const e = await localEntry(a.name, a.data, at, a.store);
      locals.push(e.local);
      recs.push(e.rec);
      at += e.local.length;
    }
    const entries = cat([z.entries, ...locals]);
    const cd = cat(recs);
    const eocd = cat([le(0x06054b50, 4), le(0, 2), le(0, 2), le(recs.length, 2), le(recs.length, 2),
                      le(cd.length, 4), le(entries.length, 4), le(0, 2)]);
    return { entries, cd, eocd };
  }

  /* ---------------------------------------------------------------- DER (the certificate) */
  function derLen(n) {
    if (n < 128) return Uint8Array.of(n);
    const b = [];
    while (n) { b.unshift(n & 0xFF); n >>>= 8; }
    return Uint8Array.of(0x80 | b.length, ...b);
  }
  const der = (tag, body) => cat([Uint8Array.of(tag), derLen(body.length), body]);
  const seq = (...xs) => der(0x30, cat(xs));
  const set = (...xs) => der(0x31, cat(xs));
  function oid(s) {
    const p = s.split('.').map(Number);
    const out = [40 * p[0] + p[1]];
    for (let i = 2; i < p.length; i++) {
      let v = p[i];
      const bytes = [v & 0x7F];
      while ((v >>>= 7)) bytes.unshift(0x80 | (v & 0x7F));
      out.push(...bytes);
    }
    return der(0x06, Uint8Array.from(out));
  }
  const NULL = Uint8Array.of(0x05, 0x00);
  const int = (bytes) => der(0x02, bytes[0] & 0x80 ? cat([Uint8Array.of(0), bytes]) : bytes);
  const utcTime = (s) => der(0x17, utf8(s));
  const genTime = (s) => der(0x18, utf8(s));
  const bitString = (b) => der(0x03, cat([Uint8Array.of(0), b]));
  const SHA256_RSA = seq(oid('1.2.840.113549.1.1.11'), NULL);

  async function makeCertificate(keys, spki) {
    const name = seq(set(seq(oid('2.5.4.3'), der(0x0C, utf8('Bonanza Bros. port (built from your ROM)')))));
    const serial = new Uint8Array(8);
    (root.crypto || globalThis.crypto).getRandomValues(serial);
    serial[0] &= 0x7F;
    const tbs = seq(
      der(0xA0, int(Uint8Array.of(2))),           /* v3 */
      int(serial),
      SHA256_RSA,
      name,
      seq(utcTime('260101000000Z'), genTime('20760101000000Z')),
      name,
      spki);
    const sig = new Uint8Array(await subtle.sign('RSASSA-PKCS1-v1_5', keys.privateKey, tbs));
    return seq(tbs, SHA256_RSA, bitString(sig));
  }

  /* the signing key: made once, kept in this browser (IndexedDB), or in memory */
  async function signingKey(store) {
    let saved = store ? await store.get() : null;
    if (saved) {
      const privateKey = await subtle.importKey('pkcs8', saved.pkcs8, { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' }, false, ['sign']);
      return { privateKey, spki: new Uint8Array(saved.spki), cert: new Uint8Array(saved.cert) };
    }
    const keys = await subtle.generateKey({ name: 'RSASSA-PKCS1-v1_5', modulusLength: 2048,
      publicExponent: Uint8Array.of(1, 0, 1), hash: 'SHA-256' }, true, ['sign', 'verify']);
    const spki = new Uint8Array(await subtle.exportKey('spki', keys.publicKey));
    const pkcs8 = new Uint8Array(await subtle.exportKey('pkcs8', keys.privateKey));
    const cert = await makeCertificate(keys, spki);
    if (store) await store.put({ pkcs8, spki, cert });
    return { privateKey: keys.privateKey, spki, cert };
  }

  /* ---------------------------------------------------------------- APK Signature Scheme v2 */
  const RSA_PKCS1_SHA256 = 0x0103;
  const CHUNK = 1048576;
  async function chunkedDigest(sections) {
    const digests = [];
    for (const s of sections) {
      for (let o = 0; o < s.length; o += CHUNK) {
        const c = s.subarray(o, Math.min(o + CHUNK, s.length));
        digests.push(await sha256(cat([Uint8Array.of(0xA5), le(c.length, 4), c])));
      }
    }
    return sha256(cat([Uint8Array.of(0x5A), le(digests.length, 4), ...digests]));
  }

  async function signV2(parts, key) {
    const { entries, cd, eocd } = parts;
    const digest = await chunkedDigest([entries, cd, eocd]);   /* eocd's directory offset: the block's place */
    const signedData = cat([
      lp(lp(cat([le(RSA_PKCS1_SHA256, 4), lp(digest)]))),       /* digests */
      lp(lp(key.cert)),                                          /* certificates */
      lp(new Uint8Array(0)),                                      /* additional attributes */
    ]);
    const sig = new Uint8Array(await subtle.sign('RSASSA-PKCS1-v1_5', key.privateKey, signedData));
    const signer = cat([lp(signedData), lp(lp(cat([le(RSA_PKCS1_SHA256, 4), lp(sig)]))), lp(key.spki)]);
    const value = lp(lp(signer));
    const pair = cat([le(4 + value.length, 8), le(0x7109871a, 4), value]);
    const size = pair.length + 8 + 16;
    const block = cat([le(size, 8), pair, le(size, 8), utf8('APK Sig Block 42')]);
    const eocd2 = eocd.slice();
    eocd2.set(le(entries.length + block.length, 4), 16);       /* the directory moved past the block */
    return cat([entries, block, cd, eocd2]);
  }

  /* the engine's APK + [{name, data, store}] -> the signed APK (store: not compressed) */
  async function build(apk, add, store) {
    const parts = await addEntries(apk, add);
    const key = await signingKey(store);
    return signV2(parts, key);
  }

  /* the key kept in IndexedDB (in a page) */
  function idbStore(dbName) {
    const open = () => new Promise((ok, bad) => {
      const r = indexedDB.open(dbName, 1);
      r.onupgradeneeded = () => r.result.createObjectStore('files');
      r.onsuccess = () => ok(r.result);
      r.onerror = () => bad(r.error);
    });
    return {
      async get() {
        try {
          const db = await open();
          return await new Promise(ok => {
            const q = db.transaction('files').objectStore('files').get('apk-signing-key');
            q.onsuccess = () => ok(q.result || null); q.onerror = () => ok(null);
          });
        } catch (e) { return null; }
      },
      async put(v) {
        try {
          const db = await open();
          db.transaction('files', 'readwrite').objectStore('files').put(v, 'apk-signing-key');
        } catch (e) { /* no storage: a new key next time */ }
      },
    };
  }

  const api = { build, idbStore, crc32, parseZip };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  root.BonanzaApk = api;
})(typeof window !== 'undefined' ? window : globalThis);
