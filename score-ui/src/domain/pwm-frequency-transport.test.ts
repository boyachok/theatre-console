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
    uncertain?: boolean;
    reboot?: boolean;
  } = {},
) {
  const calls: string[] = [];
  let saved = 25000,
    posts = 0;
  const json = (x: unknown) => new Response(JSON.stringify(x));
  const fetcher: typeof fetch = async (url, init = {}) => {
    const path = String(url).split('?')[0];
    calls.push(`${init.method ?? 'GET'} ${path}`);
    if (path === '/api/console/status')
      return json({
        role: 'master',
        version: INTERFACE_VERSION,
        token: 't',
        serverMs: 100,
        mac: 'A0:85:E3:E2:BC:D4',
        nodes: [
          {
            id: 3,
            mac,
            deviceBoot: options.reboot && posts ? 43 : 42,
            role: 'slave',
            protocol: 4,
            ageMs: 20,
            sent: 3,
            received: 3,
            timeouts: 0,
            history: [],
            pwmFrequency: options.unsupported
              ? null
              : {
                  activeHz: 25000,
                  savedHz: saved,
                  minHz: 2000,
                  maxHz: 25000,
                  bits: 8,
                  mcpwmHzCalculated: 25000,
                  rebootRequired: saved !== 25000,
                },
            requestId: posts,
            commandPending: false,
            commandStatus: 'applied',
          },
        ],
        console: {
          version: 384,
          running: options.running ?? false,
          livePreview: false,
          editorLocked: options.locked ?? false,
          startupPending: false,
        },
      });
    assert.equal(path, '/command');
    assert.equal(init.method, 'POST');
    const body = new URLSearchParams(String(init.body));
    assert.equal(body.get('op'), '23');
    assert.equal(body.get('channel'), '0');
    assert.equal(body.get('mac'), mac);
    assert.equal(body.get('id'), '3');
    assert.equal(body.get('token'), 't');
    assert(Number(body.get('deadline')) <= 1100);
    saved = Number(body.get('value'));
    posts++;
    if (options.uncertain) throw new TypeError('reply lost');
    return json({ requestId: posts });
  };
  return {
    fetcher,
    calls,
    get posts() {
      return posts;
    },
    get saved() {
      return saved;
    },
  };
}
test('frequency setting is one MAC-bound POST, read-only ACK polling, no GO/reboot/score writes', async () => {
  const old = globalThis.fetch,
    s = server();
  globalThis.fetch = s.fetcher;
  try {
    const t = new EspConsoleTransport();
    await t.setPwmFrequency(mac, 2001);
    assert.equal(s.posts, 1);
    assert.equal(s.saved, 2001);
    assert.equal(t.getSnapshot().devices[mac].pwmFrequency?.activeHz, 25000);
    assert.equal(t.getSnapshot().devices[mac].pwmFrequency?.rebootRequired, true);
    assert(s.calls.every((c) => c === 'GET /api/console/status' || c === 'POST /command'));
  } finally {
    globalThis.fetch = old;
  }
});
test('frequency cannot be sent during show/lock or to old firmware, no clamping invalid number', async () => {
  const old = globalThis.fetch;
  try {
    for (const options of [{ running: true }, { locked: true }, { unsupported: true }]) {
      const s = server(options);
      globalThis.fetch = s.fetcher;
      await assert.rejects(() => new EspConsoleTransport().setPwmFrequency(mac, 2000));
      assert.equal(s.posts, 0);
    }
    for (const hz of [0, 1999, 25001, 2000.5, NaN]) {
      const s = server();
      globalThis.fetch = s.fetcher;
      await assert.rejects(() => new EspConsoleTransport().setPwmFrequency(mac, hz));
      assert.equal(s.posts, 0);
    }
  } finally {
    globalThis.fetch = old;
  }
});
test('uncertain frequency save and changed device boot are not replayed', async () => {
  const old = globalThis.fetch;
  try {
    for (const options of [{ uncertain: true }, { reboot: true }]) {
      const s = server(options);
      globalThis.fetch = s.fetcher;
      await assert.rejects(() => new EspConsoleTransport().setPwmFrequency(mac, 2000));
      assert.equal(s.posts, 1);
    }
  } finally {
    globalThis.fetch = old;
  }
});
