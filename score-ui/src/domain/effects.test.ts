import test from 'node:test';
import assert from 'node:assert/strict';
import { clone, createProject } from './project.ts';
import { PreviewEngine } from './playback.ts';
test('static programs hold their color; edited program is adopted only on explicit GO', () => {
  const p = createProject(),
    d = p.devices[0].id,
    fx = p.effects.find((f) => f.mode === 'static')!;
  p.steps[0].devices[d].strip = { action: 'play', effectId: fx.id };
  const engine = new PreviewEngine(p);
  engine.apply(p, p.steps[0], 'first', 1, 0);
  engine.tick(100000);
  assert.equal(engine.getSnapshot().devices[d].strip?.effect.mode, 'static');
  const before = engine.getSnapshot().devices[d].strip?.effect.color;
  fx.color = '#ff0000';
  assert.equal(engine.getSnapshot().devices[d].strip?.effect.color, before);
  const next = clone(p.steps[0]);
  next.id = 'next';
  engine.apply(p, next, 'forward', 2, 100000);
  assert.equal(engine.getSnapshot().devices[d].strip?.effect.color, '#ff0000');
  assert.equal(engine.getSnapshot().devices[d].strip?.startedAt, 100000);
  fx.name = 'Новое имя';
  engine.apply(p, p.steps[0], 'forward', 3, 110000);
  assert.equal(engine.getSnapshot().devices[d].strip?.startedAt, 100000);
});
