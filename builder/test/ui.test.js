#!/usr/bin/env node
/* Browser sweep of Bonanza Maker:  node builder/test/ui.test.js ROM [page]
 * Needs Playwright (npm i playwright, or NODE_PATH to one). For every stage: every tab and
 * card, placing, painting, objects, flip, hand, eraser, area tools, undo/redo, save and
 * open again, then New stage and Play. Page errors are failures. */
const path = require('path'), fs = require('fs');
let pw;
try { pw = require('playwright'); } catch (e) { pw = require('/opt/node-tools/node_modules/playwright'); }
const ROM = path.resolve(process.argv[2] || 'baserom.md');
const PAGE = process.argv[3] || 'file://' + path.resolve(__dirname, '../levelmaker.html');
(async () => {
  const b = await pw.chromium.launch({ args: ['--autoplay-policy=no-user-gesture-required', '--allow-file-access-from-files'] });
  const ctx = await b.newContext({ viewport: { width: 1500, height: 1000 }, acceptDownloads: true });
  const p = await ctx.newPage();
  const errors = [];
  p.on('pageerror', e => errors.push(e.message));
  p.on('dialog', d => d.accept());
  let fails = 0;
  const check = (cond, what) => { if (!cond) { fails++; console.log('FAIL', what); } };
  await p.goto(PAGE);
  await p.setInputFiles('#romfile', ROM);
  await p.waitForSelector('#coursepick', { state: 'visible' });
  const map = await p.$('#map');
  const at = async (x, y) => { const bb = await map.boundingBox(); return [bb.x + x * 2, bb.y + y * 2]; };
  /* scroll the building so x is in view, then click there */
  const click = async (x, y) => {
    await p.evaluate(x => { const v = document.getElementById('viewport'); v.scrollLeft = Math.max(0, x * 2 - 500); }, x);
    await p.mouse.click(...await at(x, y));
    await p.waitForTimeout(30);
  };
  const drag = async (x0, x1, y) => {
    await p.evaluate(x => { const v = document.getElementById('viewport'); v.scrollLeft = Math.max(0, x * 2 - 500); }, x0);
    await p.mouse.move(...await at(x0, y)); await p.mouse.down();
    await p.mouse.move(...await at(x1, y), { steps: 4 }); await p.mouse.up();
  };
  const nCourses = (await p.$$('.course:not([disabled])')).length;
  for (let k = 0; k < nCourses; k++) {
    if (k) { await p.click('#stagesbtn'); await p.waitForSelector('#coursepick', { state: 'visible' }); }
    await (await p.$$('.course:not([disabled])'))[k].click();
    await p.waitForTimeout(300);
    const stage = await p.textContent('#stagename');
    const errs0 = errors.length;
    const floors = await p.evaluate(() => document.getElementById('map').height / 2 / 80 | 0);
    const lower = Math.max(1, floors - 1);
    await p.evaluate(() => { document.getElementById('viewport').scrollLeft = 0; });
    /* Building: every card once */
    const cards = await p.$$('#parts .part');
    for (let i = 0; i < cards.length; i++) {
      await (await p.$$('#parts .part'))[i].click();
      await click(18 * 16 + 8 + (i % 3) * 64, lower * 80 + 40);
    }
    await p.keyboard.press('f');
    await click(30 * 16, lower * 80 + 40);
    /* guards, loot, gadgets: first card of each */
    for (const t of ['guards', 'loot', 'gadgets']) {
      await p.click(`[data-tab=${t}]`);
      const c = await p.$$('#parts .part:not([disabled])');
      if (c.length) { await c[0].click(); await click(12 * 16, lower * 80 + 50); }
    }
    /* paint: objects (own and another stage), single blocks with each kind */
    await p.click('[data-tab=paint]');
    let objs = await p.$$('#parts .part');
    for (let i = 0; i < Math.min(objs.length, 6); i++) {
      await (await p.$$('#parts .part'))[i].click();
      if (i % 2) await p.keyboard.press('f');
      await click((8 + 6 * i) * 16, (lower - (i % 2)) * 80 + 40);
    }
    await p.selectOption('.kinds select', { index: 1 });
    await p.waitForTimeout(200);
    objs = await p.$$('#parts .part');
    if (objs.length > 3) { await objs[3].click(); await click(44 * 16, lower * 80 + 40); }
    await p.click('.kinds button:has-text("Single blocks")');
    await p.selectOption('.kinds select', { index: 0 });
    for (const kind of ['Solid', 'Back lane', 'Walk-through']) {
      await p.click(`.kinds button:has-text("${kind}")`);
      const bl = await p.$$('#parts .part');
      await bl[Math.min(bl.length - 1, 40)].click();
      await click(10 * 16 + 8, lower * 80 + 56);
    }
    const stairBlock = await p.$$('#parts .part');
    await stairBlock[0].click();                         /* the Stairs group comes first */
    await click(24 * 16 + 8, lower * 80 + 40);
    await p.click('.kinds button:has-text("Objects")');
    /* hand: move a guard and the robbers, select an area: stamp, flip, clear, widen, narrow */
    await p.click('#handbtn');
    await drag(4 * 16, 7 * 16, 4);
    for (const id of ['#stampbtn']) { if (await p.isVisible(id)) await p.click(id); }
    await p.click('#handbtn');
    await drag(36 * 16, 39 * 16, lower * 80 + 20);
    for (const id of ['#flipareabtn', '#widenbtn', '#narrowbtn']) if (await p.isVisible(id)) await p.click(id);
    await drag(36 * 16, 37 * 16, lower * 80 + 20);
    if (await p.isVisible('#clearbtn')) await p.click('#clearbtn');
    await p.click('[data-tab=mine]');
    const mine = await p.$$('#parts .part');
    if (mine.length) { await mine[0].click(); await click(50 * 16, lower * 80 + 40); }
    /* eraser over the building, a door pillar and the robbers */
    await p.click('#erasebtn');
    await click(26 * 16, lower * 80 + 40);
    await click(22 * 16, lower * 80 + 40);
    /* undo everything, redo everything */
    for (let i = 0; i < 60; i++) await p.keyboard.press('Control+z');
    for (let i = 0; i < 60; i++) await p.keyboard.press('Control+y');
    const problems = await p.textContent('#problems');
    check(!/Too many|cannot|broken/.test(problems), stage + ': problems after editing: ' + problems);
    /* the edits above may have erased the exit: put the exit stairs back */
    if (/no exit/.test(problems)) {
      await p.click('[data-tab=build]');
      for (const c of await p.$$('#parts .part')) if ((await c.innerText()).includes('Exit')) { await c.click(); break; }
      await click(56 * 16, lower * 80 + 40);
    }
    /* save and open again */
    const before = await p.evaluate(() => document.getElementById('counts').textContent);
    await p.click('#savebtn');
    const [dl] = await Promise.all([p.waitForEvent('download', { timeout: 10000 }).catch(() => null), p.click('#saveok')]);
    if (!dl) { check(false, stage + ': save failed: ' + await p.textContent('#tip')); }
    else {
      const file = path.join(require('os').tmpdir(), 'bm-ui-' + k + '.bbro');
      await dl.saveAs(file);
      await p.setInputFiles('#openfile', file);
      await p.waitForTimeout(400);
      const after = await p.evaluate(() => document.getElementById('counts').textContent);
      check(before === after, stage + ': opened again differs: ' + before + ' / ' + after);
    }
    check(errors.length === errs0, stage + ': page errors: ' + errors.slice(errs0).join(' | '));
    console.log('done', stage);
  }
  /* new stage + play */
  await p.click('#stagesbtn');
  await p.waitForSelector('#coursepick', { state: 'visible' });
  await p.click('#ns-go');
  await p.waitForTimeout(400);
  await p.click('#playbtn');
  await p.waitForTimeout(12000);
  const playing = await p.evaluate(() => document.getElementById('playscreen').classList.contains('show'));
  check(playing, 'Play screen shown');
  check(!errors.length || errors.every(e => /autoplay/.test(e)), 'page errors: ' + errors.join(' | '));
  console.log(fails ? fails + ' failed' : 'all passed');
  await b.close();
  process.exit(fails ? 1 : 0);
})();
