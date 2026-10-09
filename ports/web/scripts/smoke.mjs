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
const shot = async (name, wait) => {
  await new Promise((r) => setTimeout(r, wait));
  await page.screenshot({ path: `${out}/${name}.png` });
};
await page.goto('http://localhost:4173/?debug');
await shot('1_start', 500);
await page.mouse.click(416, 276);
await shot('2_ear_loading', 2000);
await shot('3_ear_decrunching', 3000);
await shot('4_aura', 13000);
await shot('5_hfd', 14000);
await shot('6_aeon', 17000);
const tick = await page.evaluate(() => JSON.stringify(window.hfd.music.current()));
console.log('replay tick heard:', tick);
await shot('7_textpage', 8000);
await shot('7b_textpage', 6000);
await page.keyboard.press('Space');
await shot('7c_menu', 2000);
await page.keyboard.press('ArrowUp');
await page.keyboard.press('ArrowUp');
await page.keyboard.press('Enter');
await shot('8_loading', 4000);
await shot('9_menu_new_song', 6000);
console.log(errors.length ? errors.join('\n') : 'no console errors');
await browser.close();
server.httpServer.close();
