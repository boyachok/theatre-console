import test from 'node:test';
import assert from 'node:assert/strict';
import {
  clone,
  createProject,
  duplicateStep,
  moveStep,
  percentToDuty,
  updateCell,
} from './project.ts';
import { parseBackup, validateProject } from './validation.ts';
import { PreviewEngine } from './playback.ts';
import { LocalPreviewTransport } from '../transport/preview.ts';
import type { Project, Step, Workspace } from './schema.ts';
const backup = (p: Project): Workspace => ({
  format: 'theatre-score',
  schemaVersion: 1,
  interfaceVersion: '0.1.0',
  draft: p,
  published: null,
});
function scene(p: Project, source: Step = p.steps[0]) {
  const s = clone(source);
  s.id = crypto.randomUUID();
  return s;
}
test('one initial step, ten universal devices, correct percent conversion', () => {
  const p = createProject();
  assert.equal(p.steps.length, 1);
  assert.equal(p.devices.length, 10);
  assert(p.devices.every((d) => d.dimmers.length === 12 && d.relays.length === 2));
  assert.equal(p.steps[0].devices[p.devices[0].id].audio.track, null);
  assert.deepEqual([0, 50, 100].map(percentToDuty), [0, 128, 255]);
  validateProject(p);
});
test('duplicate inserts after selected and carries all values, fades and delays independently', () => {
  const p = createProject(),
    id = p.devices[0].id,
    s = p.steps[0];
  s.timings.lightIn = 4;
  s.timings.audioOut = 7;
  s.devices[id].dimmers['pwm-1'] = { value: 68, fadeIn: 2, fadeOut: 6 };
  s.devices[id].audio.track = 3;
  s.devices[id].relays['relay-2'] = { on: true, delayOn: 1, delayOff: 3 };
  const result = duplicateStep(p, s.id),
    copy = result.project.steps[1];
  assert.notEqual(copy.id, s.id);
  assert.deepEqual(copy.devices, s.devices);
  assert.deepEqual(copy.timings, s.timings);
  copy.devices[id].dimmers['pwm-1'].value = 17;
  assert.equal(s.devices[id].dimmers['pwm-1'].value, 68);
  const next = duplicateStep(result.project, s.id);
  assert.equal(next.project.steps[1].id, next.id);
  assert.equal(next.project.steps[2].id, copy.id);
});
test('whole-row move preserves stable IDs, values and unedited row identity', () => {
  let p = createProject();
  p = duplicateStep(p, p.steps[0].id).project;
  p = duplicateStep(p, p.steps[1].id).project;
  const row = p.steps[2];
  row.timings.relayOff = 5;
  const moved = moveStep(p, row.id, 0);
  assert.equal(moved.steps[0], row);
  assert.equal(moved.steps[0].timings.relayOff, 5);
  assert.equal(moved.steps[1].id, p.steps[0].id);
  const edited = updateCell(
    moved,
    { stepId: row.id, deviceId: p.devices[0].id, kind: 'dimmer', parameterId: 'pwm-1' },
    { value: 42 },
  );
  assert.equal(edited.steps[1], moved.steps[1]);
  assert.equal(row.devices[p.devices[0].id].dimmers['pwm-1'].value, 0);
});
test('JSON roundtrip and invalid values/identifiers are rejected', () => {
  const p = createProject();
  assert.deepEqual(parseBackup(JSON.stringify(backup(p))).draft, p);
  const invalid = clone(p);
  invalid.steps[0].devices[p.devices[0].id].dimmers['pwm-1'].value = 256;
  assert.throws(() => validateProject(invalid));
  const duplicate = clone(p);
  duplicate.devices[1].id = duplicate.devices[0].id;
  assert.throws(() => validateProject(duplicate));
  assert.throws(() => parseBackup('{"schemaVersion":99}'));
  assert.throws(() => parseBackup('not json'));
});
test('linear fade is interrupted from the attained level; retries do not restart it', () => {
  const p = createProject(),
    id = p.devices[0].id,
    a = p.steps[0],
    b = scene(p);
  a.timings.lightIn = 10;
  a.devices[id].dimmers['pwm-1'].value = 100;
  b.timings.lightOut = 2;
  const e = new PreviewEngine(p);
  e.apply(p, a, 'first', 1, 0);
  e.tick(5000);
  assert.equal(e.getSnapshot().devices[id].dimmers['pwm-1'].value, 50);
  e.apply(p, b, 'forward', 2, 5000);
  e.tick(6000);
  assert.equal(e.getSnapshot().devices[id].dimmers['pwm-1'].value, 25);
  assert.equal(e.apply(p, b, 'forward', 2, 6000), false);
  assert.equal(e.apply(p, a, 'first', 1, 6000), false);
  e.tick(7000);
  assert.equal(e.getSnapshot().devices[id].dimmers['pwm-1'].value, 0);
});
test('audio continues across equal tracks, stays paused at EOF even backwards and fades out on blank', () => {
  const p = createProject(),
    id = p.devices[0].id,
    a = p.steps[0];
  a.devices[id].audio.track = 3;
  const b = scene(p, a),
    blank = scene(p, a);
  blank.devices[id].audio.track = null;
  blank.timings.audioOut = 2;
  const e = new PreviewEngine(p);
  e.apply(p, a, 'first', 1, 0);
  assert.equal(e.getSnapshot().devices[id].audio.generation, 1);
  e.apply(p, b, 'forward', 2, 10);
  assert.equal(e.getSnapshot().devices[id].audio.generation, 1);
  e.trackEnded(id);
  e.apply(p, a, 'forward', 3, 20);
  assert.equal(e.getSnapshot().devices[id].audio.status, 'paused');
  assert.equal(e.getSnapshot().devices[id].audio.generation, 1);
  e.apply(p, b, 'back', 4, 30);
  assert.equal(e.getSnapshot().devices[id].audio.status, 'paused');
  assert.equal(e.getSnapshot().devices[id].audio.generation, 1);
  e.apply(p, blank, 'forward', 5, 1000);
  e.tick(2000);
  assert.equal(e.getSnapshot().devices[id].audio.volume, 50);
  e.tick(3000);
  assert.equal(e.getSnapshot().devices[id].audio.status, 'stopped');
  assert.equal(e.getSnapshot().devices[id].audio.track, null);
});
test('changing cue during relay delay cancels old pending switching', () => {
  const p = createProject(),
    id = p.devices[0].id,
    a = p.steps[0],
    b = scene(p);
  a.timings.relayOn = 3;
  a.devices[id].relays['relay-1'].on = true;
  const e = new PreviewEngine(p);
  e.apply(p, a, 'first', 1, 0);
  e.tick(1000);
  assert.equal(e.getSnapshot().devices[id].relays['relay-1'].on, false);
  e.apply(p, b, 'forward', 2, 1000);
  e.tick(4000);
  assert.equal(e.getSnapshot().devices[id].relays['relay-1'].on, false);
  e.apply(p, a, 'forward', 3, 5000);
  e.tick(8000);
  assert.equal(e.getSnapshot().devices[id].relays['relay-1'].on, true);
  b.timings.relayOff = 2;
  e.apply(p, b, 'back', 4, 8000);
  e.tick(9000);
  assert.equal(e.getSnapshot().devices[id].relays['relay-1'].on, true);
  e.tick(10000);
  assert.equal(e.getSnapshot().devices[id].relays['relay-1'].on, false);
});
test('editing draft never alters currently previewed targets or published snapshot', async () => {
  const p = createProject(),
    id = p.devices[0].id;
  p.steps[0].devices[id].dimmers['pwm-1'].value = 20;
  const e = new PreviewEngine(p);
  e.apply(p, p.steps[0], 'first', 1, 0);
  const transport = new LocalPreviewTransport(),
    version = await transport.publish(p, null);
  const edited = updateCell(
    p,
    { stepId: p.steps[0].id, deviceId: id, kind: 'dimmer', parameterId: 'pwm-1' },
    { value: 90 },
  );
  e.tick(1000);
  assert.equal(e.getSnapshot().devices[id].dimmers['pwm-1'].value, 20);
  assert.equal(version.project.steps[0].devices[id].dimmers['pwm-1'].value, 20);
  assert.equal(edited.steps[0].devices[id].dimmers['pwm-1'].value, 90);
  assert.equal((await transport.publish(edited, version)).version, 2);
});
test('local fades finish without further transitions; recovery keeps an already playing file and its volume', () => {
  const p = createProject(),
    id = p.devices[0].id,
    step = p.steps[0];
  step.timings.lightIn = 3;
  step.timings.audioIn = 2;
  step.devices[id].dimmers['pwm-1'].value = 75;
  step.devices[id].audio.track = 7;
  const e = new PreviewEngine(p);
  e.apply(p, step, 'first', 1, 0);
  e.tick(4000);
  assert.equal(e.getSnapshot().devices[id].dimmers['pwm-1'].value, 75);
  e.apply(p, step, 'recover', 2, 5000);
  assert.equal(e.getSnapshot().devices[id].audio.volume, 100);
  e.tick(6000);
  assert.equal(e.getSnapshot().devices[id].audio.volume, 100);
  assert.equal(e.getSnapshot().devices[id].audio.generation, 1);
});
test('strip effect is copied for preview; same forward effect continues, backwards restarts', () => {
  const p = createProject(),
    id = p.devices[0].id,
    a = p.steps[0];
  a.devices[id].strip = { action: 'play', effectId: p.effects[0].id };
  const b = scene(p, a);
  const e = new PreviewEngine(p);
  e.apply(p, a, 'first', 1, 0);
  e.apply(p, b, 'forward', 2, 1000);
  assert.equal(e.getSnapshot().devices[id].strip?.startedAt, 0);
  p.effects[0].color = '#ff0000';
  assert.notEqual(e.getSnapshot().devices[id].strip?.effect.color, '#ff0000');
  e.apply(p, a, 'back', 3, 2000);
  assert.equal(e.getSnapshot().devices[id].strip?.startedAt, 2000);
});
