import test from 'node:test';
import assert from 'node:assert/strict';
import { defaultDimmerAnimation, dimmerHash, dimmerFactor } from './dimmer.ts';
import { createProject, duplicateStep, fingerprint, updateCell, clone } from './project.ts';
import { compileDevices } from './compile.ts';
import { PreviewEngine } from './playback.ts';
import { validateProject, parseBackup } from './validation.ts';
import type { DimmerAnimation } from './schema.ts';
const near = (a: number, b: number) => assert(Math.abs(a - b) < 1e-7, `${a} != ${b}`);
function fixture() {
  const p = createProject();
  p.devices = [p.devices[0]];
  const d = p.devices[0];
  d.mac = 'C0:4E:30:27:69:98';
  const s = p.steps[0],
    c = s.devices[d.id].dimmers['pwm-1'];
  c.value = 100;
  c.animation = { ...defaultDimmerAnimation(), enabled: true, period: 1 };
  const engine = new PreviewEngine(p),
    value = () => engine.getSnapshot().devices[d.id].dimmers['pwm-1'].value;
  return { p, d, s, c, engine, value };
}
test('dimmer wave compilation is optional, strict, channel-local and retains immutable old publications', () => {
  const { p, d, s, c } = fixture();
  assert.deepEqual(compileDevices(p, [s])[0].cues[0].d[0], [255, 0, 0, 1, 1000, 0, 0, 50, 0]);
  assert.deepEqual(compileDevices(p, [s])[0].cues[0].d[1], [0, 0, 0]);
  c.animation = { ...c.animation!, mode: 'random', seed: 0xffffffff, floor: 25, smooth: 100 };
  assert.deepEqual(
    compileDevices(p, [s])[0].cues[0].d[0],
    [255, 0, 0, 4, 1000, 0xffffffff, 25, 50, 100],
  );
  const copy = duplicateStep(p, s.id).project;
  copy.steps[1].devices[d.id].dimmers['pwm-1'].animation!.seed = 5;
  assert.equal(c.animation.seed, 0xffffffff);
  delete c.animation;
  const fp = fingerprint(p),
    published = {
      version: 14,
      fingerprint: fp,
      createdAt: p.updatedAt,
      project: clone(p),
      mode: 'esp',
    };
  const backup = JSON.stringify({
    format: 'theatre-score',
    schemaVersion: 1,
    interfaceVersion: '0.7.9',
    draft: p,
    published,
  });
  const loaded = parseBackup(backup);
  assert.equal(fingerprint(loaded.published!.project), fp);
  assert.equal(loaded.draft.steps[0].devices[d.id].dimmers['pwm-1'].animation, undefined);
  const changed = updateCell(
    loaded.draft,
    { stepId: s.id, deviceId: d.id, parameterId: 'pwm-1', kind: 'dimmer' },
    { animation: { ...defaultDimmerAnimation(), enabled: true } },
  );
  assert.notEqual(fingerprint(changed), fp);
  assert.equal(fingerprint(loaded.published!.project), fp);
});
test('dimmer animation rejects bad periods/NaN/phase/seed/mode/floor/duty/smoothing, even while disabled', () => {
  for (const patch of [
    { period: 0.019 },
    { period: 3601 },
    { period: NaN },
    { phase: 361 },
    { seed: -1 },
    { seed: 1.5 },
    { seed: 0x100000000 },
    { floor: 101 },
    { duty: -1 },
    { smooth: Infinity },
    { mode: 'broken' },
    { enabled: 1 },
  ]) {
    const { p, c } = fixture();
    Object.assign(c.animation!, patch);
    assert.throws(() => validateProject(p));
  }
  const { p, c } = fixture();
  c.animation!.enabled = false;
  c.animation!.period = 0;
  assert.throws(() => validateProject(p));
});
test('DIM sine phase/amplitude/floor and static preserve old fade semantics and preview snapshots', () => {
  const { p, s, c, engine, value } = fixture();
  c.value = 60;
  c.animation!.floor = 25;
  engine.apply(p, s, 'forward', 1, 0);
  near(value(), 15);
  const previous = engine.getSnapshot();
  engine.tick(250);
  near(value(), 37.5);
  engine.tick(500);
  near(value(), 60);
  near(previous.devices[p.devices[0].id].dimmers['pwm-1'].value, 15);
  c.animation!.phase = 180;
  engine.apply(p, s, 'back', 2, 500);
  near(value(), 60);
  c.value = 0;
  engine.apply(p, s, 'forward', 3, 500);
  near(value(), 0);
});
test('same-scene LIVE edits and packet repeats keep phase; speed rescales position; scenes/back restart, identical recovery holds', () => {
  const { p, d, s, c, engine, value } = fixture();
  engine.apply(p, s, 'forward', 1, 0);
  engine.tick(250);
  near(value(), 50);
  s.devices[d.id].audio.volume = 40;
  engine.apply(p, s, 'forward', 2, 250);
  near(value(), 50);
  assert(!engine.apply(p, s, 'forward', 2, 400));
  engine.tick(500);
  near(value(), 100);
  c.animation!.period = 2;
  engine.apply(p, s, 'forward', 3, 500);
  near(value(), 100);
  engine.tick(1000);
  near(value(), 50);
  engine.apply(p, s, 'back', 4, 1000);
  near(value(), 0);
  engine.tick(1500);
  near(value(), 50);
  const second = clone(s);
  second.id = 'second';
  p.steps.push(second);
  engine.apply(p, second, 'forward', 5, 1500);
  near(value(), 0);
  engine.tick(2000);
  engine.apply(p, second, 'recover', 6, 2000);
  near(value(), 50);
});
test('DIM square strobes, asymmetric ramps, deterministic random and smoothing match firmware math', () => {
  const a = { ...defaultDimmerAnimation(), enabled: true, period: 1 };
  const at = (ms: number, config: DimmerAnimation = a) =>
    dimmerFactor(config, {
      at: ms,
      cycle: Math.floor(ms / (config.period * 1000)),
      position: ms % (config.period * 1000),
    });
  a.mode = 'square';
  a.duty = 25;
  near(at(249), 1);
  near(at(250), 0);
  near(at(1000), 1);
  a.period = 0.02;
  a.duty = 1;
  near(at(0), 1);
  near(at(1), 0);
  a.period = 1;
  a.mode = 'ramp';
  a.duty = 50;
  near(at(250), 0.5);
  near(at(500), 1);
  near(at(750), 0.5);
  a.duty = 100;
  near(at(250), 0.25);
  a.duty = 0;
  near(at(250), 0.75);
  assert.equal(dimmerHash(1, 0), 1753845952);
  a.mode = 'random';
  a.seed = 42;
  const first = at(0),
    next = at(1000);
  assert.notEqual(first, next);
  near(at(999), first);
  a.smooth = 100;
  near(at(500), (first + next) / 2);
});
test('animation handoff fades from actual attained output; independent channels and disabled mask stay quiet', () => {
  const { p, d, s, c, engine, value } = fixture();
  engine.apply(p, s, 'forward', 1, 0);
  engine.tick(250);
  near(value(), 50);
  c.animation!.enabled = false;
  c.fadeIn = 1;
  engine.apply(p, s, 'forward', 2, 250);
  near(value(), 50);
  engine.tick(750);
  near(value(), 75);
  engine.tick(1250);
  near(value(), 100);
  c.fadeOut = 1;
  d.dimmers[0].enabled = false;
  engine.apply(p, s, 'forward', 3, 1250);
  engine.tick(1750);
  near(value(), 50);
  engine.tick(2250);
  near(value(), 0);
  assert.equal(c.value, 100); // output gating must not erase the authored cue.
  const active = fixture();
  active.c.animation!.mode = 'random';
  active.c.animation!.seed = 5;
  active.s.devices[active.d.id].dimmers['pwm-2'] = {
    value: 80,
    fadeIn: null,
    fadeOut: null,
    animation: { ...active.c.animation!, seed: 7 },
  };
  active.engine.apply(active.p, active.s, 'forward', 1, 0);
  active.engine.tick(1500);
  const other = active.engine.getSnapshot().devices[active.d.id].dimmers['pwm-2'].value;
  assert.notEqual(active.value() / 100, other / 80);
});
