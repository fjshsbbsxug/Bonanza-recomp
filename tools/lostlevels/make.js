#!/usr/bin/env node
/* Lost Levels: builds LostLevels.bbro, the arcade's stages the Mega Drive left out, as
 * extra stages for the port (played between the cartridge's own; see port/src/extrastages.c).
 *   node tools/lostlevels/make.js baserom.md [out.bbro]
 * The stages are made from your ROM's data, so nothing of the game is stored here.
 * The .bbro also has each stage as a Bonanza Maker level (levels/*.json) to keep editing. */
const fs = require('fs'), path = require('path');
const LC = require('../../builder/levelcore.js');
const JSZip = require('../../builder/jszip.min.js');

const [romPath, outPath = 'LostLevels.bbro'] = process.argv.slice(2);
if (!romPath) { console.error('usage: make.js baserom.md [out.bbro]'); process.exit(1); }
const rom = new Uint8Array(fs.readFileSync(romPath));
const stages = [require('./department_store.js')(LC, rom)];

/* each extra stage is its own patch (its own ROM slot), so they combine */
const zip = new JSZip();
zip.file('mod.ini', LC.modIni({
  name: 'Lost Levels', author: 'Bonanza Maker', version: '1.0',
  description: 'The arcade stages the Mega Drive left out: ' + stages.map(l => l.extra.name + ' (' + LC.EXTRA_AFTER[l.extra.after] + ')').join(', '),
}));
for (const L of stages) {
  const ex = LC.exportLevel(rom, L);
  for (const p of ex.problems) console.warn(L.extra.name + ': ' + p);
  const f = 'extra' + (L.extra.slot + 1);
  zip.file('patches/' + f + '.ips', LC.makeIps(rom, ex.rom));
  zip.file('levels/' + f + '.json', JSON.stringify(L));
}
zip.generateAsync({ type: 'nodebuffer', compression: 'DEFLATE' }).then(buf => {
  fs.writeFileSync(outPath, buf);
  console.log('wrote ' + path.resolve(outPath) + ' (' + stages.map(l => l.extra.name).join(', ') + ')');
});
