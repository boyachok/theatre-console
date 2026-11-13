// Authored cycles only. Historical JSON remains readable; fades are NOT limited to15s.
export const EFFECT_MIN_SECONDS = 1 / 15;
export const EFFECT_MAX_SECONDS = 15;
export const effectPeriod = (seconds: number) =>
  Math.max(EFFECT_MIN_SECONDS, Math.min(EFFECT_MAX_SECONDS, seconds));
