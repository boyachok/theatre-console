import type { Project, Workspace } from './schema.ts';
import { fingerprint, upgradeDraft } from './project.ts';
const object = (v: unknown): v is Record<string, any> =>
  !!v && typeof v === 'object' && !Array.isArray(v);
function ensure(ok: unknown, message: string): asserts ok {
  if (!ok) throw new Error(message);
}
const text = (v: unknown) => typeof v === 'string' && v.trim().length > 0 && v.length <= 160;
const number = (v: unknown, max = 3600) =>
  typeof v === 'number' && Number.isFinite(v) && v >= 0 && v <= max;
const optional = (v: unknown) => v === null || number(v);
function unique(items: any[]) {
  ensure(
    items.every((v) => object(v) && text(v.id)),
    'Неверные идентификаторы',
  );
  ensure(new Set(items.map((v) => v.id)).size === items.length, 'Повторяющиеся идентификаторы');
}
export function validateProject(value: unknown): asserts value is Project {
  ensure(
    object(value) &&
      text(value.id) &&
      text(value.name) &&
      number(value.revision, Number.MAX_SAFE_INTEGER),
    'Неверный проект',
  );
  ensure(
    typeof value.updatedAt === 'string' && Number.isFinite(Date.parse(value.updatedAt)),
    'Неверная дата проекта',
  );
  ensure(
    Array.isArray(value.devices) && value.devices.length >= 1 && value.devices.length <= 10,
    'Нужно от 1 до 10 приборов',
  );
  ensure(Array.isArray(value.steps) && value.steps.length >= 1, 'Нужен хотя бы один шаг');
  ensure(Array.isArray(value.effects), 'Неверный список эффектов');
  for (const device of value.devices) {
    ensure(object(device) && text(device.id) && text(device.name), 'Неверный прибор');
    ensure(
      Array.isArray(device.dimmers) &&
        [8, 12].includes(device.dimmers.length) &&
        Array.isArray(device.relays) &&
        device.relays.length === 2,
      'Ожидаются 12 диммеров и 2 реле (либо старый файл с 8)',
    );
    ensure(
      device.ip === undefined ||
        device.ip === null ||
        (typeof device.ip === 'string' &&
          /^(?:\d{1,3}\.){3}\d{1,3}$/.test(device.ip) &&
          device.ip.split('.').every((part: string) => Number(part) <= 255)),
      'Неверный IP-адрес',
    );
    ensure(
      device.mac == null ||
        (typeof device.mac === 'string' && /^(?:[0-9a-f]{2}:){5}[0-9a-f]{2}$/i.test(device.mac)),
      'Неверная MAC',
    );
    ensure(
      device.stripOrder === undefined || ['RGB', 'GRB'].includes(device.stripOrder),
      'Неверный порядок цветов',
    );
    ensure(
      device.stripRate === undefined || [800, 400].includes(device.stripRate),
      'Неверная частота ленты',
    );
    const parameters = [...device.dimmers, ...device.relays, device.audio, device.strip];
    for (const p of parameters)
      ensure(
        object(p) &&
          text(p.id) &&
          text(p.name) &&
          ['red', 'green', 'blue', 'white', 'warm'].includes(p.color) &&
          typeof p.enabled === 'boolean' &&
          (p.relayMode === undefined || [0, 1, 2].includes(p.relayMode)),
        'Неверный параметр прибора',
      );
    unique(parameters);
    ensure(
      Number.isInteger(device.ledCount) && device.ledCount > 0 && device.ledCount <= 10000,
      'Неверная длина ленты',
    );
  }
  unique(value.devices);
  unique(value.steps);
  unique(value.effects);
  for (const fx of value.effects)
    ensure(
      object(fx) &&
        text(fx.id) &&
        text(fx.name) &&
        ['wave', 'fill', 'static'].includes(fx.mode) &&
        /^#[0-9a-f]{6}$/i.test(fx.color) &&
        /^#[0-9a-f]{6}$/i.test(fx.background) &&
        number(fx.brightness, 100) &&
        number(fx.width, 100) &&
        fx.width > 0 &&
        number(fx.duration) &&
        fx.duration > 0 &&
        ['forward', 'reverse'].includes(fx.direction),
      'Неверный эффект ленты',
    );
  for (const step of value.steps) {
    ensure(
      object(step) &&
        text(step.id) &&
        text(step.name) &&
        object(step.timings) &&
        object(step.devices),
      'Неверный шаг',
    );
    for (const key of ['lightIn', 'lightOut', 'audioIn', 'audioOut', 'relayOn', 'relayOff'])
      ensure(number(step.timings[key]), 'Неверное время перехода');
    for (const device of value.devices) {
      const cue = step.devices[device.id];
      ensure(
        object(cue) &&
          object(cue.dimmers) &&
          object(cue.relays) &&
          object(cue.audio) &&
          object(cue.strip),
        'В шаге нет данных прибора',
      );
      for (const p of device.dimmers) {
        const c = cue.dimmers[p.id];
        ensure(
          object(c) && number(c.value, 100) && optional(c.fadeIn) && optional(c.fadeOut),
          'Неверная яркость/фейд',
        );
      }
      for (const p of device.relays) {
        const c = cue.relays[p.id];
        ensure(
          object(c) && typeof c.on === 'boolean' && optional(c.delayOn) && optional(c.delayOff),
          'Неверные параметры реле',
        );
      }
      const a = cue.audio;
      ensure(
        (a.track === null || (Number.isInteger(a.track) && a.track > 0 && a.track <= 65535)) &&
          (a.lastTrack === undefined ||
            (Number.isInteger(a.lastTrack) && a.lastTrack > 0 && a.lastTrack <= 65535)) &&
          number(a.volume, 100) &&
          optional(a.fadeIn) &&
          optional(a.fadeOut),
        'Неверные параметры звука',
      );
      ensure(
        ['hold', 'stop', 'play'].includes(cue.strip.action) &&
          (cue.strip.action !== 'play' ||
            value.effects.some((f: any) => f.id === cue.strip.effectId)),
        'Не найден эффект ленты',
      );
    }
  }
}
export function parseBackup(content: string): Workspace {
  const value: unknown = JSON.parse(content);
  ensure(
    object(value) &&
      value.format === 'theatre-score' &&
      value.schemaVersion === 1 &&
      typeof value.interfaceVersion === 'string',
    'Неподдерживаемый формат партитуры',
  );
  validateProject(value.draft);
  if (value.published !== null) {
    ensure(
      object(value.published) &&
        Number.isInteger(value.published.version) &&
        value.published.version > 0 &&
        ['local', 'esp'].includes(value.published.mode) &&
        text(value.published.fingerprint),
      'Неверная опубликованная версия',
    );
    validateProject(value.published.project);
    ensure(
      value.published.fingerprint === fingerprint(value.published.project) &&
        typeof value.published.createdAt === 'string' &&
        Number.isFinite(Date.parse(value.published.createdAt)),
      'Опубликованная версия повреждена',
    );
  }
  return { ...value, draft: upgradeDraft(value.draft) } as unknown as Workspace;
}
