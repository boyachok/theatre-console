import type { StripEffect } from './schema.ts';
export const stripModes = [
  'wave',
  'fill',
  'static',
  'waiting',
  'disco',
  'alarm',
  'loading',
] as const;
export const stripLabels = [
  'Волна',
  'Заполнение',
  'Статика',
  'Ожидание',
  'Дискотека',
  'Тревога',
  'Загрузка',
];
export const hashStrip = (v: number) => {
  v ^= v >>> 16;
  v = Math.imul(v, 0x7feb352d);
  v ^= v >>> 15;
  v = Math.imul(v, 0x846ca68b);
  return (v ^ (v >>> 16)) >>> 0;
};
const fract = (n: number) => n - Math.floor(n),
  clamp = (n: number) => Math.max(0, Math.min(1, n));
const rgb = (hex: string) => [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16) / 255);
export function stripColor(
  e: StripEffect,
  index: number,
  count: number,
  elapsed: number,
  gate = 1,
) {
  let [r, g, b] = rgb(e.color),
    v = 0,
    phase = elapsed / e.duration,
    part = fract(phase),
    p = count > 1 ? index / (count - 1) : 0.5,
    width = Math.max(1, (count * e.width) / 100);
  if (e.mode === 'static') v = 1;
  else if (e.mode === 'fill')
    v =
      (e.direction === 'reverse' ? count - 1 - index : index) < Math.min(1, phase) * count ? 1 : 0;
  else if (e.mode === 'waiting')
    v = Math.max(
      0,
      1 - Math.abs(index - (1 - Math.abs(2 * part - 1)) * (count - 1)) / Math.max(1, width * 0.5),
    );
  else if (e.mode === 'disco') {
    let h = p + phase * 0.2;
    if (e.palette === 1) h = 0.02 + fract(h) * 0.13;
    else if (e.palette === 2) h = 0.45 + fract(h) * 0.3;
    h = fract(h) * 6;
    r = clamp(Math.abs(h - 3) - 1);
    g = clamp(2 - Math.abs(h - 2));
    b = clamp(2 - Math.abs(h - 4));
    v = 0.25 + 0.65 * (1 - Math.abs(2 * fract(p * (1 + (e.density ?? 50) / 20) - phase) - 1));
    if (
      (hashStrip(index + (e.seed ?? 1) + Math.floor((elapsed * 1000) / 80) * 1021) & 65535) /
        65535 <
      (e.sparkles ?? 25) / 400
    ) {
      v = 1;
      r = g = b = 1;
    }
  } else if (e.mode === 'alarm') {
    r = 1;
    g = b = 0;
    const noise = e.noise ?? 30;
    v =
      part <
      0.35 + (((hashStrip(Math.floor(phase) + (e.seed ?? 1)) & 255) / 255 - 0.5) * noise) / 200
        ? 1
        : 0;
    v *=
      1 -
      ((noise / 100) *
        (hashStrip(index + (e.seed ?? 1) + Math.floor((elapsed * 1000) / 33) * 1021) & 65535)) /
        65535;
  } else if (e.mode === 'loading')
    v = Math.max(
      0,
      1 -
        fract(
          (count > 1 ? Math.abs(index * 2 - (count - 1)) / (count - 1) : 0) *
            (1 + (e.density ?? 50) / 10) -
            phase,
        ) /
          Math.max(0.01, e.width / 100),
    );
  else {
    let center = part * count;
    if (e.direction === 'reverse') center = count - center;
    let d = Math.abs(index - center);
    d = Math.min(d, count - d);
    v = Math.max(0, 1 - d / (width * 0.5));
  }
  const bg = rgb(e.background),
    bgGain = e.backgroundEnabled === false ? 0 : (e.backgroundBrightness ?? 100) / 100,
    a = clamp(v * gate);
  return [r, g, b].map((c, k) =>
    Math.floor((bg[k] * bgGain * (1 - a) + c * a) * e.brightness * 2.55 + 1e-7),
  );
}
