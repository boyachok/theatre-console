import type { BackupInfo, Workspace } from './schema.ts';
import { fingerprint } from './project.ts';
export const BACKUP_INTERVAL = 10 * 60 * 1000;
export const backupInfo = (w: Workspace): BackupInfo => w.backup ?? { major: 1, minor: 0 };
// Compare editable content only: navigation, publication bookkeeping and backup metadata are not edits.
export const backupFingerprint = (w: Workspace) =>
  fingerprint({
    ...w.draft,
    revision: 0,
    updatedAt: '',
    ...(w.helmetVoices ? { helmetVoices: w.helmetVoices } : {}),
  });
export const backupDirty = (w: Workspace) =>
  backupInfo(w).exportedFingerprint !== backupFingerprint(w);
export const backupNumber = (b: BackupInfo) => `${b.major}.${String(b.minor).padStart(3, '0')}`;
export function changeBackupMajor(w: Workspace, major: number): Workspace {
  if (!Number.isSafeInteger(major) || major < 1 || major > 999999)
    throw new Error('Серия JSON: целое число 1–999999');
  const b = backupInfo(w);
  return { ...w, backup: { ...b, major, minor: major === b.major ? b.minor : 0 } };
}
export function prepareBackup(w: Workspace, floor = 0, now = Date.now()): Workspace {
  const b = backupInfo(w),
    minor = Math.max(b.minor, Number.isSafeInteger(floor) && floor >= 0 ? floor : 0) + 1;
  if (!Number.isSafeInteger(minor) || minor >= Number.MAX_SAFE_INTEGER)
    throw new Error('Счётчик JSON исчерпан: выберите новую серию');
  return {
    ...w,
    backup: {
      major: b.major,
      minor,
      exportedFingerprint: backupFingerprint(w),
      exportedAt: new Date(now).toISOString(),
    },
  };
}
export function importedBackup(w: Workspace): Workspace {
  return { ...w, backup: { ...backupInfo(w), exportedFingerprint: backupFingerprint(w) } };
}
// A quick browser close can precede the debounced master save. Preserve the newer local export receipt, never its draft.
export function restoreBackupMetadata(server: Workspace, local: Workspace): Workspace {
  if (server.draft.id !== local.draft.id || !local.backup) return server;
  if (
    !server.backup ||
    (Date.parse(local.backup.exportedAt ?? '') || 0) >
      (Date.parse(server.backup.exportedAt ?? '') || 0)
  )
    return { ...server, backup: { ...local.backup } };
  return server;
}
export class BackupReminder {
  private project = '';
  private wasDirty = false;
  private next = 0;
  observe(w: Workspace, now: number) {
    if (this.project !== w.draft.id) {
      this.project = w.draft.id;
      this.wasDirty = false;
    }
    if (!backupDirty(w)) {
      this.wasDirty = false;
      this.next = now + BACKUP_INTERVAL;
      return false;
    }
    if (!this.wasDirty) {
      this.wasDirty = true;
      this.next = now + BACKUP_INTERVAL;
    }
    return now >= this.next;
  }
  snooze(now: number) {
    this.next = now + BACKUP_INTERVAL;
  }
}
