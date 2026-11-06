import type { Project } from './schema.ts';
import { compileDevices } from './compile.ts';
import { fingerprint } from './project.ts';
export interface DraftPlan {
  fingerprint: string;
  steps: string[];
  devices: ReturnType<typeof compileDevices>;
}
export function compileDraft(project: Project): DraftPlan {
  return {
    fingerprint: fingerprint(project),
    steps: project.steps.map((s) => s.id),
    devices: compileDevices(project, project.steps),
  };
}
export function draftDelta(next: DraftPlan, previous: DraftPlan | null, base: string) {
  const full = { base, ...next };
  if (
    !previous ||
    JSON.stringify(next.steps) !== JSON.stringify(previous.steps) ||
    JSON.stringify(next.devices.map((d) => [d.id, d.mac])) !==
      JSON.stringify(previous.devices.map((d) => [d.id, d.mac]))
  )
    return full;
  const patches = next.devices.flatMap((d, device) =>
    d.cues.flatMap((cue, step) =>
      JSON.stringify(cue) === JSON.stringify(previous.devices[device].cues[step])
        ? []
        : [{ id: d.id, step, cue }],
    ),
  );
  const delta = { base, fingerprint: next.fingerprint, patches };
  return JSON.stringify(delta).length < JSON.stringify(full).length ? delta : full;
}
