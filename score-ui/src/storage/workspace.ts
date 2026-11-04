import type { Workspace } from '../domain/schema.ts';
import { createProject } from '../domain/project.ts';
import { parseBackup, validateProject } from '../domain/validation.ts';
import { INTERFACE_VERSION } from '../version.ts';
export const STORAGE_KEY = 'theatre-score.workspace.v1';
export function emptyWorkspace(): Workspace {
  return {
    format: 'theatre-score',
    schemaVersion: 1,
    interfaceVersion: INTERFACE_VERSION,
    draft: createProject(),
    published: null,
  };
}
export function loadWorkspace(): Workspace {
  const content = localStorage.getItem(STORAGE_KEY);
  return content
    ? { ...parseBackup(content), interfaceVersion: INTERFACE_VERSION }
    : emptyWorkspace();
}
export function saveWorkspace(workspace: Workspace) {
  validateProject(workspace.draft);
  localStorage.setItem(
    STORAGE_KEY,
    JSON.stringify({ ...workspace, interfaceVersion: INTERFACE_VERSION }),
  );
}
export function downloadWorkspace(workspace: Workspace) {
  const url = URL.createObjectURL(
    new Blob([JSON.stringify({ ...workspace, interfaceVersion: INTERFACE_VERSION }, null, 2)], {
      type: 'application/json',
    }),
  );
  const link = document.createElement('a');
  link.href = url;
  link.download = `${workspace.draft.name.replace(/[^\p{L}\p{N}_-]+/gu, '-')}-${new Date().toISOString().slice(0, 10)}.json`;
  link.click();
  setTimeout(() => URL.revokeObjectURL(url), 2000);
}
