import type { AudioCue } from './schema.ts';
export interface AudioRuntime {
  track: number | null;
  targetTrack: number | null;
  status: 'playing' | 'paused' | 'stopped' | 'waiting';
  volume: number;
  envelope: { from: number; to: number; at: number; duration: number };
  stopAt: number | null;
  generation: number;
  startAt: number | null;
  requestedAt: number;
  startDelay: number;
  startVolume: number;
  fadeIn: number;
}
const ramp = (from: number, to: number, seconds: number, at: number) => ({
  from,
  to,
  at,
  duration: seconds * 1000,
});
export const initialAudio = (): AudioRuntime => ({
  track: null,
  targetTrack: null,
  status: 'stopped',
  volume: 0,
  envelope: ramp(0, 0, 0, 0),
  stopAt: null,
  generation: 0,
  startAt: null,
  requestedAt: 0,
  startDelay: 0,
  startVolume: 0,
  fadeIn: 0,
});
function start(audio: AudioRuntime, at: number) {
  audio.startAt = null;
  audio.track = audio.targetTrack;
  audio.status = 'playing';
  audio.generation++;
  audio.envelope = ramp(0, audio.startVolume, audio.fadeIn, at);
}
export function sampleAudio(previous: AudioRuntime, now: number): AudioRuntime {
  const a = { ...previous };
  if (a.startAt !== null && now >= a.startAt) start(a, a.startAt); // Ideal browser timeline, not UART/sound feedback.
  const e = a.envelope;
  a.volume = e.duration
    ? e.from + (e.to - e.from) * Math.min(1, Math.max(0, (now - e.at) / e.duration))
    : e.to;
  if (a.stopAt !== null && now >= a.stopAt) {
    a.track = null;
    a.status = 'stopped';
    a.stopAt = null;
  }
  return a.volume !== previous.volume ||
    a.status !== previous.status ||
    a.startAt !== previous.startAt ||
    a.stopAt !== previous.stopAt
    ? a
    : previous;
}
export function retargetAudio(
  previous: AudioRuntime,
  target: AudioCue,
  enabled: boolean,
  fadeIn: number,
  fadeOut: number,
  restart: boolean,
  newScene: boolean,
  now: number,
): AudioRuntime {
  const a = { ...sampleAudio(previous, now) },
    track = enabled ? target.track : null;
  const pending = a.startAt !== null,
    different = track !== a.targetTrack,
    delay = Math.round((target.startDelay ?? 0) * 1000);
  a.startVolume = target.volume;
  a.fadeIn = fadeIn;
  a.stopAt = null;
  if (track === null) {
    a.startAt = null;
    if (pending) {
      a.track = null;
      a.status = 'stopped';
      a.volume = 0;
    }
    a.envelope = ramp(a.volume, 0, fadeOut, now);
    a.stopAt = now + fadeOut * 1000;
  } else if (different || restart || (pending && (newScene || delay !== a.startDelay))) {
    if (different || restart || newScene) a.requestedAt = now;
    a.startAt = a.requestedAt + delay;
    a.track = null;
    a.status = 'waiting';
    a.volume = 0;
    a.envelope = ramp(0, 0, 0, now);
  } else if (!pending)
    a.envelope = ramp(a.volume, target.volume, target.volume >= a.volume ? fadeIn : fadeOut, now);
  a.targetTrack = track;
  a.startDelay = delay;
  if (a.startAt !== null && a.startAt <= now) start(a, now); // A shortened LIVE delay cannot play retroactively.
  return sampleAudio(a, now);
}
