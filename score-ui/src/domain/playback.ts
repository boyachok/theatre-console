import type { Project, Step, StripEffect } from './schema.ts';
import { clone, effectiveStrip, acParameter, acCue } from './project.ts';
import { retargetDimmer, sampleDimmer } from './dimmer.ts';
import type { LightRuntime } from './dimmer.ts';
import { initialAudio, retargetAudio, sampleAudio } from './audio.ts';
import type { AudioRuntime } from './audio.ts';
export type { AudioRuntime } from './audio.ts';
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
export interface DeviceRuntime {
  ac: LightRuntime;
  dimmers: Record<string, LightRuntime>;
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
          ac: { value: 0, envelope: envelope(0, 0, 0, 0) },
          dimmers: Object.fromEntries(
            d.dimmers.map((p) => [p.id, { value: 0, envelope: envelope(0, 0, 0, 0) }]),
          ),
          relays: Object.fromEntries(
            d.relays.map((p) => [p.id, { on: false, target: false, due: null }]),
          ),
          audio: initialAudio(),
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
      for (const p of device.dimmers) {
        const light = runtime.dimmers[p.id],
          target = cue.dimmers[p.id];
        runtime.dimmers[p.id] = retargetDimmer(
          light,
          target,
          p.enabled,
          target.fadeIn ?? step.timings.lightIn,
          target.fadeOut ?? step.timings.lightOut,
          now,
          restart || state.stepId !== step.id,
        );
      }
      const ac = acCue(cue);
      runtime.ac = retargetDimmer(
        runtime.ac,
        ac,
        device.profile !== 'helmet' && acParameter(device).enabled,
        ac.fadeIn ?? step.timings.lightIn,
        ac.fadeOut ?? step.timings.lightOut,
        now,
        restart || state.stepId !== step.id,
      );
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
      if (device.profile !== 'helmet') {
        runtime.audio = retargetAudio(
          runtime.audio,
          cue.audio,
          device.audio.enabled,
          cue.audio.fadeIn ?? step.timings.audioIn,
          cue.audio.fadeOut ?? step.timings.audioOut,
          restart,
          state.stepId !== step.id,
          now,
        );
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
          ac: sampleDimmer(previous.ac, now),
        };
        if (d.ac.value !== previous.ac.value || d.ac.clock || d.ac.startAt !== previous.ac.startAt)
          changed = true;
        for (const [key, light] of Object.entries(previous.dimmers)) {
          const next = sampleDimmer(light, now);
          if (next.value !== light.value || next.clock || next.startAt !== light.startAt) {
            changed = true;
            d.dimmers[key] = next;
          }
        }
        for (const [key, relay] of Object.entries(previous.relays))
          if (relay.due !== null && now >= relay.due) {
            changed = true;
            d.relays[key] = { ...relay, on: relay.target, due: null };
          }
        const audio = sampleAudio(previous.audio, now);
        if (audio !== previous.audio) {
          changed = true;
          d.audio = audio;
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
    if (!device || (device.audio.loop && device.audio.targetTrack !== null)) return;
    this.state = {
      ...this.state,
      devices: {
        ...this.state.devices,
        [deviceId]: { ...device, audio: { ...device.audio, status: 'paused', startAt: null } },
      },
    };
    this.emit();
  }
}
