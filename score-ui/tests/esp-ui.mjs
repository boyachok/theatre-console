// Production ESP build against a MOCK HTTP master. No GPIO, RF or physical outputs.
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import { INTERFACE_VERSION } from '../src/version.ts';
import { createProject } from '../src/domain/project.ts';
const require = createRequire(import.meta.url),
  { chromium } = require(process.env.PLAYWRIGHT_PATH || 'playwright');
const project = createProject(),
  mac = 'C0:4E:30:27:69:98';
project.devices[0].mac = mac;
let workspace = {
  format: 'theatre-score',
  schemaVersion: 1,
  interfaceVersion: INTERFACE_VERSION,
  draft: project,
  published: null,
};
const peer = {
  mac,
  name: 'Bench',
  ip: '192.168.50.33',
  ageMs: 10,
  scoreSupported: true,
  localScoreSupported: true,
  sceneStatus: 'idle',
  scoreReceived: 0,
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
const remote = {
  ...peer,
  role: 'remote',
  mac: 'AA:BB:CC:00:00:22',
  name: 'remote',
  ip: '192.168.50.88',
  p50: 14,
  p95: 20,
  rssiSlave: -65,
  worstRssi: -65,
  scoreSupported: false,
};
const helmet = {
  ...peer,
  role: 'helmet',
  mac: '30:30:F9:32:02:30',
  name: 'Helmet bench',
  ip: '192.168.50.65',
  voiceAvailable: false,
};
const status = {
  role: 'master',
  version: INTERFACE_VERSION,
  token: 'mock-only-token',
  mac: 'A0:85:E3:E2:BC:D4',
  serverMs: 0,
  measuring: true,
  console: {
    version: 0,
    fingerprint: '',
    syncFingerprint: '',
    draftFingerprint: '',
    draftRevision: 0,
    step: null,
    stepId: '',
    preview: false,
    livePreview: false,
    running: false,
    syncing: false,
    navigation: 0,
    transition: 0,
    ready: false,
    storedDevices: 0,
    plannedDevices: 0,
  },
  nodes: [peer, helmet],
  remotes: [remote],
};
const calls = [],
  errors = [];
let draft = null,
  lastSequence = 0;
function command(form) {
  assert.equal(form.get('token'), status.token);
  const left = (Number(form.get('deadline')) - (Math.floor(performance.now()) >>> 0)) | 0;
  assert(left >= 0 && left <= 1000);
  assert(Number(form.get('seq')) > lastSequence);
  lastSequence = Number(form.get('seq'));
  return Number(form.get('navigation')) === status.console.navigation;
}
function select(index, live) {
  status.console.step = index + 1;
  status.console.stepId = (live ? draft.steps : workspace.published.project.steps.map((s) => s.id))[
    index
  ];
  status.console.livePreview = status.console.preview = live;
  status.console.running = true;
  status.console.syncing = false;
  status.console.navigation++;
  status.console.transition++;
  peer.confirmedStep = null;
  peer.sceneStatus = 'waiting_ack';
}
const server = createServer(async (req, res) => {
  try {
    const url = new URL(req.url, 'http://localhost'),
      path = url.pathname;
    if (path.startsWith('/api/console/')) {
      let raw = '';
      for await (const data of req) raw += data;
      calls.push({ path, raw, url });
      res.setHeader('Content-Type', 'application/json');
      if (path.endsWith('/status')) {
        status.serverMs = Math.floor(performance.now()) >>> 0;
        res.end(JSON.stringify(status));
        return;
      }
      if (path.endsWith('/workspace')) {
        if (req.method === 'POST') {
          const next = JSON.parse(raw);
          if ((next.published?.version ?? 0) < status.console.version) {
            res.writeHead(409).end('Stale workspace');
            return;
          }
          workspace = next;
        }
        res.end(JSON.stringify(req.method === 'POST' ? {} : workspace));
        return;
      }
      if (path.endsWith('/draft')) {
        const body = JSON.parse(raw);
        assert.equal(body.base, status.console.draftFingerprint);
        assert.equal(body.revision, status.console.draftRevision);
        if (body.devices) draft = body;
        else
          for (const patch of body.patches)
            draft.devices.find((d) => d.id === patch.id).cues[patch.step] = patch.cue;
        draft.fingerprint = body.fingerprint;
        status.console.draftFingerprint = body.fingerprint;
        status.console.draftRevision++;
        if (!status.console.livePreview) status.console.syncFingerprint = body.fingerprint;
        if (status.console.livePreview) status.console.transition++;
        res.end(JSON.stringify({ fingerprint: body.fingerprint }));
        return;
      }
      if (path.endsWith('/preview-mode') || path.endsWith('/draft-go')) {
        const form = new URLSearchParams(raw);
        const current = command(form);
        if (!current && !(path.endsWith('/preview-mode') && form.get('enabled') === '0')) {
          res.writeHead(409).end('Сцена уже переключена мастером');
          return;
        }
        if (path.endsWith('/preview-mode') && form.get('enabled') === '0') {
          status.console.syncFingerprint = form.get('fingerprint');
          status.console.livePreview = status.console.preview = status.console.running = false;
          status.console.syncing = true;
          status.console.navigation++;
        } else {
          assert(draft);
          select(draft.steps.indexOf(form.get('stepId')), true);
        }
        res.end('{}');
        return;
      }
      if (path.endsWith('/publish')) {
        const body = JSON.parse(raw);
        assert.equal(body.version, status.console.version + 1);
        const wasLive = status.console.livePreview;
        workspace = JSON.parse(body.workspaceJson);
        status.console.version = body.version;
        status.console.fingerprint = status.console.syncFingerprint = body.fingerprint;
        status.console.livePreview =
          status.console.preview =
          status.console.running =
          status.console.ready =
            false;
        status.console.syncing = true;
        status.console.storedDevices = 0;
        status.console.plannedDevices = body.devices.length;
        if (wasLive) status.console.navigation++;
        peer.confirmedStep = null;
        peer.sceneStatus = 'loading';
        res.end(JSON.stringify({ version: body.version }));
        return;
      }
      if (path.endsWith('/navigate')) {
        const form = new URLSearchParams(raw);
        if (!command(form) || (!status.console.livePreview && !status.console.ready)) {
          res.writeHead(409).end('Показ не готов: ожидаем загрузку партитур');
          return;
        }
        const action = Number(form.get('action')),
          count = status.console.livePreview
            ? draft.steps.length
            : workspace.published.project.steps.length;
        const at = status.console.step == null ? -1 : status.console.step - 1;
        const next =
          action === 3
            ? 0
            : action === 4
              ? count - 1
              : action === 2
                ? Math.max(0, at - 1)
                : Math.min(count - 1, at + 1);
        select(next, status.console.livePreview);
        res.end('{}');
        return;
      }
      if (path.endsWith('/go')) {
        const form = new URLSearchParams(raw);
        assert.equal(Number(form.get('version')), status.console.version);
        assert(!form.has('cue') && !form.has('devices'));
        if (!command(form) || !status.console.ready) {
          res.writeHead(409).end('Показ не готов: ожидаем загрузку партитур');
          return;
        }
        select(Number(form.get('step')), false);
        res.end('{}');
        return;
      }
      const form = new URLSearchParams(raw);
      assert.equal(form.get('token'), status.token);
      if (path.endsWith('/measure')) status.measuring = form.get('enabled') === '1';
      if (path.endsWith('/network/reset')) {
        status.nodes = [];
        status.remotes = [];
      }
      res.end('{}');
      return;
    }
    if (url.pathname === '/favicon.ico') {
      res.writeHead(204).end();
      return;
    }
    const file = url.pathname === '/' ? '/index.html' : url.pathname;
    if (file.includes('..')) {
      res.writeHead(403).end();
      return;
    }
    res.setHeader(
      'Content-Type',
      file.endsWith('.js') ? 'text/javascript' : file.endsWith('.css') ? 'text/css' : 'text/html',
    );
    res.end(await readFile(new URL('../dist' + file, import.meta.url)));
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
  await page.waitForFunction(() =>
    document.querySelector('.version-chip')?.textContent.includes('v1'),
  );
  assert.equal(status.console.step, null);
  assert.equal(status.console.running, false); // Initial sync cannot execute.
  const cell = page.locator('.parameter-row[data-parameter="pwm-1"]').first().locator('td').first();
  await cell.locator('button').click();
  await page.getByLabel('Вход', { exact: true }).fill('0.2');
  assert.equal(
    calls.filter(
      (c) => c.path.endsWith('/go') || c.path.endsWith('/navigate') || c.path.endsWith('/draft-go'),
    ).length,
    0,
  );
  await page.getByRole('switch', { name: 'Предпросмотр', exact: true }).click();
  await page.waitForFunction(
    () => document.querySelector('#playing-step option:checked')?.textContent === '1',
  );
  assert.equal(status.console.livePreview, true);
  assert.deepEqual(draft.devices[0].cues[0].d[0], [255, 200, 0]);
  const publications = calls.filter((c) => c.path.endsWith('/publish')).length;
  await page.getByLabel('Яркость', { exact: true }).fill('60');
  await page.waitForTimeout(600);
  assert.equal(calls.filter((c) => c.path.endsWith('/publish')).length, publications); // Live editing never publishes.
  assert(calls.some((c) => c.path.endsWith('/draft') && JSON.parse(c.raw).patches));
  assert.equal(draft.devices[0].cues[0].d[0][0], 153);
  await page.getByRole('button', { name: 'Сеть', exact: true }).click();
  const panel = page.getByRole('dialog', { name: 'Сеть', exact: true });
  await panel.getByText('Подключено', { exact: true }).waitFor();
  assert.equal(await panel.locator('tbody tr').count(), 3);
  await panel.getByRole('button', { name: 'Мигать remote', exact: true }).click();
  await page.waitForTimeout(80);
  assert(
    calls.some(
      (c) => c.path.endsWith('/identify') && new URLSearchParams(c.raw).get('mac') === remote.mac,
    ),
  );
  const sceneRow = panel.locator('tbody tr').first();
  assert.equal((await sceneRow.locator('td').nth(9).textContent()).trim(), '—');
  peer.confirmedStep = 1;
  peer.sceneStatus = 'applied';
  peer.scoreReceived++;
  await panel.getByText('Применена', { exact: true }).waitFor();
  assert.equal((await sceneRow.locator('td').nth(9).textContent()).trim(), '1');
  await panel.getByRole('switch', { name: 'Пинг-тест', exact: true }).click();
  await page.waitForTimeout(250);
  assert.equal(status.measuring, false);
  await panel.getByRole('button', { name: 'Закрыть', exact: true }).click();
  await page.getByRole('button', { name: 'Приборы', exact: true }).click();
  const settings = page.getByRole('dialog', { name: 'Приборы', exact: true });
  await settings.getByRole('button', { name: 'Настроить Прибор 1', exact: true }).click();
  assert.equal(
    await settings
      .getByLabel('Контроллер прибора', { exact: true })
      .locator(`option[value="${remote.mac}"]`)
      .count(),
    0,
  );
  await settings.getByLabel('Тип прибора', { exact: true }).selectOption('helmet');
  assert.equal(
    await settings
      .getByLabel('Контроллер прибора', { exact: true })
      .locator(`option[value="${helmet.mac}"]`)
      .count(),
    1,
  );
  assert.equal(
    await settings
      .getByLabel('Контроллер прибора', { exact: true })
      .locator(`option[value="${mac}"]`)
      .count(),
    0,
  );
  await settings.getByRole('button', { name: 'Отмена', exact: true }).click();
  await page.getByRole('button', { name: 'Сдублировать шаг', exact: true }).click();
  await page.waitForFunction(
    () => document.querySelector('#playing-step option:checked')?.textContent === '2',
  );
  assert.equal(draft.steps.length, 2);
  assert.equal(status.console.version, workspace.published.version);
  await page
    .locator('.parameter-row[data-parameter="pwm-1"]')
    .first()
    .locator('td')
    .nth(1)
    .locator('button')
    .click();
  await page.getByLabel('Яркость', { exact: true }).fill('60');
  await page.waitForTimeout(600);
  // A remote action mutates ONLY the mock master's authoritative cursor. Browser must follow without posting GO back.
  const before = calls.filter((c) => c.path.endsWith('/draft-go')).length;
  select(0, true);
  await page.waitForFunction(
    () => document.querySelector('#playing-step option:checked')?.textContent === '1',
  );
  await page.getByLabel('Яркость', { exact: true }).fill('45');
  await page.waitForTimeout(600);
  assert.equal(draft.devices[0].cues[0].d[0][0], 115);
  assert.equal(draft.devices[0].cues[1].d[0][0], 153);
  assert.equal(calls.filter((c) => c.path.endsWith('/draft-go')).length, before); // No echo loop or stale browser override.
  await page.getByRole('switch', { name: 'Предпросмотр', exact: true }).click();
  await page.waitForFunction(() => !document.querySelector('[aria-label="Предпросмотр"]').disabled);
  assert.equal(status.console.livePreview, false);
  assert.equal(status.console.running, false);
  assert.equal(status.console.step, 1);
  assert.equal(peer.scoreVersion, 0);
  assert.equal(peer.confirmedStep, null); // HTTP acceptance is NOT storage/execution ACK.
  await page.getByRole('button', { name: 'Следующий шаг', exact: true }).click();
  await page.getByRole('status').filter({ hasText: 'Показ не готов' }).waitFor();
  assert.equal(status.console.step, 1);
  status.console.ready = true;
  status.console.syncing = false;
  status.console.storedDevices = 1;
  peer.scoreVersion = status.console.version;
  peer.sceneStatus = 'ready';
  await page.getByRole('button', { name: 'Следующий шаг', exact: true }).click();
  await page.waitForFunction(
    () => document.querySelector('#playing-step option:checked')?.textContent === '2',
  );
  assert.equal(status.console.preview, false);
  assert.equal(peer.confirmedStep, null);
  select(0, false);
  await page.waitForFunction(
    () => document.querySelector('#playing-step option:checked')?.textContent === '1',
  );
  assert.equal(
    await page.getByRole('switch', { name: 'Предпросмотр', exact: true }).isChecked(),
    false,
  );
  // In show mode edits synchronize storage, but never apply the edited current scene.
  const goes = calls.filter((c) => c.path.endsWith('/go') || c.path.endsWith('/navigate')).length,
    oldVersion = status.console.version;
  await cell.locator('button').click();
  await page.waitForTimeout(900);
  assert(status.console.version > oldVersion);
  assert.equal(
    calls.filter((c) => c.path.endsWith('/go') || c.path.endsWith('/navigate')).length,
    goes,
  );
  assert.equal(status.console.running, false);
  for (const width of [390, 375, 320]) {
    await page.setViewportSize({ width, height: 844 });
    for (const name of ['Следующий шаг', 'Сеть']) {
      const b = await page.getByRole('button', { name, exact: true }).boundingBox();
      assert(b.x >= 0 && b.x + b.width <= width, `${width}: ${name} overflow`);
    }
    const b = await page.getByRole('switch', { name: 'Предпросмотр', exact: true }).boundingBox();
    assert(b.x >= 0 && b.x + b.width <= width);
  }
  await page.setViewportSize({ width: 1440, height: 900 });
  await page.getByRole('button', { name: 'Сеть', exact: true }).click();
  await page.getByRole('button', { name: 'Сбросить сеть', exact: true }).click();
  await page
    .getByRole('dialog', { name: 'Подтверждение', exact: true })
    .getByRole('button', { name: 'Сбросить', exact: true })
    .click();
  await page.waitForFunction(
    () => document.querySelectorAll('.network-table tbody tr').length === 0,
  );
  assert.deepEqual(errors, []);
  console.log(
    'MOCK ESP production UI: preview switch, full draft/patches, live edits without publication, remote draft/show cursor follow + inspector without echo, auto flash sync without GO, real ACK distinction/preload block, remote exclusion/helmet profile, ping/identify/reset, 320/375/390 header passed; NOT physical GPIO/RF tests.',
  );
} finally {
  await browser.close();
  await new Promise((resolve) => server.close(resolve));
}
