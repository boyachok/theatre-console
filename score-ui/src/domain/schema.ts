export type ChannelColor = 'red' | 'green' | 'blue' | 'white' | 'warm';
export type ParameterKind = 'dimmer' | 'relay' | 'audio' | 'strip';
export interface Parameter {
  id: string;
  name: string;
  color: ChannelColor;
  enabled: boolean;
  relayMode?: 0 | 1 | 2;
}
export interface Device {
  id: string;
  name: string;
  ip?: string | null;
  mac?: string | null;
  dimmers: Parameter[];
  relays: Parameter[];
  audio: Parameter;
  strip: Parameter;
  ledCount: number;
}
export interface Timings {
  lightIn: number;
  lightOut: number;
  audioIn: number;
  audioOut: number;
  relayOn: number;
  relayOff: number;
}
export interface DimmerCue {
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
export interface Workspace {
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
  red: { name: 'Красный', hex: '#f17b83' },
  green: { name: 'Зелёный', hex: '#7bd8ae' },
  blue: { name: 'Синий', hex: '#88aaff' },
  white: { name: 'Белый', hex: '#e5e8f1' },
  warm: { name: 'Тёплый', hex: '#edc18d' },
};
