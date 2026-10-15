// node apk_test.mjs IN.apk OUT.apk: the engine's APK with a stand-in library added and
// signed as the Android builder page signs it (builder/apk.js), for apksigner to check
import fs from 'fs';
import { createRequire } from 'module';
const require = createRequire(import.meta.url);
const apk = require('../../builder/apk.js');
const [input, output] = process.argv.slice(2);
const lib = new Uint8Array(4096).map((_, i) => i * 7);
const out = await apk.build(new Uint8Array(fs.readFileSync(input)),
  ['arm64-v8a', 'armeabi-v7a', 'x86_64'].map(abi => ({ name: 'lib/' + abi + '/libbonanza_game.so', data: lib })), null);
fs.writeFileSync(output, out);
console.log('signed', output, out.length, 'bytes');
