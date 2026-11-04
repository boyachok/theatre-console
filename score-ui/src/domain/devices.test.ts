import test from 'node:test';
import assert from 'node:assert/strict';
import {
  clone,
  createDevice,
  createProject,
  duplicateStep,
  replaceDevices,
  updateCell,
} from './project.ts';
import { validateProject } from './validation.ts';
import { PreviewEngine } from './playback.ts';
import { LocalPreviewTransport } from '../transport/preview.ts';
import { DisconnectedNetworkTransport } from '../transport/network.ts';
test('hiding a module retains all cue data and stable row identities', () => {
  const p = createProject(),
    id = p.devices[0].id;
  p.steps[0].devices[id].dimmers['pwm-1'].value = 75;
  const devices = clone(p.devices);
  devices[0].dimmers[0].enabled = false;
  const hidden = replaceDevices(p, devices);
  assert.equal(hidden.steps, p.steps);
  assert.equal(hidden.steps[0].devices[id].dimmers['pwm-1'].value, 75);
  devices[0].dimmers[0].enabled = true;
  assert.equal(replaceDevices(hidden, devices).steps[0].devices[id].dimmers['pwm-1'].value, 75);
});
test('adding/removing a device updates every step without changing other cues or published version', async () => {
  let p = createProject();
  p = duplicateStep(p, p.steps[0].id).project;
  const first = p.devices[0],
    removed = p.devices[9];
  p.steps[0].devices[first.id].audio.track = 8;
  const published = await new LocalPreviewTransport().publish(p, null);
  const smaller = replaceDevices(p, p.devices.slice(0, -1));
  assert(smaller.steps.every((step) => !(removed.id in step.devices)));
  const fresh = createDevice(9),
    added = replaceDevices(smaller, [...smaller.devices, fresh]);
  assert(
    added.steps.every(
      (step) =>
        step.devices[fresh.id].dimmers['pwm-1'].value === 0 &&
        step.devices[fresh.id].audio.track === null,
    ),
  );
  assert.equal(added.steps[0].devices[first.id], p.steps[0].devices[first.id]);
  assert.equal(published.project.devices.length, 10);
  assert(removed.id in published.project.steps[0].devices);
  validateProject(added);
  assert.throws(() => replaceDevices(added, [...added.devices, createDevice(10)]));
  assert.throws(() => replaceDevices(added, []));
});
test('new device joins preview only on an explicit subsequent transition', () => {
  let p = createProject();
  p = duplicateStep(p, p.steps[0].id).project;
  const engine = new PreviewEngine(p);
  engine.apply(p, p.steps[0], 'first', 1, 0);
  const fresh = createDevice(9);
  const next = replaceDevices(p, [...p.devices.slice(0, -1), fresh]);
  const edited = updateCell(
    next,
    { stepId: next.steps[1].id, deviceId: fresh.id, parameterId: 'pwm-1', kind: 'dimmer' },
    { value: 40 },
  );
  assert.equal(engine.getSnapshot().devices[fresh.id], undefined);
  engine.apply(edited, edited.steps[1], 'forward', 2, 1000);
  assert.equal(engine.getSnapshot().devices[fresh.id].dimmers['pwm-1'].value, 40);
});
test('static effects and optional real IPs are backward-compatible with existing schema', () => {
  const p = createProject();
  p.devices[0].ip = '192.168.50.33';
  assert(p.effects.some((f) => f.mode === 'static'));
  validateProject(p);
  delete p.devices[0].ip;
  validateProject(p);
  p.devices[0].ip = '999.168.0.1';
  assert.throws(() => validateProject(p));
});
test('network switch has no invented telemetry, packets, or acknowledgements', () => {
  const network = new DisconnectedNetworkTransport();
  let changes = 0;
  const unsubscribe = network.subscribe(() => changes++);
  network.setMeasurement(true);
  assert.equal(network.getSnapshot().testEnabled, true);
  assert.equal(network.getSnapshot().connected, false);
  assert.deepEqual(network.getSnapshot().devices, {});
  network.setMeasurement(false);
  network.setMeasurement(false);
  assert.equal(changes, 2);
  unsubscribe();
});
