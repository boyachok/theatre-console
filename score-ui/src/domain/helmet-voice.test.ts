import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createProject, deviceProfile, createStep, clone } from './project.ts';
import { compileDevices } from './compile.ts';
import { defaultVoiceBank, validVoiceBank, voiceSettings } from './voice.ts';
import { validateProject, parseBackup } from './validation.ts';
import { stripColor } from './strip.ts';
import { INTERFACE_VERSION } from '../version.ts';
test('helmet global voice bank compiles identically in every scene, independent light snapshots and JSON bank/working persistence', () => {
  const p = createProject();
  p.devices = [deviceProfile(p.devices[0], 'helmet')];
  const d = p.devices[0];
  d.mac = 'C0:4E:30:29:F4:8C';
  d.voice = defaultVoiceBank();
  d.voice.working = { ...voiceSettings(d), algorithm: 'chip', mode: 'robot', frequency: 1000 };
  p.steps = [createStep(p.devices), createStep(p.devices)];
  const fx = {
    ...p.effects[0],
    mode: 'loading' as const,
    density: 70,
    audioReactive: true,
    release: 0.75,
  };
  p.steps[0].devices[d.id].strip = { action: 'play', effectId: fx.id, settings: fx };
  p.steps[0].devices[d.id].audio.voiceOn = true;
  const cues = compileDevices(p, p.steps)[0].cues;
  assert.deepEqual(cues[0].v, cues[1].v);
  assert.equal(cues[0].v![2], 4);
  assert.equal(cues[0].v![9], 1000);
  assert.equal(cues[0].a[2], 1);
  assert.equal(cues[1].a[2], 0);
  assert.equal(cues[0].s[2], 6);
  assert.equal(cues[0].x![4], 1);
  assert.equal(cues[1].s[1], 1);
  assert.equal(p.effects[0].mode, 'wave');
  const w = {
    format: 'theatre-score',
    schemaVersion: 1,
    interfaceVersion: INTERFACE_VERSION,
    draft: p,
    published: null,
  };
  const loaded = parseBackup(JSON.stringify(w));
  assert.deepEqual(loaded.draft.devices[0].voice, d.voice);
  assert.deepEqual(loaded.draft.steps[0].devices[d.id].strip.settings, fx);
  const broken = clone(p);
  broken.devices[0].voice!.working!.gain = NaN;
  assert.throws(() => validateProject(broken));
  broken.devices[0].voice!.working!.gain = 8;
  broken.steps[0].devices[d.id].strip.settings!.density = 101;
  assert.throws(() => validateProject(broken));
});
test('helmet working gate is compiled for every scene without overwriting the saved preset', () => {
  const p = createProject();
  p.devices = [deviceProfile(p.devices[0], 'helmet')];
  const d = p.devices[0];
  d.mac = 'C0:4E:30:36:C6:88';
  d.voice = defaultVoiceBank();
  const saved = clone(d.voice.presets);
  d.voice.working = { ...voiceSettings(d), mode: 'dry', mix: 0, gateDb: -25 };
  p.steps = [createStep(p.devices), createStep(p.devices)];
  for (const cue of compileDevices(p, p.steps)[0].cues) {
    assert.equal(cue.v![12], -25);
    assert.equal(cue.v![1], 0);
    assert.equal(cue.v![10], 0);
  }
  assert.deepEqual(d.voice.presets, saved);
  d.voice.working.gateDb = -90;
  assert.equal(compileDevices(p, p.steps)[0].cues[0].v![12], -90);
});
test('voice bank bounds, complete settings and eight presets reject corrupt or unsupported profiles', () => {
  const b = defaultVoiceBank();
  assert(validVoiceBank(b));
  b.presets[0].settings.algorithm = 'chip';
  b.presets[0].settings.frequency = 1000;
  assert(validVoiceBank(b));
  b.presets[0].settings.frequency = 1001;
  assert(!validVoiceBank(b));
  b.presets[0].settings.frequency = 85;
  for (let i = 1; i < 9; i++) b.presets.push({ ...structuredClone(b.presets[0]), id: String(i) });
  assert(!validVoiceBank(b));
});
test('new strip modes are bounded, loading symmetric and audio gate does not extinguish an enabled background', () => {
  const e = createProject().effects[0];
  for (const mode of ['waiting', 'disco', 'alarm', 'loading'] as const)
    for (const count of [1, 2, 3, 60, 1024])
      for (const t of [0, 0.5, 2]) {
        const f = { ...e, mode };
        for (let i = 0; i < count; i++) {
          const rgb = stripColor(f, i, count, t);
          assert(rgb.every((v) => Number.isInteger(v) && v >= 0 && v <= 255));
          if (mode === 'loading') assert.deepEqual(rgb, stripColor(f, count - 1 - i, count, t));
        }
      }
  const f = {
    ...e,
    mode: 'static' as const,
    background: '#ffffff',
    backgroundBrightness: 50,
    audioReactive: true,
  };
  assert.deepEqual(stripColor(f, 0, 60, 0, 0), [127, 127, 127]);
  assert.deepEqual(stripColor({ ...f, backgroundEnabled: false }, 0, 60, 0, 0), [0, 0, 0]);
});
