import type { Project, Workspace } from './schema.ts';
import { fingerprint, upgradeDraft } from './project.ts';
import { validVoiceBank } from './voice.ts';
import { dimmerModes } from './dimmer.ts';
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
  const order = (x: unknown, max: number) =>
    Array.isArray(x) &&
    x.length <= max &&
    x.every((k) => typeof k === 'string' && k.length > 0 && k.length <= 400) &&
    new Set(x).size === x.length;
  ensure(
    value.layout === undefined ||
      (object(value.layout) &&
        ['devices', 'custom'].includes(value.layout.mode) &&
        order(value.layout.order, 200)),
    'Неверная раскладка строк',
  );
  for (const device of value.devices) {
    ensure(object(device) && text(device.id) && text(device.name), 'Неверный прибор');
    ensure(
      device.rowOrder === undefined || order(device.rowOrder, 17),
      'Неверный порядок параметров',
    );
    ensure(
      device.profile === undefined || ['standard', 'helmet'].includes(device.profile),
      'Неверный тип прибора',
    );
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
    ensure(device.voice === undefined || validVoiceBank(device.voice), 'Неверные пресеты голоса');
    const parameters = [
      ...device.dimmers,
      ...device.relays,
      device.audio,
      device.strip,
      ...(device.acDimmer === undefined ? [] : [device.acDimmer]),
    ];
    for (const p of parameters)
      ensure(
        object(p) &&
          text(p.id) &&
          text(p.name) &&
          ['red', 'green', 'blue', 'white', 'warm'].includes(p.color) &&
          typeof p.enabled === 'boolean' &&
          (p.inverted === undefined || typeof p.inverted === 'boolean') &&
          (p.relayMode === undefined || [0, 1, 2].includes(p.relayMode)),
        'Неверный параметр прибора',
      );
    unique(parameters);
    if (device.profile === 'helmet')
      ensure(
        [...device.dimmers, ...device.relays, ...(device.acDimmer ? [device.acDimmer] : [])].every(
          (p: any) => !p.enabled,
        ),
        'Helmet не поддерживает диммеры и реле',
      );
    ensure(
      Number.isInteger(device.ledCount) && device.ledCount > 0 && device.ledCount <= 10000,
      'Неверная длина ленты',
    );
  }
  unique(value.devices);
  unique(value.steps);
  unique(value.effects);
  const snapshots: unknown[] = [];
  for (const step of value.steps)
    if (object(step) && object(step.devices))
      for (const c of Object.values(step.devices))
        if (object(c) && object(c.strip) && c.strip.settings !== undefined)
          snapshots.push(c.strip.settings);
  for (const fx of [...value.effects, ...snapshots])
    ensure(
      object(fx) &&
        text(fx.id) &&
        text(fx.name) &&
        ['wave', 'fill', 'static', 'waiting', 'disco', 'alarm', 'loading'].includes(fx.mode) &&
        /^#[0-9a-f]{6}$/i.test(fx.color) &&
        /^#[0-9a-f]{6}$/i.test(fx.background) &&
        number(fx.brightness, 100) &&
        number(fx.width, 100) &&
        fx.width > 0 &&
        number(fx.duration) &&
        fx.duration > 0 &&
        ['forward', 'reverse'].includes(fx.direction) &&
        ['density', 'sparkles', 'noise', 'sensitivity', 'backgroundBrightness'].every(
          (k) => fx[k] === undefined || number(fx[k], 100),
        ) &&
        (fx.palette === undefined || [0, 1, 2].includes(fx.palette)) &&
        (fx.audioReactive === undefined || typeof fx.audioReactive === 'boolean') &&
        (fx.backgroundEnabled === undefined || typeof fx.backgroundEnabled === 'boolean') &&
        (fx.release === undefined || number(fx.release)) &&
        (fx.seed === undefined || (Number.isInteger(fx.seed) && number(fx.seed, 0xffffffff))),
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
      for (const c of [
        ...device.dimmers.map((p: any) => cue.dimmers[p.id]),
        ...(cue.ac === undefined ? [] : [cue.ac]),
      ]) {
        ensure(
          object(c) &&
            number(c.value, 100) &&
            optional(c.fadeIn) &&
            optional(c.fadeOut) &&
            (c.startDelay === undefined || number(c.startDelay)),
          'Неверная яркость/фейд',
        );
        if (c.animation !== undefined) {
          const a = c.animation;
          ensure(
            object(a) &&
              typeof a.enabled === 'boolean' &&
              dimmerModes.includes(a.mode) &&
              number(a.period) &&
              a.period >= (c === cue.ac ? 0.04 : 0.02) &&
              number(a.phase, 360) &&
              Number.isInteger(a.seed) &&
              number(a.seed, 0xffffffff) &&
              number(a.floor, 100) &&
              number(a.duty, 100) &&
              number(a.smooth, 100),
            'Неверная анимация диммера (период 0.02–3600с, seed uint32)',
          );
        }
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
        (a.loop === undefined || typeof a.loop === 'boolean') &&
          (a.startDelay === undefined || number(a.startDelay)) &&
          (a.voiceOn === undefined || typeof a.voiceOn === 'boolean') &&
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
  if (value.backup !== undefined) {
    const b = value.backup;
    ensure(
      object(b) &&
        Number.isSafeInteger(b.major) &&
        b.major >= 1 &&
        b.major <= 999999 &&
        Number.isSafeInteger(b.minor) &&
        b.minor >= 0 &&
        b.minor < Number.MAX_SAFE_INTEGER &&
        (b.exportedFingerprint === undefined || text(b.exportedFingerprint)) &&
        (b.exportedAt === undefined ||
          (typeof b.exportedAt === 'string' && Number.isFinite(Date.parse(b.exportedAt)))),
      'Неверная версия JSON-бэкапа',
    );
  }
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
