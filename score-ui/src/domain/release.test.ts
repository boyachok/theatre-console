import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createProject, clone, fingerprint, duplicateStep, updateCell } from './project.ts';
import { parseBackup } from './validation.ts';
import { compileDevices } from './compile.ts';
import { INTERFACE_VERSION, PRODUCT_NAME } from '../version.ts';
test('one product release is shared by web and both firmware roles', () => {
  const release = JSON.parse(
    readFileSync(new URL('../../../version.json', import.meta.url), 'utf8'),
  );
  assert.equal(INTERFACE_VERSION, release.version);
  assert.equal(PRODUCT_NAME, 'Theatre console');
  const header = readFileSync(new URL('../../../console/release.h', import.meta.url), 'utf8');
  assert(header.includes(`THEATRE_VERSION "${release.version}"`));
});
test('8-channel backup migrates only the draft; immutable publication keeps its fingerprint', () => {
  const old = createProject();
  for (const d of old.devices) {
    d.dimmers = d.dimmers.slice(0, 8);
    for (const s of old.steps)
      for (let n = 9; n <= 12; n++) delete s.devices[d.id].dimmers[`pwm-${n}`];
  }
  old.steps[0].devices[old.devices[0].id].dimmers['pwm-1'].value = 61;
  const published = {
    project: clone(old),
    version: 3,
    fingerprint: fingerprint(old),
    createdAt: new Date().toISOString(),
    mode: 'local',
  };
  const result = parseBackup(
    JSON.stringify({
      format: 'theatre-score',
      schemaVersion: 1,
      interfaceVersion: '0.3.0',
      draft: old,
      published,
    }),
  );
  assert.equal(result.draft.devices[0].dimmers.length, 12);
  assert(result.draft.devices[0].dimmers.slice(8).every((p) => !p.enabled));
  assert.equal(result.draft.steps[0].devices[old.devices[0].id].dimmers['pwm-1'].value, 61);
  assert.equal(result.published!.project.devices[0].dimmers.length, 8);
  assert.equal(result.published!.fingerprint, fingerprint(result.published!.project));
});
test('audio remembers selected file when a cell is toggled off', () => {
  let p = createProject();
  const d = p.devices[0],
    cell = {
      stepId: p.steps[0].id,
      deviceId: d.id,
      kind: 'audio' as const,
      parameterId: d.audio.id,
    };
  p = updateCell(p, cell, { track: 7 });
  p = updateCell(p, cell, { track: null });
  assert.equal(p.steps[0].devices[d.id].audio.track, null);
  assert.equal(p.steps[0].devices[d.id].audio.lastTrack, 7);
});
test('WS2811 inherited program is absolute even when jumping directly to a hold step', () => {
  let p = createProject();
  const d = p.devices[0];
  d.mac = 'C0:4E:30:27:69:98';
  p.steps[0].devices[d.id].strip = { action: 'play', effectId: p.effects[2].id };
  p = duplicateStep(p, p.steps[0].id).project;
  p.steps[1].devices[d.id].strip.action = 'hold';
  const [wire] = compileDevices(p, [p.steps[1]]);
  assert.equal(wire.cues[0].d.length, 12);
  assert.equal(wire.cues[0].s[1], 2);
  assert.equal(wire.cues[0].s[2], 2);
  assert.deepEqual(wire.cues[0].s.slice(4, 7), [255, 255, 255]);
  assert.equal(wire.cues[0].s[14], compileDevices(p, [p.steps[0]])[0].cues[0].s[14]);
  p.effects[2].brightness = 50.5;
  p.effects[2].width = 0.2;
  const quantized = compileDevices(p, [p.steps[1]])[0].cues[0].s;
  assert.equal(quantized[10], 51);
  assert.equal(quantized[11], 1);
});
