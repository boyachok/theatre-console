import assert from 'node:assert/strict';
export async function editorChecks(page, navigationCalls, workspace) {
  await page.setViewportSize({ width: 1440, height: 1000 });
  const sheet = page.getByRole('dialog', { name: 'Параметры', exact: true });
  if (await sheet.count())
    await sheet.getByRole('button', { name: 'Закрыть', exact: true }).click();
  assert.equal(await page.locator('.quick-toolbar').count(), 0);
  assert.equal(await page.getByRole('button', { name: 'Горячие клавиши', exact: true }).count(), 0);
  const id = workspace().draft.devices[0].id,
    nav = navigationCalls(),
    row = (key, device = id) =>
      page.locator(`.parameter-row[data-device="${device}"][data-parameter="${key}"]`),
    cell = (key, i) => row(key).locator('td button').nth(i);
  await cell('pwm-1', 0).click();
  await cell('pwm-2', 1).click({ modifiers: ['Shift'] });
  assert.equal(await page.locator('td.selected-cell').count(), 4);
  const beforeKeys = JSON.stringify(workspace().draft);
  for (const key of ['5', '0', '=', 'i', 'o', 'd', 'v', '?', 'Control+z', 'Control+Shift+z'])
    await page.keyboard.press(key);
  await page.waitForTimeout(600);
  assert.equal(
    JSON.stringify(workspace().draft),
    beforeKeys,
    'Removed editing shortcuts must not mutate the score',
  );
  assert.equal(await page.getByRole('dialog').count(), 0);
  await page.getByLabel('Яркость', { exact: true }).fill('50');
  await page.waitForTimeout(800);
  for (const s of workspace().draft.steps.slice(0, 2))
    for (const key of ['pwm-1', 'pwm-2']) assert.equal(s.devices[id].dimmers[key].value, 50);
  const fade = page.getByLabel('Вход', { exact: true });
  await fade.fill('');
  assert.equal(await fade.inputValue(), '');
  await fade.fill('.25');
  await fade.blur();
  await page.waitForTimeout(700);
  assert.equal(workspace().draft.steps[0].devices[id].dimmers['pwm-1'].fadeIn, 0.25);
  assert.equal(navigationCalls(), nav);
  await cell('pwm-1', 0).click();
  await page.keyboard.press('ArrowDown');
  await page.keyboard.press('ArrowRight');
  await page.keyboard.press('Shift+ArrowUp');
  await page.keyboard.press('Shift+ArrowLeft');
  assert.equal(await page.locator('td.selected-cell').count(), 4);
  assert.equal(navigationCalls(), nav, 'Selection arrows never navigate the show');
  await page.keyboard.press('Escape');
  await cell('pwm-1', 0).dblclick();
  await page.waitForTimeout(600);
  assert.equal(
    workspace().draft.steps[0].devices[id].dimmers['pwm-1'].value,
    0,
    'Double click toggles; single click only selects',
  );
  const drag = async (source, target, after = true) => {
    const box = await target.boundingBox();
    assert(box);
    await source.dragTo(target, { targetPosition: { x: 90, y: after ? box.height - 2 : 2 } });
    await page.waitForTimeout(700);
  };
  const ids = workspace().draft.devices.map((d) => d.id),
    bindings = workspace()
      .draft.devices.map((d) => [d.id, d.mac])
      .sort(),
    cueBefore = JSON.stringify(workspace().draft.steps);
  for (const device of ids.slice(0, 2))
    await page.locator(`[data-device-group="${device}"] .collapse-device`).click();
  await drag(
    page.locator(`[data-device-group="${ids[0]}"] .device-name`),
    page.locator(`[data-device-group="${ids[1]}"]`),
  );
  assert.equal(workspace().draft.devices[1].id, ids[0]);
  assert.deepEqual(
    workspace()
      .draft.devices.map((d) => [d.id, d.mac])
      .sort(),
    bindings,
  );
  await drag(
    page.locator(`[data-device-group="${ids[0]}"] .device-name`),
    page.locator(`[data-device-group="${ids[1]}"]`),
    false,
  );
  assert.deepEqual(
    workspace().draft.devices.map((d) => d.id),
    ids,
  );
  assert.equal(JSON.stringify(workspace().draft.steps), cueBefore);
  for (const device of ids.slice(0, 2))
    await page.locator(`[data-device-group="${device}"] .collapse-device`).click();
  const channelBefore = await page
    .locator(`.parameter-row[data-device="${id}"]`)
    .evaluateAll((rs) => rs.map((r) => r.dataset.parameter));
  await drag(row('pwm-1').locator('.row-select'), row('pwm-3'));
  assert.deepEqual(
    (
      await page
        .locator(`.parameter-row[data-device="${id}"]`)
        .evaluateAll((rs) => rs.map((r) => r.dataset.parameter))
    ).slice(0, 3),
    ['pwm-2', 'pwm-3', 'pwm-1'],
  );
  await drag(row('pwm-1').locator('.row-select'), row('pwm-2'), false);
  assert.deepEqual(
    await page
      .locator(`.parameter-row[data-device="${id}"]`)
      .evaluateAll((rs) => rs.map((r) => r.dataset.parameter)),
    channelBefore,
  );
  await page.getByRole('button', { name: 'Вид сортировки', exact: true }).click();
  assert.equal(await page.locator('.device-group').count(), 0);
  await row('pwm-1').locator('.row-select').click();
  await row('pwm-2')
    .locator('.row-select')
    .click({ modifiers: ['Shift'] });
  await row('pwm-4')
    .locator('.row-select')
    .click({ modifiers: [process.platform === 'darwin' ? 'Meta' : 'Control'] });
  assert.equal(await page.locator('tr.selected-row').count(), 3);
  const allKeys = await page
      .locator('.parameter-row')
      .evaluateAll((rs) => rs.map((r) => JSON.stringify([r.dataset.device, r.dataset.parameter]))),
    moving = allKeys.filter((k) =>
      ['pwm-1', 'pwm-2', 'pwm-4'].some((p) => k === JSON.stringify([id, p])),
    ),
    cues = JSON.stringify(workspace().draft.steps);
  await drag(row('pwm-2').locator('.row-select'), row('pwm-5'));
  const expected = allKeys.filter((k) => !moving.includes(k));
  expected.splice(expected.indexOf(JSON.stringify([id, 'pwm-5'])) + 1, 0, ...moving);
  assert.deepEqual(workspace().draft.layout.order, expected);
  assert.equal(JSON.stringify(workspace().draft.steps), cues);
  assert.equal(navigationCalls(), nav);
  assert.equal(await page.locator('tr.selected-row').count(), 3);
  await page.getByRole('button', { name: 'По приборам', exact: true }).click();
  assert((await page.locator('.device-group').count()) > 0);
  const beforeHeader = navigationCalls();
  await page
    .getByRole('button', { name: 'Редактировать шаг 1', exact: true })
    .click({ modifiers: ['Control'] });
  await page
    .getByRole('button', { name: 'Редактировать шаг 2', exact: true })
    .click({ modifiers: ['Shift'] });
  assert((await page.locator('td.selected-cell').count()) > 4);
  assert.equal(navigationCalls(), beforeHeader);
  await page.keyboard.press('Escape');
  for (const width of [320, 375, 390]) {
    await page.setViewportSize({ width, height: 844 });
    assert((await page.evaluate(() => document.documentElement.scrollWidth)) <= width);
    const button = page.getByRole('button', { name: 'Вид сортировки', exact: true });
    await button.scrollIntoViewIfNeeded();
    assert(await button.isVisible());
    assert.equal(await page.locator('.quick-toolbar,.shortcut-list').count(), 0);
  }
  await page.setViewportSize({ width: 1280, height: 900 });
  const preview = page.getByRole('switch', { name: 'Предпросмотр', exact: true });
  await preview.click();
  await page.waitForFunction(() => document.querySelector('[aria-label="Предпросмотр"]').checked);
  await page.waitForTimeout(700);
  const liveNav = navigationCalls(),
    playing = await page.locator('#playing-step').inputValue();
  await cell('pwm-1', 1).focus();
  await page.keyboard.press('ArrowLeft');
  await page.keyboard.press('ArrowDown');
  await page.keyboard.press('5');
  await page.waitForTimeout(500);
  assert.equal(navigationCalls(), liveNav);
  assert.equal(await page.locator('#playing-step').inputValue(), playing);
  const liveCues = JSON.stringify(workspace().draft.steps);
  await page.getByRole('button', { name: 'Вид сортировки', exact: true }).click();
  await row('pwm-1').locator('.row-select').click();
  await row('pwm-2')
    .locator('.row-select')
    .click({ modifiers: ['Shift'] });
  await drag(row('pwm-2').locator('.row-select'), row('pwm-4'));
  assert.equal(JSON.stringify(workspace().draft.steps), liveCues);
  assert.equal(navigationCalls(), liveNav);
  assert.equal(await page.locator('#playing-step').inputValue(), playing);
  await page.getByRole('button', { name: 'По приборам', exact: true }).click();
  await preview.click();
  await page.waitForFunction(() => !document.querySelector('[aria-label="Предпросмотр"]').checked);
  await page.waitForTimeout(800);
  console.log(
    'Operator editor: no quick panel/editing shortcuts, pointer/Shift/Cmd selection, inspector batch fields, double-click toggle, native device/channel/group drag, preserved IDs/MAC/cues, sort view+JSON, LIVE/OFF no GO,320/375/390 passed',
  );
}
