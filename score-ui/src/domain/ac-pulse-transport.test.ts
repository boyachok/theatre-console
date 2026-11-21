import test from 'node:test';
import assert from 'node:assert/strict';
import { EspConsoleTransport } from '../transport/esp.ts';
import { INTERFACE_VERSION } from '../version.ts';
const mac = 'C0:4E:30:27:69:98';
function server(
  options: {
    running?: boolean;
    locked?: boolean;
    unsupported?: boolean;
    stale?: boolean;
    requested?: number;
    uncertain?: boolean;
    reboot?: boolean;
    result?: string;
  } = {},
) {
  let pulse = 200,
    posts = 0;
  const calls: string[] = [];
  const json = (v: unknown) => new Response(JSON.stringify(v));
  const fetcher: typeof fetch = async (url, init = {}) => {
    const path = String(url).split('?')[0];
    calls.push(`${init.method ?? 'GET'} ${path}`);
    if (path === '/api/console/status')
      return json({
        role: 'master',
        version: INTERFACE_VERSION,
        token: 't',
        serverMs: 100,
        nodes: [
          {
            id: 5,
            mac,
            role: 'slave',
            protocol: 4,
            deviceBoot: options.reboot && posts ? 43 : 42,
            ageMs: 10,
            sent: 1,
            received: 1,
            timeouts: 0,
            requestId: posts,
            commandPending: false,
            commandStatus: options.result ?? 'accepted',
            acDimmer: options.unsupported
              ? null
              : {
                  pulseUs: pulse,
                  minUs: 200,
                  maxUs: 400,
                  configFault: false,
                  requested: options.requested ?? 0,
                  paired: options.requested ?? 0,
                  ageMs: options.stale ? 4000 : 10,
                  qualified: true,
                  halfUs: 10000,
                  goodZero: 12,
                  scheduled: 0,
                  skippedPulses: 0,
                  late: 0,
                  syncLosses: 0,
                },
          },
        ],
        console: {
          version: 384,
          editorLocked: options.locked ?? false,
          running: options.running ?? false,
          livePreview: false,
          startupPending: false,
        },
      });
    assert.equal(path, '/command');
    assert.equal(init.method, 'POST');
    const form = new URLSearchParams(String(init.body));
    assert.equal(form.get('mac'), mac);
    assert.equal(form.get('id'), '5');
    assert.equal(form.get('op'), '24');
    assert.equal(form.get('channel'), '0');
    assert.equal(form.get('token'), 't');
    pulse = Number(form.get('value'));
    posts++;
    if (options.uncertain) throw new TypeError('lost response after commit');
    return json({ requestId: posts });
  };
  return {
    fetcher,
    calls,
    get posts() {
      return posts;
    },
    get pulse() {
      return pulse;
    },
  };
}
test('AC pulse applies with one MAC-bound POST in STOP, no GO/reboot/publish or automatic replay', async () => {
  const old = globalThis.fetch,
    s = server();
  globalThis.fetch = s.fetcher;
  try {
    const t = new EspConsoleTransport();
    await t.setAcPulseUs(mac, 300);
    assert.equal(s.posts, 1);
    assert.equal(s.pulse, 300);
    assert.equal(t.getSnapshot().devices[mac].acDimmer?.pulseUs, 300);
    assert(s.calls.every((c) => c === 'POST /command' || c === 'GET /api/console/status'));
  } finally {
    globalThis.fetch = old;
  }
});
test('AC pulse rejects active load/show, stale telemetry, locked/old slave and invalid numbers without POST', async () => {
  const old = globalThis.fetch;
  try {
    for (const options of [
      { running: true },
      { locked: true },
      { unsupported: true },
      { stale: true },
      { requested: 77 },
    ]) {
      const s = server(options);
      globalThis.fetch = s.fetcher;
      await assert.rejects(() => new EspConsoleTransport().setAcPulseUs(mac, 300));
      assert.equal(s.posts, 0);
    }
    for (const value of [0, 199, 401, 300.5, NaN]) {
      const s = server();
      globalThis.fetch = s.fetcher;
      await assert.rejects(() => new EspConsoleTransport().setAcPulseUs(mac, value));
      assert.equal(s.posts, 0);
    }
  } finally {
    globalThis.fetch = old;
  }
});
test('production accepted ACK succeeds; negative/unknown ACK cannot be inferred from matching telemetry', async () => {
  const old = globalThis.fetch;
  try {
    for (const result of ['not_ready', 'io_or_queue_error', 'UNKNOWN: no ACK; may have executed']) {
      const s = server({ result });
      globalThis.fetch = s.fetcher;
      await assert.rejects(() => new EspConsoleTransport().setAcPulseUs(mac, 300));
      assert.equal(s.posts, 1);
    }
  } finally {
    globalThis.fetch = old;
  }
});
test('AC pulse unknown POST outcome and changed boot are never blindly repeated', async () => {
  const old = globalThis.fetch;
  try {
    for (const options of [{ uncertain: true }, { reboot: true }]) {
      const s = server(options);
      globalThis.fetch = s.fetcher;
      await assert.rejects(() => new EspConsoleTransport().setAcPulseUs(mac, 400));
      assert.equal(s.posts, 1);
    }
  } finally {
    globalThis.fetch = old;
  }
});
