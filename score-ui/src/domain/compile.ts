import type { Project, Step } from './schema.ts';
import { percentToDuty } from './project.ts';
import { validateProject } from './validation.ts';
export interface WireCue {
  dimMask: number;
  relayMask: number;
  d: number[][];
  r: number[][];
  a: number[];
  stripAction: number;
}
export interface CompiledDevice {
  id: string;
  mac: string;
  cues: WireCue[];
}
const ms = (seconds: number) => Math.round(seconds * 1000);
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
        (device.audio.enabled && c.audio.track !== null) ||
        (device.strip.enabled && c.strip.action === 'play')
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
      if (device.strip.enabled && c.strip.action === 'play')
        throw new Error('Сначала нужно подтвердить тип и подключение ленты');
      return {
        dimMask: device.dimmers.reduce((mask, p, i) => mask | (p.enabled ? 1 << i : 0), 0),
        relayMask: device.relays.reduce((mask, p, i) => mask | (p.enabled ? 1 << i : 0), 0),
        d: device.dimmers.map((p) => [
          percentToDuty(c.dimmers[p.id].value),
          ms(c.dimmers[p.id].fadeIn ?? step.timings.lightIn),
          ms(c.dimmers[p.id].fadeOut ?? step.timings.lightOut),
        ]),
        r: device.relays.map((p) => [
          Number(c.relays[p.id].on),
          p.relayMode ?? 0,
          ms(c.relays[p.id].delayOn ?? step.timings.relayOn),
          ms(c.relays[p.id].delayOff ?? step.timings.relayOff),
        ]),
        a: [
          c.audio.track ?? 0,
          Math.round((c.audio.volume * 30) / 100),
          Number(device.audio.enabled),
          ms(c.audio.fadeIn ?? step.timings.audioIn),
          ms(c.audio.fadeOut ?? step.timings.audioOut),
        ],
        stripAction: device.strip.enabled
          ? c.strip.action === 'play'
            ? 2
            : c.strip.action === 'stop'
              ? 1
              : 0
          : 0,
      };
    });
    result.push({ id: device.id, mac, cues });
  }
  if (!result.length) throw new Error('Назначьте хотя бы один контроллер в настройках прибора');
  return result;
}
