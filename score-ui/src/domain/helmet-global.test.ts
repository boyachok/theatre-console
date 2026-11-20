import test from 'node:test';
import assert from 'node:assert/strict';
import { createProject, fingerprint } from './project.ts';
import {
  defaultVoiceBank,
  defaultVoiceEq,
  defaultVoicePresence,
  validVoiceSettings,
  validVoiceBank,
  voiceWire,
} from './voice.ts';
import { parseBackup } from './validation.ts';
import { backupFingerprint } from './backup.ts';
import type { Workspace } from './schema.ts';
import { INTERFACE_VERSION } from '../version.ts';
test('EQ/presence are optional, neutral by default, strictly validated and remain ordinary editable presets', () => {
  const bank = defaultVoiceBank(),
    s = bank.presets[0].settings;
  assert(validVoiceSettings(s));
  s.eq = defaultVoiceEq();
  s.presence = defaultVoicePresence();
  assert(validVoiceBank(bank));
  assert.equal(s.eq.enabled, false);
  assert.equal(s.presence.enabled, false);
  for (const [field, value] of [
    ['gainDb', 13],
    ['q', 0],
    ['frequency', 17000],
  ] as const) {
    const bad = structuredClone(s);
    bad.eq!.bands[0][field] = value;
    assert.equal(validVoiceSettings(bad), false);
  }
  const bad = structuredClone(s);
  bad.presence!.amount = 1.1;
  assert.equal(validVoiceSettings(bad), false);
  const project = createProject(),
    d = project.devices[0];
  d.voice = bank;
  const old = voiceWire(d);
  s.eq.enabled = true;
  s.eq.bands[1].gainDb = 4.5;
  s.presence.enabled = true;
  s.presence.amount = 0.2;
  assert.deepEqual(voiceWire(d), old, 'No growth/change to legacy scene voice encoding');
});
test('global voice/EQ survives JSON backup without changing the score or legacy banks', () => {
  const p = createProject(),
    mac = 'C0:4E:30:36:C6:88';
  p.devices[0].profile = 'helmet';
  p.devices[0].mac = mac;
  // Use the standard project for validation: converting a device requires its scene IO to be normalized first.
  p.devices[0].profile = 'standard';
  const w: Workspace = {
    format: 'theatre-score',
    schemaVersion: 1,
    interfaceVersion: INTERFACE_VERSION,
    draft: p,
    published: null,
  };
  const before = fingerprint(p),
    backupBefore = backupFingerprint(w),
    bank = defaultVoiceBank();
  bank.presets[0].settings.eq = defaultVoiceEq();
  bank.presets[0].settings.eq.enabled = true;
  bank.presets[0].settings.eq.bands[1].gainDb = -4;
  const next = { ...w, helmetVoices: { [mac]: bank }, helmetVoiceRevision: 2 };
  assert.equal(fingerprint(next.draft), before);
  assert.notEqual(backupFingerprint(next), backupBefore);
  assert.deepEqual(parseBackup(JSON.stringify(next)).helmetVoices, next.helmetVoices);
  assert.equal(w.draft.devices[0].voice, undefined);
  const bad = structuredClone(next);
  bad.helmetVoices[mac].presets[0].settings.eq!.bands[0].q = NaN;
  assert.throws(() => parseBackup(JSON.stringify(bad)), /банки/);
});
