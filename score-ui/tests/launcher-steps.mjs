import assert from 'node:assert/strict';
export async function enterScore(page, { declineFirst = false } = {}) {
  await page.getByRole('button', { name: 'Меню голоса шлема', exact: true }).waitFor();
  assert.equal(
    await page.locator('.parameter-row').count(),
    0,
    'Score not mounted behind launcher',
  );
  const choose = async () => {
    await page.getByRole('button', { name: 'Меню голоса шлема', exact: true }).click();
    await page.getByRole('menuitem', { name: 'Редактирование партитуры', exact: true }).click();
    await page.getByText('Вы точно хотите редактировать партитуру?', { exact: true }).waitFor();
  };
  if (declineFirst) {
    await choose();
    await page.getByRole('button', { name: 'Нет', exact: true }).click();
    assert.equal(await page.locator('.app-header').count(), 0);
    assert.equal(await page.getByRole('dialog').count(), 0);
  }
  await choose();
  await page.getByRole('button', { name: 'Да', exact: true }).click();
  await page.getByRole('button', { name: 'Разрешить редактирование', exact: true }).waitFor();
  assert(
    await page.getByLabel('Название спектакля', { exact: true }).isDisabled(),
    'Yes opens score still locked',
  );
}
export async function launcherChecks(
  page,
  { getWorkspace, getConsole, calls, mac, unlockElsewhere },
) {
  await page.setViewportSize({ width: 1280, height: 720 });
  await page.getByRole('button', { name: 'Вернуться к голосу шлема', exact: true }).click();
  await page.getByLabel('Голос шлема', { exact: true }).waitFor();
  assert.equal(await page.locator('.app-header,.parameter-row').count(), 0);
  const baseDraft = JSON.stringify(getWorkspace().draft),
    basePublished = JSON.stringify(getWorkspace().published),
    runtime = () =>
      JSON.stringify(
        Object.fromEntries(
          ['step', 'stepId', 'running', 'livePreview', 'navigation', 'transition'].map((k) => [
            k,
            getConsole()[k],
          ]),
        ),
      ),
    baseRuntime = runtime(),
    start = calls.length;
  assert.equal(getConsole().editorLocked, true);
  const selector = page.getByLabel('Голос шлема', { exact: true });
  assert.deepEqual(
    await selector.locator('option').allTextContents(),
    getWorkspace().helmetVoices[mac].presets.map((p) => p.name),
  );
  await page.getByRole('button', { name: 'Меню голоса шлема', exact: true }).focus();
  await page.keyboard.press('Digit8');
  await page.keyboard.press('Control+z');
  await page.keyboard.press('ArrowRight');
  assert.equal(JSON.stringify(getWorkspace().draft), baseDraft);
  await page.getByRole('button', { name: 'Меню голоса шлема', exact: true }).click();
  await page.getByRole('menuitem', { name: 'Редактирование партитуры', exact: true }).click();
  await page.getByRole('button', { name: 'Нет', exact: true }).click();
  assert.equal(await page.locator('.app-header').count(), 0);
  await page.getByRole('button', { name: 'Меню голоса шлема', exact: true }).click();
  await page.getByRole('menuitem', { name: 'Редактирование звука', exact: true }).click();
  const dialog = page.getByRole('dialog', { name: /^Звук шлема/ });
  await dialog.waitFor();
  const bankBefore = getWorkspace().helmetVoices[mac],
    selected = bankBefore.selected,
    other = bankBefore.presets.find((p) => p.id !== selected);
  assert(other);
  const otherSettings = JSON.stringify(other.settings);
  await dialog.getByLabel('Название голосового пресета', { exact: true }).fill('Launcher EQ');
  await dialog.getByText('Параметрический эквалайзер и разборчивость', { exact: true }).click();
  await dialog.getByLabel('Эквалайзер голоса', { exact: true }).check();
  await dialog.getByLabel('Усиление EQ 2', { exact: true }).fill('-3.2');
  await dialog.getByLabel('Полоса разборчивости', { exact: true }).check();
  await dialog.getByText('Параметры звука', { exact: true }).click();
  await dialog.getByLabel('Алгоритм голоса', { exact: true }).selectOption('chip');
  await dialog.getByRole('button', { name: 'Сохранить пресет', exact: true }).click();
  await page.waitForTimeout(900);
  let saved = getWorkspace().helmetVoices[mac].presets.find((p) => p.id === selected);
  assert.equal(saved.settings.eq.enabled, true);
  assert.equal(saved.settings.eq.bands[1].gainDb, -3.2);
  assert.equal(saved.settings.presence.enabled, true);
  assert.equal(saved.settings.algorithm, 'chip');
  assert(!getWorkspace().helmetVoices[mac].working);
  await dialog.getByLabel('Звуковой пресет шлема', { exact: true }).selectOption(other.id);
  await page.waitForTimeout(700);
  assert.equal(
    JSON.stringify(
      getWorkspace().helmetVoices[mac].presets.find((p) => p.id === other.id).settings,
    ),
    otherSettings,
  );
  await dialog.getByLabel('Звуковой пресет шлема', { exact: true }).selectOption(selected);
  await page.waitForTimeout(700);
  assert.equal(await dialog.getByLabel('Усиление EQ 2', { exact: true }).inputValue(), '-3.2');
  assert.equal(await dialog.getByLabel('Алгоритм голоса', { exact: true }).inputValue(), 'chip');
  const count = getWorkspace().helmetVoices[mac].presets.length;
  if (count < 8) {
    await dialog.getByRole('button', { name: 'Сохранить как новый', exact: true }).click();
    await page.waitForTimeout(700);
    assert.equal(getWorkspace().helmetVoices[mac].presets.length, count + 1);
    await dialog.getByRole('button', { name: 'Удалить пресет', exact: true }).click();
    await page
      .getByRole('dialog', { name: 'Подтверждение', exact: true })
      .getByRole('button', { name: 'Удалить', exact: true })
      .click();
    await page.waitForTimeout(700);
    assert.equal(getWorkspace().helmetVoices[mac].presets.length, count);
  }
  await dialog
    .locator('.modal-footer')
    .getByRole('button', { name: 'Закрыть', exact: true })
    .click();
  await selector.waitFor();
  await selector.selectOption(other.id);
  await page.waitForTimeout(700);
  assert.equal(getWorkspace().helmetVoices[mac].selected, other.id);
  for (const [width, height] of [
    [1280, 360],
    [390, 620],
    [375, 568],
    [320, 568],
    [844, 320],
  ]) {
    await page.setViewportSize({ width, height });
    const box = await page.locator('.voice-launcher-card').boundingBox();
    assert(
      box && box.x >= 0 && box.y >= 0 && box.x + box.width <= width && box.y + box.height <= height,
    );
    await page.getByRole('button', { name: 'Меню голоса шлема', exact: true }).click();
    const menu = await page
      .getByRole('menu', { name: 'Редактирование', exact: true })
      .boundingBox();
    assert(
      menu &&
        menu.x >= 0 &&
        menu.x + menu.width <= width &&
        menu.y >= 0 &&
        menu.y + menu.height <= height,
    );
    if (width === 320) await page.screenshot({ path: '/tmp/theatre-0810-launcher-menu-320.png' });
    await page.keyboard.press('Escape');
    if (width === 320) await page.screenshot({ path: '/tmp/theatre-0810-launcher-320.png' });
  }
  await page.setViewportSize({ width: 1280, height: 720 });
  // Another browser leaving the shared master unlocked cannot bypass this view's own consent.
  unlockElsewhere();
  await page.waitForTimeout(1100);
  await enterScore(page);
  assert.equal(getConsole().editorLocked, true);
  unlockElsewhere();
  await page.waitForTimeout(1100);
  assert(await page.getByLabel('Название спектакля', { exact: true }).isDisabled());
  assert.equal(JSON.stringify(getWorkspace().draft), baseDraft);
  assert.equal(JSON.stringify(getWorkspace().published), basePublished);
  assert.equal(runtime(), baseRuntime);
  assert(
    !calls
      .slice(start)
      .some((c) =>
        /\/(publish|draft|draft-go|go|navigate|preview-mode|preview|stop|stop-all|command|reset)$/.test(
          c.path,
        ),
      ),
    'Launcher/voice/view entry cannot send scene or score mutation',
  );
  await page.getByRole('button', { name: 'Разрешить редактирование', exact: true }).click();
  await page.waitForFunction(
    () => !document.querySelector('[aria-label="Название спектакля"]')?.disabled,
  );
  assert(!(await page.getByLabel('Название спектакля', { exact: true }).isDisabled()));
  assert.equal(runtime(), baseRuntime);
  await page.getByRole('button', { name: 'Вернуться к голосу шлема', exact: true }).click();
  assert.equal(getConsole().editorLocked, true);
  await page.reload();
  await page.getByLabel('Голос шлема', { exact: true }).waitFor();
  assert.equal(await page.locator('.parameter-row').count(), 0);
  assert.equal(runtime(), baseRuntime);
  console.log(
    'Voice launcher: only names/select and bottom-rightmenu; full preset/EQ/presence/algorithm/save/new/delete; No and Yes+separate lock; no GO/score writes/stop or runtime changes; per-view consent, reload, keyboard and320/375/390/landscape passed',
  );
}
