// Evidence run 3: +map direct start, fullscreen, skip cinematic with Esc,
// then move the camera onto the hero: double-click the hero portrait
// (top-left) and click the minimap at the hero dot, so the 3D viewport
// shows terrain + units together with the full HUD.
const puppeteer = require('puppeteer-core');
const fs = require('fs');
const path = require('path');
const W = '/kaggle/working/wre';
const OUT = path.join(W, 'shots');
const PROFILE = process.env.PROFILE || path.join(W, 'chrome-profile');
const MAP = process.env.MAP_PATH || 'Maps\\FrozenThrone\\Campaign\\NightElfX01.w3x';
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
  page.on('console', (m) => console.log('[console]', m.type(), m.text().slice(0, 250)));
  page.on('pageerror', (e) => console.log('[pageerror]', String(e).slice(0, 250)));
  await page.evaluateOnNewDocument((mapPath) => {
    try { sessionStorage.setItem('openrealm-wc3-browser-mode', 'tft'); } catch (e) {}
    let real = null;
    Object.defineProperty(window, 'openrealmStartEngine', {
      configurable: true,
      enumerable: true,
      get() { return real; },
      set(v) {
        real = function (args) {
          const full = args.concat(['+map', mapPath]);
          console.log('[harness] startEngine args ' + JSON.stringify(full));
          return v.call(window, full);
        };
      },
    });
  }, MAP);
  await page.goto('http://127.0.0.1:8000/', { waitUntil: 'load', timeout: 120000 });
  console.log('page loaded');
  await sleep(25000);
  await page.evaluate(() => {
    const b = document.querySelector('#play-game');
    if (b && !b.hidden) b.click();
  });
  console.log('play clicked');
  const clickAt = async (x, y, hold) => {
    await page.mouse.move(x, y);
    await sleep(300);
    await page.mouse.down();
    await sleep(hold || 700);
    await page.mouse.up();
  };
  for (let i = 1; i <= 14; i++) {
    await sleep(20000);
    if (i === 3 || i === 4) {
      await page.evaluate(() => {
        const b = document.querySelector('#game-escape');
        if (b) b.click();
      });
      console.log('escape sent at step', i);
      await sleep(4000);
    }
    if (i === 6) {
      await clickAt(37, 95, 500);
      await sleep(600);
      await clickAt(37, 95, 500);
      console.log('hero portrait double-click sent');
      await sleep(4000);
    }
    if (i === 8) {
      await clickAt(152, 922, 500);
      console.log('minimap hero click sent');
      await sleep(4000);
    }
    if (i === 10) {
      await clickAt(152, 922, 500);
      console.log('minimap hero click sent again');
      await sleep(4000);
    }
    const name = 'map3_' + i + '.png';
    try {
      await page.screenshot({ path: path.join(OUT, name) });
      console.log('shot', name, new Date().toISOString());
    } catch (e) {
      console.log('shot failed', name, String(e).slice(0, 200));
    }
  }
  await browser.close();
  console.log('STARTMAP3 DONE');
})().catch((e) => { console.error('FATAL', e); process.exit(1); });
