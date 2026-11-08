import type { Project, Step } from './schema.ts';
import { percentToDuty, effectiveStrip } from './project.ts';
import { validateProject } from './validation.ts';
import { dimmerAnimationWire } from './dimmer.ts';
export interface WireCue {
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
        fx = strip.action === 'play' ? project.effects.find((f) => f.id === strip.effectId) : null;
      if (device.strip.enabled && device.ledCount > 1024)
        throw new Error('WS2811: максимум 1024 управляемые точки');
      const action = device.strip.enabled && fx ? 2 : 1,
        enabled = Number(device.strip.enabled);
      return {
        dimMask: device.dimmers.reduce((mask, p, i) => mask | (p.enabled ? 1 << i : 0), 0),
        relayMask: device.relays.reduce((mask, p, i) => mask | (p.enabled ? 1 << i : 0), 0),
        d: Array.from({ length: 12 }, (_, i) => {
          const p = device.dimmers[i];
          return p && device.profile !== 'helmet'
            ? [
                percentToDuty(c.dimmers[p.id].value),
                ms(c.dimmers[p.id].fadeIn ?? step.timings.lightIn),
                ms(c.dimmers[p.id].fadeOut ?? step.timings.lightOut),
                ...dimmerAnimationWire(c.dimmers[p.id]),
              ]
            : [0, 0, 0];
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
                ...(ms(c.audio.startDelay ?? 0) ? [ms(c.audio.startDelay ?? 0)] : []),
              ],
        stripAction: action,
        s: [
          enabled,
          action,
          fx ? ['wave', 'fill', 'static'].indexOf(fx.mode) : 0,
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
