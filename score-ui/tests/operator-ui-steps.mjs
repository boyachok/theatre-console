import assert from 'node:assert/strict';
export async function operatorDeviceChecks(page, { workspace, peer, calls }) {
  const before = JSON.stringify(workspace().draft),
    start = calls.length;
  await page.getByRole('button', { name: 'Приборы', exact: true }).click();
  const dialog = page.getByRole('dialog', { name: 'Приборы', exact: true }),
    card = dialog.locator('[data-config-device]').first();
  assert.equal(
    await dialog.locator('[data-config-device]').count(),
    workspace().draft.devices.length,
  );
  assert.equal(await card.locator('[data-parameter-cell]').count(), 13);
  assert.equal(await card.locator('.device-parameter-grid > div').count(), 16);
  for (const label of [
    'Название DIM9',
    'Инвертировать DIM12',
    'Точек WS2811',
    'Порядок цветов ленты',
    'Частота PWM DIM1–12',
    'Импульс открытия220, мкс',
  ])
    assert.equal(await dialog.getByLabel(label, { exact: true }).count(), 0);
  assert.equal(await dialog.locator('svg.network-chart,.field-help').count(), 0);
  assert(!(await dialog.innerText()).includes('GPIO41'));
  const disabled = card.getByRole('checkbox', {
    name: `${workspace().draft.devices[0].name} · DIM1`,
    exact: true,
  });
  await disabled.uncheck();
  assert(await card.getByLabel('Название DIM1', { exact: true }).isDisabled());
  assert(await card.getByLabel('Цвет DIM1', { exact: true }).isDisabled());
  assert(await card.getByLabel('Инвертировать DIM1', { exact: true }).isDisabled());
  await disabled.check();
  assert(!(await card.getByLabel('Название DIM1', { exact: true }).isDisabled()));
  for (const health of ['excellent', 'good', 'marginal', 'bad', 'offline', 'warmup']) {
    peer.health = health;
    await page.waitForFunction(
      (h) =>
        document
          .querySelector('.device-card [data-link-health]')
          ?.getAttribute('data-link-health') === h,
      health,
    );
  }
  peer.health = 'excellent';
  peer.ageMs = 4000;
  await page.waitForFunction(
    () =>
      document
        .querySelector('.device-card [data-link-health]')
        ?.getAttribute('data-link-health') === 'offline',
  );
  peer.ageMs = 10;
  peer.health = 'good';
  await page.waitForTimeout(450);
  await card
    .getByRole('button', { name: `Мигать ${workspace().draft.devices[0].name}`, exact: true })
    .click();
  assert(calls.slice(start).some((c) => c.path.endsWith('/identify')));
  await page.setViewportSize({ width: 1280, height: 720 });
  await card.scrollIntoViewIfNeeded();
  const dimensions = await dialog.evaluate((el) => {
    const body = el.querySelector('.device-cards-scroll'),
      card = el.querySelector('.device-card');
    return {
      body: body.clientHeight,
      card: card.getBoundingClientRect().height,
      columns: getComputedStyle(
        card.querySelector('.device-parameter-grid'),
      ).gridTemplateColumns.split(' ').length,
    };
  });
  assert.equal(dimensions.columns, 4);
  assert(
    dimensions.card <= dimensions.body,
    'One controller must fit on desktop without inner scrolling',
  );
  await page.screenshot({ path: '/tmp/theatre-0811-device-desktop.png' });
  for (const width of [390, 375, 320]) {
    await page.setViewportSize({ width, height: 700 });
    await card.scrollIntoViewIfNeeded();
    const b = await card.boundingBox();
    assert(b && b.x >= 0 && b.x + b.width <= width);
    assert(await dialog.evaluate((el) => el.scrollWidth <= el.clientWidth));
    assert.equal(
      await card
        .locator('.device-parameter-grid')
        .evaluate((el) => getComputedStyle(el).gridTemplateColumns.split(' ').length),
      2,
    );
    if (width === 320) await page.screenshot({ path: '/tmp/theatre-0811-device-320.png' });
  }
  await page.setViewportSize({ width: 1440, height: 900 });
  await card.getByLabel('Тип прибора', { exact: true }).selectOption('helmet');
  assert.equal(await card.locator('[data-parameter-cell]').count(), 2);
  assert.equal(await card.getByLabel('Название DIM1', { exact: true }).count(), 0);
  await dialog.getByRole('button', { name: 'Отмена', exact: true }).click();
  assert.equal(
    JSON.stringify(workspace().draft),
    before,
    'Cancelling compact settings preserves all data',
  );
  assert(
    !calls
      .slice(start)
      .some(
        (c) =>
          c.path === '/command' ||
          /\/(publish|draft|go|navigate|draft-go|preview|preview-mode|reset|measure)$/.test(c.path),
      ),
    'Opening/cancelling compact settings must not mutate score or hardware',
  );
  assert.equal(await page.locator('.quick-toolbar').count(), 0);
  assert.equal(await page.getByRole('button', { name: 'Горячие клавиши', exact: true }).count(), 0);
  assert.equal(await page.locator('.version-chip').count(), 0);
  console.log(
    'Operator settings:4x4 cards, disabled cells inert, DIM9–12/WS hardware/service controls hidden, real health palette+stale override, explicit identify only, helmet profile, cancel/no GO, desktop fit and320/375/390 passed',
  );
}
