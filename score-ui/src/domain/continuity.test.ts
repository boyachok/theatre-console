import test from 'node:test';
import assert from 'node:assert/strict';
import {
  createProject,
  clone,
  fingerprint,
  deviceProfile,
  captureDevicePolarity,
} from './project.ts';
import { compileDevices } from './compile.ts';
import { validateProject, parseBackup } from './validation.ts';
import { PreviewEngine } from './playback.ts';
import { defaultDimmerAnimation } from './dimmer.ts';
function fixture() {
  const p = createProject();
  p.devices = [p.devices[0]];
  const d = p.devices[0];
  d.mac = '74:4D:BD:73:0A:A4';
  const a = p.steps[0],
    b = clone(a);
  b.id = 'b';
  p.steps.push(b);
  const e = new PreviewEngine(p),
    r = () => e.getSnapshot().devices[d.id];
  return { p, d, a, b, e, r };
}
test('same audio file continues forward/back/FIRST/recovery without replaying or restarting fade; blank/different file permits a fresh start', () => {
  const { p, d, a, b, e, r } = fixture();
  for (const s of [a, b]) {
    s.devices[d.id].audio.track = 7;
    s.devices[d.id].audio.fadeIn = 2;
  }
  e.apply(p, a, 'first', 1, 0);
  e.tick(500);
  assert.equal(r().audio.volume, 25);
  e.apply(p, b, 'forward', 2, 500);
  e.apply(p, a, 'back', 3, 750);
  assert.equal(r().audio.generation, 1);
  assert.equal(r().audio.envelope.at, 0);
  e.tick(1000);
  assert.equal(r().audio.volume, 50);
  e.apply(p, b, 'first', 4, 1200);
  e.apply(p, a, 'recover', 5, 1400);
  assert.equal(r().audio.generation, 1);
  e.tick(2000);
  assert.equal(r().audio.volume, 100);
  e.trackEnded(d.id);
  e.apply(p, b, 'back', 6, 2100);
  assert.equal(r().audio.status, 'paused');
  assert.equal(r().audio.generation, 1);
  a.devices[d.id].audio.track = null;
  e.apply(p, a, 'back', 7, 2200);
  e.apply(p, b, 'forward', 8, 2300);
  assert.equal(r().audio.generation, 2);
  assert.equal(r().audio.volume, 0);
  a.devices[d.id].audio.track = 8;
  e.apply(p, a, 'back', 9, 2400);
  assert.equal(r().audio.generation, 3);
  e.apply(p, b, 'forward', 10, 2500);
  assert.equal(r().audio.generation, 4);
});
test('explicit settings Save can capture known legacy polarity into JSON without silently modifying old data', () => {
  const { p, d, a } = fixture();
  d.dimmers.forEach((x) => delete x.inverted);
  assert.equal(captureDevicePolarity(d), d);
  assert.equal(captureDevicePolarity(d, 4096), d);
  const fp = fingerprint(p),
    captured = captureDevicePolarity(d, 0x801);
  assert.equal(fingerprint(p), fp);
  assert(captured.dimmers[0].inverted && captured.dimmers[11].inverted);
  assert.equal(captured.dimmers[1].inverted, false);
  const next = { ...p, devices: [captured] };
  assert.equal(compileDevices(next, [a])[0].cues[0].invertedMask, 0x801);
  const loaded = parseBackup(
    JSON.stringify({
      format: 'theatre-score',
      schemaVersion: 1,
      interfaceVersion: '0.7.13',
      draft: next,
      published: null,
    }),
  );
  assert.equal(loaded.draft.devices[0].dimmers[11].inverted, true);
});
test('same waiting audio keeps original deadline in both directions, and edits still reschedule from that request', () => {
  const { p, d, a, b, e, r } = fixture();
  for (const s of [a, b]) {
    s.devices[d.id].audio.track = 7;
    s.devices[d.id].audio.startDelay = 2;
  }
  e.apply(p, a, 'first', 1, 0);
  e.apply(p, b, 'forward', 2, 500);
  e.apply(p, a, 'back', 3, 1000);
  assert.equal(r().audio.startAt, 2000);
  e.tick(1999);
  assert.equal(r().audio.generation, 0);
  e.tick(2000);
  assert.equal(r().audio.generation, 1);
});
test('one GO can start sound now, hold previous light five seconds then fade to darkness', () => {
  const { p, d, a, b, e, r } = fixture();
  a.devices[d.id].dimmers['pwm-1'].value = 70;
  b.devices[d.id].dimmers['pwm-1'] = { value: 0, startDelay: 5, fadeIn: null, fadeOut: 1 };
  b.devices[d.id].audio.track = 8;
  e.apply(p, a, 'first', 1, 0);
  assert.equal(r().dimmers['pwm-1'].value, 70);
  e.apply(p, b, 'forward', 2, 1000);
  assert.equal(r().audio.status, 'playing');
  assert.equal(r().audio.track, 8);
  assert.equal(r().dimmers['pwm-1'].value, 70);
  e.tick(5999);
  assert.equal(r().dimmers['pwm-1'].value, 70);
  e.tick(6000);
  assert.equal(r().dimmers['pwm-1'].value, 70);
  e.tick(6500);
  assert.equal(r().dimmers['pwm-1'].value, 35);
  e.tick(7000);
  assert.equal(r().dimmers['pwm-1'].value, 0);
});
test('previous animation continues during wait and handoff samples its attained output at due', () => {
  const { p, d, a, b, e, r } = fixture();
  a.devices[d.id].dimmers['pwm-1'] = {
    value: 100,
    fadeIn: 0,
    fadeOut: 0,
    animation: { ...defaultDimmerAnimation(), enabled: true, mode: 'sine', period: 1 },
  };
  b.devices[d.id].dimmers['pwm-1'] = { value: 80, startDelay: 1, fadeIn: 1, fadeOut: 1 };
  e.apply(p, a, 'first', 1, 0);
  e.tick(250);
  assert.ok(Math.abs(r().dimmers['pwm-1'].value - 50) < 0.001);
  e.apply(p, b, 'forward', 2, 250);
  e.tick(500);
  assert.equal(r().dimmers['pwm-1'].value, 100);
  e.tick(1250);
  assert.ok(Math.abs(r().dimmers['pwm-1'].value - 50) < 0.001);
  e.tick(1750);
  assert.ok(Math.abs(r().dimmers['pwm-1'].value - 65) < 0.001);
});
test('all 12 hardware inversion flags default off, compile masks not flipped logical values, export/import and old absent fields preserve intent', () => {
  const { p, d, a } = fixture();
  assert(d.dimmers.every((v) => v.inverted === false));
  d.dimmers[0].inverted = true;
  d.dimmers[11].inverted = true;
  a.devices[d.id].dimmers['pwm-12'].value = 100;
  let wire = compileDevices(p, [a])[0].cues[0];
  assert.equal(wire.invertedMask, 0x801);
  assert.equal(wire.inversionSet, 0xfff);
  assert.equal(wire.d[11][0], 255);
  const helmet = deviceProfile(d, 'helmet');
  const hp = clone(p);
  hp.devices = [helmet];
  wire = compileDevices(hp, [a])[0].cues[0];
  assert.equal(wire.inversionSet, undefined);
  for (const param of d.dimmers) delete param.inverted;
  const fp = fingerprint(p),
    backup = {
      format: 'theatre-score',
      schemaVersion: 1,
      interfaceVersion: '0.7.11',
      draft: p,
      published: {
        project: clone(p),
        fingerprint: fp,
        version: 109,
        mode: 'esp',
        createdAt: p.updatedAt,
      },
    };
  const loaded = parseBackup(JSON.stringify(backup));
  assert.equal(fingerprint(loaded.published!.project), fp);
  wire = compileDevices(loaded.draft, [loaded.draft.steps[0]])[0].cues[0];
  assert.equal(wire.inversionSet, undefined);
  Object.assign(d.dimmers[0], { inverted: 1 });
  assert.throws(() => validateProject(p));
});
