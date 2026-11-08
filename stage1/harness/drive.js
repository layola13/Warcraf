// Persistent interactive driver: one long-lived browser session.
// Protocol: write /kaggle/working/wre/cmd.json {"seq":N,"action":"click","fx":0.8,"fy":0.2,"wait":12000}
// or {"action":"key","key":"Escape"} or {"action":"shot"} or {"action":"quit"}.
// After executing, writes drive_status.json {seq, done:true, shot:"drive_N.png"}.
const puppeteer = require('puppeteer-core');
const fs = require('fs');
const path = require('path');

const W = '/kaggle/working/wre';
const CMD = path.join(W, 'cmd.json');
const STATUS = path.join(W, 'drive_status.json');
const OUT = path.join(W, 'shots');
const PROFILE = process.env.PROFILE || path.join(W, 'chrome-profile');
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

(async () => {
  const browser = await puppeteer.launch({
    executablePath: '/usr/bin/google-chrome',
    headless: 'new',
    userDataDir: PROFILE,
    args: [
      '--no-sandbox', '--use-angle=swiftshader', '--enable-unsafe-swiftshader',
      '--enable-webgl', '--ignore-gpu-blocklist', '--window-size=1280,960',
      '--autoplay-policy=no-user-gesture-required', '--mute-audio',
    ],
  });
  const page = await browser.newPage();
  await page.setViewport({ width: 1280, height: 960 });
  page.on('pageerror', (e) => console.log('[pageerror]', String(e).slice(0, 200)));
  await page.goto('http://127.0.0.1:8000/', { waitUntil: 'load', timeout: 120000 });
  await sleep(5000);
  await page.evaluate(() => {
    const s = document.querySelector('select');
    if (s) {
      const tft = [...s.options].find((o) => /frozen throne/i.test(o.text));
      if (tft && !s.disabled) { s.value = tft.value; s.dispatchEvent(new Event('change', { bubbles: true })); }
    }
  });
  await sleep(2000);
  await page.evaluate(() => {
    const b = document.querySelector('#play-game') || [...document.querySelectorAll('button')].find((x) => /play game/i.test(x.textContent));
    if (b) b.click();
  });
  console.log('play clicked; waiting for menu settle');
  await sleep(75000);
  await page.screenshot({ path: path.join(OUT, 'drive_0.png') });
  fs.writeFileSync(STATUS, JSON.stringify({ seq: 0, done: true, shot: 'drive_0.png' }));
  console.log('drive loop ready');

  let lastSeq = 0;
  for (;;) {
    await sleep(1000);
    let cmd = null;
    try { cmd = JSON.parse(fs.readFileSync(CMD, 'utf8')); } catch (e) { continue; }
    if (!cmd || cmd.seq === lastSeq) continue;
    lastSeq = cmd.seq;
    try {
      if (cmd.action === 'quit') { fs.writeFileSync(STATUS, JSON.stringify({ seq: cmd.seq, done: true, bye: true })); break; }
      if (cmd.action === 'click') {
        const rect = await page.evaluate(() => { const c = document.querySelector('canvas'); const r = c.getBoundingClientRect(); return { x: r.x, y: r.y, w: r.width, h: r.height }; });
        const px = Math.round(rect.x + cmd.fx * rect.w);
        const py = Math.round(rect.y + cmd.fy * rect.h);
        await page.mouse.move(px, py);
        await sleep(300);
        await page.mouse.down();
        await sleep(cmd.hold || 900);
        await page.mouse.up();
        console.log('click seq', cmd.seq, px, py);
      } else if (cmd.action === 'key') {
        await page.keyboard.press(cmd.key);
        console.log('key seq', cmd.seq, cmd.key);
      }
      await sleep(cmd.wait || 10000);
      const name = 'drive_' + cmd.seq + '.png';
      await page.screenshot({ path: path.join(OUT, name) });
      fs.writeFileSync(STATUS, JSON.stringify({ seq: cmd.seq, done: true, shot: name }));
    } catch (e) {
      fs.writeFileSync(STATUS, JSON.stringify({ seq: cmd.seq, done: true, error: String(e).slice(0, 200) }));
    }
  }
  await browser.close();
  console.log('drive end');
})().catch((e) => { console.error('FATAL', e); process.exit(1); });
