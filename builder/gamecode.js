/* The game's code, made in this page from the player's ROM: the project's recompiler
 * (tools/recomp.py, tools/z80recomp.py) runs in Python compiled to WebAssembly
 * (Pyodide), and TinyCC compiled to WebAssembly builds what it writes into the library
 * the engine loads (bonanza_game.dll / libbonanza_game.so: port/src/gamelib.h). Nothing
 * leaves the page; nothing of the game is in it before the ROM is given.
 *
 * BonanzaGameCode.make({ rom, platform, res, log }) -> [{ path, data }]
 *   platform: 'windows' | 'linux' | 'android'
 *   res(name) -> Uint8Array: the page's resources ('gamekit.zip' and 'pyodide/...')
 */
(function (root) {
  'use strict';

  /* the libraries each platform's package gets: [TinyCC kind, path in the package] */
  const LIBS = {
    windows: [['win64', 'bonanza_game.dll']],
    linux: [['linux64', 'libbonanza_game.so']],
    android: [['arm64', 'lib/arm64-v8a/libbonanza_game.so'],
              ['arm', 'lib/armeabi-v7a/libbonanza_game.so'],
              ['linux64', 'lib/x86_64/libbonanza_game.so']],
  };
  const GAME_SOURCES = /^(recomp_0\d+|recomp_index|m68k_costs|z80_driver|game_link)\.c$/;

  async function importBytes(bytes) {
    const url = URL.createObjectURL(new Blob([bytes], { type: 'text/javascript' }));
    try { return await import(url); } finally { setTimeout(() => URL.revokeObjectURL(url), 60000); }
  }
  function scriptBytes(bytes) {
    return new Promise((ok, bad) => {
      const url = URL.createObjectURL(new Blob([bytes], { type: 'text/javascript' }));
      const s = document.createElement('script');
      s.src = url;
      s.onload = () => ok();
      s.onerror = () => bad(new Error('could not load a compiler'));
      document.head.appendChild(s);
    });
  }

  /* ---------------------------------------------------------------- Python (Pyodide) */
  const PY_BASE = 'https://bonanza-builder.invalid/pyodide/';
  let pyodide = null;
  async function python(res, log) {
    if (pyodide) return pyodide;
    /* Pyodide fetches its parts from its base URL: they come from the page */
    const realFetch = root.fetch.bind(root);
    root.fetch = async (input, init) => {
      const url = typeof input === 'string' ? input : input instanceof URL ? input.href : input && input.url;
      if (url && url.startsWith(PY_BASE)) {
        const name = url.slice(PY_BASE.length);
        const type = name.endsWith('.wasm') ? 'application/wasm' : name.endsWith('.json') ? 'application/json' : 'application/octet-stream';
        return new Response(await res('pyodide/' + name), { headers: { 'Content-Type': type } });
      }
      return realFetch(input, init);
    };
    const [mod, asm] = await Promise.all([importBytes(await res('pyodide/pyodide.mjs')),
                                          importBytes(await res('pyodide/pyodide.asm.mjs'))]);
    pyodide = await mod.loadPyodide({
      indexURL: PY_BASE,
      createPyodideModule: asm.default,
      stdout: (t) => { if (/recompiled|z80 driver/.test(t)) log('  ' + t); },
      stderr: (t) => log('  ' + t),
    });
    return pyodide;
  }

  /* ---------------------------------------------------------------- TinyCC */
  async function tinycc(kind, kit) {
    const name = 'TCC_' + kind;
    if (!root[name]) await scriptBytes(await kit.file('tcc/tcc-' + kind + '.js').async('uint8array'));
    let out = '';
    const m = await root[name]({ print: (t) => { out += t + '\n'; }, printErr: (t) => { out += t + '\n'; } });
    return { m, output: () => out };
  }

  async function make({ rom, platform, res, log }) {
    const JSZip = root.JSZip;
    const kit = await JSZip.loadAsync(await res('gamekit.zip'));
    const t0 = Date.now();

    log('Translating the game\'s code from your ROM (the project\'s recompiler, in Python)…');
    const py = await python(res, log);
    const FS = py.FS;
    for (const path of Object.keys(kit.files)) {
      if (!path.startsWith('recomp/') || kit.files[path].dir) continue;
      const to = '/' + path;
      FS.mkdirTree(to.replace(/\/[^/]*$/, ''));
      FS.writeFile(to, await kit.file(path).async('uint8array'));
    }
    FS.writeFile('/rom.md', rom);
    try { FS.mkdirTree('/out'); } catch (e) { /* there */ }
    py.runPython(`
import sys
sys.path.insert(0, '/recomp/tools')
for argv, mod in ((['recomp.py', '/rom.md', '--out', '/out'], 'recomp'),
                  (['z80recomp.py', '/rom.md', '--out', '/out'], 'z80recomp')):
    sys.argv = argv
    __import__(mod).main()
`);
    const gen = {};
    for (const f of FS.readdir('/out')) if (f[0] !== '.') gen[f] = FS.readFile('/out/' + f);
    log('  the game\'s code: ' + Object.keys(gen).length + ' files, ' +
        Math.round(Object.values(gen).reduce((n, b) => n + b.length, 0) / 1024) + ' KiB of C (' +
        ((Date.now() - t0) / 1000).toFixed(1) + ' s)');

    const headers = [];
    for (const path of Object.keys(kit.files))
      if (path.startsWith('include/') && !kit.files[path].dir) headers.push(path);
    const out = [];
    for (const [kind, path] of LIBS[platform]) {
      const t1 = Date.now();
      log('Compiling it for ' + { win64: 'Windows x64', linux64: platform === 'android' ? 'Android x86_64' : 'Linux x86-64',
                                  arm64: 'Android arm64-v8a', arm: 'Android armeabi-v7a' }[kind] + ' (TinyCC)…');
      const { m, output } = await tinycc(kind, kit);
      for (const h of headers) {
        const to = '/' + h;
        m.FS.mkdirTree(to.replace(/\/[^/]*$/, ''));
        m.FS.writeFile(to, await kit.file(h).async('uint8array'));
      }
      m.FS.mkdirTree('/gen');
      for (const [f, b] of Object.entries(gen)) m.FS.writeFile('/gen/' + f, b);
      m.FS.writeFile('/libtcc1.a', await kit.file('tcc/' + kind + '-libtcc1.a').async('uint8array'));
      const lib = path.split('/').pop();
      const args = ['-shared', '-nostdinc', '-nostdlib', '-w',
        '-I/include/gamelib', '-I/include/tcc', '-I/include/src', '-I/include/musashi',
        '-include', '/gen/game_bind.h', '-o', '/' + lib,
        ...Object.keys(gen).filter(f => GAME_SOURCES.test(f)).sort().map(f => '/gen/' + f),
        '/libtcc1.a'];
      if (kind !== 'win64') args.unshift('-Wl,-soname=' + lib);
      const rc = m.callMain(args);
      if (rc !== 0) throw new Error('compiling the game\'s code failed: ' + output().slice(0, 400));
      const data = m.FS.readFile('/' + lib);
      out.push({ path, data });
      log('  ' + path + ': ' + Math.round(data.length / 1024) + ' KiB (' + ((Date.now() - t1) / 1000).toFixed(1) + ' s)');
    }
    return out;
  }

  root.BonanzaGameCode = { make, LIBS };
})(window);
