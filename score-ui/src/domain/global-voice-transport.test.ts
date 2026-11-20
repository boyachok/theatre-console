import test from 'node:test';
import assert from 'node:assert/strict';
import { EspConsoleTransport } from '../transport/esp.ts';
import { defaultVoiceBank, defaultVoiceEq } from './voice.ts';
import { INTERFACE_VERSION } from '../version.ts';
const mac = 'C0:4E:30:36:C6:88';
function server(initial = 1, uncertain = false) {
  const calls: string[] = [];
  let revision = initial,
    posts = 0;
  let banks: Record<string, ReturnType<typeof defaultVoiceBank>> = { [mac]: defaultVoiceBank() };
  const json = (v: unknown) =>
    new Response(JSON.stringify(v), { headers: { 'Content-Type': 'application/json' } });
  const fetcher: typeof fetch = async (url, options = {}) => {
    const path = String(url).split('?')[0];
    calls.push(`${options.method ?? 'GET'} ${path}`);
    if (path === '/api/console/status')
      return json({
        role: 'master',
        version: INTERFACE_VERSION,
        token: 't',
        serverMs: 123,
        mac: 'A0:85:E3:E2:BC:D4',
        nodes: [],
        remotes: [],
        console: {
          version: 383,
          step: 1,
          stepId: 'first',
          running: true,
          livePreview: false,
          editorLocked: true,
          helmetVoiceRevision: revision,
          navigation: 1,
          transition: 1,
        },
      });
    assert.equal(path, '/api/console/helmet-voices');
    if (options.method === 'POST') {
      const b = JSON.parse(String(options.body));
      assert.equal(b.revision, revision);
      banks = { ...banks, [b.mac]: b.bank };
      revision++;
      posts++;
      if (uncertain) throw new TypeError('response lost after commit');
    }
    return json({ revision, banks, saved: true });
  };
  return {
    fetcher,
    calls,
    get posts() {
      return posts;
    },
    get banks() {
      return banks;
    },
  };
}
test('independent EQ/preset is applied while score locked, without scene/draft/publication requests', async () => {
  const old = globalThis.fetch,
    s = server();
  globalThis.fetch = s.fetcher;
  try {
    const t = new EspConsoleTransport(),
      b = defaultVoiceBank();
    b.presets[0].settings.eq = defaultVoiceEq();
    b.presets[0].settings.eq.enabled = true;
    b.presets[0].settings.eq.bands[1].gainDb = 5;
    await t.saveHelmetBank(mac, b, 1);
    assert.equal(s.posts, 1);
    assert.equal(s.banks[mac].presets[0].settings.eq?.bands[1].gainDb, 5);
    assert.equal(t.getSnapshot().activeStep, 1);
    assert.equal(t.getSnapshot().running, true);
    assert.equal(t.getSnapshot().editorLocked, true);
    assert(s.calls.every((p) => p.includes('/status') || p.includes('/helmet-voices')));
  } finally {
    globalThis.fetch = old;
  }
});
test('foreign voice revision is a conflict, never overwritten by stale local settings', async () => {
  const old = globalThis.fetch,
    s = server(2);
  globalThis.fetch = s.fetcher;
  try {
    await assert.rejects(
      () => new EspConsoleTransport().saveHelmetBank(mac, defaultVoiceBank(), 1),
      /другим редактором/,
    );
    assert.equal(s.posts, 0);
  } finally {
    globalThis.fetch = old;
  }
});
test('uncertain voice response is not blindly retried', async () => {
  const old = globalThis.fetch,
    s = server(1, true);
  globalThis.fetch = s.fetcher;
  try {
    await assert.rejects(
      () => new EspConsoleTransport().saveHelmetBank(mac, defaultVoiceBank(), 1),
      /результат запроса неизвестен/,
    );
    assert.equal(s.posts, 1);
  } finally {
    globalThis.fetch = old;
  }
});
test('rapid edits coalesce per helmet, not across different targets', async () => {
  const old = globalThis.fetch,
    s = server();
  globalThis.fetch = s.fetcher;
  try {
    const t = new EspConsoleTransport(),
      a = defaultVoiceBank(),
      b = defaultVoiceBank(),
      other = 'C0:4E:30:36:C6:90';
    a.presets[0].settings.gain = 4;
    b.presets[0].settings.gain = 5;
    await Promise.all([
      t.saveHelmetBank(mac, a, 1),
      t.saveHelmetBank(mac, b, 1),
      t.saveHelmetBank(other, a, 1),
    ]);
    assert.equal(s.posts, 2);
    assert.equal(s.banks[mac].presets[0].settings.gain, 5);
    assert.equal(s.banks[other].presets[0].settings.gain, 4);
  } finally {
    globalThis.fetch = old;
  }
});
