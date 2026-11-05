import test from 'node:test';
import assert from 'node:assert/strict';
import {
  createProject,
  deviceProfile,
  columnsFor,
  updateCell,
  clone,
  fingerprint,
} from './project.ts';
import { compileDevices } from './compile.ts';
import { parseBackup, validateProject } from './validation.ts';
import { PreviewEngine } from './playback.ts';
import { EspConsoleTransport } from '../transport/esp.ts';
import { INTERFACE_VERSION } from '../version.ts';

test('helmet has only strip/voice columns; codec receives a gate, never a track/PWM/relay', () => {
  let p = createProject();
  p.devices = [deviceProfile(p.devices[0], 'helmet')];
  const d = p.devices[0];
  d.mac = '30:30:F9:32:02:30';
  d.ledCount = 127;
  const step = p.steps[0];
  // Old authored values remain in the editor data but cannot leak to helmet GPIO.
  step.devices[d.id].dimmers['pwm-1'].value = 100;
  step.devices[d.id].relays['relay-1'].on = true;
  step.devices[d.id].audio.track = 7;
  step.devices[d.id].strip = { action: 'play', effectId: p.effects[0].id };
  p = updateCell(
    p,
    { stepId: step.id, deviceId: d.id, parameterId: d.audio.id, kind: 'audio' },
    { voiceOn: true },
  );
  assert.deepEqual(
    columnsFor(p, 'all').map((c) => c.kind),
    ['strip', 'audio'],
  );
  let cue = compileDevices(p, p.steps)[0].cues[0];
  assert.equal(cue.dimMask, 0);
  assert.equal(cue.relayMask, 0);
  assert(cue.d.every((v) => v.every((n) => n === 0)));
  assert(cue.r.every((v) => v.every((n) => n === 0)));
  assert.deepEqual(cue.a, [0, 0, 1, 0, 0]);
  assert.equal(cue.s[3], 127);
  p.steps[0].devices[d.id].audio.voiceOn = false;
  cue = compileDevices(p, p.steps)[0].cues[0];
  assert.deepEqual(cue.a, [0, 0, 0, 0, 0]);
  d.ledCount = 1025;
  assert.throws(() => compileDevices({ ...p, devices: [d] }, p.steps), /1024/);
});
test('helmet backup is round-trippable without rewriting an immutable old publication', () => {
  const old = createProject();
  const draft = clone(old);
  draft.devices[0] = deviceProfile(draft.devices[0], 'helmet');
  draft.steps[0].devices[draft.devices[0].id].audio.voiceOn = true;
  const fp = fingerprint(old);
  const loaded = parseBackup(
    JSON.stringify({
      format: 'theatre-score',
      schemaVersion: 1,
      interfaceVersion: '0.7.3',
      draft,
      published: {
        version: 1,
        fingerprint: fp,
        createdAt: new Date().toISOString(),
        mode: 'esp',
        project: old,
      },
    }),
  );
  assert.equal(loaded.draft.devices[0].profile, 'helmet');
  assert.equal(loaded.published!.fingerprint, fp);
  assert.equal(fingerprint(loaded.published!.project), fp);
  assert.equal(loaded.published!.project.devices[0].profile, undefined);
});
test('ESP transport distinguishes helmet from ordinary slaves and rejects wrong-role binding', async () => {
  const original = globalThis.fetch,
    mac = '30:30:F9:32:02:30';
  let role = 'helmet',
    posts = 0;
  globalThis.fetch = async (input, options) => {
    if (options?.method === 'POST') posts++;
    return String(input).endsWith('/status')
      ? Response.json({
          role: 'master',
          version: INTERFACE_VERSION,
          token: 'mock',
          serverMs: 1,
          console: { version: 0, step: 2, preview: false },
          nodes: [
            {
              role,
              mac,
              ip: null,
              ageMs: 0,
              scoreSupported: true,
              localScoreSupported: true,
              sent: 0,
              received: 0,
              timeouts: 0,
            },
          ],
        })
      : Response.json(null);
  };
  try {
    const p = createProject();
    p.devices[0] = deviceProfile(p.devices[0], 'helmet');
    p.devices[0].mac = mac;
    const transport = new EspConsoleTransport();
    await transport.loadWorkspace();
    assert.equal(transport.getSnapshot().devices[mac].role, 'helmet');
    assert.equal(transport.getSnapshot().activeStep, 2);
    await transport.publish(p, null);
    assert.equal(posts, 1);
    role = 'slave';
    await assert.rejects(() => transport.publish(p, null), /не совпадают/);
    assert.equal(posts, 1);
    role = 'helmet';
    p.devices[0] = deviceProfile(p.devices[0], 'standard');
    await assert.rejects(() => transport.publish(p, null), /не совпадают/);
    assert.equal(posts, 1);
  } finally {
    globalThis.fetch = original;
  }
});
test('helmet cannot enable unsupported modules or invent local audio playback', () => {
  const p = createProject();
  p.devices[0] = deviceProfile(p.devices[0], 'helmet');
  const d = p.devices[0];
  p.steps[0].devices[d.id].audio.track = 1;
  p.steps[0].devices[d.id].audio.voiceOn = true;
  const engine = new PreviewEngine(p);
  engine.apply(p, p.steps[0], 'forward', 1, 0);
  assert.equal(engine.getSnapshot().devices[d.id].audio.status, 'stopped');
  d.dimmers[0].enabled = true;
  assert.throws(() => validateProject(p), /Helmet/);
});
