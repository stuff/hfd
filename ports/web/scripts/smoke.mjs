// Headless smoke test: serves dist/, clicks to start, moves the cursor, selects a song,
// and reports console errors and the replay state. usage: node scripts/smoke.mjs <outdir>
import puppeteer from 'puppeteer-core';
import { preview } from 'vite';

const out = process.argv[2] ?? '.';
const server = await preview({ preview: { port: 4173, strictPort: true } });
const browser = await puppeteer.launch({
  executablePath: '/usr/bin/google-chrome',
  args: ['--autoplay-policy=no-user-gesture-required', '--no-sandbox'],
});
const page = await browser.newPage();
await page.setViewport({ width: 832, height: 552 });
const errors = [];
page.on('console', (m) => (m.type() === 'error' || m.type() === 'warning') && errors.push(m.text()));
page.on('pageerror', (e) => errors.push(String(e)));
page.on('response', (r) => r.status() >= 400 && errors.push(`${r.status()} ${r.url()}`));
await page.goto('http://localhost:4173/?authentic&debug');
await new Promise((r) => setTimeout(r, 500));
await page.screenshot({ path: `${out}/1_start.png` });
await page.mouse.click(416, 276);
await new Promise((r) => setTimeout(r, 2000));
await page.screenshot({ path: `${out}/2_menu.png` });
const tick = await page.evaluate(() => JSON.stringify(window.hfd.music.current()));
console.log('replay tick heard:', tick);
await page.keyboard.press('ArrowUp');
await page.keyboard.press('ArrowUp');
await new Promise((r) => setTimeout(r, 300));
await page.screenshot({ path: `${out}/3_cursor.png` });
await page.keyboard.press('Enter');
await new Promise((r) => setTimeout(r, 4000));
await page.screenshot({ path: `${out}/4_loading.png` });
await new Promise((r) => setTimeout(r, 15000));
await page.screenshot({ path: `${out}/5_menu_new_song.png` });
console.log(errors.length ? errors.join('\n') : 'no console errors');
await browser.close();
server.httpServer.close();
