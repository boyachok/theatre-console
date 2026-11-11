import test from 'node:test';
import assert from 'node:assert/strict';
import { createProject, clone, fingerprint } from './project.ts';
import type { Workspace } from './schema.ts';
import {
  BACKUP_INTERVAL,
  BackupReminder,
  backupDirty,
  backupFingerprint,
  backupInfo,
  backupNumber,
  changeBackupMajor,
  importedBackup,
  prepareBackup,
  restoreBackupMetadata,
} from './backup.ts';
import { parseBackup } from './validation.ts';
import { downloadWorkspace } from '../storage/workspace.ts';
const workspace = (): Workspace => ({
  format: 'theatre-score',
  schemaVersion: 1,
  interfaceVersion: '0.7.13',
  draft: createProject(),
  published: null,
});
test('JSON versions are independent, monotone per series; metadata/publish do not mark score edited', () => {
  const w = workspace(),
    original = clone(w);
  assert(backupDirty(w));
  const a = prepareBackup(w, 0, 1000);
  assert.equal(backupNumber(a.backup!), '1.001');
  assert.equal(a.backup!.exportedAt, new Date(1000).toISOString());
  assert(!backupDirty(a));
  assert.deepEqual(w, original);
  const b = prepareBackup(a, 0, 2000);
  assert.equal(backupNumber(b.backup!), '1.002');
  assert(!backupDirty(b));
  const series = changeBackupMajor(b, 2);
  assert.equal(backupNumber(prepareBackup(series).backup!), '2.001');
  assert(!backupDirty(series));
  const same = clone(a);
  same.draft.revision++;
  same.draft.updatedAt = new Date().toISOString();
  same.published = {
    version: 400,
    mode: 'esp',
    createdAt: same.draft.updatedAt,
    fingerprint: fingerprint(same.draft),
    project: clone(same.draft),
  };
  assert(!backupDirty(same));
  same.draft.steps[0].devices[same.draft.devices[0].id].dimmers['pwm-1'].value = 40;
  assert(backupDirty(same));
  same.draft.steps[0].devices[same.draft.devices[0].id].dimmers['pwm-1'].value = 0;
  assert(!backupDirty(same));
  for (const major of [0, -1, 1.5, NaN, Infinity, 1000000])
    assert.throws(() => changeBackupMajor(w, major));
});
test('a newer local export receipt survives delayed master save without rolling back its newer draft', () => {
  const w = workspace(),
    local = prepareBackup(w, 0, 2000);
  const stale = clone(w);
  assert(!backupDirty(restoreBackupMetadata(stale, local)));
  stale.draft.name = 'A newer master edit';
  const merged = restoreBackupMetadata(stale, local);
  assert.equal(merged.draft.name, 'A newer master edit');
  assert(backupDirty(merged));
  const newer = prepareBackup(stale, 4, 3000);
  assert.equal(restoreBackupMetadata(newer, local).backup!.minor, 5);
});
test('reminder is dirty-only, ten minutes from first dirty state, not postponed by every edit; snooze and reload metadata', () => {
  const w = workspace(),
    c = new BackupReminder();
  assert(!c.observe(w, 100));
  assert(!c.observe(w, 100 + BACKUP_INTERVAL - 1));
  w.draft.name = 'Правка';
  assert(c.observe(w, 100 + BACKUP_INTERVAL));
  c.snooze(100 + BACKUP_INTERVAL);
  assert(!c.observe(w, 100 + 2 * BACKUP_INTERVAL - 1));
  assert(c.observe(w, 100 + 2 * BACKUP_INTERVAL));
  const saved = prepareBackup(w);
  assert(!c.observe(saved, 200 + 2 * BACKUP_INTERVAL));
  assert(!c.observe(saved, 99 * BACKUP_INTERVAL));
  saved.draft.name = 'Новая правка';
  assert(!c.observe(saved, 100 * BACKUP_INTERVAL));
  assert(c.observe(saved, 101 * BACKUP_INTERVAL));
  const loaded = parseBackup(JSON.stringify(prepareBackup(saved)));
  assert(!backupDirty(loaded));
  const clock = new BackupReminder();
  assert(!clock.observe(loaded, 0) && !clock.observe(loaded, 20 * BACKUP_INTERVAL));
});
test('old/imported JSON retains immutable publication; optional metadata validates and import is a known external copy', () => {
  const old = workspace();
  old.interfaceVersion = '0.7.11';
  const fp = fingerprint(old.draft);
  old.published = {
    version: 109,
    fingerprint: fp,
    project: clone(old.draft),
    createdAt: old.draft.updatedAt,
    mode: 'esp',
  };
  const loaded = importedBackup(parseBackup(JSON.stringify(old)));
  assert(!backupDirty(loaded));
  assert.equal(backupInfo(loaded).minor, 0);
  assert.equal(fingerprint(loaded.published!.project), fp);
  assert.equal(backupFingerprint(loaded), loaded.backup!.exportedFingerprint);
  for (const b of [
    { major: 0, minor: 0 },
    { major: 1, minor: -1 },
    { major: 1, minor: '2' },
    { major: 1, minor: 0, exportedAt: 'bad' },
    { major: 1.1, minor: 0 },
  ])
    assert.throws(() => parseBackup(JSON.stringify({ ...old, backup: b })));
});
test('actual download exports metadata and new names; browser high-water counter survives old imports and storage failure still permits a file', async () => {
  const map = new Map<string, string>();
  let fail = false;
  const downloads: { href: string; download: string }[] = [];
  const previousStorage = Object.getOwnPropertyDescriptor(globalThis, 'localStorage'),
    previousDocument = Object.getOwnPropertyDescriptor(globalThis, 'document');
  Object.defineProperty(globalThis, 'localStorage', {
    configurable: true,
    value: {
      getItem: (k: string) => map.get(k) ?? null,
      setItem: (k: string, v: string) => {
        if (fail) throw new Error('quota');
        map.set(k, v);
      },
    },
  });
  Object.defineProperty(globalThis, 'document', {
    configurable: true,
    value: {
      createElement: () => {
        const link = {
          href: '',
          download: '',
          click() {
            downloads.push({ ...this });
          },
        };
        return link;
      },
    },
  });
  try {
    const w = workspace();
    w.draft.name = 'Мой спектакль';
    const a = downloadWorkspace(w);
    assert.equal(downloads[0].download, 'Мой-спектакль_v1.001.json');
    const body = await (await fetch(downloads[0].href)).json();
    assert.equal(body.backup.minor, 1);
    assert.deepEqual(body.draft, w.draft);
    assert(!backupDirty(a));
    const b = downloadWorkspace(a);
    assert.equal(b.backup!.minor, 2);
    const c = downloadWorkspace(importedBackup(a));
    assert.equal(c.backup!.minor, 3);
    const d = downloadWorkspace(changeBackupMajor(c, 2));
    assert.equal(d.backup!.minor, 1);
    const e = downloadWorkspace(changeBackupMajor(d, 1));
    assert.equal(e.backup!.minor, 4);
    fail = true;
    assert.equal(downloadWorkspace(e).backup!.minor, 5);
  } finally {
    if (previousStorage) Object.defineProperty(globalThis, 'localStorage', previousStorage);
    else Reflect.deleteProperty(globalThis, 'localStorage');
    if (previousDocument) Object.defineProperty(globalThis, 'document', previousDocument);
    else Reflect.deleteProperty(globalThis, 'document');
  }
});
