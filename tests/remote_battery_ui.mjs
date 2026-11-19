import assert from 'node:assert/strict';
import fs from 'node:fs';
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);
const { chromium } = require(process.env.PLAYWRIGHT_PATH);
const html = fs.readFileSync(process.env.BATTERY_HTML_DUMP, 'utf8');
const browser = await chromium.launch({ executablePath: '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome', headless: true });
try {
  for (const width of [1280, 390, 320]) {
    const page = await browser.newPage({ viewport: { width, height: 640 } });
    const posts = [], errors = [];
    let lowAttempts = 0;
    const battery = { batteryMv: 4200, fresh: true, stable: true, fullCaptured: false, lowCaptured: false, reservePct: null, reserveIsNotStateOfCharge: true };
    page.on('pageerror', (e) => errors.push(e.message));
    await page.addInitScript(() => { window.confirm = () => { throw Error('Native confirmation forbidden'); }; });
    await page.route('http://remote.test/**', async (route) => {
      const req = route.request(), url = new URL(req.url());
      if (url.pathname === '/battery') return route.fulfill({ contentType: 'text/html; charset=utf-8', body: html });
      if (url.pathname === '/api/status') return route.fulfill({ contentType: 'application/json', body: JSON.stringify({ battery }) });
      assert.equal(url.pathname, '/api/remote/battery/calibrate'); assert.equal(req.method(), 'POST');
      assert.equal(url.searchParams.get('role'), 'remote'); assert.match(url.searchParams.get('token'), /^[a-f0-9]{32}$/); assert.match(url.searchParams.get('target'), /^([A-F0-9]{2}:){5}[A-F0-9]{2}$/);
      const point = url.searchParams.get('point'); posts.push(point);
      if (point === 'low' && ++lowAttempts === 1) return route.fulfill({ status: 409, body: 'ADC not stable/fresh: release buttons and wait 4 seconds' });
      battery[point === 'full' ? 'fullCaptured' : 'lowCaptured'] = true;
      return route.fulfill({ contentType: 'application/json', body: JSON.stringify(battery) });
    });
    await page.goto('http://remote.test/battery');
    await page.waitForFunction(() => document.getElementById('state').textContent.includes('4200'));
    assert.equal(posts.length, 0, 'Reading/calibration page must not write anything');
    await page.getByRole('button', { name: 'Запомнить FULL сейчас' }).click();
    await page.waitForFunction(() => document.getElementById('result').textContent.startsWith('Сохранено:'));
    await page.getByRole('button', { name: 'Запомнить LOW сейчас' }).click();
    await page.waitForFunction(() => document.getElementById('result').textContent.startsWith('Не сохранено:'));
    assert.equal(lowAttempts, 1, 'No automatic retry of calibration');
    await page.getByRole('button', { name: 'Запомнить LOW сейчас' }).click();
    await page.waitForFunction(() => document.getElementById('result').textContent.startsWith('Сохранено:'));
    assert.deepEqual(posts, ['full', 'low', 'low']); assert.deepEqual(errors, []);
    assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), 'Calibration result must wrap on mobile');
    await page.close();
  }
  console.log('Remote battery page: actual firmware HTML, read-only polling/no scene APIs, explicit authorized FULL/LOW only, visible error/no retry, no native confirmation,320/390/1280px without horizontal overflow (browser backend mocked)');
} finally { await browser.close(); }
