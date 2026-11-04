import type { Project, Step, StripEffect } from './schema.ts';
import { clone, effectiveStrip } from './project.ts';
export interface Envelope {
  from: number;
  to: number;
  at: number;
  duration: number;
}
export interface RelayRuntime {
  on: boolean;
  target: boolean;
  due: number | null;
}
export interface AudioRuntime {
  track: number | null;
  targetTrack: number | null;
  status: 'playing' | 'paused' | 'stopped';
  volume: number;
  envelope: Envelope;
  stopAt: number | null;
  generation: number;
}
export interface DeviceRuntime {
  dimmers: Record<string, { value: number; envelope: Envelope }>;
  relays: Record<string, RelayRuntime>;
  audio: AudioRuntime;
  strip: { effect: StripEffect; startedAt: number } | null;
}
export interface PlaybackSnapshot {
  stepId: string | null;
  transitionId: number;
  devices: Record<string, DeviceRuntime>;
}
export type TransitionReason = 'forward' | 'back' | 'first' | 'last' | 'select' | 'recover';
export const envelope = (from: number, to: number, seconds: number, now: number): Envelope => ({
  from,
  to,
  at: now,
  duration: Math.max(0, seconds) * 1000,
});
export function sample(e: Envelope, now: number) {
  return e.duration === 0
    ? e.to
    : e.from + (e.to - e.from) * Math.min(1, Math.max(0, (now - e.at) / e.duration));
}
function initial(project: Project): PlaybackSnapshot {
  return {
    stepId: null,
    transitionId: 0,
    devices: Object.fromEntries(
      project.devices.map((d) => [
        d.id,
        {
          dimmers: Object.fromEntries(
            d.dimmers.map((p) => [p.id, { value: 0, envelope: envelope(0, 0, 0, 0) }]),
          ),
          relays: Object.fromEntries(
            d.relays.map((p) => [p.id, { on: false, target: false, due: null }]),
          ),
          audio: {
            track: null,
            targetTrack: null,
            status: 'stopped' as const,
            volume: 0,
            envelope: envelope(0, 0, 0, 0),
            stopAt: null,
            generation: 0,
          },
          strip: null,
        },
      ]),
    ),
  };
}
export class PreviewEngine {
  private state: PlaybackSnapshot;
  private listeners = new Set<() => void>();
  constructor(project: Project) {
    this.state = initial(project);
  }
  getSnapshot = () => this.state;
  subscribe = (listener: () => void) => {
    this.listeners.add(listener);
    return () => {
      this.listeners.delete(listener);
    };
  };
  private emit() {
    for (const listener of this.listeners) listener();
  }
  reset(project: Project) {
    this.state = initial(project);
    this.emit();
  }
  apply(
    project: Project,
    step: Step,
    reason: TransitionReason,
    transitionId: number,
    now = performance.now(),
  ): boolean {
    if (transitionId <= this.state.transitionId) return false;
    this.tick(now);
    const state = clone(this.state),
      restart = reason === 'back' || reason === 'first' || reason === 'recover';
    const defaults = initial(project).devices;
    state.devices = Object.fromEntries(
      project.devices.map((device) => [device.id, state.devices[device.id] ?? defaults[device.id]]),
    );
    for (const device of project.devices) {
      const runtime = state.devices[device.id],
        cue = step.devices[device.id];
      if (!runtime || !cue) continue;
      for (const p of device.dimmers.filter((p) => p.enabled)) {
        const light = runtime.dimmers[p.id],
          target = cue.dimmers[p.id];
        const duration =
          target.value >= light.value
            ? (target.fadeIn ?? step.timings.lightIn)
            : (target.fadeOut ?? step.timings.lightOut);
        light.envelope = envelope(light.value, target.value, duration, now);
        light.value = sample(light.envelope, now);
      }
      for (const p of device.relays.filter((p) => p.enabled)) {
        const relay = runtime.relays[p.id],
          target = cue.relays[p.id];
        const delay = target.on
          ? (target.delayOn ?? step.timings.relayOn)
          : (target.delayOff ?? step.timings.relayOff);
        relay.target = target.on;
        relay.due = relay.on === relay.target ? null : now + delay * 1000;
        if (relay.due !== null && relay.due <= now) {
          relay.on = relay.target;
          relay.due = null;
        }
      }
      if (device.audio.enabled) {
        const audio = runtime.audio,
          target = cue.audio;
        const fadeIn = target.fadeIn ?? step.timings.audioIn,
          fadeOut = target.fadeOut ?? step.timings.audioOut;
        const continues = !restart && target.track !== null && target.track === audio.targetTrack;
        audio.targetTrack = target.track;
        audio.stopAt = null;
        if (target.track === null) {
          audio.envelope = envelope(audio.volume, 0, fadeOut, now);
          audio.stopAt = now + fadeOut * 1000;
        } else if (continues) {
          audio.envelope = envelope(
            audio.volume,
            target.volume,
            target.volume >= audio.volume ? fadeIn : fadeOut,
            now,
          );
        } else {
          audio.track = target.track;
          audio.status = 'playing';
          audio.generation++;
          audio.envelope = envelope(0, target.volume, fadeIn, now);
        }
        audio.volume = sample(audio.envelope, now);
        if (audio.stopAt !== null && audio.stopAt <= now) {
          audio.track = null;
          audio.status = 'stopped';
          audio.stopAt = null;
        }
      }
      if (device.strip.enabled) {
        const strip = effectiveStrip(project, step, device.id);
        if (strip.action === 'stop') runtime.strip = null;
        if (strip.action === 'play') {
          const effect = project.effects.find((e) => e.id === strip.effectId);
          if (effect) {
            const previous = runtime.strip;
            const changed =
              !previous ||
              previous.effect.id !== effect.id ||
              (
                [
                  'mode',
                  'color',
                  'background',
                  'brightness',
                  'width',
                  'duration',
                  'direction',
                ] as const
              ).some((key) => previous.effect[key] !== effect[key]);
            runtime.strip = {
              effect: clone(effect),
              startedAt: restart || changed ? now : previous!.startedAt,
            };
          }
        }
      } else runtime.strip = null;
    }
    state.stepId = step.id;
    state.transitionId = transitionId;
    this.state = state;
    this.emit();
    return true;
  }
  tick(now = performance.now()) {
    let changed = false;
    const devices = Object.fromEntries(
      Object.entries(this.state.devices).map(([id, previous]) => {
        const d: DeviceRuntime = {
          ...previous,
          dimmers: { ...previous.dimmers },
          relays: { ...previous.relays },
          audio: { ...previous.audio },
        };
        for (const [key, light] of Object.entries(previous.dimmers)) {
          const value = sample(light.envelope, now);
          if (value !== light.value) {
            changed = true;
            d.dimmers[key] = { ...light, value };
          }
        }
        for (const [key, relay] of Object.entries(previous.relays))
          if (relay.due !== null && now >= relay.due) {
            changed = true;
            d.relays[key] = { ...relay, on: relay.target, due: null };
          }
        const volume = sample(d.audio.envelope, now);
        if (volume !== d.audio.volume) {
          changed = true;
          d.audio.volume = volume;
        }
        if (d.audio.stopAt !== null && now >= d.audio.stopAt) {
          changed = true;
          d.audio.track = null;
          d.audio.status = 'stopped';
          d.audio.stopAt = null;
        }
        return [id, d];
      }),
    );
    if (changed) {
      this.state = { ...this.state, devices };
      this.emit();
    }
  }
  trackEnded(deviceId: string) {
    const device = this.state.devices[deviceId];
    if (!device) return;
    this.state = {
      ...this.state,
      devices: {
        ...this.state.devices,
        [deviceId]: { ...device, audio: { ...device.audio, status: 'paused' } },
      },
    };
    this.emit();
  }
}
