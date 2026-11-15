import assert from 'node:assert/strict';
export async function editorChecks(page, navigationCalls, workspace) {
  await page.setViewportSize({ width: 1440, height: 1000 });
  const sheet = page.getByRole('dialog', { name: 'Параметры', exact: true });
  if (await sheet.count())
    await sheet.getByRole('button', { name: 'Закрыть', exact: true }).first().click();
  const row = page.locator('.parameter-row[data-parameter="pwm-1"]').first();
  await row.locator('td button').first().click();
  const nav = navigationCalls();
  await page.keyboard.press('ArrowDown');
  await page.keyboard.press('ArrowRight');
  await page.keyboard.press('Shift+ArrowUp');
  await page.keyboard.press('Shift+ArrowLeft');
  assert.equal(await page.locator('td.selected-cell').count(), 4);
  await page.waitForTimeout(500);
  assert.equal(navigationCalls(), nav, 'Arrow/Shift selection must not GO');
  await page.keyboard.press('5');
  await page.waitForTimeout(800);
  const state = workspace(),
    id = state.draft.devices[0].id;
  for (const step of state.draft.steps.slice(0, 2))
    for (const key of ['pwm-1', 'pwm-2']) assert.equal(step.devices[id].dimmers[key].value, 50);
  await page.keyboard.press('i');
  const quick = page.getByRole('dialog', { name: 'Fade in / включение', exact: true });
  await quick.getByLabel('Быстрое значение', { exact: true }).fill('1.5');
  await quick.getByRole('button', { name: 'Применить', exact: true }).click();
  await page.waitForTimeout(800);
  for (const step of workspace().draft.steps.slice(0, 2))
    for (const key of ['pwm-1', 'pwm-2']) assert.equal(step.devices[id].dimmers[key].fadeIn, 1.5);
  const fade = page.getByLabel('Вход', { exact: true });
  await fade.fill('');
  assert.equal(await fade.inputValue(), '', 'Empty focused field must remain empty');
  await fade.fill('0.25');
  await fade.blur();
  await page.waitForTimeout(800);
  assert.equal(workspace().draft.steps[0].devices[id].dimmers['pwm-1'].fadeIn, 0.25);
  await page.keyboard.press('Control+z');
  await page.waitForTimeout(800);
  assert.equal(workspace().draft.steps[0].devices[id].dimmers['pwm-1'].fadeIn, 1.5);
  await page.keyboard.press('Control+Shift+z');
  await page.waitForTimeout(800);
  assert.equal(workspace().draft.steps[0].devices[id].dimmers['pwm-1'].fadeIn, 0.25);
  assert.equal(navigationCalls(), nav, 'Batch edits and undo OFF must not GO');
  await page.getByRole('button', { name: 'Горячие клавиши', exact: true }).click();
  await page
    .getByRole('dialog', { name: 'Горячие клавиши', exact: true })
    .getByText('Перемещение курсора без изменения значений и без GO', { exact: true })
    .waitFor();
  await page
    .getByRole('dialog', { name: 'Горячие клавиши', exact: true })
    .getByRole('button', { name: 'Закрыть', exact: true })
    .first()
    .click();
  await page.getByRole('button', { name: 'По приборам', exact: true }).click();
  assert.equal(await page.locator('.device-group').count(), 0);
  const first = await page.locator('.parameter-row').first().getAttribute('data-parameter');
  await page.locator('.parameter-row').first().hover();
  await page.locator('.parameter-row').first().locator('.row-order button').last().click();
  assert.notEqual(
    await page.locator('.parameter-row').first().getAttribute('data-parameter'),
    first,
  );
  await page.waitForTimeout(800);
  assert.equal(workspace().draft.layout.mode, 'custom');
  assert(workspace().draft.layout.order.length > 10);
  await page.getByRole('button', { name: 'Своя партитура', exact: true }).click();
  assert((await page.locator('.device-group').count()) > 0);
  await page.waitForTimeout(500);
  // Device drag keeps IDs/cues/bindings; undo restores the presentation order only.
  const deviceIds = workspace().draft.devices.map((d) => d.id);
  const bindings = workspace().draft.devices.map((d) => [d.id, d.mac]);
  const transfer = await page.evaluateHandle(() => new DataTransfer());
  await page
    .getByRole('button', { name: 'Переместить прибор Прибор 1', exact: true })
    .dispatchEvent('dragstart', { dataTransfer: transfer });
  await page
    .locator(`tr[data-device-group="${deviceIds[1]}"]`)
    .dispatchEvent('drop', { dataTransfer: transfer, clientY: 100000 });
  await page.waitForTimeout(800);
  assert.equal(workspace().draft.devices[1].id, deviceIds[0]);
  assert.deepEqual(
    workspace()
      .draft.devices.map((d) => [d.id, d.mac])
      .sort(),
    [...bindings].sort(),
  );
  await page.keyboard.press('Control+z');
  await page.waitForTimeout(800);
  assert.deepEqual(
    workspace().draft.devices.map((d) => d.id),
    deviceIds,
  );
  // Real mouse drag by channel name, not just synthetic DataTransfer events.
  const rowFor = (parameter, device = deviceIds[0]) =>
    page.locator(`.parameter-row[data-device="${device}"][data-parameter="${parameter}"]`);
  const dragAfter = async (source, target) => {
    const box = await target.boundingBox();
    assert(box);
    await source.dragTo(target, { targetPosition: { x: 90, y: box.height - 2 } });
  };
  // Whole device blocks can be grabbed by their title, including when collapsed.
  for (const id of deviceIds.slice(0, 2))
    await page.locator(`[data-device-group="${id}"] .collapse-device`).click();
  await dragAfter(
    page.locator(`[data-device-group="${deviceIds[0]}"] .device-name`),
    page.locator(`[data-device-group="${deviceIds[1]}"]`),
  );
  await page.waitForTimeout(800);
  assert.equal(
    workspace().draft.devices[1].id,
    deviceIds[0],
    'Native device-title drag must move the whole block',
  );
  assert.deepEqual(
    workspace()
      .draft.devices.map((d) => [d.id, d.mac])
      .sort(),
    [...bindings].sort(),
  );
  await page.keyboard.press('Control+z');
  await page.waitForTimeout(800);
  for (const id of deviceIds.slice(0, 2))
    await page.locator(`[data-device-group="${id}"] .collapse-device`).click();
  const channelBefore = await page
    .locator(`.parameter-row[data-device="${deviceIds[0]}"]`)
    .evaluateAll((rs) => rs.map((r) => r.dataset.parameter));
  await dragAfter(rowFor('pwm-1').locator('.row-select'), rowFor('pwm-3'));
  await page.waitForTimeout(800);
  const channelAfter = await page
    .locator(`.parameter-row[data-device="${deviceIds[0]}"]`)
    .evaluateAll((rs) => rs.map((r) => r.dataset.parameter));
  assert.notDeepEqual(channelAfter, channelBefore, 'Native channel-name drag must reorder rows');
  assert.deepEqual(channelAfter.slice(0, 3), ['pwm-2', 'pwm-3', 'pwm-1']);
  assert.equal(navigationCalls(), nav, 'Row-name selection/drag must not GO');
  await page.keyboard.press('Control+z');
  await page.waitForTimeout(800);
  assert.deepEqual(
    await page
      .locator(`.parameter-row[data-device="${deviceIds[0]}"]`)
      .evaluateAll((rs) => rs.map((r) => r.dataset.parameter)),
    channelBefore,
  );
  await page.getByRole('button', { name: 'По приборам', exact: true }).click();
  await rowFor('pwm-1').locator('.row-select').click();
  await rowFor('pwm-2')
    .locator('.row-select')
    .click({ modifiers: ['Shift'] });
  await rowFor('pwm-4')
    .locator('.row-select')
    .click({ modifiers: [process.platform === 'darwin' ? 'Meta' : 'Control'] });
  assert.equal(
    await page.locator('tr.selected-row').count(),
    3,
    JSON.stringify(
      await page.locator('tr.selected-row').evaluateAll((rs) => rs.map((r) => r.dataset.parameter)),
    ),
  );
  const beforeGroup = workspace().draft,
    cuesBeforeGroup = JSON.stringify(beforeGroup.steps);
  const allKeys = await page
    .locator('.parameter-row')
    .evaluateAll((rs) => rs.map((r) => JSON.stringify([r.dataset.device, r.dataset.parameter])));
  const wanted = ['pwm-1', 'pwm-2', 'pwm-4'].map((p) => JSON.stringify([deviceIds[0], p]));
  const moving = allKeys.filter((k) => wanted.includes(k));
  await dragAfter(rowFor('pwm-2').locator('.row-select'), rowFor('pwm-5'));
  await page.waitForTimeout(800);
  const expected = allKeys.filter((k) => !moving.includes(k)),
    target = JSON.stringify([deviceIds[0], 'pwm-5']);
  expected.splice(expected.indexOf(target) + 1, 0, ...moving);
  assert.deepEqual(
    workspace().draft.layout.order,
    expected,
    'Native drag moves selected channels as one stable group',
  );
  assert.equal(JSON.stringify(workspace().draft.steps), cuesBeforeGroup);
  assert.equal(navigationCalls(), nav, 'Group drag must not GO');
  assert.equal(await page.locator('tr.selected-row').count(), 3);
  assert.deepEqual(
    workspace().draft.devices,
    beforeGroup.devices,
    'Group layout must not reassign MACs or channel IDs',
  );
  // JSON contains the order and undo restores it with one operation.
  await page.keyboard.press('Control+z');
  await page.waitForTimeout(800);
  assert.deepEqual(
    await page
      .locator('.parameter-row')
      .evaluateAll((rs) => rs.map((r) => JSON.stringify([r.dataset.device, r.dataset.parameter]))),
    allKeys,
  );
  await page.getByRole('button', { name: 'Своя партитура', exact: true }).click();
  // Headers select entire step ranges without navigation when Shift is held.
  const beforeHeader = navigationCalls();
  await page
    .getByRole('button', { name: 'Редактировать шаг 1', exact: true })
    .click({ modifiers: ['Control'] });
  await page
    .getByRole('button', { name: 'Редактировать шаг 2', exact: true })
    .click({ modifiers: ['Shift'] });
  assert((await page.locator('td.selected-cell').count()) > 4);
  await page.waitForTimeout(400);
  assert.equal(navigationCalls(), beforeHeader);
  await page.keyboard.press('Escape');
  await page.screenshot({ path: '/tmp/theatre-082-editor-desktop.png', fullPage: true });
  for (const width of [320, 375, 390]) {
    await page.setViewportSize({ width, height: 844 });
    assert((await page.evaluate(() => document.documentElement.scrollWidth)) <= width);
    await page
      .getByRole('button', { name: 'Горячие клавиши', exact: true })
      .scrollIntoViewIfNeeded();
    await page.getByRole('button', { name: 'Горячие клавиши', exact: true }).click();
    const help = page.getByRole('dialog', { name: 'Горячие клавиши', exact: true });
    const bounds = await help.boundingBox();
    assert(bounds.x >= 0 && bounds.x + bounds.width <= width);
    await help.getByRole('button', { name: 'Закрыть', exact: true }).first().click();
  }
  await page.setViewportSize({ width: 1280, height: 900 });
  // LIVE: moving editor focus and editing another step must not move master playback.
  const preview = page.getByRole('switch', { name: 'Предпросмотр', exact: true });
  await preview.click();
  await page.waitForFunction(
    () => document.querySelector('[aria-label="Предпросмотр"]')?.checked === true,
  );
  await page.waitForTimeout(700);
  const liveNav = navigationCalls(),
    playing = await page.locator('#playing-step').inputValue();
  await page
    .locator('.parameter-row[data-parameter="pwm-1"]')
    .first()
    .locator('td button')
    .nth(1)
    .focus();
  await page.keyboard.press('ArrowLeft');
  await page.keyboard.press('ArrowDown');
  await page.keyboard.press('5');
  await page.waitForTimeout(800);
  assert.equal(navigationCalls(), liveNav);
  assert.equal(await page.locator('#playing-step').inputValue(), playing);
  const liveCues = JSON.stringify(workspace().draft.steps);
  await page.getByRole('button', { name: 'По приборам', exact: true }).click();
  await rowFor('pwm-1').locator('.row-select').click();
  await rowFor('pwm-2')
    .locator('.row-select')
    .click({ modifiers: ['Shift'] });
  await dragAfter(rowFor('pwm-2').locator('.row-select'), rowFor('pwm-4'));
  await page.waitForTimeout(800);
  assert.equal(
    JSON.stringify(workspace().draft.steps),
    liveCues,
    'LIVE layout drag never changes cue values',
  );
  assert.equal(navigationCalls(), liveNav, 'LIVE header selection/group drag must not GO');
  assert.equal(await page.locator('#playing-step').inputValue(), playing);
  await page.keyboard.press('Control+z');
  await page.waitForTimeout(800);
  await page.getByRole('button', { name: 'Своя партитура', exact: true }).click();
  await preview.click();
  await page.waitForFunction(
    () => document.querySelector('[aria-label="Предпросмотр"]')?.checked === false,
  );
  await page.waitForTimeout(800);
  console.log(
    '0.8.2 editor MOCK ESP: native device-title/channel-name/group drag, stable visual order and MAC/cue preservation, discrete undo; header Shift/Cmd selection and LIVE drag without GO; keyboard/batch/JSON/320/375/390 passed',
  );
}
