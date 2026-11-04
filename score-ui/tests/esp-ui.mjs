// Production ESP build against an explicitly mocked HTTP master; never talks to an ESP.
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import { createProject } from '../src/domain/project.ts';
const require = createRequire(import.meta.url),
  { chromium } = require(process.env.PLAYWRIGHT_PATH || 'playwright');
const project = createProject(),
  mac = 'C0:4E:30:27:69:98';
project.devices[0].mac = mac;
let workspace = {
  format: 'theatre-score',
  schemaVersion: 1,
  interfaceVersion: '0.3.0',
  draft: project,
  published: null,
};
const peer = {
  mac,
  name: 'Bench',
  ip: '192.168.50.33',
  ageMs: 10,
  scoreSupported: true,
  scoreVersion: 0,
  confirmedStep: null,
  sent: 3,
  received: 3,
  timeouts: 0,
  p50: 10,
  p95: 15,
  worstRssi: -50,
  rssiSlave: -50,
};
const status = {
    role: 'master',
    token: 'mock-only-token',
    mac: 'A0:85:E3:E2:BC:D4',
    serverMs: 0,
    measuring: true,
    console: { version: 0, fingerprint: '', step: null, preview: false },
    nodes: [peer],
  },
  calls = [],
  errors = [];
let lastSequence = 0;
const server = createServer(async (req, res) => {
  try {
    const url = new URL(req.url, 'http://localhost'),
      path = url.pathname;
    if (path.startsWith('/api/console/')) {
      let raw = '';
      for await (const data of req) raw += data;
      calls.push({ path, raw, url });
      res.setHeader('Content-Type', 'application/json');
      if (path === '/api/console/status') {
        status.serverMs = Math.floor(performance.now()) >>> 0;
        res.end(JSON.stringify(status));
        return;
      }
      if (path === '/api/console/workspace') {
        if (req.method === 'POST') workspace = JSON.parse(raw);
        res.end(JSON.stringify(req.method === 'POST' ? {} : workspace));
        return;
      }
      if (path === '/api/console/preview') {
        const seq = Number(url.searchParams.get('seq')),
          remaining =
            (Number(url.searchParams.get('deadline')) - (Math.floor(performance.now()) >>> 0)) | 0;
        assert(remaining >= 0 && remaining <= 1000);
        assert(seq > lastSequence);
        lastSequence = seq;
        const body = JSON.parse(raw);
        status.console.step = body.step + 1;
        status.console.preview = true;
        res.end(JSON.stringify({ transition: seq }));
        return;
      }
      if (path === '/api/console/publish') {
        const body = JSON.parse(raw);
        workspace = JSON.parse(body.workspaceJson);
        status.console.version = body.version;
        status.console.step = null;
        status.console.preview = false;
        res.end(JSON.stringify({ version: body.version }));
        return;
      }
      const form = new URLSearchParams(raw);
      assert.equal(form.get('token'), status.token);
      if (path === '/api/console/measure') status.measuring = form.get('enabled') === '1';
      if (path === '/api/console/network/reset') status.nodes = [];
      res.end('{}');
      return;
    }
    if (path === '/favicon.ico') { res.writeHead(204).end(); return; }
    const file = path === '/' ? '/index.html' : path;
    if (file.includes('..')) {
      res.writeHead(403).end();
      return;
    }
    const bytes = await readFile(new URL('../dist' + file, import.meta.url));
    res.setHeader(
      'Content-Type',
      file.endsWith('.js') ? 'text/javascript' : file.endsWith('.css') ? 'text/css' : 'text/html',
    );
    res.end(bytes);
  } catch (error) {
    errors.push(String(error));
    res.writeHead(500).end(String(error));
  }
});
await new Promise((resolve) => server.listen(5179, '127.0.0.1', resolve));
const browser = await chromium.launch({
  executablePath: '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  headless: true,
});
try {
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
  page.on('pageerror', (e) => errors.push(e.message));
  await page.goto('http://127.0.0.1:5179');
  await page.waitForSelector('.parameter-row');
  await page.waitForFunction(
    () => document.querySelector('.device-group code')?.textContent === '192.168.50.33',
  );
  const cell = page.locator('.parameter-row[data-parameter="pwm-1"]').first().locator('td').first();
  await cell.locator('button').click();
  assert.equal(await cell.locator('input').inputValue(), '100');
  await page.getByLabel('Вход', { exact: true }).fill('0.2');
  assert.equal(calls.filter((c) => c.path.endsWith('/preview')).length, 0);
  await page.getByRole('button', { name: 'Следующий шаг', exact: true }).click();
  await page.waitForFunction(
    () => document.querySelector('#playing-step option:checked')?.textContent === '1',
  );
  const preview = JSON.parse(calls.find((c) => c.path.endsWith('/preview')).raw);
  assert.deepEqual(preview.devices[0].cues[0].d[0], [255, 200, 0]);
  await page.getByRole('button', { name: 'Сеть', exact: true }).click();
  const panel = page.getByRole('dialog', { name: 'Сеть', exact: true });
  await panel.getByText('Подключено', { exact: true }).waitFor();
  assert.equal(await panel.locator('tbody tr').count(), 1);
  assert.equal((await panel.locator('tbody tr td').nth(9).innerText()).trim(), '—');
  peer.confirmedStep = 1;
  await page.waitForFunction(
    () => document.querySelector('.network-table tbody tr td:nth-of-type(10)')?.textContent === '1',
  );
  await panel.getByRole('button', { name: 'Мигать Прибор 1', exact: true }).click();
  await page.waitForTimeout(100);
  assert(
    calls.some(
      (c) => c.path.endsWith('/identify') && new URLSearchParams(c.raw).get('mac') === mac,
    ),
  );
  await panel.getByRole('switch', { name: 'Пинг-тест', exact: true }).click();
  await page.waitForFunction(() => !document.querySelector('[role="switch"]').checked);
  await page.waitForTimeout(150);
  assert.equal(status.measuring, false);
  await panel.getByRole('button', { name: 'Закрыть', exact: true }).click();
  await page.getByRole('button', { name: 'Опубликовать', exact: true }).click();
  await page.waitForFunction(() =>
    document.querySelector('.version-chip')?.textContent.includes('v1'),
  );
  const publication = JSON.parse(calls.find((c) => c.path.endsWith('/publish')).raw);
  assert.equal(publication.version, 1);
  assert.equal(publication.devices[0].mac, mac);
  assert.equal(peer.scoreVersion, 0); // HTTP success does not fabricate slave delivery.
  await page.getByRole('button', { name: 'Сеть', exact: true }).click();
  page.once('dialog', (dialog) => dialog.accept());
  await page.getByRole('button', { name: 'Сбросить сеть', exact: true }).click();
  await page.waitForFunction(
    () => document.querySelectorAll('.network-table tbody tr').length === 0,
  );
  assert(calls.some((c) => c.path.endsWith('/network/reset')));
  assert.deepEqual(errors, []);
  console.log(
    'ESP production UI / MOCK master: workspace load, real-adapter mode, IP/MAC, edit ≠ execution, guarded first-step preview, exact 200ms fade, receipt-only confirmation, publish without fake delivery, identify/test/reset passed.',
  );
} finally {
  await browser.close();
  await new Promise((resolve) => server.close(resolve));
}
