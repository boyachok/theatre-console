import test from 'node:test';
import assert from 'node:assert/strict';
import { createProject, clone, fingerprint, duplicateStep } from './project.ts';
import { PreviewEngine } from './playback.ts';
import { compileDevices } from './compile.ts';
import { parseBackup, validateProject } from './validation.ts';
function fixture() {
  const p = createProject();
  p.devices = [p.devices[0]];
  const d = p.devices[0];
  d.mac = '74:4D:BD:73:0A:A4';
  const s = p.steps[0],
    a = s.devices[d.id].audio;
  a.track = 7;
  a.startDelay = 2;
  a.fadeIn = 1;
  const e = new PreviewEngine(p),
    audio = () => e.getSnapshot().devices[d.id].audio;
  return { p, d, s, a, e, audio };
}
test('audio delay defaults to zero, compiles milliseconds, preserves old published fingerprint and copy isolation', () => {
  const { p, d, s, a } = fixture();
  assert.deepEqual(compileDevices(p, [s])[0].cues[0].a, [7, 30, 1, 1000, 0, 2000]);
  a.startDelay = 1.25;
  assert.equal(compileDevices(p, [s])[0].cues[0].a[5], 1250);
  const copy = duplicateStep(p, s.id).project;
  copy.steps[1].devices[d.id].audio.startDelay = 9;
  assert.equal(a.startDelay, 1.25);
  delete a.startDelay;
  assert.equal(compileDevices(p, [s])[0].cues[0].a.length, 5);
  const fp = fingerprint(p),
    published = {
      version: 46,
      fingerprint: fp,
      project: clone(p),
      createdAt: p.updatedAt,
      mode: 'esp',
    };
  const loaded = parseBackup(
    JSON.stringify({
      format: 'theatre-score',
      schemaVersion: 1,
      interfaceVersion: '0.7.10',
      draft: p,
      published,
    }),
  );
  assert.equal(fingerprint(loaded.published!.project), fp);
  assert.equal(loaded.draft.steps[0].devices[d.id].audio.startDelay, undefined);
  for (const delay of [-1, 3600.001, NaN, Infinity, '2', null]) {
    Object.assign(a, { startDelay: delay });
    assert.throws(() => validateProject(p));
  }
});
test('preview waits, duplicate/unrelated LIVE edits do not restart timer, and fade starts at due time', () => {
  const { p, d, s, a, e, audio } = fixture();
  e.apply(p, s, 'forward', 1, 0);
  assert.equal(audio().status, 'waiting');
  assert.equal(audio().track, null);
  assert.equal(audio().volume, 0);
  assert.equal(audio().generation, 0);
  e.tick(1000);
  s.devices[d.id].dimmers['pwm-1'].value = 50;
  e.apply(p, s, 'forward', 2, 1000);
  assert.equal(audio().startAt, 2000);
  assert.equal(e.apply(p, s, 'forward', 2, 1500), false);
  e.tick(1999);
  assert.equal(audio().status, 'waiting');
  e.tick(2000);
  assert.equal(audio().status, 'playing');
  assert.equal(audio().track, 7);
  assert.equal(audio().volume, 0);
  e.tick(2500);
  assert.equal(audio().volume, 50);
  e.tick(3000);
  assert.equal(audio().volume, 100);
  a.startDelay = 5;
  e.apply(p, s, 'forward', 3, 3500);
  assert.equal(audio().generation, 1);
  assert.equal(audio().status, 'playing');
  assert.equal(audio().startAt, null);
});
test('LIVE delay edits use original request time and shortening an expired delay starts now', () => {
  const { p, s, a, e, audio } = fixture();
  e.apply(p, s, 'forward', 1, 0);
  a.startDelay = 4;
  e.apply(p, s, 'forward', 2, 1000);
  assert.equal(audio().startAt, 4000);
  e.tick(3999);
  assert.equal(audio().status, 'waiting');
  e.tick(4000);
  assert.equal(audio().generation, 1);
  e.apply(p, s, 'back', 3, 5000);
  assert.equal(audio().startAt, 9000);
  a.startDelay = 0.5;
  e.apply(p, s, 'forward', 4, 6000);
  assert.equal(audio().status, 'playing');
  assert.equal(audio().envelope.at, 6000);
  assert.equal(audio().volume, 0);
});
test('blank scenes, disabled audio and reset cancel waiting starts, even with a nonzero fade out', () => {
  for (const how of ['blank', 'disabled', 'reset']) {
    const { p, d, s, a, e, audio } = fixture();
    e.apply(p, s, 'forward', 1, 0);
    a.fadeOut = 3;
    if (how === 'blank') a.track = null;
    else if (how === 'disabled') d.audio.enabled = false;
    if (how === 'reset') e.reset(p);
    else e.apply(p, s, 'forward', 2, 1000);
    e.tick(10000);
    assert.equal(audio().track, null);
    assert.equal(audio().status, 'stopped');
    assert.equal(audio().generation, 0);
  }
});
test('same pending file in a new scene gets that scene deadline; same playing file never stops to delay again', () => {
  const { p, d, s, e, audio } = fixture();
  e.apply(p, s, 'forward', 1, 0);
  const next = clone(s);
  next.id = 'next';
  p.steps.push(next);
  e.apply(p, next, 'forward', 2, 1500);
  assert.equal(audio().startAt, 3500);
  e.tick(3000);
  assert.equal(audio().generation, 0);
  e.tick(3500);
  assert.equal(audio().generation, 1);
  next.devices[d.id].audio.startDelay = 10;
  e.apply(p, next, 'forward', 3, 4000);
  assert.equal(audio().generation, 1);
  assert.equal(audio().startAt, null);
  e.apply(p, next, 'recover', 4, 5000);
  assert.equal(audio().status, 'waiting');
  assert.equal(audio().startAt, 15000);
});
