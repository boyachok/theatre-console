import test from 'node:test';
import assert from 'node:assert/strict';
import {
  createProject,
  clone,
  fingerprint,
  acParameter,
  acCue,
  columnsFor,
  duplicateStep,
  updateCell,
  deviceProfile,
} from './project.ts';
import { defaultDimmerAnimation } from './dimmer.ts';
import { compileDevices } from './compile.ts';
import { validateProject, parseBackup } from './validation.ts';
import { PreviewEngine } from './playback.ts';
function fixture() {
  const p = createProject();
  p.devices = [p.devices[0]];
  const d = p.devices[0],
    s = p.steps[0],
    cue = s.devices[d.id];
  d.mac = '74:4D:BD:73:0A:A4';
  const e = new PreviewEngine(p),
    runtime = () => e.getSnapshot().devices[d.id];
  return { p, d, s, cue, e, runtime };
}
test('Диммер220 is separate, opt-in, absent on helmets and never rewrites old publications', () => {
  const { p, d, s, cue } = fixture();
  assert.equal(acParameter(d).enabled, false);
  assert.equal(columnsFor(p, 'all').filter((c) => c.kind === 'ac').length, 0);
  assert.equal(compileDevices(p, [s])[0].cues[0].ac, undefined);
  d.acDimmer!.enabled = true;
  cue.ac!.value = 35;
  cue.ac!.startDelay = 0.75;
  assert.equal(columnsFor(p, 'all').filter((c) => c.kind === 'ac').length, 1);
  assert.deepEqual(compileDevices(p, [s])[0].cues[0].ac, { enabled: 1, d: [89, 0, 0, 750] });
  const copy = duplicateStep(p, s.id).project;
  copy.steps[1].devices[d.id].ac!.value = 90;
  assert.equal(cue.ac!.value, 35);
  const helmet = deviceProfile(d, 'helmet');
  assert.equal(acParameter(helmet).enabled, false);
  delete d.acDimmer;
  delete cue.ac;
  const old = clone(p),
    fp = fingerprint(old);
  const loaded = parseBackup(
    JSON.stringify({
      format: 'theatre-score',
      schemaVersion: 1,
      interfaceVersion: '0.7.11',
      draft: old,
      published: {
        version: 109,
        project: old,
        fingerprint: fp,
        createdAt: old.updatedAt,
        mode: 'esp',
      },
    }),
  );
  assert.equal(fingerprint(loaded.published!.project), fp);
  assert.equal(loaded.draft.devices[0].acDimmer, undefined);
  assert.equal(loaded.draft.steps[0].devices[d.id].ac, undefined);
  assert.equal(acParameter(loaded.draft.devices[0]).enabled, false);
  assert.equal(acCue(loaded.draft.steps[0].devices[d.id]).value, 0);
  const next = updateCell(
    loaded.draft,
    { stepId: s.id, deviceId: d.id, parameterId: 'ac-220', kind: 'ac' },
    { value: 50 },
  );
  assert.equal(next.steps[0].devices[d.id].ac!.value, 50);
  assert.equal(loaded.draft.steps[0].devices[d.id].ac, undefined);
});
test('new audio loop and per-channel delays compile with unambiguous old/default wire shapes', () => {
  const { p, d, s, cue } = fixture();
  const light = cue.dimmers[d.dimmers[0].id];
  light.value = 100;
  light.startDelay = 1.25;
  light.fadeIn = 2;
  let wire = compileDevices(p, [s])[0].cues[0];
  assert.deepEqual(wire.d[0], [255, 2000, 0, 1250]);
  assert.equal(wire.d[1].length, 3);
  light.animation = { ...defaultDimmerAnimation(), enabled: true, mode: 'sine' };
  wire = compileDevices(p, [s])[0].cues[0];
  assert.equal(wire.d[0].length, 10);
  assert.equal(wire.d[0][9], 1250);
  cue.audio.track = 7;
  cue.audio.loop = true;
  assert.deepEqual(compileDevices(p, [s])[0].cues[0].a, [7, 30, 1, 0, 0, 0, 1]);
  cue.audio.startDelay = 2.5;
  assert.deepEqual(compileDevices(p, [s])[0].cues[0].a, [7, 30, 1, 0, 0, 2500, 1]);
  cue.audio.loop = false;
  delete cue.audio.startDelay;
  delete light.startDelay;
  assert.equal(compileDevices(p, [s])[0].cues[0].a.length, 5);
  assert.equal(compileDevices(p, [s])[0].cues[0].d[0].length, 9);
  d.mac = null;
  d.acDimmer!.enabled = true;
  cue.ac!.value = 50;
  assert.throws(() => compileDevices(p, [s]), /Назначьте/);
});
test('all new fields reject malformed or out-of-range values, including disabled AC channels', () => {
  const { p, d, cue } = fixture();
  const light = cue.dimmers[d.dimmers[0].id];
  for (const bad of [-1, 3600.001, NaN, Infinity, null, '1']) {
    Object.assign(light, { startDelay: bad });
    assert.throws(() => validateProject(p));
    Object.assign(cue.ac!, { startDelay: bad });
    delete light.startDelay;
    assert.throws(() => validateProject(p));
    delete cue.ac!.startDelay;
  }
  for (const bad of [1, 0, null, 'true', {}]) {
    Object.assign(cue.audio, { loop: bad });
    assert.throws(() => validateProject(p));
  }
  cue.audio.loop = false;
  cue.ac!.animation = { ...defaultDimmerAnimation(), enabled: true, mode: 'square', period: 0.02 };
  assert.throws(() => validateProject(p));
  cue.ac!.animation.period = 0.04;
  validateProject(p);
  d.profile = 'helmet';
  d.dimmers.forEach((p) => (p.enabled = false));
  d.relays.forEach((p) => (p.enabled = false));
  d.acDimmer!.enabled = true;
  assert.throws(() => validateProject(p));
});
test('light preview independently waits then starts fade/phase; retries and unrelated LIVE edits do not extend deadlines', () => {
  const { p, d, s, cue, e, runtime } = fixture();
  const a = cue.dimmers[d.dimmers[0].id],
    b = cue.dimmers[d.dimmers[1].id];
  a.value = b.value = 100;
  a.startDelay = 1;
  a.fadeIn = 1;
  b.startDelay = 2;
  e.apply(p, s, 'forward', 1, 0);
  assert.equal(runtime().dimmers['pwm-1'].value, 0);
  assert.equal(runtime().dimmers['pwm-1'].startAt, 1000);
  b.value = 50;
  e.apply(p, s, 'forward', 2, 500);
  assert.equal(runtime().dimmers['pwm-1'].startAt, 1000);
  assert.equal(runtime().dimmers['pwm-2'].startAt, 2000);
  e.tick(1500);
  assert.equal(runtime().dimmers['pwm-1'].value, 50);
  assert.equal(runtime().dimmers['pwm-2'].value, 0);
  e.tick(2000);
  assert.equal(runtime().dimmers['pwm-1'].value, 100);
  assert.equal(runtime().dimmers['pwm-2'].value, 50);
  const next = clone(s);
  next.id = 'next';
  p.steps.push(next);
  e.apply(p, next, 'forward', 3, 2200);
  assert.equal(runtime().dimmers['pwm-1'].value, 0);
  assert.equal(runtime().dimmers['pwm-1'].startAt, 3200);
  e.apply(p, next, 'forward', 3, 2500);
  assert.equal(runtime().dimmers['pwm-1'].startAt, 3200);
});
test('pending light retargets stay scene-anchored; shortening, disabling, zero and reset cancel safely', () => {
  const { p, d, s, cue, e, runtime } = fixture();
  const light = cue.dimmers['pwm-1'];
  light.value = 100;
  light.startDelay = 2;
  e.apply(p, s, 'forward', 1, 0);
  light.startDelay = 4;
  e.apply(p, s, 'forward', 2, 1000);
  assert.equal(runtime().dimmers['pwm-1'].startAt, 4000);
  light.startDelay = 0.1;
  e.apply(p, s, 'forward', 3, 1500);
  assert.equal(runtime().dimmers['pwm-1'].value, 100);
  assert.equal(runtime().dimmers['pwm-1'].startAt, null);
  light.startDelay = 3;
  e.apply(p, s, 'forward', 4, 1600);
  assert.equal(runtime().dimmers['pwm-1'].value, 100); // Already on: delay edit is not a blackout.
  e.apply(p, s, 'back', 5, 2000);
  assert.equal(runtime().dimmers['pwm-1'].startAt, 5000);
  d.dimmers[0].enabled = false;
  e.apply(p, s, 'forward', 6, 2500);
  e.tick(6000);
  assert.equal(runtime().dimmers['pwm-1'].value, 0);
  d.dimmers[0].enabled = true;
  e.apply(p, s, 'forward', 7, 7000);
  light.value = 0;
  e.apply(p, s, 'forward', 8, 7200);
  e.tick(12000);
  assert.equal(runtime().dimmers['pwm-1'].value, 0);
  light.value = 100;
  e.apply(p, s, 'back', 9, 13000);
  e.reset(p);
  e.tick(20000);
  assert.equal(runtime().dimmers['pwm-1'].value, 0);
});
test('AC preview uses its own delay/fade/animation and remains an ideal model, not zero-cross feedback', () => {
  const { p, d, s, cue, e, runtime } = fixture();
  d.acDimmer!.enabled = true;
  cue.ac!.value = 100;
  cue.ac!.startDelay = 1;
  cue.ac!.animation = {
    ...defaultDimmerAnimation(),
    enabled: true,
    mode: 'sine',
    period: 1,
    phase: 90,
  };
  e.apply(p, s, 'forward', 1, 0);
  e.tick(999);
  assert.equal(runtime().ac.value, 0);
  e.tick(1000);
  assert.ok(Math.abs(runtime().ac.value - 50) < 0.001);
  e.tick(1250);
  assert.equal(runtime().ac.value, 100);
  assert.equal(runtime().dimmers['pwm-1'].value, 0);
});
test('loop checkbox preserves playing file/fade and repeats EOF without generating a fabricated PLAY', () => {
  const { p, d, s, cue, e, runtime } = fixture();
  cue.audio.track = 7;
  cue.audio.loop = true;
  cue.audio.startDelay = 1;
  cue.audio.fadeIn = 1;
  e.apply(p, s, 'forward', 1, 0);
  assert.equal(runtime().audio.status, 'waiting');
  e.tick(2000);
  assert.equal(runtime().audio.volume, 100);
  const generation = runtime().audio.generation;
  e.trackEnded(d.id);
  assert.equal(runtime().audio.status, 'playing');
  assert.equal(runtime().audio.generation, generation);
  cue.audio.loop = false;
  e.apply(p, s, 'forward', 2, 2500);
  assert.equal(runtime().audio.generation, generation);
  e.trackEnded(d.id);
  assert.equal(runtime().audio.status, 'paused');
  cue.audio.loop = true;
  e.apply(p, s, 'forward', 3, 3000);
  assert.equal(runtime().audio.status, 'paused');
  assert.equal(runtime().audio.generation, generation);
  e.apply(p, s, 'back', 4, 4000);
  assert.equal(runtime().audio.startAt, 5000);
  cue.audio.track = null;
  e.apply(p, s, 'forward', 5, 4500);
  e.tick(10000);
  assert.equal(runtime().audio.track, null);
});
