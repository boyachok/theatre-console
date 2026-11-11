import type {
  CellSelection,
  Column,
  Device,
  DeviceCue,
  DimmerCue,
  Parameter,
  Project,
  Step,
  StripEffect,
  Timings,
} from './schema.ts';
export const uid = () =>
  globalThis.crypto?.randomUUID?.() ??
  `${Date.now().toString(36)}-${Math.random().toString(36).slice(2)}`;
export const clone = <T>(value: T): T => structuredClone(value);
export const clamp = (n: number, min: number, max: number) =>
  Math.min(max, Math.max(min, Number.isFinite(n) ? n : min));
export const percentToDuty = (percent: number) => Math.round((clamp(percent, 0, 100) * 255) / 100);
export const defaultTimings = (): Timings => ({
  lightIn: 0,
  lightOut: 0,
  audioIn: 0,
  audioOut: 0,
  relayOn: 0,
  relayOff: 0,
});
const parameter = (id: string, name: string, color: Parameter['color'] = 'white'): Parameter => ({
  id,
  name,
  color,
  enabled: true,
});
export const acParameter = (device: Device): Parameter =>
  device.acDimmer ?? { id: 'ac-220', name: 'Диммер220', color: 'warm', enabled: false };
export const lightDefault = (): DimmerCue => ({ value: 0, fadeIn: null, fadeOut: null });
export const acCue = (cue: DeviceCue): DimmerCue => cue.ac ?? lightDefault();
// Called only by an explicit device-settings Save, never by telemetry or JSON parsing.
export function captureDevicePolarity(device: Device, mask?: number): Device {
  if (
    device.profile === 'helmet' ||
    typeof mask !== 'number' ||
    !Number.isInteger(mask) ||
    mask < 0 ||
    mask > 4095
  )
    return device;
  return {
    ...device,
    dimmers: device.dimmers.map((p, i) =>
      p.inverted === undefined ? { ...p, inverted: !!(mask & (1 << i)) } : p,
    ),
  };
}
export function createDevice(index: number): Device {
  return {
    id: uid(),
    name: `Прибор ${index + 1}`,
    ip: null,
    acDimmer: { id: 'ac-220', name: 'Диммер220', color: 'warm', enabled: false },
    dimmers: Array.from({ length: 12 }, (_, i) => ({
      ...parameter(`pwm-${i + 1}`, `Диммер ${i + 1}`),
      inverted: false,
    })),
    relays: Array.from({ length: 2 }, (_, i) =>
      parameter(`relay-${i + 1}`, `Реле ${i + 1}`, 'warm'),
    ),
    audio: parameter('audio', 'Звук', 'blue'),
    strip: parameter('strip', 'Лента', 'green'),
    ledCount: 60,
  };
}
export function deviceProfile(device: Device, profile: 'standard' | 'helmet'): Device {
  return {
    ...device,
    profile,
    acDimmer: {
      ...acParameter(device),
      enabled: profile === 'helmet' ? false : acParameter(device).enabled,
    },
    dimmers: device.dimmers.map((p) => ({
      ...p,
      enabled: profile === 'helmet' ? false : p.enabled,
    })),
    relays: device.relays.map((p) => ({ ...p, enabled: profile === 'helmet' ? false : p.enabled })),
    audio: { ...device.audio, name: profile === 'helmet' ? 'Голос' : 'Звук' },
  };
}
export function upgradeDraft(project: Project): Project {
  if (project.devices.every((d) => d.dimmers.length === 12)) return project;
  const next = clone(project);
  for (const device of next.devices) {
    while (device.dimmers.length < 12) {
      const i = device.dimmers.length;
      const p = {
        ...parameter(`pwm-${i + 1}`, `Диммер ${i + 1}`),
        enabled: false,
        inverted: false,
      };
      device.dimmers.push(p);
      for (const step of next.steps)
        step.devices[device.id].dimmers[p.id] = { value: 0, fadeIn: null, fadeOut: null };
    }
  }
  return touch(next, {});
}
export function effectiveStrip(project: Project, step: Step, deviceId: string) {
  let value: DeviceCue['strip'] = { action: 'stop', effectId: null };
  for (const cue of project.steps) {
    const next = cue.devices[deviceId]?.strip;
    if (next && next.action !== 'hold') value = next;
    if (cue.id === step.id) break;
  }
  return value;
}
export function createDeviceCue(device: Device): DeviceCue {
  return {
    ac: lightDefault(),
    dimmers: Object.fromEntries(
      device.dimmers.map((p) => [p.id, { value: 0, fadeIn: null, fadeOut: null }]),
    ),
    relays: Object.fromEntries(
      device.relays.map((p) => [p.id, { on: false, delayOn: null, delayOff: null }]),
    ),
    audio: { track: null, volume: 100, fadeIn: null, fadeOut: null },
    strip: { action: 'hold', effectId: null },
  };
}
export function createStep(devices: Device[], name = 'Шаг 1'): Step {
  return {
    id: uid(),
    name,
    timings: defaultTimings(),
    devices: Object.fromEntries(devices.map((d) => [d.id, createDeviceCue(d)])),
  };
}
export function createProject(): Project {
  const devices = Array.from({ length: 10 }, (_, i) => createDevice(i));
  return {
    id: uid(),
    name: 'Новый спектакль',
    revision: 1,
    updatedAt: new Date().toISOString(),
    devices,
    steps: [createStep(devices)],
    effects: [
      {
        id: uid(),
        name: 'Бегущая волна',
        mode: 'wave',
        color: '#638dff',
        background: '#000000',
        brightness: 100,
        width: 20,
        duration: 4,
        direction: 'forward',
      },
      {
        id: uid(),
        name: 'Заполнение',
        mode: 'fill',
        color: '#edc18d',
        background: '#000000',
        brightness: 100,
        width: 12,
        duration: 4,
        direction: 'forward',
      },
      {
        id: uid(),
        name: 'Статика',
        mode: 'static',
        color: '#ffffff',
        background: '#000000',
        brightness: 100,
        width: 100,
        duration: 1,
        direction: 'forward',
      },
    ],
  };
}
export function revise(project: Project, change: (draft: Project) => void): Project {
  const next = clone(project);
  change(next);
  next.revision++;
  next.updatedAt = new Date().toISOString();
  return next;
}
export function duplicateStep(project: Project, id: string): { project: Project; id: string } {
  const source = project.steps.find((s) => s.id === id);
  if (!source) throw new Error('Шаг не найден');
  const copy = clone(source);
  copy.id = uid();
  copy.name = /^Шаг \d+$/.test(source.name)
    ? `Шаг ${project.steps.length + 1}`
    : `${source.name.replace(/ · копия$/, '')} · копия`;
  const steps = [...project.steps];
  steps.splice(steps.findIndex((s) => s.id === id) + 1, 0, copy);
  return { project: touch(project, { steps }), id: copy.id };
}
export function moveStep(project: Project, id: string, destination: number): Project {
  const index = project.steps.findIndex((s) => s.id === id),
    target = clamp(destination, 0, project.steps.length - 1);
  if (index < 0 || index === target) return project;
  const steps = [...project.steps];
  const [step] = steps.splice(index, 1);
  steps.splice(target, 0, step);
  return touch(project, { steps });
}
export function columnsFor(project: Project, deviceId: string): Column[] {
  return project.devices
    .filter((d) => deviceId === 'all' || d.id === deviceId)
    .flatMap((device) => [
      ...(device.profile === 'helmet'
        ? []
        : device.dimmers.map((parameter) => ({ device, parameter, kind: 'dimmer' as const }))),
      ...(device.profile === 'helmet'
        ? []
        : device.relays.map((parameter) => ({ device, parameter, kind: 'relay' as const }))),
      ...(device.profile === 'helmet'
        ? []
        : [{ device, parameter: acParameter(device), kind: 'ac' as const }]),
      { device, parameter: device.strip, kind: 'strip' as const },
      { device, parameter: device.audio, kind: 'audio' as const },
    ])
    .filter((c) => c.parameter.enabled);
}
export function replaceDevices(project: Project, devices: Device[]): Project {
  if (
    devices.length < 1 ||
    devices.length > 10 ||
    new Set(devices.map((d) => d.id)).size !== devices.length
  )
    throw new Error('Нужно от 1 до 10 разных приборов');
  const sameIds =
    devices.length === project.devices.length &&
    devices.every((d, i) => d.id === project.devices[i].id);
  const steps = sameIds
    ? project.steps
    : project.steps.map((step) => ({
        ...step,
        devices: Object.fromEntries(
          devices.map((d) => [d.id, step.devices[d.id] ?? createDeviceCue(d)]),
        ),
      }));
  return touch(project, { devices, steps });
}
export function parameterCode(device: Device, parameter: Parameter): string {
  if (parameter.id === acParameter(device).id) return 'AC220';
  const dim = device.dimmers.findIndex((p) => p.id === parameter.id);
  if (dim >= 0) return `DIM${dim + 1}`;
  const relay = device.relays.findIndex((p) => p.id === parameter.id);
  if (relay >= 0) return `REL${relay + 1}`;
  return parameter.id === device.strip.id ? 'LED' : device.profile === 'helmet' ? 'VOICE' : 'SOU1';
}
export function touch(project: Project, patch: Partial<Project>): Project {
  return {
    ...project,
    ...patch,
    revision: project.revision + 1,
    updatedAt: new Date().toISOString(),
  };
}
export function updateStep(project: Project, id: string, change: (step: Step) => Step): Project {
  return touch(project, {
    steps: project.steps.map((step) => (step.id === id ? change(step) : step)),
  });
}
export function updateCell(
  project: Project,
  cell: CellSelection,
  changes: Record<string, unknown>,
): Project {
  return updateStep(project, cell.stepId, (step) => {
    const source = step.devices[cell.deviceId];
    let cue: DeviceCue;
    if (cell.kind === 'ac') cue = { ...source, ac: { ...acCue(source), ...changes } };
    else if (cell.kind === 'dimmer')
      cue = {
        ...source,
        dimmers: {
          ...source.dimmers,
          [cell.parameterId]: { ...source.dimmers[cell.parameterId], ...changes },
        },
      };
    else if (cell.kind === 'relay')
      cue = {
        ...source,
        relays: {
          ...source.relays,
          [cell.parameterId]: { ...source.relays[cell.parameterId], ...changes },
        },
      };
    else if (cell.kind === 'audio')
      cue = {
        ...source,
        audio: {
          ...source.audio,
          ...changes,
          lastTrack:
            typeof changes.track === 'number'
              ? changes.track
              : (source.audio.track ?? source.audio.lastTrack),
        },
      };
    else cue = { ...source, [cell.kind]: { ...source[cell.kind], ...changes } };
    return { ...step, devices: { ...step.devices, [cell.deviceId]: cue } };
  });
}
export function fingerprint(project: Project): string {
  let hash = 2166136261;
  const content = JSON.stringify(project);
  for (let i = 0; i < content.length; i++) {
    hash ^= content.charCodeAt(i);
    hash = Math.imul(hash, 16777619);
  }
  return (hash >>> 0).toString(16).padStart(8, '0');
}
export function effectLabel(project: Project, cue: DeviceCue['strip']): string {
  return cue.action === 'hold'
    ? '—'
    : cue.action === 'stop'
      ? 'СТОП'
      : (project.effects.find((e) => e.id === cue.effectId)?.name ?? '—');
}
export function duplicateEffect(project: Project, effect: StripEffect): Project {
  return revise(project, (p) =>
    p.effects.push({
      ...clone(effect),
      id: uid(),
      name: `${effect.name.replace(/ · копия$/, '')} · копия`,
    }),
  );
}
