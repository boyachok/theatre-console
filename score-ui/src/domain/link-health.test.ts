import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { LINK_HEALTH, parseLinkHealth, displayedLinkHealth, LINK_LABELS } from './link-health.ts';
test('Link statuses use the firmware LED health order and do not infer a grade from RSSI', () => {
  assert.deepEqual(LINK_HEALTH, ['warmup', 'excellent', 'good', 'marginal', 'bad', 'offline']);
  LINK_HEALTH.forEach((name, i) => {
    assert.equal(parseLinkHealth(i), name);
    assert.equal(parseLinkHealth(name.toUpperCase()), name);
  });
  for (const bad of [-1, 6, 1.5, NaN, null, {}, 'green', ''])
    assert.equal(parseLinkHealth(bad), undefined);
  assert.equal(displayedLinkHealth({ online: true }), 'unknown');
});
test('Stale or disconnected reports never retain an old positive LED grade', () => {
  assert.equal(displayedLinkHealth({ online: false, health: 'excellent' }), 'offline');
  assert.equal(displayedLinkHealth({ online: null, health: 'excellent' }), 'unknown');
  assert.equal(displayedLinkHealth({ online: true, health: 'excellent' }, false), 'unknown');
  assert.equal(displayedLinkHealth(), 'unknown');
  assert.equal(displayedLinkHealth({ online: true, health: 'marginal' }), 'marginal');
});
test('Shared firmware palette remains blue/green/amber/red with offline and warmup blinking', () => {
  const firmware = readFileSync(new URL('../../../console/network_led.h', import.meta.url), 'utf8');
  for (const token of [
    'EXCELLENT:return 0x000014',
    'GOOD:return 0x001400',
    'MARGINAL:return 0x120a00',
    'BAD:return 0x140000',
    'now/300',
    'now/700',
  ])
    assert(firmware.includes(token));
  assert.equal(LINK_LABELS.offline, 'Нет связи');
});
