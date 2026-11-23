import { useCallback, useEffect, useMemo, useReducer, useRef, useState } from 'react';
import {
  ChevronLeft,
  ChevronRight,
  Download,
  Layers3,
  LockKeyhole,
  LockKeyholeOpen,
  PanelRightOpen,
  Radio,
  Settings2,
  Trash2,
  Upload,
  Waves,
  Wifi,
} from 'lucide-react';
import type {
  CellSelection,
  Device,
  Project,
  Step,
  StripEffect,
  Workspace,
} from './domain/schema.ts';
import {
  duplicateStep,
  fingerprint,
  moveStep,
  replaceDevices,
  touch,
  updateStep,
} from './domain/project.ts';
import { PreviewEngine } from './domain/playback.ts';
import type { TransitionReason } from './domain/playback.ts';
import { parseBackup } from './domain/validation.ts';
import {
  downloadWorkspace,
  backupSequenceFloor,
  emptyWorkspace,
  loadWorkspace,
  saveWorkspace,
} from './storage/workspace.ts';
import { LocalPreviewTransport } from './transport/preview.ts';
import type { ScoreTransport } from './transport/preview.ts';
import { EspConsoleTransport, useEspBackend } from './transport/esp.ts';
import { DisconnectedNetworkTransport } from './transport/network.ts';
import { Matrix } from './components/Matrix.tsx';
import {
  cellKey,
  columnCells,
  editorRows,
  moveCursor,
  moveDevice,
  moveEditorRow,
  moveEditorRows,
  patchCells,
  rectangle,
  rowKey,
  type Modifiers,
} from './domain/editor.ts';
import { historyReducer } from './domain/history.ts';
import { Inspector } from './components/Inspector.tsx';
import { HelmetDialog } from './components/HelmetDialog.tsx';
import { VoiceLauncher } from './components/VoiceLauncher.tsx';
import { voiceBank } from './domain/voice.ts';
import { DevicesDialog } from './components/DevicesDialog.tsx';
import { EffectsDialog } from './components/EffectsDialog.tsx';
import { NetworkDialog } from './components/NetworkDialog.tsx';
import { Modal } from './components/Modal.tsx';
import { useConfirm } from './components/Confirm.tsx';
import { BackupPrompt, BackupSeries } from './components/BackupControls.tsx';
import {
  backupDirty,
  backupInfo,
  backupNumber,
  BackupReminder,
  changeBackupMajor,
  importedBackup,
  restoreBackupMetadata,
} from './domain/backup.ts';
import { StepPlayer } from './components/StepPlayer.tsx';
import { INTERFACE_VERSION, PRODUCT_NAME } from './version.ts';
const previewSignature = (project: Project, step: Step) =>
  JSON.stringify([step.devices, step.timings, project.devices, project.effects]);
function useEditorEvent<T extends (...args: any[]) => any>(fn: T): T {
  const ref = useRef(fn);
  ref.current = fn;
  return useCallback(((...args: Parameters<T>) => ref.current(...args)) as T, []);
}
function startup() {
  try {
    return { workspace: loadWorkspace(), error: '' };
  } catch {
    return {
      workspace: emptyWorkspace(),
      error: 'Не удалось прочитать партитуру. Исходные данные не перезаписаны.',
    };
  }
}
export default function App() {
  const ask = useConfirm();
  const [initial] = useState(startup);
  const [view, setView] = useState<'launcher' | 'score'>('launcher'),
    [viewBusy, setViewBusy] = useState(false);
  const localEditConsent = useRef(false);
  const [editorLocked, setEditorLocked] = useState(true),
    [lockBusy, setLockBusy] = useState(false);
  const editLockRef = useRef(true),
    voicePending = useRef(0),
    authoredSinceUnlock = useRef(false);
  editLockRef.current = editorLocked;
  const [history, dispatchHistory] = useReducer(historyReducer, {
    workspace: initial.workspace,
    past: [],
    future: [],
    at: 0,
  });
  const workspace = history.workspace;
  const setWorkspace = useCallback(
    (update: Workspace | ((w: Workspace) => Workspace), discrete = false, system = false) =>
      dispatchHistory({
        type: 'set',
        update: (w) => {
          const next = typeof update === 'function' ? update(w) : update;
          if (!system && editLockRef.current && next.draft !== w.draft) return w;
          if (!system && next.draft !== w.draft) authoredSinceUnlock.current = true;
          return next;
        },
        at: Date.now(),
        discrete,
      }),
    [],
  );
  const project = workspace.draft,
    latest = useRef(workspace);
  latest.current = workspace;
  const [engine] = useState(() => new PreviewEngine(initial.workspace.draft));
  const [network] = useState(() =>
    useEspBackend ? new EspConsoleTransport() : new DisconnectedNetworkTransport(),
  );
  const [backendReady, setBackendReady] = useState(!useEspBackend);
  const [backupOpen, setBackupOpen] = useState(false),
    [exporting, setExporting] = useState(false);
  const exportBusy = useRef(false);
  const backupClock = useRef(new BackupReminder());
  const savedAuthoring = useRef<{
    draft: Project;
    published: Workspace['published'];
    backup: Workspace['backup'];
  } | null>(null);
  const [showMode, setShowMode] = useState(true);
  const [selectionIntent, setSelectionIntent] = useState<{ id: string; nonce: number } | null>(
    null,
  );
  const [editorSyncing, setEditorSyncing] = useState(false);
  const [preload, setPreload] = useState({ pending: false, stored: 0, total: 0 });
  const requestBusy = useRef(false),
    appliedSignature = useRef(''),
    authorityKey = useRef('');
  const transition = useRef(0),
    [activeId, setActiveId] = useState<string | null>(null);
  const [selectedStepId, setSelectedStepId] = useState(project.steps[0].id),
    [selection, setSelection] = useState<CellSelection | null>(null);
  const [selectedCells, setSelectedCells] = useState<CellSelection[]>([]);
  const [selectedStepIds, setSelectedStepIds] = useState<string[]>([]);
  const [collapsedDevices, setCollapsedDevices] = useState<string[]>([]);
  const selectedCellsRef = useRef(selectedCells);
  selectedCellsRef.current = selectedCells;
  const selectedStepIdsRef = useRef(selectedStepIds);
  selectedStepIdsRef.current = selectedStepIds;
  const anchorCell = useRef<CellSelection | null>(null),
    anchorStep = useRef(selectedStepId);
  const visibleRows = useMemo(
    () => editorRows(project, collapsedDevices),
    [project, collapsedDevices],
  );
  const [deviceId, setDeviceId] = useState(project.devices[0].id),
    [compact, setCompact] = useState(() => window.matchMedia('(max-width: 960px)').matches);
  const [inspectorOpen, setInspectorOpen] = useState(false),
    [dialog, setDialog] = useState<'devices' | 'effects' | 'network' | 'helmet' | null>(null);
  const [deviceDialogId, setDeviceDialogId] = useState<string | undefined>();
  const [helmetDeviceId, setHelmetDeviceId] = useState<string>();
  const globalHelmet = (d: Device, w = workspace): Device => ({
    ...d,
    voice: w.helmetVoices?.[(d.mac ?? '').toUpperCase()] ?? voiceBank(d),
  });
  const selectedHelmetDevice = project.devices.find((d) => d.id === helmetDeviceId);
  const helmetDevice = selectedHelmetDevice ? globalHelmet(selectedHelmetDevice) : undefined;
  const headerHelmets = project.devices
    .filter((d) => d.profile === 'helmet' && d.mac)
    .map((d) => globalHelmet(d));
  const [saveState, setSaveState] = useState<'pending' | 'saved' | 'error'>('pending'),
    [saveBlocked, setSaveBlocked] = useState(!!initial.error);
  const [toast, setToast] = useState(initial.error),
    [publishing, setPublishing] = useState(false);
  const importInput = useRef<HTMLInputElement>(null),
    transport = useRef<ScoreTransport>(
      network instanceof EspConsoleTransport ? network : new LocalPreviewTransport(),
    );
  const selectedRef = useRef(selectedStepId);
  selectedRef.current = selectedStepId;
  useEffect(() => {
    if (!(network instanceof EspConsoleTransport)) return;
    const stop = network.start();
    let mounted = true;
    const sync = network.subscribe(() => {
      if (!mounted) return;
      const state = network.getSnapshot();
      if (!state.connected) return;
      if (state.editorLocked !== undefined) {
        if (state.editorLocked) localEditConsent.current = false;
        const locked = state.editorLocked || !localEditConsent.current;
        editLockRef.current = locked;
        setEditorLocked(locked);
      }
      if (
        !voicePending.current &&
        state.helmetVoices &&
        state.helmetVoiceRevision !== latest.current.helmetVoiceRevision
      )
        setWorkspace((w) => ({
          ...w,
          helmetVoices: state.helmetVoices,
          helmetVoiceRevision: state.helmetVoiceRevision,
        }));
      setPreload({
        pending: !!state.syncing,
        stored: state.storedDevices ?? 0,
        total: state.plannedDevices ?? 0,
      });
      const live = state.livePreview ?? state.preview ?? false;
      setShowMode(!live);
      const source =
        live || state.syncing ? latest.current.draft : latest.current.published?.project;
      const target =
        state.activeStep == null
          ? null
          : state.activeStepId
            ? source?.steps.find((s) => s.id === state.activeStepId)
            : source?.steps[state.activeStep - 1];
      if (!target) {
        authorityKey.current = '';
        setActiveId(null);
        return;
      }
      setActiveId(target.id);
      const key = `${state.navigation ?? state.transition ?? ''}:${live}:${state.activeStepId ?? state.activeStep}`;
      if (authorityKey.current !== key) {
        authorityKey.current = key;
        if (live && selectedCellsRef.current.length <= 1) {
          setSelectedStepId(target.id);
          setSelection((cell) => (cell ? { ...cell, stepId: target.id } : null));
          setSelectedCells((cells) => cells.map((cell) => ({ ...cell, stepId: target.id })));
        }
      }
      if (state.running === false || (live && state.draftFingerprint !== fingerprint(source!)))
        return;
      const signature = (live ? 'preview:' : 'show:') + previewSignature(source!, target);
      if (
        source &&
        (engine.getSnapshot().stepId !== target.id || appliedSignature.current !== signature)
      ) {
        engine.apply(source, target, 'recover', ++transition.current);
        appliedSignature.current = signature;
      }
    });
    network
      .loadWorkspace()
      .then((saved) => {
        if (!mounted) return;
        if (saved) {
          setWorkspace(restoreBackupMetadata(saved, latest.current), false, true);
          dispatchHistory({ type: 'clear' });
          setSelectedCells([]);
          setSelectedStepIds([]);
          setShowMode(!(network.getSnapshot().livePreview ?? false));
          setSelectedStepId(saved.draft.steps[0].id);
          setDeviceId(saved.draft.devices[0].id);
          setSelection(null);
          engine.reset(saved.draft);
          transition.current = 0;
          setActiveId(null);
          appliedSignature.current = '';
        }
        setBackendReady(true);
      })
      .catch((error) => {
        if (mounted) setToast(`Мастер: ${error.message}`);
      });
    return () => {
      mounted = false;
      sync();
      stop();
    };
  }, [network, engine]);
  useEffect(() => {
    if (!toast) return;
    const timer = setTimeout(() => setToast(''), 5000);
    return () => clearTimeout(timer);
  }, [toast]);
  useEffect(() => {
    const media = window.matchMedia('(max-width: 960px)'),
      change = () => setCompact(media.matches);
    media.addEventListener('change', change);
    return () => media.removeEventListener('change', change);
  }, []);
  useEffect(() => {
    if (!useEspBackend && !engine.getSnapshot().stepId) {
      const p = latest.current.draft,
        first = p.steps[0];
      engine.apply(p, first, 'first', ++transition.current);
      setActiveId(first.id);
      if (!useEspBackend) appliedSignature.current = previewSignature(p, first);
    }
    const timer = setInterval(() => engine.tick(), 33);
    return () => clearInterval(timer);
  }, [engine]);
  useEffect(() => {
    if (!backendReady) return;
    if (saveBlocked) {
      setSaveState('error');
      return;
    }
    setSaveState('pending');
    const timer = setTimeout(
      () => {
        try {
          saveWorkspace(workspace);
          if (network instanceof EspConsoleTransport) {
            const before = savedAuthoring.current;
            if (
              before?.draft === workspace.draft &&
              before?.published === workspace.published &&
              before?.backup === workspace.backup
            ) {
              setSaveState('saved');
              return;
            }
            network
              .saveWorkspace(workspace)
              .then(() => {
                savedAuthoring.current = {
                  draft: workspace.draft,
                  published: workspace.published,
                  backup: workspace.backup,
                };
                setSaveState('saved');
              })
              .catch(() => setSaveState('error'));
          } else setSaveState('saved');
        } catch {
          setSaveState('error');
        }
      },
      network instanceof EspConsoleTransport ? 1500 : 300,
    );
    return () => clearTimeout(timer);
  }, [workspace, saveBlocked, backendReady, network]);
  useEffect(() => {
    const flush = (event: BeforeUnloadEvent) => {
      if (saveBlocked) return;
      try {
        saveWorkspace(latest.current);
      } catch {
        event.preventDefault();
        event.returnValue = '';
      }
    };
    window.addEventListener('beforeunload', flush);
    return () => window.removeEventListener('beforeunload', flush);
  }, [saveBlocked]);
  useEffect(() => {
    if (!backendReady || saveBlocked || editorLocked) return;
    const check = () => {
      const w = latest.current;
      const due = backupClock.current.observe(w, Date.now());
      if (!backupDirty(w)) setBackupOpen(false);
      else if (due && !document.hidden && !document.querySelector('[aria-modal="true"]'))
        setBackupOpen(true);
    };
    check();
    const timer = setInterval(check, 1000);
    document.addEventListener('visibilitychange', check);
    return () => {
      clearInterval(timer);
      document.removeEventListener('visibilitychange', check);
    };
  }, [backendReady, saveBlocked, editorLocked]);
  useEffect(() => {
    if (network instanceof EspConsoleTransport) network.rememberBackup(workspace.backup);
    backupClock.current.observe(workspace, Date.now());
    if (!backupDirty(workspace)) setBackupOpen(false);
  }, [workspace, network]);
  const exportJSON = async () => {
    if (exportBusy.current) return;
    exportBusy.current = true;
    setExporting(true);
    try {
      const requested = latest.current;
      let floor = backupSequenceFloor(requested),
        localOnly = false;
      if (network instanceof EspConsoleTransport) {
        try {
          if (!network.getSnapshot().connected) throw new Error('offline');
          floor = (await network.reserveBackupSequence(requested, floor)) - 1;
        } catch {
          localOnly = true;
        }
      }
      const current = latest.current;
      if (
        current.draft.id !== requested.draft.id ||
        backupInfo(current).major !== backupInfo(requested).major
      ) {
        floor = 0;
        localOnly = true;
      }
      const next = downloadWorkspace(current, floor);
      setWorkspace((w) => (w.draft.id === next.draft.id ? { ...w, backup: next.backup } : w));
      setBackupOpen(false);
      setToast(
        `JSON ${backupNumber(next.backup!)} передан браузеру${localOnly ? ' · локальный номер' : ''}`,
      );
    } catch (e) {
      setToast(e instanceof Error ? e.message : 'Не удалось скачать JSON');
    } finally {
      exportBusy.current = false;
      setExporting(false);
    }
  };
  const changeEditorLock = async () => {
    if (lockBusy) return;
    setLockBusy(true);
    try {
      const next = !editLockRef.current;
      if (network instanceof EspConsoleTransport) await network.setEditLock(next);
      localEditConsent.current = !next;
      editLockRef.current = next;
      authoredSinceUnlock.current = false;
      setEditorLocked(next);
    } catch (error) {
      setToast(error instanceof Error ? error.message : 'Не удалось изменить блокировку');
    } finally {
      setLockBusy(false);
    }
  };
  const lockForView = async () => {
    localEditConsent.current = false;
    editLockRef.current = true;
    authoredSinceUnlock.current = false;
    setEditorLocked(true);
    const state = network.getSnapshot();
    if (network instanceof EspConsoleTransport && state.connected && state.editorLocked === false)
      await network.setEditLock(true);
  };
  const enterScore = async () => {
    if (viewBusy) return;
    setViewBusy(true);
    try {
      if (
        !(await ask({
          message: 'Вы точно хотите редактировать партитуру?',
          accept: 'Да',
          cancel: 'Нет',
        }))
      ) {
        requestAnimationFrame(() =>
          document.querySelector<HTMLButtonElement>('[aria-label="Меню голоса шлема"]')?.focus(),
        );
        return;
      }
      await lockForView();
      setDialog(null);
      setInspectorOpen(false);
      setBackupOpen(false);
      setView('score');
      requestAnimationFrame(() =>
        document
          .querySelector<HTMLButtonElement>('[aria-label="Разрешить редактирование"]')
          ?.focus(),
      );
    } catch (error) {
      setToast(error instanceof Error ? error.message : 'Не удалось открыть партитуру');
    } finally {
      setViewBusy(false);
    }
  };
  const returnToLauncher = async () => {
    if (viewBusy || lockBusy) return;
    setViewBusy(true);
    try {
      await lockForView();
    } catch (error) {
      setToast(error instanceof Error ? error.message : 'Блокировка на master не подтверждена');
    } finally {
      setDialog(null);
      setInspectorOpen(false);
      setBackupOpen(false);
      setView('launcher');
      setViewBusy(false);
    }
  };
  const closeHelmetDialog = () => {
    setDialog(null);
    if (view === 'launcher')
      requestAnimationFrame(() =>
        document.querySelector<HTMLButtonElement>('[aria-label="Меню голоса шлема"]')?.focus(),
      );
  };
  const saveGlobalVoice = (device: Device) => {
    if (!device.voice) return;
    if (!device.mac) {
      setWorkspace(
        (w) => ({
          ...w,
          draft: {
            ...w.draft,
            devices: w.draft.devices.map((d) =>
              d.id === device.id ? { ...d, voice: device.voice } : d,
            ),
          },
        }),
        false,
        true,
      );
      setToast('Звук сохранён локально: для применения назначьте MAC шлема');
      return;
    }
    const mac = device.mac.toUpperCase(),
      bank = device.voice;
    const base =
      network.getSnapshot().helmetVoiceRevision ?? latest.current.helmetVoiceRevision ?? 1;
    setWorkspace((w) => ({ ...w, helmetVoices: { ...w.helmetVoices, [mac]: bank } }));
    if (!(network instanceof EspConsoleTransport)) return;
    voicePending.current++;
    void network
      .saveHelmetBank(mac, bank, base)
      .then((result) => {
        if (result) setWorkspace((w) => ({ ...w, helmetVoiceRevision: result.revision }));
      })
      .catch((error) => {
        let kept = false;
        try {
          sessionStorage.setItem(
            `theatre.voice-unsent.${mac}`,
            JSON.stringify({ bank, at: Date.now() }),
          );
          kept = true;
        } catch {
          /* Keep the visible working copy even if browser storage is full. */
        }
        setToast(
          `Голос не подтверждён: ${error.message}${kept ? ' · локальная копия сохранена' : ''}`,
        );
      })
      .finally(() => {
        voicePending.current--;
      });
  };
  const chooseGlobalPreset = async (device: Device, id: string) => {
    const bank = voiceBank(device);
    if (
      bank.working &&
      !(await ask({
        message: 'Загрузить пресет вместо несохранённых звуковых параметров?',
        accept: 'Загрузить',
      }))
    )
      return;
    saveGlobalVoice({ ...device, voice: { ...bank, selected: id, working: undefined } });
  };
  const changeCell = useCallback(
    (cell: CellSelection, changes: Record<string, unknown>) =>
      setWorkspace((w) => ({
        ...w,
        draft: patchCells(
          w.draft,
          selectedCellsRef.current.some((c) => cellKey(c) === cellKey(cell))
            ? selectedCellsRef.current
            : [cell],
          cell,
          changes,
        ),
      })),
    [],
  );
  const changeStep = useCallback(
    (id: string, changes: Partial<Step>) =>
      setWorkspace((w) => ({
        ...w,
        draft: (() => {
          const original = w.draft.steps.find((s) => s.id === id);
          if (!changes.timings || !original || !selectedStepIdsRef.current.includes(id))
            return updateStep(w.draft, id, (step) => ({ ...step, ...changes }));
          const delta = Object.fromEntries(
            Object.entries(changes.timings).filter(
              ([key, value]) => value !== original.timings[key as keyof typeof original.timings],
            ),
          );
          return touch(w.draft, {
            steps: w.draft.steps.map((step) =>
              step.id === id
                ? { ...step, ...changes }
                : selectedStepIdsRef.current.includes(step.id)
                  ? { ...step, timings: { ...step.timings, ...delta } }
                  : step,
            ),
          });
        })(),
      })),
    [],
  );
  const selectStep = (id: string, mods: Modifiers = {}) => {
    anchorCell.current = null;
    let ids = [id];
    if (mods.shiftKey) {
      const a = project.steps.findIndex((s) => s.id === anchorStep.current),
        b = project.steps.findIndex((s) => s.id === id);
      ids = project.steps.slice(Math.max(0, Math.min(a, b)), Math.max(a, b) + 1).map((s) => s.id);
    } else if (mods.ctrlKey || mods.metaKey)
      ids = selectedStepIds.includes(id)
        ? selectedStepIds.filter((s) => s !== id)
        : [...selectedStepIds, id];
    else anchorStep.current = id;
    setSelectedStepId(id);
    setSelection(null);
    setSelectedStepIds(ids);
    setSelectedCells(columnCells(project, visibleRows, ids));
    if (compact && !mods.shiftKey && !mods.ctrlKey && !mods.metaKey) setInspectorOpen(true);
    if (!mods.shiftKey && !mods.ctrlKey && !mods.metaKey)
      setSelectionIntent((old) => ({ id, nonce: (old?.nonce ?? 0) + 1 }));
  };
  const focusCell = (cell: CellSelection, mods: Modifiers = {}, human = false) => {
    const anchor = anchorCell.current ?? cell;
    const cells = mods.shiftKey
      ? rectangle(project, visibleRows, anchor, cell)
      : mods.ctrlKey || mods.metaKey
        ? selectedCells.some((c) => cellKey(c) === cellKey(cell))
          ? selectedCells.filter((c) => cellKey(c) !== cellKey(cell))
          : [...selectedCells, cell]
        : [cell];
    if (!mods.shiftKey) anchorCell.current = cell;
    setSelectedStepId(cell.stepId);
    setDeviceId(cell.deviceId);
    setSelection(cell);
    setSelectedStepIds([]);
    setSelectedCells(cells);
    if (human) {
      if (compact && !mods.shiftKey && !mods.ctrlKey && !mods.metaKey) setInspectorOpen(true);
      if (!mods.shiftKey && !mods.ctrlKey && !mods.metaKey)
        setSelectionIntent((old) => ({ id: cell.stepId, nonce: (old?.nonce ?? 0) + 1 }));
    }
  };
  const selectRow = (key: string, mods: Modifiers = {}) => {
    const row = visibleRows.find((r) => rowKey(r) === key);
    if (!row) return;
    const cell = {
      stepId: selectedStepId,
      deviceId: row.device.id,
      parameterId: row.parameter.id,
      kind: row.kind,
    };
    let keys = new Set([key]);
    if (mods.shiftKey && anchorCell.current) {
      const a = visibleRows.findIndex((r) => rowKey(r) === rowKey(anchorCell.current!)),
        b = visibleRows.findIndex((r) => rowKey(r) === key);
      if (a >= 0) keys = new Set(visibleRows.slice(Math.min(a, b), Math.max(a, b) + 1).map(rowKey));
    } else if (mods.ctrlKey || mods.metaKey) {
      keys = new Set(selectedCells.map(rowKey));
      if (keys.has(key)) keys.delete(key);
      else keys.add(key);
    }
    if (!mods.shiftKey) anchorCell.current = cell;
    setSelection(keys.size ? cell : null);
    setDeviceId(row.device.id);
    setSelectedStepIds([]);
    setSelectedCells(
      columnCells(
        project,
        visibleRows.filter((r) => keys.has(rowKey(r))),
        project.steps.map((s) => s.id),
      ),
    );
    // Header selection is presentation-only: never set selectionIntent / GO.
  };
  const selectCell = (cell: CellSelection, mods: Modifiers = {}) => focusCell(cell, mods, true);
  const handleSelectCell = useEditorEvent(selectCell),
    handleSelectStep = useEditorEvent(selectStep);
  const reorder = useCallback(
    (id: string, index: number) =>
      setWorkspace((w) => ({ ...w, draft: moveStep(w.draft, id, index) }), true),
    [],
  );
  const duplicate = useCallback(() => {
    if (editLockRef.current) return;
    const w = latest.current,
      result = duplicateStep(w.draft, selectedRef.current);
    setWorkspace({ ...w, draft: result.project });
    setSelectedCells([]);
    setSelectedStepIds([]);
    setSelectedStepId(result.id);
    setSelectionIntent((old) => ({ id: result.id, nonce: (old?.nonce ?? 0) + 1 }));
    setSelection(null);
    requestAnimationFrame(() =>
      document
        .getElementById(`step-${result.id}`)
        ?.scrollIntoView({ block: 'nearest', inline: 'nearest' }),
    );
  }, []);
  const remove = async () => {
    if (editLockRef.current) return;
    const id = selectedRef.current,
      p = latest.current.draft,
      number = p.steps.findIndex((s) => s.id === id);
    if (
      p.steps.length <= 1 ||
      number < 0 ||
      !(await ask({
        message: `Удалить шаг ${number + 1}?`,
        accept: 'Удалить',
        remember: 'delete-step',
      }))
    )
      return;
    const current = latest.current.draft,
      index = current.steps.findIndex((s) => s.id === id);
    if (index < 0 || current.steps.length <= 1) return;
    const steps = current.steps.filter((s) => s.id !== id);
    setWorkspace((w) => ({ ...w, draft: touch(w.draft, { steps }) }));
    setSelectedCells([]);
    setSelectedStepIds([]);
    setSelectedStepId(steps[Math.min(index, steps.length - 1)].id);
    setSelection(null);
  };
  const run = useCallback(
    async (id: string, reason: TransitionReason) => {
      const p = showMode
          ? (latest.current.published?.project ?? latest.current.draft)
          : latest.current.draft,
        step = p.steps.find((s) => s.id === id);
      if (!step || requestBusy.current) return;
      const signature = (showMode ? 'show:' : 'preview:') + previewSignature(p, step);
      if (
        engine.getSnapshot().stepId === id &&
        appliedSignature.current === signature &&
        (!(network instanceof EspConsoleTransport) || network.getSnapshot().running === true)
      )
        return;
      if (network instanceof EspConsoleTransport) {
        try {
          requestBusy.current = true;
          if (showMode && latest.current.published)
            await network.go(
              latest.current.published.version,
              p.steps.findIndex((s) => s.id === id),
              reason === 'back' || reason === 'first',
            );
          else await network.selectDraft(p, step.id, reason === 'back' || reason === 'first');
        } catch (error) {
          setToast(error instanceof Error ? error.message : 'Нет подтверждения мастера');
          return;
        } finally {
          requestBusy.current = false;
        }
        return; // ESP status is authoritative: never repaint a requested stale step optimistically.
      }
      if (engine.apply(p, step, reason, ++transition.current)) {
        appliedSignature.current = signature;
        setActiveId(id);
        requestAnimationFrame(() =>
          document
            .getElementById(`step-${id}`)
            ?.scrollIntoView({ block: 'nearest', inline: 'nearest' }),
        );
      }
    },
    [engine, network, showMode],
  );
  useEffect(() => {
    if (selectionIntent && !showMode && !editLockRef.current)
      void run(selectionIntent.id, 'select');
    // Human selection only. Master-follow updates must not send another navigation command.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [selectionIntent]);
  useEffect(() => {
    if (!backendReady || saveBlocked || editorLocked || !authoredSinceUnlock.current) return;
    if (!(network instanceof EspConsoleTransport)) {
      if (!showMode) {
        const step =
          project.steps.find((s) => s.id === engine.getSnapshot().stepId) ?? project.steps[0];
        if (step) {
          engine.apply(project, step, 'select', ++transition.current);
          setActiveId(step.id);
        }
      }
      return;
    }
    let mounted = true;
    const timer = setTimeout(
      () => {
        setEditorSyncing(true);
        network
          .reconcile(project)
          .then((published) => {
            if (published)
              setWorkspace((w) =>
                !w.published || published.version > w.published.version ? { ...w, published } : w,
              );
          })
          .catch((error) => {
            if (mounted) setToast(`Синхронизация: ${error.message}`);
          })
          .finally(() => {
            if (mounted) setEditorSyncing(false);
          });
      },
      showMode ? 350 : 100,
    );
    return () => {
      mounted = false;
      clearTimeout(timer);
    };
  }, [project, backendReady, saveBlocked, network, showMode, engine, selectedStepId, editorLocked]);
  const changePreviewMode = async (enabled: boolean) => {
    if (!(network instanceof EspConsoleTransport)) {
      setShowMode(!enabled);
      return;
    }
    setPublishing(true);
    try {
      const p = latest.current.draft;
      const published = await network.changePreview(enabled, p, selectedRef.current);
      if (published) setWorkspace((w) => ({ ...w, published }));
    } catch (error) {
      setToast(error instanceof Error ? error.message : 'Не удалось изменить режим');
    } finally {
      setPublishing(false);
    }
  };
  const navigate = useCallback(
    (action: 'back' | 'forward' | 'first' | 'last') => {
      if (network instanceof EspConsoleTransport) {
        if (requestBusy.current) return;
        requestBusy.current = true;
        void network
          .navigate(action)
          .catch((error) => setToast(error.message))
          .finally(() => {
            requestBusy.current = false;
          });
        return; // Relative navigation is resolved only by the master, never the browser engine.
      }
      const steps = (
          showMode
            ? (latest.current.published?.project ?? latest.current.draft)
            : latest.current.draft
        ).steps,
        index =
          useEspBackend && network.getSnapshot().activeStep == null
            ? -1
            : steps.findIndex((s) => s.id === engine.getSnapshot().stepId);
      const to =
        action === 'first'
          ? 0
          : action === 'last'
            ? steps.length - 1
            : action === 'back'
              ? Math.max(0, index - 1)
              : Math.min(steps.length - 1, index + 1);
      run(steps[to].id, action);
    },
    [run, engine, network, showMode],
  );
  const choosePlaybackStep = (id: string) => {
    const steps = (showMode ? (workspace.published?.project ?? project) : project).steps;
    const state = network.getSnapshot();
    const before =
        network instanceof EspConsoleTransport
          ? state.activeStep == null
            ? -1
            : state.activeStepId
              ? steps.findIndex((s) => s.id === state.activeStepId)
              : state.activeStep - 1
          : steps.findIndex((s) => s.id === engine.getSnapshot().stepId),
      after = steps.findIndex((s) => s.id === id);
    run(id, after < before ? 'back' : 'select');
  };
  const publish = async () => {
    if (editLockRef.current) return;
    setPublishing(true);
    try {
      const published = await transport.current.publish(workspace.draft, workspace.published);
      setWorkspace((w) => ({ ...w, published }));
      if (network instanceof EspConsoleTransport) {
        appliedSignature.current = '';
        setShowMode(true);
      }
      setToast(`Версия ${published.version} сохранена`);
    } catch (error) {
      setToast(error instanceof Error ? error.message : 'Ошибка сохранения');
    } finally {
      setPublishing(false);
    }
  };
  const importFile = async (file: File | undefined) => {
    if (!file || editLockRef.current) return;
    try {
      if (file.size > 20 * 1024 * 1024) throw new Error('Файл больше 20 МБ');
      const next = parseBackup(await file.text());
      if (!(await ask({ message: 'Заменить текущую партитуру из JSON?', accept: 'Заменить' })))
        return;
      if (network instanceof EspConsoleTransport) await network.importHelmetBanks(next);
      setWorkspace({ ...importedBackup(next), interfaceVersion: INTERFACE_VERSION });
      dispatchHistory({ type: 'clear' });
      setSelectedCells([]);
      setSelectedStepIds([]);
      setSaveBlocked(false);
      setBackendReady(true);
      setSelectedStepId(next.draft.steps[0].id);
      setSelection(null);
      setDeviceId(next.draft.devices[0].id);
      engine.reset(next.draft);
      transition.current = 0;
      engine.apply(next.draft, next.draft.steps[0], 'first', ++transition.current);
      setActiveId(next.draft.steps[0].id);
      appliedSignature.current = useEspBackend
        ? ''
        : previewSignature(next.draft, next.draft.steps[0]);
      setToast('Партитура загружена');
    } catch (error) {
      setToast(error instanceof Error ? error.message : 'Ошибка JSON');
    }
  };
  const saveDevices = (devices: Device[]) => {
    if (editLockRef.current) return;
    setWorkspace((w) => ({ ...w, draft: replaceDevices(w.draft, devices) }));
    setSelectedCells([]);
    setSelectedStepIds([]);
    if (!devices.some((d) => d.id === deviceId)) setDeviceId(devices[0].id);
    setSelection(null);
  };
  const saveEffects = (effects: StripEffect[]) =>
    setWorkspace((w) => ({ ...w, draft: touch(w.draft, { effects }) }));
  const openDevices = useCallback((id?: string) => {
    if (editLockRef.current) return;
    setDeviceDialogId(id);
    setDialog('devices');
  }, []);
  const toggleLayout = () =>
    setWorkspace(
      (w) => ({
        ...w,
        draft: touch(w.draft, {
          layout: {
            mode: w.draft.layout?.mode === 'custom' ? 'devices' : 'custom',
            order: w.draft.layout?.order ?? editorRows(w.draft).map(rowKey),
          },
        }),
      }),
      true,
    );
  const moveRow = (source: string, target: string) =>
    setWorkspace((w) => ({ ...w, draft: moveEditorRow(w.draft, source, target) }), true);
  useEffect(() => {
    const keydown = (e: KeyboardEvent) => {
      if (!backendReady || view !== 'score') return;
      const el = e.target instanceof Element ? e.target : null;
      if (
        el?.closest('input,textarea,select,[contenteditable="true"]') ||
        document.querySelector('[role="dialog"]')
      )
        return;
      if (e.ctrlKey || e.metaKey || e.altKey) return;
      const arrows: Record<string, [number, number]> = {
        ArrowLeft: [-1, 0],
        ArrowRight: [1, 0],
        ArrowUp: [0, -1],
        ArrowDown: [0, 1],
      };
      if (arrows[e.key]) {
        e.preventDefault();
        const [dx, dy] = arrows[e.key];
        if (!selection && selectedStepIds.length && e.shiftKey && dx) {
          const i = project.steps.findIndex((s) => s.id === selectedStepId);
          selectStep(project.steps[Math.max(0, Math.min(project.steps.length - 1, i + dx))].id, {
            shiftKey: true,
          });
          return;
        }
        const next = moveCursor(
          project,
          visibleRows,
          selection ??
            (visibleRows[0]
              ? {
                  stepId: selectedStepId,
                  deviceId: visibleRows[0].device.id,
                  parameterId: visibleRows[0].parameter.id,
                  kind: visibleRows[0].kind,
                }
              : null),
          dx,
          dy,
        );
        if (next) {
          focusCell(next, { shiftKey: e.shiftKey });
          requestAnimationFrame(() => {
            const td = document.querySelector<HTMLElement>(
              `td[data-step-id="${CSS.escape(next.stepId)}"][data-device="${CSS.escape(next.deviceId)}"][data-parameter="${CSS.escape(next.parameterId)}"]`,
            );
            td?.querySelector('button')?.focus({ preventScroll: true });
            td?.scrollIntoView({ block: 'nearest', inline: 'nearest' });
          });
        }
        return;
      }
      if (e.key === 'Escape') {
        setSelectedCells([]);
        setSelectedStepIds([]);
        setSelection(null);
      }
    };
    window.addEventListener('keydown', keydown);
    return () => window.removeEventListener('keydown', keydown);
  });
  const selectedIndex = project.steps.findIndex((s) => s.id === selectedStepId),
    published = workspace.published;
  const draftFingerprint = useMemo(() => fingerprint(project), [project]),
    dirty = !published || published.fingerprint !== draftFingerprint;
  const inspector = (
    <Inspector
      readOnly={editorLocked}
      project={{
        ...project,
        devices: project.devices.map((d) => (d.profile === 'helmet' ? globalHelmet(d) : d)),
      }}
      stepId={selectedStepId}
      selection={selection}
      deviceId={deviceId}
      engine={engine}
      onChangeCell={changeCell}
      onHelmet={(id) => {
        setHelmetDeviceId(id);
        setDialog('helmet');
      }}
      onDevice={(device) =>
        device.profile === 'helmet'
          ? saveGlobalVoice(device)
          : setWorkspace((w) => ({
              ...w,
              draft: touch(w.draft, {
                devices: w.draft.devices.map((d) => (d.id === device.id ? device : d)),
              }),
            }))
      }
      network={network}
      onStep={changeStep}
    />
  );
  const helmetEditor =
    dialog === 'helmet' && helmetDevice ? (
      <HelmetDialog
        device={helmetDevice}
        network={network}
        onClose={closeHelmetDialog}
        onChange={saveGlobalVoice}
      />
    ) : null;
  const toastNotice = toast ? (
    <div className="toast" role="status" onClick={() => setToast('')}>
      {toast}
    </div>
  ) : null;
  if (view === 'launcher')
    return (
      <div className={`app-shell launcher-shell ${editorLocked ? 'editing-locked' : ''}`}>
        <VoiceLauncher
          helmets={headerHelmets}
          ready={backendReady}
          busy={viewBusy}
          onPreset={(device, id) => void chooseGlobalPreset(device, id)}
          onSound={(id) => {
            setHelmetDeviceId(id);
            setDialog('helmet');
          }}
          onScore={() => void enterScore()}
        />
        {helmetEditor}
        {toastNotice}
      </div>
    );
  return (
    <div className={`app-shell ${editorLocked ? 'editing-locked' : ''}`}>
      <header className="app-header">
        <div className="brand">
          <span className="brand-mark">
            <Layers3 size={20} />
          </span>
          <div>
            <h1>{PRODUCT_NAME}</h1>
            <input
              className="project-name"
              disabled={!backendReady || editorLocked}
              aria-label="Название спектакля"
              maxLength={120}
              value={project.name}
              onChange={(e) =>
                setWorkspace((w) => ({ ...w, draft: touch(w.draft, { name: e.target.value }) }))
              }
              onBlur={(e) => {
                if (!e.target.value.trim())
                  setWorkspace((w) => ({
                    ...w,
                    draft: touch(w.draft, { name: 'Новый спектакль' }),
                  }));
              }}
            />
          </div>
        </div>
        <div className="step-control-strip" inert={!backendReady}>
          <StepPlayer
            steps={
              showMode && workspace.published && !preload.pending
                ? workspace.published.project.steps
                : project.steps
            }
            activeId={activeId}
            selectedIndex={selectedIndex}
            onNavigate={navigate}
            onSelect={choosePlaybackStep}
            onDuplicate={duplicate}
          />
          <div className="step-order-tools">
            <button
              className="icon-button"
              aria-label="Шаг левее"
              title="Переместить шаг влево"
              disabled={editorLocked || selectedIndex <= 0}
              onClick={() => reorder(selectedStepId, selectedIndex - 1)}
            >
              <ChevronLeft size={14} />
            </button>
            <button
              className="icon-button"
              aria-label="Шаг правее"
              title="Переместить шаг вправо"
              disabled={editorLocked || selectedIndex === project.steps.length - 1}
              onClick={() => reorder(selectedStepId, selectedIndex + 1)}
            >
              <ChevronRight size={14} />
            </button>
            <button
              className="icon-button delete-button"
              aria-label="Удалить шаг"
              title="Удалить шаг"
              disabled={editorLocked || project.steps.length === 1}
              onClick={remove}
            >
              <Trash2 size={14} />
            </button>
          </div>
        </div>
        <div className="header-actions">
          <button
            aria-label="Вернуться к голосу шлема"
            title="Стартовое окно"
            disabled={viewBusy || lockBusy}
            onClick={() => void returnToLauncher()}
          >
            <Waves size={17} />
            Голос шлема
          </button>
          <button
            className={`edit-lock-button ${editorLocked ? 'locked' : ''}`}
            aria-label={editorLocked ? 'Разрешить редактирование' : 'Заблокировать редактирование'}
            aria-pressed={editorLocked}
            disabled={
              (!backendReady &&
                !(network instanceof EspConsoleTransport && network.getSnapshot().connected)) ||
              lockBusy
            }
            onClick={() => void changeEditorLock()}
            title={editorLocked ? 'Разрешить редактирование' : 'Заблокировать редактирование'}
          >
            {editorLocked ? <LockKeyhole size={17} /> : <LockKeyholeOpen size={17} />}
            {editorLocked ? 'Просмотр' : 'Редактирование'}
          </button>
          {headerHelmets.map((device) => (
            <select
              key={device.id}
              className="helmet-quick-preset"
              aria-label={`Пресет голоса · ${device.name}`}
              value={voiceBank(device).selected}
              disabled={!backendReady}
              title="Голос шлема"
              onChange={(e) => void chooseGlobalPreset(device, e.target.value)}
            >
              {voiceBank(device).presets.map((preset) => (
                <option key={preset.id} value={preset.id}>
                  {preset.name}
                </option>
              ))}
            </select>
          ))}
          <label className="preview-toggle" title="Предпросмотр">
            <input
              type="checkbox"
              role="switch"
              aria-label="Предпросмотр"
              checked={!showMode}
              disabled={publishing || !backendReady || editorLocked}
              onChange={(e) => void changePreviewMode(e.target.checked)}
            />
            Предпросмотр
          </label>
          {useEspBackend && (
            <span className="sync-chip" role="status">
              {!backendReady
                ? 'Чтение партитуры…'
                : network.getSnapshot().startupPending
                  ? 'Ожидание приборов…'
                  : publishing || editorSyncing
                    ? 'Синхронизация…'
                    : preload.pending
                      ? 'Подготовка…'
                      : ''}
            </span>
          )}
          <button
            className="icon-button"
            aria-label="Приборы"
            disabled={!backendReady || editorLocked}
            title="Приборы"
            onClick={() => openDevices()}
          >
            <Settings2 size={18} />
          </button>
          <button
            className="icon-button"
            aria-label="Сеть"
            title="Сеть"
            onClick={() => setDialog('network')}
          >
            <Wifi size={18} />
          </button>
          <button
            className="icon-button"
            aria-label="Эффекты ленты"
            disabled={!backendReady || editorLocked}
            title="Эффекты ленты"
            onClick={() => {
              if (!editLockRef.current) setDialog('effects');
            }}
          >
            <Waves size={18} />
          </button>
          <span className="header-divider" />
          <BackupSeries
            info={backupInfo(workspace)}
            disabled={exporting}
            onChange={(major) => setWorkspace((w) => changeBackupMajor(w, major))}
          />
          <button
            className="icon-button"
            aria-label="Скачать JSON"
            title="Скачать JSON"
            onClick={exportJSON}
            disabled={exporting}
            aria-busy={exporting}
          >
            <Download size={17} />
          </button>
          <button
            className="icon-button"
            aria-label="Загрузить JSON"
            title="Загрузить JSON"
            onClick={() => importInput.current?.click()}
            disabled={exporting || editorLocked}
          >
            <Upload size={17} />
          </button>
          {!useEspBackend && (
            <button
              className="icon-button publish-button"
              aria-label="Опубликовать"
              title="Опубликовать"
              disabled={publishing || !dirty || editorLocked}
              onClick={publish}
            >
              <Radio size={18} />
            </button>
          )}
          <span
            className={`save-dot ${network.getSnapshot().helmetVoiceSaved === false ? 'pending' : saveState}`}
            title={
              network.getSnapshot().helmetVoiceSaved === false
                ? 'Голос применён в RAM; ожидается запись в память'
                : saveState === 'saved'
                  ? 'Сохранено'
                  : saveState === 'pending'
                    ? 'Сохранение'
                    : 'Не сохранено'
            }
          />
          {compact && (
            <button
              className="icon-button"
              aria-label="Параметры"
              title="Параметры"
              onClick={() => setInspectorOpen(true)}
            >
              <PanelRightOpen size={18} />
            </button>
          )}
          <input
            ref={importInput}
            type="file"
            accept=".json,application/json"
            hidden
            onChange={(e) => {
              importFile(e.target.files?.[0]);
              e.target.value = '';
            }}
          />
        </div>
      </header>
      {backupOpen && (
        <BackupPrompt
          onDownload={exportJSON}
          disabled={exporting}
          onLater={() => {
            backupClock.current.snooze(Date.now());
            setBackupOpen(false);
          }}
        />
      )}
      <main className="workspace" inert={!backendReady} aria-busy={!backendReady}>
        <section className="matrix-area">
          <Matrix
            project={project}
            readOnly={editorLocked}
            onLayout={toggleLayout}
            selectedStepId={selectedStepId}
            activeStepId={activeId}
            selection={selection}
            selectedCells={selectedCells}
            selectedStepIds={selectedStepIds}
            collapsedDevices={collapsedDevices}
            onToggleDevice={(id) =>
              setCollapsedDevices((old) =>
                old.includes(id) ? old.filter((x) => x !== id) : [...old, id],
              )
            }
            onMoveRow={moveRow}
            onSelectRow={selectRow}
            onMoveRows={(sources, target, position) =>
              setWorkspace(
                (w) => ({
                  ...w,
                  draft: moveEditorRows(w.draft, sources, target, position),
                }),
                true,
              )
            }
            onMoveDevice={(id, index) =>
              setWorkspace((w) => ({ ...w, draft: moveDevice(w.draft, id, index) }), true)
            }
            compact={compact}
            onSelectStep={handleSelectStep}
            onSelectCell={handleSelectCell}
            onChangeCell={changeCell}
            onMove={reorder}
            onDevices={openDevices}
          />
        </section>
        {!compact && <aside className="inspector">{inspector}</aside>}
      </main>
      {compact && inspectorOpen && dialog !== 'helmet' && (
        <Modal
          title="Параметры"
          className="inspector-modal"
          onClose={() => setInspectorOpen(false)}
        >
          {inspector}
        </Modal>
      )}
      {dialog === 'devices' && (
        <DevicesDialog
          project={project}
          initialId={deviceDialogId}
          onSave={saveDevices}
          onClose={() => setDialog(null)}
          network={network}
        />
      )}
      {dialog === 'effects' && (
        <EffectsDialog project={project} onSave={saveEffects} onClose={() => setDialog(null)} />
      )}
      {helmetEditor}
      {dialog === 'network' && (
        <NetworkDialog project={project} transport={network} onClose={() => setDialog(null)} />
      )}
      {toastNotice}
    </div>
  );
}
