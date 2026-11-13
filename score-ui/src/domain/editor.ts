import type { CellSelection, Column, DeviceCue, Project } from './schema.ts';
import { acCue, clone, columnsFor, effectiveStrip, touch } from './project.ts';
export type EditField = 'level' | 'fadeIn' | 'fadeOut' | 'startDelay';
export interface Modifiers {
  shiftKey?: boolean;
  ctrlKey?: boolean;
  metaKey?: boolean;
}
export const rowKey = (row: Column | CellSelection) =>
  JSON.stringify(
    'device' in row ? [row.device.id, row.parameter.id] : [row.deviceId, row.parameterId],
  );
export const cellKey = (cell: CellSelection) =>
  JSON.stringify([cell.stepId, cell.deviceId, cell.parameterId]);
// Copy only touched step/device cues: big scores and undo keep structural sharing.
function editable(project: Project) {
  const next = { ...project, steps: [...project.steps] };
  const indices = new Map(project.steps.map((s, i) => [s.id, i]));
  const steps = new Set<string>(),
    cues = new Set<string>();
  const mutable = (
    cell: CellSelection,
  ): { step: Project['steps'][number]; cue: DeviceCue } | null => {
    const i = indices.get(cell.stepId);
    if (i === undefined || !next.steps[i].devices[cell.deviceId]) return null;
    if (!steps.has(cell.stepId)) {
      next.steps[i] = { ...next.steps[i], devices: { ...next.steps[i].devices } };
      steps.add(cell.stepId);
    }
    const step = next.steps[i],
      key = JSON.stringify([cell.stepId, cell.deviceId]);
    if (!cues.has(key)) {
      step.devices[cell.deviceId] = clone(step.devices[cell.deviceId]);
      cues.add(key);
    }
    return { step, cue: step.devices[cell.deviceId] };
  };
  return { next, mutable };
}
export function editorRows(project: Project, collapsed: string[] = []): Column[] {
  const rows = columnsFor(project, 'all');
  const ordered = project.devices.flatMap((d) => {
    const own = rows.filter((r) => r.device.id === d.id);
    return own.sort((a, b) => rank(d.rowOrder, a.parameter.id) - rank(d.rowOrder, b.parameter.id));
  });
  return project.layout?.mode === 'custom'
    ? ordered.sort(
        (a, b) => rank(project.layout?.order, rowKey(a)) - rank(project.layout?.order, rowKey(b)),
      )
    : ordered.filter((r) => !collapsed.includes(r.device.id));
}
const rank = (order: string[] | undefined, key: string) => {
  const i = order?.indexOf(key) ?? -1;
  return i < 0 ? 100000 : i;
};
export const cellAt = (project: Project, row: Column, step: number): CellSelection => ({
  stepId: project.steps[step].id,
  deviceId: row.device.id,
  parameterId: row.parameter.id,
  kind: row.kind,
});
export function rectangle(
  project: Project,
  rows: Column[],
  a: CellSelection,
  b: CellSelection,
): CellSelection[] {
  const x = project.steps.findIndex((s) => s.id === a.stepId),
    y = project.steps.findIndex((s) => s.id === b.stepId),
    u = rows.findIndex((r) => rowKey(r) === rowKey(a)),
    v = rows.findIndex((r) => rowKey(r) === rowKey(b));
  if (Math.min(x, y, u, v) < 0) return [b];
  return rows.slice(Math.min(u, v), Math.max(u, v) + 1).flatMap((row) =>
    project.steps.slice(Math.min(x, y), Math.max(x, y) + 1).map((s) => ({
      ...b,
      stepId: s.id,
      deviceId: row.device.id,
      parameterId: row.parameter.id,
      kind: row.kind,
    })),
  );
}
export function moveCursor(
  project: Project,
  rows: Column[],
  cell: CellSelection | null,
  dx: number,
  dy: number,
): CellSelection | null {
  if (!rows.length || !project.steps.length) return null;
  const x = cell ? project.steps.findIndex((s) => s.id === cell.stepId) : 0,
    y = cell ? rows.findIndex((r) => rowKey(r) === rowKey(cell)) : 0;
  return cellAt(
    project,
    rows[Math.max(0, Math.min(rows.length - 1, Math.max(0, y) + dy))],
    Math.max(0, Math.min(project.steps.length - 1, Math.max(0, x) + dx)),
  );
}
export function columnCells(project: Project, rows: Column[], ids: string[]): CellSelection[] {
  return rows.flatMap((r) =>
    project.steps
      .filter((s) => ids.includes(s.id))
      .map((s) => ({
        stepId: s.id,
        deviceId: r.device.id,
        parameterId: r.parameter.id,
        kind: r.kind,
      })),
  );
}
export function moveEditorRow(project: Project, source: string, target: string): Project {
  const rows = editorRows(project);
  const from = rows.find((r) => rowKey(r) === source),
    to = rows.find((r) => rowKey(r) === target);
  if (!from || !to || source === target) return project;
  if (project.layout?.mode === 'custom') {
    const order = rows.map(rowKey),
      destination = order.indexOf(target);
    order.splice(order.indexOf(source), 1);
    order.splice(destination, 0, source);
    return touch(project, { layout: { mode: 'custom', order } });
  }
  if (from.device.id !== to.device.id) return project;
  const order = rows.filter((r) => r.device.id === from.device.id).map((r) => r.parameter.id),
    destination = order.indexOf(to.parameter.id);
  order.splice(order.indexOf(from.parameter.id), 1);
  order.splice(destination, 0, from.parameter.id);
  return touch(project, {
    devices: project.devices.map((d) => (d.id === from.device.id ? { ...d, rowOrder: order } : d)),
  });
}
export function moveDevice(project: Project, id: string, destination: number): Project {
  const devices = [...project.devices],
    index = devices.findIndex((d) => d.id === id);
  if (index < 0) return project;
  const [d] = devices.splice(index, 1);
  devices.splice(Math.max(0, Math.min(devices.length, destination)), 0, d);
  return touch(project, { devices });
}
export function applyEdit(
  project: Project,
  cells: CellSelection[],
  field: EditField,
  value: number | null,
): { project: Project; count: number } {
  if (
    value !== null &&
    (!Number.isFinite(value) || value < 0 || value > (field === 'level' ? 100 : 3600))
  )
    return { project, count: 0 };
  const { next, mutable } = editable(project);
  let count = 0;
  const seen = new Set<string>();
  for (const c of cells) {
    if (seen.has(cellKey(c))) continue;
    seen.add(cellKey(c));
    const edit = mutable(c),
      device = next.devices.find((d) => d.id === c.deviceId);
    if (!edit || !device) continue;
    const { step, cue } = edit;
    if (c.kind === 'dimmer' || c.kind === 'ac') {
      const light = c.kind === 'ac' ? (cue.ac ??= { ...acCue(cue) }) : cue.dimmers[c.parameterId];
      if (!light) continue;
      if (field === 'level') {
        if (value === null) continue;
        light.value = value;
      } else if (field === 'startDelay') light.startDelay = value ?? 0;
      else light[field] = value;
      count++;
    } else if (c.kind === 'audio' && device.profile !== 'helmet') {
      if (field === 'level') {
        if (value === null) continue;
        cue.audio.volume = value;
      } else if (field === 'startDelay') cue.audio.startDelay = value ?? 0;
      else cue.audio[field] = value;
      count++;
    } else if (c.kind === 'relay' && (field === 'fadeIn' || field === 'fadeOut')) {
      cue.relays[c.parameterId][field === 'fadeIn' ? 'delayOn' : 'delayOff'] = value;
      count++;
    } else if (
      field === 'level' &&
      value !== null &&
      (value === 0 || value === 100) &&
      (c.kind === 'relay' || (c.kind === 'audio' && device.profile === 'helmet'))
    ) {
      if (c.kind === 'relay') cue.relays[c.parameterId].on = value === 100;
      else cue.audio.voiceOn = value === 100;
      count++;
    } else if (field === 'level' && value !== null && c.kind === 'strip') {
      const source = effectiveStrip(project, step, c.deviceId),
        effect = source.settings ?? project.effects.find((e) => e.id === source.effectId);
      if (!effect || source.action !== 'play') continue;
      cue.strip = {
        action: 'play',
        effectId: source.effectId,
        settings: { ...effect, brightness: value },
      };
      count++;
    }
  }
  return { project: count ? touch(next, {}) : project, count };
}
// Details-panel patches affect only compatible selected rows, never track IDs on dimmers etc.
export function patchCells(
  project: Project,
  cells: CellSelection[],
  source: CellSelection,
  patch: Record<string, unknown>,
): Project {
  const { next, mutable } = editable(project);
  let count = 0;
  for (const cell of cells) {
    if (
      cell.kind === 'audio' &&
      (project.devices.find((d) => d.id === cell.deviceId)?.profile === 'helmet') !==
        (project.devices.find((d) => d.id === source.deviceId)?.profile === 'helmet')
    )
      continue;
    if (!(
      cell.kind === source.kind ||
      (['dimmer', 'ac'].includes(cell.kind) && ['dimmer', 'ac'].includes(source.kind))
    ))
      continue;
    const edit = mutable(cell);
    if (!edit) continue;
    const cue = edit.cue;
    if (cell.kind === 'dimmer' && cue.dimmers[cell.parameterId])
      Object.assign(cue.dimmers[cell.parameterId], patch);
    else if (cell.kind === 'ac') Object.assign((cue.ac ??= { ...acCue(cue) }), patch);
    else if (cell.kind === 'relay' && cue.relays[cell.parameterId])
      Object.assign(cue.relays[cell.parameterId], patch);
    else if (cell.kind === 'audio') {
      const lastTrack =
        typeof patch.track === 'number' ? patch.track : (cue.audio.track ?? cue.audio.lastTrack);
      Object.assign(cue.audio, patch);
      cue.audio.lastTrack = lastTrack;
    } else if (cell.kind === 'strip') Object.assign(cue.strip, patch);
    else continue;
    count++;
  }
  return count ? touch(next, {}) : project;
}
