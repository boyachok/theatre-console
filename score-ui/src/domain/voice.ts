import type { Device } from './schema.ts';
export const voiceAlgorithms = ['ring', 'vocoder', 'autotune', 'monotone', 'chip'] as const;
export type VoiceAlgorithm = (typeof voiceAlgorithms)[number];
export interface VoiceSettings {
  mode: 'dry' | 'robot';
  algorithm: VoiceAlgorithm;
  gain: number;
  volume: number;
  frequency: number;
  mix: number;
  drive: number;
  gateDb: number;
  notch: boolean;
  reduceThumps: boolean;
  breathReduction: number;
  wave: number;
  toneHz: number;
  formant: number;
  clarity: number;
  tuneKey: number;
  tuneScale: number;
  retuneMs: number;
  tuneStrength: number;
  transpose: number;
}
export interface VoicePreset {
  id: string;
  name: string;
  settings: VoiceSettings;
}
export interface VoiceBank {
  selected: string;
  presets: VoicePreset[];
  working?: VoiceSettings;
}
export const defaultVoiceSettings = (): VoiceSettings => ({
  mode: 'dry',
  algorithm: 'ring',
  gain: 8,
  volume: 0.15,
  frequency: 85,
  mix: 1,
  drive: 0,
  gateDb: -70,
  notch: true,
  reduceThumps: false,
  breathReduction: 0,
  wave: 0,
  toneHz: 4500,
  formant: 0,
  clarity: 0.5,
  tuneKey: 0,
  tuneScale: 0,
  retuneMs: 0,
  tuneStrength: 1,
  transpose: 0,
});
export const defaultVoiceBank = (): VoiceBank => ({
  selected: 'voice-default',
  presets: [{ id: 'voice-default', name: 'Голос 1', settings: defaultVoiceSettings() }],
});
export const voiceBank = (d: Device): VoiceBank => d.voice ?? defaultVoiceBank();
export const voiceSettings = (d: Device): VoiceSettings => {
  const b = voiceBank(d);
  return b.working ?? b.presets.find((p) => p.id === b.selected)!.settings;
};
const between = (n: unknown, min: number, max: number) =>
  typeof n === 'number' && Number.isFinite(n) && n >= min && n <= max;
export function validVoiceSettings(s: any): s is VoiceSettings {
  if (
    !s ||
    !['dry', 'robot'].includes(s.mode) ||
    !voiceAlgorithms.includes(s.algorithm) ||
    typeof s.notch !== 'boolean' ||
    typeof s.reduceThumps !== 'boolean'
  )
    return false;
  const ranges: [string, number, number][] = [
    ['gain', 0.25, 64],
    ['volume', 0, 1],
    ['frequency', s.algorithm === 'monotone' ? 65 : 20, s.algorithm === 'chip' ? 1000 : 400],
    ['mix', 0, 1],
    ['drive', 0, 12],
    ['gateDb', -90, -25],
    ['breathReduction', 0, 100],
    ['wave', 0, 1],
    ['formant', -6, 6],
    ['clarity', 0, 1],
    ['tuneKey', 0, 11],
    ['tuneScale', 0, 2],
    ['retuneMs', 0, 250],
    ['tuneStrength', 0, 1],
    ['transpose', -12, 12],
  ];
  return (
    ranges.every(([k, a, b]) => between(s[k], a, b)) &&
    (s.toneHz === 0 || between(s.toneHz, 20, 20000)) &&
    Number.isInteger(s.tuneKey) &&
    Number.isInteger(s.tuneScale) &&
    Number.isInteger(s.breathReduction)
  );
}
export function validVoiceBank(b: any): boolean {
  return (
    !!b &&
    Array.isArray(b.presets) &&
    b.presets.length > 0 &&
    b.presets.length <= 8 &&
    new Set(b.presets.map((p: any) => p.id)).size === b.presets.length &&
    b.presets.some((p: any) => p.id === b.selected) &&
    b.presets.every(
      (p: any) =>
        typeof p.id === 'string' &&
        p.id.length > 0 &&
        p.id.length <= 160 &&
        typeof p.name === 'string' &&
        p.name.trim().length > 0 &&
        p.name.length <= 28 &&
        validVoiceSettings(p.settings),
    ) &&
    (b.working === undefined || validVoiceSettings(b.working))
  );
}
export function voiceWire(d: Device): number[] {
  const s = voiceSettings(d);
  return [
    1,
    Number(s.mode === 'robot'),
    voiceAlgorithms.indexOf(s.algorithm),
    s.tuneKey,
    s.tuneScale,
    Number(s.notch) | (Number(s.reduceThumps) << 1),
    s.breathReduction,
    s.gain,
    s.volume,
    s.frequency,
    s.mix,
    s.drive,
    s.gateDb,
    s.wave,
    s.toneHz,
    s.formant,
    s.clarity,
    s.retuneMs,
    s.tuneStrength,
    s.transpose,
  ];
}
