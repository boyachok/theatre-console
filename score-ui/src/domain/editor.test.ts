import test from 'node:test';
import assert from 'node:assert/strict';
import { createProject, duplicateStep, clone, deviceProfile } from './project.ts';
import {
  applyEdit,
  cellAt,
  editorRows,
  rectangle,
  moveCursor,
  moveEditorRow,
  rowKey,
  moveDevice,
  patchCells,
  columnCells,
} from './editor.ts';
import { historyReducer } from './history.ts';
import { validateProject, parseBackup } from './validation.ts';
import { fingerprint } from './project.ts';
function fixture() {
  let p = createProject();
  p.devices = p.devices.slice(0, 2);
  p = duplicateStep(p, p.steps[0].id).project;
  p = duplicateStep(p, p.steps[1].id).project;
  return p;
}
test('editor: arrows only move, Shift rectangle spans controllers and steps, boundaries clamp', () => {
  const p = fixture(),
    r = editorRows(p),
    before = JSON.stringify(p),
    a = cellAt(p, r[0], 0),
    b = cellAt(p, r[2], 2);
  assert.equal(rectangle(p, r, a, b).length, 9);
  assert.equal(moveCursor(p, r, a, -1, -1)?.parameterId, a.parameterId);
  assert.equal(moveCursor(p, r, a, 1, 1)?.stepId, p.steps[1].id);
  assert.equal(JSON.stringify(p), before);
});
test('editor: batched zero and fades are atomic, no cross-step/device aliasing or unrelated edits', () => {
  const p = fixture(),
    r = editorRows(p),
    cells = rectangle(p, r, cellAt(p, r[0], 0), cellAt(p, r[1], 2));
  const a = applyEdit(p, cells, 'level', 50);
  assert.equal(a.count, 6);
  const b = applyEdit(a.project, cells, 'fadeIn', 0);
  assert.equal(b.count, 6);
  assert.equal(p.steps[0].devices[p.devices[0].id].dimmers['pwm-1'].value, 0);
  assert.equal(b.project.steps[0].devices[p.devices[0].id].dimmers['pwm-1'].value, 50);
  assert.equal(b.project.steps[0].devices[p.devices[0].id].dimmers['pwm-1'].fadeIn, 0);
  assert.equal(b.project.steps[1].devices[p.devices[0].id].dimmers['pwm-2'].fadeIn, 0);
  assert.equal(b.project.steps[0].devices[p.devices[0].id].dimmers['pwm-3'].fadeIn, null);
  assert.equal(applyEdit(p, cells, 'fadeIn', -1).count, 0);
});
test('editor: untouched scores structurally shared, row keys tolerate punctuation in IDs', () => {
  const p = fixture(),
    r = editorRows(p),
    cell = cellAt(p, r[0], 0);
  const q = applyEdit(p, [cell], 'level', 12).project;
  assert.equal(q.devices, p.devices);
  assert.equal(q.effects, p.effects);
  assert.equal(q.steps[1], p.steps[1]);
  assert.equal(q.steps[0].devices[p.devices[1].id], p.steps[0].devices[p.devices[1].id]);
  assert.notEqual(q.steps[0].devices[cell.deviceId], p.steps[0].devices[cell.deviceId]);
  assert.notEqual(
    rowKey({ ...cell, deviceId: 'a/b', parameterId: 'c' }),
    rowKey({ ...cell, deviceId: 'a', parameterId: 'b/c' }),
  );
});
test('editor: compatible panel patches only, boolean outputs reject partial percentages', () => {
  const p = fixture(),
    r = editorRows(p),
    dim = cellAt(p, r[0], 0),
    relay = cellAt(
      p,
      r.find((c) => c.kind === 'relay')!,
      0,
    );
  assert.equal(applyEdit(p, [relay], 'level', 50).count, 0);
  assert.equal(applyEdit(p, [relay], 'level', 100).count, 1);
  const q = patchCells(p, [dim, relay], dim, { fadeIn: 4 });
  assert.equal(q.steps[0].devices[dim.deviceId].dimmers[dim.parameterId].fadeIn, 4);
  assert.equal(q.steps[0].devices[relay.deviceId].relays[relay.parameterId].delayOn, null);
});
test('editor: local/global row order and device order preserve stable physical bindings and every cue', () => {
  const p = fixture(),
    before = JSON.stringify(p.steps),
    r = editorRows(p);
  let q = moveEditorRow(p, rowKey(r[0]), rowKey(r[1]));
  assert.equal(editorRows(q)[1].parameter.id, r[0].parameter.id);
  q = { ...q, layout: { mode: 'custom', order: [] } };
  const rows = editorRows(q);
  q = moveEditorRow(q, rowKey(rows[0]), rowKey(rows.at(-1)!));
  assert.equal(rowKey(editorRows(q).at(-1)!), rowKey(rows[0]));
  q = moveDevice(q, p.devices[0].id, 1);
  assert.equal(q.devices[1].id, p.devices[0].id);
  assert.equal(JSON.stringify(q.steps), before);
  validateProject(q);
});
test('editor: layout JSON roundtrip, corrupt order rejected, collapsed rows excluded only in grouped mode', () => {
  const p = fixture();
  p.layout = { mode: 'custom', order: editorRows(p).map(rowKey).reverse() };
  p.devices[0].rowOrder = ['audio', 'pwm-1'];
  const w = {
    format: 'theatre-score',
    schemaVersion: 1,
    interfaceVersion: '0.8.0',
    draft: p,
    published: {
      version: 1,
      project: clone(p),
      fingerprint: fingerprint(p),
      createdAt: p.updatedAt,
      mode: 'local',
    },
  };
  const loaded = parseBackup(JSON.stringify(w));
  assert.deepEqual(loaded.draft.layout, p.layout);
  assert.equal(editorRows(p, [p.devices[0].id]).length, editorRows(p).length);
  p.layout.mode = 'devices';
  assert(editorRows(p, [p.devices[0].id]).every((r) => r.device.id !== p.devices[0].id));
  (p.layout.order as unknown[]).push(22);
  assert.throws(() => validateProject(p));
});
test('editor: panel batch keeps remembered audio file when toggled off', () => {
  const p = fixture(),
    row = editorRows(p).find((r) => r.kind === 'audio')!,
    cell = cellAt(p, row, 0);
  let q = patchCells(p, [cell], cell, { track: 5 });
  q = patchCells(q, [cell], cell, { track: null });
  assert.equal(q.steps[0].devices[cell.deviceId].audio.track, null);
  assert.equal(q.steps[0].devices[cell.deviceId].audio.lastTrack, 5);
});
test('editor: undo/redo restores draft only, keeps latest publication, clearing history isolates imports', () => {
  const p = fixture(),
    workspace = {
      format: 'theatre-score' as const,
      schemaVersion: 1 as const,
      interfaceVersion: '0.8.0',
      draft: p,
      published: null,
    };
  let s = historyReducer(
    { workspace, past: [], future: [], at: 0 },
    { type: 'set', at: 1000, update: (w) => ({ ...w, draft: { ...w.draft, name: 'Changed' } }) },
  );
  s = historyReducer(s, { type: 'undo', at: 2000 });
  assert.equal(s.workspace.draft.name, p.name);
  s = historyReducer(s, { type: 'redo', at: 3000 });
  assert.equal(s.workspace.draft.name, 'Changed');
  s = historyReducer(s, { type: 'clear' });
  assert.equal(s.past.length, 0);
});
test('editor: header selection covers only intended steps; helmet percentage never changes global preset', () => {
  const p = fixture();
  p.devices[0] = deviceProfile(p.devices[0], 'helmet');
  const r = editorRows(p);
  const cells = columnCells(p, r, [p.steps[1].id]);
  assert(cells.every((c) => c.stepId === p.steps[1].id));
  const audio = cellAt(
    p,
    r.find((c) => c.kind === 'audio' && c.device.id === p.devices[0].id)!,
    1,
  );
  const before = JSON.stringify(p.devices[0].voice);
  assert.equal(applyEdit(p, [audio], 'level', 50).count, 0);
  const q = applyEdit(p, [audio], 'level', 100);
  assert.equal(q.project.steps[1].devices[p.devices[0].id].audio.voiceOn, true);
  assert.equal(JSON.stringify(q.project.devices[0].voice), before);
});
