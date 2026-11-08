// Direct map-start harness: intercept window.openrealmStartEngine before the
// page's inline script defines it, append a Quake-style +map argument for the
// TFT campaign mission 1 map (inside War3x.mpq), log all console output, and
// screenshot every 30 s. This uses the engine's own command-line facility.
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
  page.on('console', (m) => console.log('[console]', m.type(), m.text().slice(0, 300)));
  page.on('pageerror', (e) => console.log('[pageerror]', String(e).slice(0, 300)));
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
  console.log('page loaded; waiting for engine + map load');
  for (let i = 1; i <= 10; i++) {
    await sleep(30000);
    const name = 'map_' + i + '.png';
    try {
      await page.screenshot({ path: path.join(OUT, name) });
      console.log('shot', name, new Date().toISOString());
    } catch (e) {
      console.log('shot failed', name, String(e).slice(0, 200));
    }
  }
  await browser.close();
  console.log('STARTMAP DONE');
})().catch((e) => { console.error('FATAL', e); process.exit(1); });
