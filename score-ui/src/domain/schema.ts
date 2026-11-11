export type ChannelColor = 'red' | 'green' | 'blue' | 'white' | 'warm';
export type ParameterKind = 'dimmer' | 'relay' | 'audio' | 'strip' | 'ac';
export interface Parameter {
  inverted?: boolean;
  id: string;
  name: string;
  color: ChannelColor;
  enabled: boolean;
  relayMode?: 0 | 1 | 2;
}
export interface Device {
  profile?: 'standard' | 'helmet';
  id: string;
  name: string;
  ip?: string | null;
  mac?: string | null;
  acDimmer?: Parameter;
  dimmers: Parameter[];
  relays: Parameter[];
  audio: Parameter;
  strip: Parameter;
  ledCount: number;
  stripOrder?: 'RGB' | 'GRB';
  stripRate?: 800 | 400;
}
export interface Timings {
  lightIn: number;
  lightOut: number;
  audioIn: number;
  audioOut: number;
  relayOn: number;
  relayOff: number;
}
export type DimmerMode = 'static' | 'sine' | 'square' | 'ramp' | 'random';
export interface DimmerAnimation {
  enabled: boolean;
  mode: DimmerMode;
  period: number; // seconds; 0.02–3600
  phase: number; // degrees; random uses seed instead
  seed: number;
  floor: number; // percent of peak brightness, not absolute output
  duty: number; // square on-time / ramp apex, percent
  smooth: number; // random transition fraction, percent
}
export interface DimmerCue {
  startDelay?: number;
  animation?: DimmerAnimation;
  value: number;
  fadeIn: number | null;
  fadeOut: number | null;
}
export interface RelayCue {
  on: boolean;
  delayOn: number | null;
  delayOff: number | null;
}
export interface AudioCue {
  loop?: boolean;
  startDelay?: number; // seconds, per device/step; absent means zero
  voiceOn?: boolean;
  lastTrack?: number;
  track: number | null;
  volume: number;
  fadeIn: number | null;
  fadeOut: number | null;
}
export interface StripCue {
  action: 'hold' | 'stop' | 'play';
  effectId: string | null;
}
export interface DeviceCue {
  ac?: DimmerCue;
  dimmers: Record<string, DimmerCue>;
  relays: Record<string, RelayCue>;
  audio: AudioCue;
  strip: StripCue;
}
export interface Step {
  id: string;
  name: string;
  timings: Timings;
  devices: Record<string, DeviceCue>;
}
export interface StripEffect {
  id: string;
  name: string;
  mode: 'wave' | 'fill' | 'static';
  color: string;
  background: string;
  brightness: number;
  width: number;
  duration: number;
  direction: 'forward' | 'reverse';
}
export interface Project {
  id: string;
  name: string;
  revision: number;
  updatedAt: string;
  devices: Device[];
  steps: Step[];
  effects: StripEffect[];
}
export interface PublishedScore {
  version: number;
  fingerprint: string;
  createdAt: string;
  project: Project;
  mode: 'local' | 'esp';
}
export interface BackupInfo {
  major: number;
  minor: number;
  exportedFingerprint?: string;
  exportedAt?: string;
}
export interface Workspace {
  backup?: BackupInfo;
  format: 'theatre-score';
  schemaVersion: 1;
  interfaceVersion: string;
  draft: Project;
  published: PublishedScore | null;
}
export interface CellSelection {
  stepId: string;
  deviceId: string;
  kind: ParameterKind;
  parameterId: string;
}
export interface Column {
  device: Device;
  parameter: Parameter;
  kind: ParameterKind;
}
export const COLORS: Record<ChannelColor, { name: string; hex: string }> = {
  red: { name: 'Красный', hex: '#c52135' },
  green: { name: 'Зелёный', hex: '#08784b' },
  blue: { name: 'Синий', hex: '#1b55d9' },
  white: { name: 'Белый', hex: '#4b5d75' },
  warm: { name: 'Тёплый', hex: '#986006' },
};
