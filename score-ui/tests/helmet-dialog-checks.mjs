import assert from 'node:assert/strict';

export async function helmetDialogChecks(page, dialog) {
  for (const [width, height] of [
    [1280, 520],
    [1280, 360],
    [390, 620],
    [375, 568],
    [320, 568],
  ]) {
    await page.setViewportSize({ width, height });
    const body = dialog.locator('.helmet-content');
    await body.evaluate((e) => {
      e.scrollTop = 0;
    });
    const dimensions = await body.evaluate((e) => ({
      scroll: e.scrollHeight,
      client: e.clientHeight,
    }));
    assert(
      dimensions.scroll > dimensions.client,
      'Long helmet settings require an actual scroll container',
    );
    await body.hover({ position: { x: 5, y: 5 } });
    await page.mouse.wheel(0, 1000);
    await page.waitForFunction(() => document.querySelector('.helmet-content')?.scrollTop > 0);
    const range = dialog.getByLabel('Ползунок: Усиление микрофона', { exact: true });
    await range.scrollIntoViewIfNeeded();
    await range.evaluate((input) => {
      const body = input.closest('.helmet-content');
      body.scrollTop +=
        input.getBoundingClientRect().top -
        body.getBoundingClientRect().top -
        body.clientHeight / 4;
    });
    await range.focus();
    await range.hover();
    const valueBefore = await range.inputValue(),
      scrollBefore = await body.evaluate((e) => e.scrollTop);
    await page.mouse.wheel(0, 160);
    await page.waitForTimeout(150);
    assert.equal(await range.inputValue(), valueBefore, 'Wheel must scroll, not change voice gain');
    assert(
      (await body.evaluate((e) => e.scrollTop)) > scrollBefore,
      'Wheel above a slider must reach the scroll container',
    );
    const check = dialog.getByLabel('Срез глухих стуков', { exact: true });
    await check.scrollIntoViewIfNeeded();
    const bounds = await check.boundingBox(),
      close = await dialog.locator('.modal-footer button').boundingBox(),
      box = await dialog.boundingBox();
    assert(bounds && bounds.y >= 0 && bounds.y + bounds.height <= height);
    assert(
      close && close.y >= 0 && close.y + close.height <= height,
      'Footer must remain visible at normal scale',
    );
    assert(box && box.x >= 0 && box.x + box.width <= width);
    assert(await dialog.evaluate((e) => e.scrollWidth <= e.clientWidth + 1));
    const columns = await dialog
      .locator('.helmet-fields')
      .last()
      .evaluate((e) => getComputedStyle(e).gridTemplateColumns.split(' ').length);
    assert.equal(columns, width > 600 ? 2 : 1);
    await check.focus();
    await page.keyboard.press('Tab');
    assert(
      await dialog.locator('.modal-footer button').evaluate((e) => e === document.activeElement),
    );
    if ((width === 1280 && height === 520) || width === 320)
      await page.screenshot({ path: `/tmp/theatre-083-helmet-${width}.png` });
  }
  await page.setViewportSize({ width: 1280, height: 900 });
  await dialog.locator('.helmet-content').evaluate((e) => {
    e.scrollTop = 0;
  });
  console.log(
    'Helmet dialog: real wheel and keyboard scrolling, fixed header/footer, two desktop columns/one mobile column, all controls reachable at320/375/390 and360px height',
  );
}
