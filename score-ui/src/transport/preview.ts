import type { Project, PublishedScore } from '../domain/schema.ts';
import { clone, fingerprint } from '../domain/project.ts';
import { validateProject } from '../domain/validation.ts';
export interface ScoreTransport {
  readonly kind: 'local' | 'esp';
  publish(project: Project, previous: PublishedScore | null): Promise<PublishedScore>;
}
// No ESP requests, radio simulation or invented device acknowledgements.
export class LocalPreviewTransport implements ScoreTransport {
  readonly kind = 'local' as const;
  async publish(project: Project, previous: PublishedScore | null): Promise<PublishedScore> {
    validateProject(project);
    return {
      version: (previous?.version ?? 0) + 1,
      project: clone(project),
      fingerprint: fingerprint(project),
      createdAt: new Date().toISOString(),
      mode: 'local',
    };
  }
}
