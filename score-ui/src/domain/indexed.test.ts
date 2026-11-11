import test from 'node:test';
import assert from 'node:assert/strict';
import { EspConsoleTransport } from '../transport/esp.ts';
import { INTERFACE_VERSION } from '../version.ts';
import { createProject } from './project.ts';

test('published GO sends only index/version; scene ACK and preload readiness are never fabricated', async () => {
  const original = globalThis.fetch,
    mac = 'C0:4E:30:27:69:98';
  const calls: { url: string; body: string }[] = [];
  const status = {
    role: 'master',
    version: INTERFACE_VERSION,
    token: 'mock',
    serverMs: 1000,
    console: {
      version: 4,
      step: null as number | null,
      preview: false,
      ready: false,
      storedDevices: 0,
      plannedDevices: 1,
    },
    nodes: [
      {
        mac,
        ip: null,
        ageMs: 0,
        localScoreSupported: true,
        scoreSupported: true,
        scoreVersion: 0,
        sceneStatus: 'loading',
        confirmedStep: null as number | null,
        scoreReceived: 0,
        sent: 20,
        received: 20,
        timeouts: 0,
      },
    ],
  };
  globalThis.fetch = async (input, options) => {
    const url = String(input);
    calls.push({ url, body: String(options?.body ?? '') });
    if (url.endsWith('/status')) return Response.json(status);
    if (url.endsWith('/workspace')) return Response.json(null);
    if (url.endsWith('/go')) {
      const form = new URLSearchParams(String(options?.body));
      assert.equal(form.get('version'), '4');
      assert.equal(form.get('step'), '2');
      assert.equal(form.get('reason'), '0');
      assert(form.has('deadline') && form.has('seq'));
      assert(!form.has('devices') && !form.has('cue'));
      if (!status.console.ready) return new Response('Показ не готов', { status: 409 });
      status.console.step = 3;
      return Response.json({ transition: 8 });
    }
    return Response.json({});
  };
  try {
    const transport = new EspConsoleTransport();
    await transport.loadWorkspace();
    assert.equal(transport.getSnapshot().publicationReady, false);
    await assert.rejects(() => transport.go(4, 2, false), /не готов/);
    assert.equal(transport.getSnapshot().activeStep, null);
    status.console.ready = true;
    status.console.storedDevices = 1;
    status.nodes[0].scoreVersion = 4;
    status.nodes[0].sceneStatus = 'waiting_ack';
    await transport.go(4, 2, false);
    let peer = transport.getSnapshot().devices[mac];
    assert.equal(transport.getSnapshot().activeStep, 3);
    assert.equal(peer.confirmedStep, null);
    assert.equal(peer.sceneResponses, 0);
    assert.equal(peer.sceneStatus, 'waiting_ack');
    status.nodes[0].sceneStatus = 'applied';
    status.nodes[0].confirmedStep = 3;
    status.nodes[0].scoreReceived = 1;
    await transport.loadWorkspace();
    peer = transport.getSnapshot().devices[mac];
    assert.equal(peer.confirmedStep, 3);
    assert.equal(peer.sceneResponses, 1);
    const p = createProject();
    p.devices[0].mac = mac;
    status.nodes[0].localScoreSupported = false;
    await assert.rejects(
      () => transport.publish(p, null),
      (e: unknown) =>
        e instanceof Error && e.message.includes(`Theatre console ${INTERFACE_VERSION}`),
    );
    assert.equal(calls.filter((c) => c.url.includes('/publish')).length, 0);
  } finally {
    globalThis.fetch = original;
  }
});
