import type { DimmerAnimation, DimmerCue } from './schema.ts';
export const dimmerModes = ['static', 'sine', 'square', 'ramp', 'random'] as const;
export const defaultDimmerAnimation = (): DimmerAnimation => ({
  enabled: false,
  mode: 'sine',
  period: 2,
  phase: 0,
  seed: 1,
  floor: 0,
  duty: 50,
  smooth: 0,
});
export function dimmerAnimationWire(cue: DimmerCue): number[] {
  const a = cue.animation;
  if (!a?.enabled || a.mode === 'static') return [];
  return [
    dimmerModes.indexOf(a.mode),
    Math.round(a.period * 1000),
    a.mode === 'random' ? a.seed : Math.round(a.phase * 10) % 3600,
    Math.round(a.floor),
    Math.round(a.duty),
    Math.round(a.smooth),
  ];
}
export function dimmerHash(seed: number, cycle: number) {
  let x = (seed + Math.imul(cycle, 0x9e3779b9)) >>> 0;
  x = Math.imul(x ^ (x >>> 16), 0x7feb352d);
  x = Math.imul(x ^ (x >>> 15), 0x846ca68b);
  return (x ^ (x >>> 16)) >>> 0;
}
export interface DimmerClock {
  at: number;
  position: number;
  cycle: number;
}
export interface LightRuntime {
  value: number;
  envelope: { from: number; to: number; at: number; duration: number };
  animation?: DimmerAnimation;
  clock?: DimmerClock;
  signature?: string;
}
const periodMs = (a: DimmerAnimation) => Math.round(a.period * 1000);
function advance(clock: DimmerClock, a: DimmerAnimation, now: number): DimmerClock {
  const total = clock.position + Math.max(0, now - clock.at),
    period = periodMs(a);
  return {
    at: now,
    position: total % period,
    cycle: (clock.cycle + Math.floor(total / period)) >>> 0,
  };
}
export function dimmerFactor(a: DimmerAnimation, clock: DimmerClock) {
  const period = periodMs(a),
    t = clock.position / period;
  let wave = 1;
  if (a.mode === 'sine') wave = (1 - Math.cos(2 * Math.PI * t)) / 2;
  else if (a.mode === 'square') {
    let high = Math.floor((period * Math.round(a.duty)) / 100);
    if (a.duty > 0 && a.duty < 100) high = Math.max(1, Math.min(period - 1, high));
    wave = clock.position < high ? 1 : 0;
  } else if (a.mode === 'ramp') {
    const apex = Math.round(a.duty) / 100;
    wave = apex === 0 ? 1 - t : apex === 1 ? t : t < apex ? t / apex : (1 - t) / (1 - apex);
  } else if (a.mode === 'random') {
    const from = dimmerHash(a.seed, clock.cycle) / 4294967295;
    const to = dimmerHash(a.seed, clock.cycle + 1) / 4294967295;
    const smooth = Math.round(a.smooth);
    let mix = smooth ? Math.min(1, (t * 100) / smooth) : 0;
    mix = mix * mix * (3 - 2 * mix);
    wave = from + (to - from) * mix;
  }
  const floor = Math.round(a.floor) / 100;
  return floor + (1 - floor) * wave;
}
export function sampleDimmer(light: LightRuntime, now: number): LightRuntime {
  const clock =
    light.animation && light.clock ? advance(light.clock, light.animation, now) : undefined;
  const factor = light.animation && clock ? dimmerFactor(light.animation, clock) : 1;
  const e = light.envelope,
    blend = e.duration ? Math.min(1, Math.max(0, (now - e.at) / e.duration)) : 1;
  return { ...light, clock, value: e.from * (1 - blend) + e.to * factor * blend };
}
export function retargetDimmer(
  light: LightRuntime,
  cue: DimmerCue,
  enabled: boolean,
  fadeIn: number,
  fadeOut: number,
  now: number,
  restart: boolean,
): LightRuntime {
  const signature = JSON.stringify([
    enabled,
    cue.value,
    fadeIn,
    fadeOut,
    ...dimmerAnimationWire(cue),
  ]);
  if (!restart && light.signature === signature) return light;
  const current = sampleDimmer(light, now),
    a =
      enabled && cue.animation?.enabled && cue.animation.mode !== 'static'
        ? { ...cue.animation }
        : undefined;
  let clock: DimmerClock | undefined;
  if (a) {
    const old = current.animation,
      previous = current.clock;
    if (
      !restart &&
      previous &&
      old?.mode === a.mode &&
      (a.mode === 'random'
        ? old.seed === a.seed
        : Math.round(old.phase * 10) % 3600 === Math.round(a.phase * 10) % 3600)
    )
      clock = {
        ...previous,
        position: Math.floor((previous.position * periodMs(a)) / periodMs(old)),
      };
    else
      clock = {
        at: now,
        cycle: 0,
        position:
          a.mode === 'random'
            ? 0
            : Math.floor((periodMs(a) * (Math.round(a.phase * 10) % 3600)) / 3600),
      };
  }
  const maximum = enabled ? cue.value : 0;
  return sampleDimmer(
    {
      value: current.value,
      signature,
      animation: a,
      clock,
      envelope: {
        from: current.value,
        to: maximum,
        at: now,
        duration: (maximum >= light.envelope.to ? fadeIn : fadeOut) * 1000,
      },
    },
    now,
  );
}
