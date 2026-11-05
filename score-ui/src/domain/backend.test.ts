import test from 'node:test';
import assert from 'node:assert/strict';
import { createProject } from './project.ts';
import { compileDevices } from './compile.ts';
import { INTERFACE_VERSION } from '../version.ts';
import { EspConsoleTransport } from '../transport/esp.ts';
const mac = 'C0:4E:30:27:69:98';
test('compiler binds MAC, sends absolute levels and millisecond fades, rejects unsafe relay and compiles WS2811', () => {
  const p = createProject(),
    d = p.devices[0];
  d.mac = mac;
  p.steps[0].devices[d.id].dimmers['pwm-1'].value = 50;
  p.steps[0].timings.lightIn = 0.2;
  const wire = compileDevices(p, p.steps);
  assert.equal(wire.length, 1);
  assert.deepEqual(wire[0].cues[0].d[0], [128, 200, 0]);
  p.steps[0].devices[d.id].relays['relay-1'].on = true;
  assert.throws(() => compileDevices(p, p.steps), /HIGH/);
  d.relays[0].relayMode = 2;
  assert.equal(compileDevices(p, p.steps)[0].cues[0].r[0][1], 2);
  p.steps[0].devices[d.id].strip = { action: 'play', effectId: p.effects[0].id };
  const strip = compileDevices(p, p.steps)[0].cues[0];
  assert.equal(strip.d.length, 12);
  assert.equal(strip.s[1], 2);
  assert.equal(strip.s[3], 60);
  d.ledCount = 1025;
  assert.throws(() => compileDevices(p, p.steps), /1024/);
});
test('real transport sends publication/workspace and confirmed API calls without fake acknowledgements', async () => {
  const original = globalThis.fetch,
    calls: { url: string; body: string }[] = [];
  const status = {
    role: 'master',
    version: INTERFACE_VERSION,
    token: 'abc',
    serverMs: 1000,
    mac: 'A0:85:E3:E2:BC:D4',
    measuring: false,
    console: { version: 0, step: null, preview: false },
    nodes: [
      {
        mac,
        id: 1,
        name: 'Test',
        ip: '192.168.50.33',
        ageMs: 10,
        scoreSupported: true,
        localScoreSupported: true,
        scoreVersion: 0,
        scoreSent: 0,
        scoreReceived: 0,
        timeouts: 0,
        p95: null,
        rssiSlave: -50,
        worstRssi: -50,
        confirmedStep: null,
      },
    ],
  };
  globalThis.fetch = async (input, options) => {
    const url = String(input);
    calls.push({ url, body: String(options?.body ?? '') });
    if (url === '/api/console/status') return Response.json(status);
    if (url === '/api/console/workspace') return Response.json(null);
    if (url.startsWith('/api/console/publish')) return Response.json({ version: 1 });
    if (url.startsWith('/api/console/preview')) return Response.json({ transition: 1 });
    return new Response('OK');
  };
  try {
    const transport = new EspConsoleTransport();
    assert.equal(calls.length, 0);
    assert.equal(await transport.loadWorkspace(), null);
    assert.equal(transport.getSnapshot().devices[mac].confirmedStep, null);
    const p = createProject();
    p.devices[0].mac = mac;
    p.steps[0].devices[p.devices[0].id].dimmers['pwm-1'].value = 100;
    const published = await transport.publish(p, null);
    assert.equal(published.mode, 'esp');
    const body = JSON.parse(calls.find((c) => c.url.startsWith('/api/console/publish'))!.body);
    assert.equal(body.devices[0].mac, mac);
    assert.equal(JSON.parse(body.workspaceJson).published.version, 1);
    await transport.preview(p, p.steps[0], false);
    assert(
      calls.some(
        (c) =>
          c.url.includes('/preview?') && c.url.includes('deadline=') && c.url.includes('seq=1'),
      ),
    );
    await transport.identify(mac);
    await transport.resetNetwork();
    assert(calls.some((c) => c.url === '/api/console/identify'));
    assert(calls.some((c) => c.url === '/api/console/network/reset'));
  } finally {
    globalThis.fetch = original;
  }
});
test('remote telemetry is real transport data, never an assignable actuator', async () => {
  const original = globalThis.fetch;
  let posts = 0;
  globalThis.fetch = async (input, options) => {
    if (options?.method === 'POST') posts++;
    return String(input).endsWith('/status')
      ? Response.json({
          role: 'master',
          version: INTERFACE_VERSION,
          token: 'test',
          serverMs: 1000,
          mac: 'A0:85:E3:E2:BC:D4',
          measuring: true,
          console: { version: 0, step: null },
          nodes: [],
          remotes: [
            {
              role: 'remote',
              mac,
              name: 'remote',
              ip: '192.168.50.88',
              ageMs: 10,
              sent: 4,
              received: 4,
              timeouts: 0,
              p50: 12,
              p95: 15,
              rssiSlave: -60,
              worstRssi: -60,
            },
          ],
        })
      : Response.json(null);
  };
  try {
    const transport = new EspConsoleTransport();
    await transport.loadWorkspace();
    const report = transport.getSnapshot().devices[mac];
    assert.equal(report.role, 'remote');
    assert.equal(report.rttMs, 12);
    assert.equal(report.rssi, -60);
    assert.equal(report.confirmedStep, null);
    assert.equal(report.scoreVersion, undefined);
    const project = createProject();
    project.devices[0].mac = mac;
    await assert.rejects(() => transport.publish(project, null), /пульт/);
    assert.equal(posts, 0);
  } finally {
    globalThis.fetch = original;
  }
});
