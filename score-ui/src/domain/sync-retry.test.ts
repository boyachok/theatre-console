import test from 'node:test';
import assert from 'node:assert/strict';
import { EspConsoleTransport } from '../transport/esp.ts';
import { INTERFACE_VERSION } from '../version.ts';
import { createProject, clone, fingerprint } from './project.ts';

function project() {
  const p = createProject();
  p.devices[0].mac = 'C0:4E:30:27:69:98';
  return p;
}
function harness(mode: 'status' | 'committed' | 'navigate' | 'role' | 'publish') {
  let fp = '',
    revision = 0,
    failed = false,
    statusCalls = 0,
    draftPosts = 0,
    navPosts = 0,
    publishPosts = 0;
  const fetcher: typeof fetch = async (input, options) => {
    const url = String(input);
    if (url.endsWith('/status')) {
      statusCalls++;
      if (mode === 'status' && !failed) {
        failed = true;
        throw new TypeError('Failed to fetch');
      }
      return Response.json({
        role: 'master',
        version: INTERFACE_VERSION,
        token: 'mock',
        serverMs: 10,
        console: {
          version: 1,
          fingerprint: 'published',
          draftFingerprint: fp,
          draftRevision: revision,
          livePreview: mode !== 'publish',
          navigation: 0,
        },
        nodes:
          mode === 'role'
            ? [{ mac: 'C0:4E:30:27:69:98', role: 'helmet', ageMs: 0, localScoreSupported: true }]
            : [],
        remotes: [],
      });
    }
    if (url.includes('/draft?')) {
      draftPosts++;
      const body = JSON.parse(String(options?.body));
      fp = body.fingerprint;
      revision++;
      if (mode === 'committed' && !failed) {
        failed = true;
        throw new TypeError('Response lost after commit');
      }
      return Response.json({});
    }
    if (url.includes('/publish?')) {
      publishPosts++;
      throw new TypeError('Publication response unknown');
    }
    if (url.includes('/navigate')) {
      navPosts++;
      throw new TypeError('Response unknown');
    }
    throw new Error(`Unexpected request ${url}`);
  };
  return { fetcher, counts: () => ({ statusCalls, draftPosts, navPosts, publishPosts, fp }) };
}
test('editor reconciliation retries a transient HTTP read without GO', async () => {
  const old = globalThis.fetch,
    h = harness('status');
  globalThis.fetch = h.fetcher;
  try {
    const t = new EspConsoleTransport(),
      p = project();
    await t.reconcile(p);
    assert.equal(h.counts().draftPosts, 1);
    assert.equal(h.counts().fp, fingerprint(p));
    assert.equal(h.counts().navPosts, 0);
  } finally {
    globalThis.fetch = old;
  }
});
test('lost draft POST response is resolved by fresh status, not duplicate mutation', async () => {
  const old = globalThis.fetch,
    h = harness('committed');
  globalThis.fetch = h.fetcher;
  try {
    const t = new EspConsoleTransport();
    await t.reconcile(project());
    assert.equal(h.counts().draftPosts, 1);
    assert.equal(h.counts().navPosts, 0);
  } finally {
    globalThis.fetch = old;
  }
});
test('new editor revision cancels a stale reconciliation retry', async () => {
  const old = globalThis.fetch,
    h = harness('status');
  globalThis.fetch = h.fetcher;
  try {
    const t = new EspConsoleTransport(),
      p = project(),
      q = clone(p);
    q.steps[0].devices[q.devices[0].id].dimmers['pwm-1'].value = 70;
    const first = t.reconcile(p);
    await new Promise((r) => setTimeout(r, 10));
    const second = t.reconcile(q);
    await Promise.all([first, second]);
    assert.equal(h.counts().draftPosts, 1);
    assert.equal(h.counts().fp, fingerprint(q));
    assert.equal(h.counts().navPosts, 0);
  } finally {
    globalThis.fetch = old;
  }
});
test('GO/navigation with uncertain HTTP result is never automatically replayed', async () => {
  const old = globalThis.fetch,
    h = harness('navigate');
  globalThis.fetch = h.fetcher;
  try {
    await assert.rejects(() => new EspConsoleTransport().navigate('forward'), /HTTP-связи/);
    assert.equal(h.counts().navPosts, 1);
  } finally {
    globalThis.fetch = old;
  }
});
test('persistent publication with unknown HTTP outcome is not automatically replayed', async () => {
  const old = globalThis.fetch,
    h = harness('publish');
  globalThis.fetch = h.fetcher;
  try {
    await assert.rejects(
      () => new EspConsoleTransport().reconcile(project()),
      /результат запроса неизвестен/,
    );
    assert.equal(h.counts().publishPosts, 1);
  } finally {
    globalThis.fetch = old;
  }
});
test('wrong hardware role is not a transient error and is not retried', async () => {
  const old = globalThis.fetch,
    h = harness('role');
  globalThis.fetch = h.fetcher;
  try {
    await assert.rejects(() => new EspConsoleTransport().reconcile(project()), /не совпадают/);
    assert.equal(h.counts().statusCalls, 1);
    assert.equal(h.counts().draftPosts, 0);
  } finally {
    globalThis.fetch = old;
  }
});
