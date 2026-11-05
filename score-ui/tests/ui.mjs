import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url),
  { chromium } = require(process.env.PLAYWRIGHT_PATH || 'playwright');
const release = JSON.parse(
  await fs.readFile(new URL('../../version.json', import.meta.url), 'utf8'),
);
const base = process.env.SCORE_URL || 'http://127.0.0.1:5173';
const browser = await chromium.launch({
  executablePath: '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  headless: true,
});
const pause = (ms) => new Promise((r) => setTimeout(r, ms)),
  failures = [],
  externals = [];
function watch(p) {
  p.on('pageerror', (e) => failures.push(e.message));
  p.on('request', (r) => {
    if (!r.url().startsWith(base) && !r.url().startsWith('blob:') && !r.url().startsWith('data:'))
      externals.push(r.url());
  });
}
const current = (p) => p.locator('#playing-step option:checked').textContent();
const heading = (p, index) => p.locator('.step-heading').nth(index);
const paramRow = (p, parameter, device) =>
  p
    .locator(
      `.parameter-row[data-parameter="${parameter}"]${device ? `[data-device="${device}"]` : ''}`,
    )
    .first();
const cell = (p, index, parameter, device) =>
  paramRow(p, parameter, device).locator('td').nth(index);
async function edit(p, index, parameter, value) {
  const c = cell(p, index, parameter);
  await c.locator('button').click();
  await p
    .getByLabel(parameter === 'audio' ? 'Номер трека' : 'Яркость', { exact: true })
    .fill(String(value));
}
try {
  const ctx = await browser.newContext({ viewport: { width: 1440, height: 900 } }),
    p = await ctx.newPage();
  watch(p);
  await p.goto(base);
  await p.waitForSelector('.parameter-row');
  assert.equal(await p.locator('.step-heading').count(), 1);
  assert.equal(await p.locator('.device-group').count(), 10);
  assert.equal(await p.locator('.parameter-row').count(), 160);
  assert((await p.locator('h1').innerText()).includes('Theatre console'));
  assert((await p.getByLabel('Название спектакля').boundingBox()).width >= 280);
  await p.getByRole('button', { name: 'Свернуть Прибор 1', exact: true }).click();
  assert.equal(await p.locator('.parameter-row').count(), 144);
  await p.getByRole('button', { name: 'Развернуть Прибор 1', exact: true }).click();
  assert.equal(await p.locator('.parameter-row').count(), 160);
  await cell(p, 0, 'pwm-1').locator('button').click();
  assert.equal(await p.getByLabel('Яркость', { exact: true }).inputValue(), '100');
  assert.equal(await p.getByLabel('Вход', { exact: true }).getAttribute('step'), '0.2');
  assert.equal(
    await p.locator('[data-live-parameter="pwm-1"]').getAttribute('data-live-value'),
    '0',
  );
  await cell(p, 0, 'pwm-1').locator('button').click();
  assert.equal(await p.getByLabel('Яркость', { exact: true }).inputValue(), '0');
  await cell(p, 0, 'audio').locator('button').click();
  assert.equal(await p.getByLabel('Номер трека', { exact: true }).inputValue(), '1');
  await p.getByLabel('Номер трека', { exact: true }).fill('5');
  await cell(p, 0, 'audio').locator('button').click();
  assert.equal(await p.getByLabel('Номер трека', { exact: true }).inputValue(), '');
  await cell(p, 0, 'audio').locator('button').click();
  assert.equal(await p.getByLabel('Номер трека', { exact: true }).inputValue(), '5');
  await cell(p, 0, 'audio').locator('button').click();
  assert.equal(await p.locator('footer.step-player').count(), 0);
  assert(await p.locator('.app-header .step-player').isVisible());
  assert.equal(
    await p.getByRole('button', { name: 'Сдублировать шаг', exact: true }).textContent(),
    '',
  );
  const text = await p.locator('body').innerText();
  assert(!/Предпросмотр|Локально|один шаг · одно нажатие|\bGO\b/.test(text));
  await p.getByRole('button', { name: 'Сдублировать шаг', exact: true }).click();
  assert.equal(await p.locator('.step-heading').count(), 2);
  assert.equal(await current(p), '1');
  await edit(p, 1, 'pwm-1', 70);
  await p.getByLabel('Вход', { exact: true }).fill('2');
  await p.getByLabel('Выход', { exact: true }).fill('1');
  assert.equal(
    await p.locator('[data-live-parameter="pwm-1"]').getAttribute('data-live-value'),
    '0',
  );
  await edit(p, 1, 'audio', 3);
  await p.getByRole('button', { name: 'Сдублировать шаг', exact: true }).click();
  await edit(p, 2, 'pwm-1', 20);
  assert.equal(await p.getByLabel('Вход', { exact: true }).inputValue(), '2');
  assert.equal(await p.getByLabel('Выход', { exact: true }).inputValue(), '1');
  assert.deepEqual(await p.locator('.step-select').allTextContents(), ['1', '2', '3']);
  const ids = await p
    .locator('.step-heading')
    .evaluateAll((nodes) => nodes.map((n) => n.dataset.stepId));
  await p.getByRole('button', { name: 'Следующий шаг', exact: true }).click();
  assert.equal(await current(p), '2');
  assert.equal(
    await p.locator('.step-heading.selected-column').getAttribute('data-step-id'),
    ids[2],
  );
  await pause(180);
  const halfway = Number(
    await p.locator('[data-live-parameter="pwm-1"]').getAttribute('data-live-value'),
  );
  assert(halfway > 0 && halfway < 70);
  await p.getByRole('button', { name: 'Следующий шаг', exact: true }).click();
  await pause(2200);
  assert.equal(
    await p.locator('[data-live-parameter="pwm-1"]').getAttribute('data-live-value'),
    '20',
  );
  await p.getByRole('button', { name: 'Предыдущий шаг', exact: true }).click();
  assert.equal(await current(p), '2');
  const back = p.getByRole('button', { name: 'Предыдущий шаг', exact: true }),
    br = await back.boundingBox();
  await p.mouse.move(br.x + br.width / 2, br.y + br.height / 2);
  await p.mouse.down();
  await pause(400);
  assert.equal(await current(p), '2');
  await pause(2750);
  assert.equal(await current(p), '1');
  await p.mouse.up();
  const next = p.getByRole('button', { name: 'Следующий шаг', exact: true }),
    nr = await next.boundingBox();
  await p.mouse.move(nr.x + nr.width / 2, nr.y + nr.height / 2);
  await p.mouse.down();
  await pause(400);
  assert.equal(await current(p), '1');
  await pause(2750);
  assert.equal(await current(p), '3');
  await p.mouse.up();
  await p.selectOption('#playing-step', ids[0]);
  await next.focus();
  await p.keyboard.press('Space');
  assert.equal(await current(p), '2');
  await p.selectOption('#playing-step', ids[2]);
  await heading(p, 2)
    .locator('.drag-handle')
    .dragTo(heading(p, 0), { targetPosition: { x: 5, y: 20 } });
  assert.equal(await heading(p, 0).getAttribute('data-step-id'), ids[2]);
  assert.equal(await current(p), '1');
  assert((await cell(p, 0, 'audio').innerText()).includes('3'));
  await p.getByRole('button', { name: 'Шаг правее', exact: true }).click();
  assert.equal(await heading(p, 1).getAttribute('data-step-id'), ids[2]);
  assert.equal(await current(p), '2');
  await p.getByRole('button', { name: 'Приборы', exact: true }).click();
  const devices = p.getByRole('dialog', { name: 'Приборы', exact: true });
  assert.equal(await devices.locator('tr[data-config-device]').count(), 10);
  assert.equal(await devices.locator('input[type=checkbox]').count(), 160);
  assert(await devices.getByRole('button', { name: 'Добавить прибор', exact: true }).isDisabled());
  await devices.getByRole('button', { name: 'Удалить Прибор 10', exact: true }).click();
  await p.getByRole('dialog', { name: 'Подтверждение', exact: true }).waitFor();
  await p.keyboard.press('Escape');
  assert(await devices.isVisible());
  assert.equal(await devices.locator('tr[data-config-device]').count(), 10);
  await devices.getByRole('button', { name: 'Удалить Прибор 10', exact: true }).click();
  await p.keyboard.press('Tab');
  assert(
    await p
      .getByRole('dialog', { name: 'Подтверждение', exact: true })
      .evaluate((el) => el.contains(document.activeElement)),
  );
  await p
    .getByRole('dialog', { name: 'Подтверждение', exact: true })
    .getByRole('button', { name: 'Удалить', exact: true })
    .click();
  assert.equal(await devices.locator('tr[data-config-device]').count(), 9);
  await devices.getByRole('button', { name: 'Добавить прибор', exact: true }).click();
  assert.equal(await devices.locator('tr[data-config-device]').count(), 10);
  await devices.getByRole('button', { name: 'Настроить Прибор 1', exact: true }).click();
  assert.equal(await devices.getByLabel('IP прибора', { exact: true }).inputValue(), '—');
  assert(await devices.getByLabel('IP прибора', { exact: true }).evaluate((el) => el.readOnly));
  await devices.getByLabel('Название прибора', { exact: true }).fill('Галерея');
  await devices.getByLabel('Название DIM1', { exact: true }).fill('Красный фронт');
  await devices.getByLabel('Цвет DIM1', { exact: true }).selectOption('red');
  await devices.getByLabel('Галерея · REL2', { exact: true }).uncheck();
  await devices.getByRole('button', { name: 'Сохранить', exact: true }).click();
  assert.equal(await p.locator('.parameter-row').count(), 159);
  assert((await paramRow(p, 'pwm-1').innerText()).includes('Красный фронт'));
  await p.getByRole('button', { name: 'Эффекты ленты', exact: true }).click();
  const effects = p.getByRole('dialog', { name: 'Эффекты ленты' });
  await effects.locator('.segmented').getByRole('button', { name: 'Статика', exact: true }).click();
  await effects.getByLabel('Название эффекта').fill('Белый зал');
  assert.equal(await effects.getByLabel('Время прохода', { exact: true }).count(), 0);
  assert.equal(await effects.getByLabel('Ширина', { exact: true }).count(), 0);
  assert(await effects.getByLabel('Цвет эффекта', { exact: true }).isVisible());
  await effects.getByRole('button', { name: 'Сохранить', exact: true }).click();
  await p.getByRole('button', { name: 'Сеть', exact: true }).click();
  const network = p.getByRole('dialog', { name: 'Сеть', exact: true });
  assert(await network.getByText('Нет подключения', { exact: true }).isVisible());
  assert.equal(await network.locator('tbody tr').count(), 10);
  assert.equal(await network.locator('polyline').count(), 0);
  await network.getByRole('switch', { name: 'Пинг-тест', exact: true }).check();
  await pause(100);
  assert.equal(await network.locator('polyline').count(), 0);
  assert((await network.locator('tbody tr').first().innerText()).includes('—'));
  await network.getByRole('button', { name: 'RSSI', exact: true }).click();
  assert(await network.getByRole('img', { name: 'График RSSI', exact: true }).isVisible());
  await network.getByRole('switch', { name: 'Пинг-тест', exact: true }).uncheck();
  await network.getByRole('button', { name: 'Закрыть', exact: true }).click();
  await p.getByRole('button', { name: 'Опубликовать', exact: true }).click();
  await p.waitForFunction(() =>
    document.querySelector('.version-chip')?.textContent.includes('v1'),
  );
  const downloaded = p.waitForEvent('download');
  await p.getByRole('button', { name: 'Скачать JSON', exact: true }).click();
  const file = '/tmp/theatre-score-ui-backup-v071.json';
  await (await downloaded).saveAs(file);
  const saved = JSON.parse(await fs.readFile(file, 'utf8'));
  assert.equal(saved.interfaceVersion, release.version);
  assert.equal(saved.draft.steps.length, 3);
  assert.equal(saved.draft.devices.length, 10);
  assert.equal(saved.published.version, 1);
  const deviceId = saved.draft.devices[0].id;
  await edit(p, 1, 'pwm-1', 33);
  await pause(500);
  const storage = await p.evaluate(() =>
    JSON.parse(localStorage.getItem('theatre-score.workspace.v1')),
  );
  assert.equal(storage.draft.steps[1].devices[deviceId].dimmers['pwm-1'].value, 33);
  assert.equal(storage.published.project.steps[1].devices[deviceId].dimmers['pwm-1'].value, 20);
  await p.reload();
  await p.waitForSelector('.parameter-row');
  assert.equal((await cell(p, 1, 'pwm-1').innerText()).replace(/\s/g, ''), '33%');
  await p.locator('input[type=file]').setInputFiles(file);
  await p
    .getByRole('dialog', { name: 'Подтверждение', exact: true })
    .getByRole('button', { name: 'Заменить', exact: true })
    .click();
  await p.waitForFunction(() =>
    document
      .querySelector('.parameter-row[data-parameter="pwm-1"] td:nth-of-type(2)')
      .textContent.includes('20'),
  );
  await p.locator('input[type=file]').setInputFiles({
    name: 'bad.json',
    mimeType: 'application/json',
    buffer: Buffer.from('{"schemaVersion":99}'),
  });
  await p.waitForFunction(() =>
    document.querySelector('[role=status]')?.textContent.includes('Неподдерживаемый'),
  );
  assert.equal(await p.locator('.step-heading').count(), 3);
  await p.screenshot({ path: '/tmp/theatre-score-desktop-v071.png', fullPage: true });
  await ctx.close();
  const hundred = structuredClone(saved);
  hundred.published = null;
  hundred.draft.revision++;
  hundred.draft.devices[0].ip = '192.168.50.33';
  hundred.draft.devices.forEach((d) =>
    [...d.dimmers, ...d.relays, d.strip, d.audio].forEach((f) => (f.enabled = true)),
  );
  const template = hundred.draft.steps[0];
  hundred.draft.steps = Array.from({ length: 100 }, (_, i) => {
    const s = structuredClone(template);
    s.id = `stress-${i}`;
    s.name = String(i + 1);
    for (const d of hundred.draft.devices) {
      d.dimmers.forEach((v, j) => (s.devices[d.id].dimmers[v.id].value = (i * 13 + j * 9) % 101));
      s.devices[d.id].audio.track = Math.floor(i / 4) + 1;
    }
    return s;
  });
  const stress = await browser.newContext({ viewport: { width: 1440, height: 900 } });
  await stress.addInitScript(
    (data) => localStorage.setItem('theatre-score.workspace.v1', JSON.stringify(data)),
    hundred,
  );
  const big = await stress.newPage();
  watch(big);
  await big.goto(base);
  await big.waitForSelector('.step-heading:nth-of-type(101)');
  assert.equal(await big.locator('.step-heading').count(), 100);
  assert.equal(await big.locator('.parameter-row td').count(), 16000);
  assert((await big.locator('.device-group').first().innerText()).includes('192.168.50.33'));
  await big.screenshot({ path: '/tmp/theatre-score-100-v071.png', fullPage: true });
  await stress.close();
  for (const [width, height] of [
    [390, 844],
    [375, 667],
    [320, 568],
  ]) {
    const context = await browser.newContext({
        viewport: { width, height },
        isMobile: true,
        hasTouch: true,
      }),
      mobile = await context.newPage();
    watch(mobile);
    await mobile.goto(base);
    await mobile.waitForSelector('.parameter-row');
    for (const name of ['Предыдущий шаг', 'Следующий шаг', 'Сдублировать шаг', 'Сеть']) {
      const r = await mobile.getByRole('button', { name, exact: true }).boundingBox();
      assert(r.x >= 0 && r.x + r.width <= width);
    }
    await mobile.getByRole('button', { name: 'Сдублировать шаг', exact: true }).tap();
    await cell(mobile, 1, 'pwm-1').locator('button').tap();
    const sheet = mobile.getByRole('dialog', { name: 'Параметры', exact: true });
    await sheet.getByLabel('Яркость', { exact: true }).fill('45');
    await sheet.getByRole('button', { name: 'Закрыть', exact: true }).click();
    assert.equal(await current(mobile), '1');
    await mobile.getByRole('button', { name: 'Следующий шаг', exact: true }).tap();
    assert.equal(await current(mobile), '2');
    const bounds = await mobile.evaluate(() => ({
      w: document.documentElement.scrollWidth,
      h: document.documentElement.scrollHeight,
    }));
    assert(bounds.w <= width && bounds.h <= height);
    await mobile.screenshot({ path: `/tmp/theatre-score-phone-${width}-v071.png`, fullPage: true });
    await mobile.getByRole('button', { name: 'Приборы', exact: true }).click();
    const modal = mobile.getByRole('dialog', { name: 'Приборы', exact: true });
    assert.equal(await modal.locator('tr[data-config-device]').count(), 10);
    assert.equal(await modal.locator('input[type=checkbox]').count(), 160);
    await modal.getByRole('button', { name: 'Закрыть', exact: true }).click();
    await context.close();
  }
  const confirmContext = await browser.newContext({ viewport: { width: 1440, height: 900 } });
  await confirmContext.addInitScript(() => {
    window.nativeConfirmCalls = 0;
    window.confirm = () => {
      window.nativeConfirmCalls++;
      return false;
    };
  });
  const deletion = await confirmContext.newPage();
  watch(deletion);
  await deletion.goto(base);
  await deletion.waitForSelector('.step-heading');
  for (let i = 0; i < 4; i++)
    await deletion.getByRole('button', { name: 'Сдублировать шаг', exact: true }).click();
  const remove = () => deletion.getByRole('button', { name: 'Удалить шаг', exact: true }).click();
  await remove();
  let prompt = deletion.getByRole('dialog', { name: 'Подтверждение', exact: true });
  await prompt.getByRole('checkbox').check();
  await prompt.getByRole('button', { name: 'Отмена', exact: true }).click();
  assert.equal(await deletion.locator('.step-heading').count(), 5);
  await remove();
  prompt = deletion.getByRole('dialog', { name: 'Подтверждение', exact: true });
  await prompt.getByRole('checkbox').check();
  await prompt.getByRole('button', { name: 'Удалить', exact: true }).click();
  assert.equal(await deletion.locator('.step-heading').count(), 4);
  await remove();
  await pause(50);
  assert.equal(await deletion.locator('.step-heading').count(), 3);
  assert.equal(
    await deletion.getByRole('dialog', { name: 'Подтверждение', exact: true }).count(),
    0,
  );
  await pause(400);
  await deletion.reload();
  await deletion.waitForSelector('.step-heading');
  await remove();
  await deletion.getByRole('dialog', { name: 'Подтверждение', exact: true }).waitFor();
  await deletion.keyboard.press('Escape');
  assert.equal(await deletion.locator('.step-heading').count(), 3);
  assert.equal(await deletion.evaluate(() => window.nativeConfirmCalls), 0);
  await confirmContext.close();
  const helmetContext = await browser.newContext({ viewport: { width: 1440, height: 900 } });
  const helmet = await helmetContext.newPage();
  watch(helmet);
  await helmet.goto(base);
  await helmet.waitForSelector('.parameter-row');
  await helmet.getByRole('button', { name: 'Приборы', exact: true }).click();
  const helmetModal = helmet.getByRole('dialog', { name: 'Приборы', exact: true });
  await helmetModal.getByRole('button', { name: 'Настроить Прибор 1', exact: true }).click();
  await helmetModal.getByLabel('Тип прибора', { exact: true }).selectOption('helmet');
  const hardware = helmetModal.locator('tr[data-config-device]').first();
  assert.equal(await hardware.locator('input:disabled').count(), 14);
  assert.equal(await helmetModal.locator('.parameter-config').count(), 2);
  await helmetModal.getByLabel('Точек WS2811', { exact: true }).fill('127');
  await helmetModal.getByRole('button', { name: 'Сохранить', exact: true }).click();
  assert.equal(await helmet.locator('.parameter-row').count(), 146);
  const voiceCell = helmet
    .locator('.parameter-row[data-parameter="audio"]')
    .first()
    .locator('td')
    .first()
    .locator('button');
  await voiceCell.click();
  assert.equal(await voiceCell.getAttribute('aria-pressed'), 'true');
  assert.equal(await helmet.getByLabel('Номер трека', { exact: true }).count(), 0);
  await helmet
    .getByText('Аудиомодуль пока не подключён', { exact: false })
    .waitFor({ state: 'visible' });
  await voiceCell.click();
  assert.equal(await voiceCell.getAttribute('aria-pressed'), 'false');
  await pause(400);
  await helmet.reload();
  await helmet.waitForSelector('.parameter-row');
  assert.equal(await helmet.locator('.parameter-row').count(), 146);
  await helmetContext.close();
  assert.equal(failures.length, 0, failures.join('\n'));
  assert.equal(externals.length, 0, externals.join('\n'));
  console.log(
    'UI passed: horizontal numbered steps, vertical instruments, header-only controls, no footer/comments, reorder/copy, device feature matrix and CRUD, readonly IP, static FX, truthful offline network controls, versions/JSON, 100x160 cells, toggle light/audio, custom confirmations (native blocked / cancel / skip until reload / Escape / nested focus), 390/375/320px.',
  );
} finally {
  await browser.close();
}
