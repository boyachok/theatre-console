import type { Project, Step, DimmerCue } from './schema.ts';
import { percentToDuty, effectiveStrip, acParameter, acCue } from './project.ts';
import { validateProject } from './validation.ts';
import { dimmerAnimationWire } from './dimmer.ts';
import { voiceWire } from './voice.ts';
export interface WireCue {
  v?: number[];
  x?: number[];
  invertedMask?: number;
  inversionSet?: number;
  ac?: { enabled: number; d: number[] };
  dimMask: number;
  relayMask: number;
  d: number[][];
  r: number[][];
  a: number[];
  stripAction: number;
  s: number[];
}
export interface CompiledDevice {
  id: string;
  mac: string;
  cues: WireCue[];
}
const ms = (seconds: number) => Math.round(seconds * 1000);
const lightWire = (c: DimmerCue, step: Step) => [
  percentToDuty(c.value),
  ms(c.fadeIn ?? step.timings.lightIn),
  ms(c.fadeOut ?? step.timings.lightOut),
  ...dimmerAnimationWire(c),
  ...(ms(c.startDelay ?? 0) ? [ms(c.startDelay ?? 0)] : []),
];
const rgb = (hex: string) => [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16));
function programKey(value: unknown) {
  let h = 2166136261;
  for (const c of JSON.stringify(value)) {
    h ^= c.charCodeAt(0);
    h = Math.imul(h, 16777619);
  }
  return h >>> 0;
}
export function compileDevices(project: Project, steps: Step[]): CompiledDevice[] {
  validateProject(project);
  const result: CompiledDevice[] = [],
    used = new Set<string>();
  for (const device of project.devices) {
    const active = steps.some((step) => {
      const c = step.devices[device.id];
      return (
        device.dimmers.some((p) => p.enabled && c.dimmers[p.id].value > 0) ||
        (device.profile !== 'helmet' && acParameter(device).enabled && acCue(c).value > 0) ||
        device.relays.some((p) => p.enabled && c.relays[p.id].on) ||
        (device.audio.enabled &&
          (device.profile === 'helmet' ? !!c.audio.voiceOn : c.audio.track !== null)) ||
        (device.strip.enabled && effectiveStrip(project, step, device.id).action === 'play')
      );
    });
    if (!device.mac) {
      if (active) throw new Error(`Назначьте контроллер прибору «${device.name}»`);
      continue;
    }
    const mac = device.mac.toUpperCase();
    if (used.has(mac)) throw new Error('Контроллер назначен двум приборам');
    used.add(mac);
    const cues = steps.map((step) => {
      const c = step.devices[device.id];
      for (const p of device.relays)
        if (p.enabled && c.relays[p.id].on && !p.relayMode)
          throw new Error(`Выберите HIGH/LOW для ${device.name} · ${p.name}`);
      const strip = effectiveStrip(project, step, device.id),
        fx =
          strip.action === 'play'
            ? (strip.settings ?? project.effects.find((f) => f.id === strip.effectId))
            : null;
      if (device.strip.enabled && device.ledCount > 1024)
        throw new Error('WS2811: максимум 1024 управляемые точки');
      const action = device.strip.enabled && fx ? 2 : 1,
        enabled = Number(device.strip.enabled);
      return {
        ...(device.profile === 'helmet' ? { v: voiceWire(device) } : {}),
        x: [
          Math.round(fx?.density ?? 50),
          Math.round(fx?.sparkles ?? 25),
          Math.round(fx?.noise ?? 30),
          fx?.palette ?? 0,
          Number(!!fx?.audioReactive && device.profile === 'helmet'),
          Number(fx?.backgroundEnabled ?? true),
          Math.round(fx?.backgroundBrightness ?? 100),
          Math.round(fx?.sensitivity ?? 50),
          ms(fx?.release ?? 0.4),
          fx?.seed ?? 1,
        ],
        ...(device.profile !== 'helmet' && acParameter(device).enabled
          ? { ac: { enabled: 1, d: lightWire(acCue(c), step) } }
          : {}),
        ...(device.profile !== 'helmet' && device.dimmers.some((p) => p.inverted !== undefined)
          ? {
              inversionSet: device.dimmers.reduce(
                (m, p, i) => m | (p.inverted !== undefined ? 1 << i : 0),
                0,
              ),
              invertedMask: device.dimmers.reduce((m, p, i) => m | (p.inverted ? 1 << i : 0), 0),
            }
          : {}),
        dimMask: device.dimmers.reduce((mask, p, i) => mask | (p.enabled ? 1 << i : 0), 0),
        relayMask: device.relays.reduce((mask, p, i) => mask | (p.enabled ? 1 << i : 0), 0),
        d: Array.from({ length: 12 }, (_, i) => {
          const p = device.dimmers[i];
          return p && device.profile !== 'helmet' ? lightWire(c.dimmers[p.id], step) : [0, 0, 0];
        }),
        r: device.relays.map((p) =>
          device.profile === 'helmet'
            ? [0, 0, 0, 0]
            : [
                Number(c.relays[p.id].on),
                p.relayMode ?? 0,
                ms(c.relays[p.id].delayOn ?? step.timings.relayOn),
                ms(c.relays[p.id].delayOff ?? step.timings.relayOff),
              ],
        ),
        a:
          device.profile === 'helmet'
            ? [0, 0, Number(device.audio.enabled && !!c.audio.voiceOn), 0, 0]
            : [
                c.audio.track ?? 0,
                Math.round((c.audio.volume * 30) / 100),
                Number(device.audio.enabled),
                ms(c.audio.fadeIn ?? step.timings.audioIn),
                ms(c.audio.fadeOut ?? step.timings.audioOut),
                ...(ms(c.audio.startDelay ?? 0) || c.audio.loop
                  ? [ms(c.audio.startDelay ?? 0)]
                  : []),
                ...(c.audio.loop ? [1] : []),
              ],
        stripAction: action,
        s: [
          enabled,
          action,
          fx
            ? ['wave', 'fill', 'static', 'waiting', 'disco', 'alarm', 'loading'].indexOf(fx.mode)
            : 0,
          enabled ? device.ledCount : 0,
          ...rgb(fx?.color ?? '#000000'),
          ...rgb(fx?.background ?? '#000000'),
          Math.round(fx?.brightness ?? 0),
          Math.max(1, Math.round(fx?.width ?? 100)),
          Number(fx?.direction === 'reverse'),
          Math.max(1, ms(fx?.duration ?? 1)),
          fx
            ? programKey([
                fx.id,
                fx.mode,
                fx.color,
                fx.background,
                fx.brightness,
                fx.width,
                fx.duration,
                fx.direction,
                fx.density,
                fx.sparkles,
                fx.noise,
                fx.palette,
                fx.audioReactive,
                fx.release,
                fx.backgroundEnabled,
                fx.backgroundBrightness,
                fx.sensitivity,
                fx.seed,
              ])
            : 0,
          Number(device.stripOrder === 'GRB'),
          Number(device.stripRate === 400),
        ],
      };
    });
    result.push({ id: device.id, mac, cues });
  }
  if (!result.length) throw new Error('Назначьте хотя бы один контроллер в настройках прибора');
  return result;
}
