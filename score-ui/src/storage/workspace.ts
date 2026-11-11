import type { Workspace } from '../domain/schema.ts';
import { createProject } from '../domain/project.ts';
import { parseBackup, validateProject } from '../domain/validation.ts';
import { backupInfo, backupNumber, prepareBackup } from '../domain/backup.ts';
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
const BACKUP_LEDGER = 'theatre-score.json-sequences.v1';
function backupLedger(): Record<string, number> {
  let ledger: Record<string, number> = {};
  try {
    const value = JSON.parse(localStorage.getItem(BACKUP_LEDGER) ?? '{}');
    if (value && typeof value === 'object' && !Array.isArray(value)) ledger = value;
  } catch {
    /* JSON export must still work when browser storage is unavailable. */
  }
  return ledger;
}
export function backupSequenceFloor(workspace: Workspace) {
  const n = backupLedger()[`${workspace.draft.id}:${backupInfo(workspace).major}`];
  return Math.max(backupInfo(workspace).minor, Number.isSafeInteger(n) && n >= 0 ? n : 0);
}
export function downloadWorkspace(workspace: Workspace, minimumFloor = 0): Workspace {
  const key = `${workspace.draft.id}:${backupInfo(workspace).major}`,
    ledger = backupLedger();
  const floor = Math.max(
    backupSequenceFloor(workspace),
    Number.isSafeInteger(minimumFloor) && minimumFloor >= 0 ? minimumFloor : 0,
  );
  const next = { ...prepareBackup(workspace, floor), interfaceVersion: INTERFACE_VERSION };
  const url = URL.createObjectURL(
    new Blob([JSON.stringify(next, null, 2)], { type: 'application/json' }),
  );
  const link = document.createElement('a');
  link.href = url;
  link.download = `${workspace.draft.name.replace(/[^\p{L}\p{N}_-]+/gu, '-')}_v${backupNumber(next.backup!)}.json`;
  try {
    link.click();
    try {
      localStorage.setItem(BACKUP_LEDGER, JSON.stringify({ ...ledger, [key]: next.backup!.minor }));
    } catch {
      /* Counter remains in this session, master workspace and exported JSON. */
    }
  } finally {
    setTimeout(() => URL.revokeObjectURL(url), 2000);
  }
  return next; // Browser download request, not proof that an OS save dialog was completed.
}
