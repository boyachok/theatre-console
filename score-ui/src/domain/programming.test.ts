import test from 'node:test';
import assert from 'node:assert/strict';
import { createProject, clone, duplicateStep } from './project.ts';
import { compileDraft, draftDelta } from './programming.ts';
import { EspConsoleTransport } from '../transport/esp.ts';
import { INTERFACE_VERSION } from '../version.ts';
const mac = 'C0:4E:30:27:69:98';
test('programming uses small patches; structural steps/MAC changes require a full atomic draft', () => {
  const p = createProject();
  p.devices[0].mac = mac;
  const previous = compileDraft(p),
    changed = clone(p);
  changed.steps[0].devices[p.devices[0].id].dimmers['pwm-1'].value = 70;
  const delta = draftDelta(compileDraft(changed), previous, previous.fingerprint);
  assert('patches' in delta && delta.patches.length === 1);
  assert.equal(delta.patches[0].step, 0);
  const bigger = duplicateStep(changed, changed.steps[0].id).project;
  assert('devices' in draftDelta(compileDraft(bigger), previous, previous.fingerprint));
  changed.devices[0].mac = '30:30:F9:31:19:C4';
  assert('devices' in draftDelta(compileDraft(changed), previous, previous.fingerprint));
});
test('editor transport: live draft never publishes, OFF sync never queues GO, stale remote epoch rejects browser intent', async () => {
  const original = globalThis.fetch,
    calls: { path: string; body: string }[] = [];
  const state = {
    role: 'master',
    version: INTERFACE_VERSION,
    token: 'test',
    serverMs: 1000,
    measuring: false,
    nodes: [],
    console: {
      version: 0,
      fingerprint: '',
      draftFingerprint: '',
      draftRevision: 0,
      navigation: 0,
      livePreview: false,
      step: null as number | null,
    },
  };
  let race = false;
  globalThis.fetch = async (input, options) => {
    const path = String(input).split('?')[0],
      body = String(options?.body ?? '');
    calls.push({ path, body });
    if (path.endsWith('/status')) {
      if (race) {
        state.console.navigation++;
        state.console.step = 2;
        race = false;
      }
      return Response.json(state);
    }
    if (path.endsWith('/workspace')) return Response.json(null);
    if (path.endsWith('/draft')) {
      const d = JSON.parse(body);
      assert.equal(d.revision, state.console.draftRevision);
      assert.equal(d.base, state.console.draftFingerprint);
      state.console.draftFingerprint = d.fingerprint;
      state.console.draftRevision++;
      return Response.json({});
    }
    if (path.endsWith('/preview-mode') || path.endsWith('/draft-go')) {
      const form = new URLSearchParams(body);
      if (Number(form.get('navigation')) !== state.console.navigation)
        return new Response('Сцена уже переключена мастером', { status: 409 });
      state.console.livePreview = form.get('enabled') === '1';
      return Response.json({});
    }
    if (path.endsWith('/publish')) {
      const d = JSON.parse(body);
      state.console.version = d.version;
      state.console.fingerprint = d.fingerprint;
      state.console.livePreview = false;
      return Response.json({ version: d.version });
    }
    return Response.json({});
  };
  try {
    const p = createProject();
    p.devices[0].mac = mac;
    const transport = new EspConsoleTransport();
    await transport.loadWorkspace();
    await transport.changePreview(true, p, p.steps[0].id);
    await transport.navigate('last');
    const relative = new URLSearchParams(calls.find((c) => c.path.endsWith('/navigate'))!.body);
    assert.equal(relative.get('action'), '4');
    assert(!relative.has('step')); // No browser-side scene counter.
    const changed = clone(p);
    changed.steps[0].devices[p.devices[0].id].dimmers['pwm-1'].value = 50;
    assert.equal(await transport.reconcile(changed), null);
    assert(!calls.some((c) => c.path.endsWith('/publish')));
    assert(calls.some((c) => c.path.endsWith('/draft') && JSON.parse(c.body).patches));
    const saved = await transport.changePreview(false, changed, changed.steps[0].id);
    assert.equal(saved?.version, 1);
    assert(!calls.some((c) => c.path.endsWith('/go')));
    assert.equal(state.console.livePreview, false);
    changed.name += ' edit';
    assert.equal((await transport.reconcile(changed))?.version, 2);
    await transport.changePreview(true, changed, changed.steps[0].id);
    race = true;
    await assert.rejects(
      transport.selectDraft(changed, changed.steps[0].id, false),
      /уже переключена/,
    );
    assert.equal(state.console.step, 2);
    const invalid = clone(changed);
    invalid.steps[0].devices[invalid.devices[0].id].relays['relay-1'].on = true;
    await assert.rejects(transport.changePreview(false, invalid, invalid.steps[0].id), /HIGH/);
    assert.equal(state.console.livePreview, false); // OFF remains effective even if sync cannot compile.
  } finally {
    globalThis.fetch = original;
  }
});
