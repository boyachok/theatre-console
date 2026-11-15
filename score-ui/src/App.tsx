import { useCallback, useEffect, useMemo, useReducer, useRef, useState } from 'react';
import {
  ChevronLeft,
  ChevronRight,
  Download,
  Layers3,
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
  QuickToolbar,
  QuickValueDialog,
  ShortcutHelp,
  type EditScope,
} from './components/QuickEditor.tsx';
import {
  applyEdit,
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
  type EditField,
  type Modifiers,
} from './domain/editor.ts';
import { historyReducer } from './domain/history.ts';
import { Inspector } from './components/Inspector.tsx';
import { HelmetDialog } from './components/HelmetDialog.tsx';
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
  const [history, dispatchHistory] = useReducer(historyReducer, {
    workspace: initial.workspace,
    past: [],
    future: [],
    at: 0,
  });
  const workspace = history.workspace;
  const setWorkspace = useCallback(
    (update: Workspace | ((w: Workspace) => Workspace), discrete = false) =>
      dispatchHistory({ type: 'set', update, at: Date.now(), discrete }),
    [],
  );
  const undo = useCallback(() => dispatchHistory({ type: 'undo', at: Date.now() }), []);
  const redo = useCallback(() => dispatchHistory({ type: 'redo', at: Date.now() }), []);
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
  const [editScope, setEditScope] = useState<EditScope>('selection');
  const [quickEdit, setQuickEdit] = useState<{ field: EditField; cells: CellSelection[] } | null>(
    null,
  );
  const [shortcutHelp, setShortcutHelp] = useState(false);
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
  const helmetDevice = project.devices.find((d) => d.id === helmetDeviceId);
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
          setWorkspace(restoreBackupMetadata(saved, latest.current));
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
            network
              .saveWorkspace(workspace)
              .then(() => setSaveState('saved'))
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
    if (!backendReady || saveBlocked) return;
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
  }, [backendReady, saveBlocked]);
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
    if (selectionIntent && !showMode) void run(selectionIntent.id, 'select');
    // Human selection only. Master-follow updates must not send another navigation command.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [selectionIntent]);
  useEffect(() => {
    if (!backendReady || saveBlocked) return;
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
  }, [project, backendReady, saveBlocked, network, showMode, engine, selectedStepId]);
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
    if (!file) return;
    try {
      if (file.size > 20 * 1024 * 1024) throw new Error('Файл больше 20 МБ');
      const next = parseBackup(await file.text());
      if (!(await ask({ message: 'Заменить текущую партитуру из JSON?', accept: 'Заменить' })))
        return;
      setWorkspace({ ...importedBackup(next), interfaceVersion: INTERFACE_VERSION });
      dispatchHistory({ type: 'clear' });
      setSelectedCells([]);
      setSelectedStepIds([]);
      setSaveBlocked(false);
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
    setWorkspace((w) => ({ ...w, draft: replaceDevices(w.draft, devices) }));
    setSelectedCells([]);
    setSelectedStepIds([]);
    if (!devices.some((d) => d.id === deviceId)) setDeviceId(devices[0].id);
    setSelection(null);
  };
  const saveEffects = (effects: StripEffect[]) =>
    setWorkspace((w) => ({ ...w, draft: touch(w.draft, { effects }) }));
  const openDevices = useCallback((id?: string) => {
    setDeviceDialogId(id);
    setDialog('devices');
  }, []);
  const scopedCells = (): CellSelection[] => {
    const cells = selectedCells.length ? selectedCells : selection ? [selection] : [];
    if (editScope === 'selection' || !cells.length) return cells;
    const rows = editorRows(project);
    if (editScope === 'allSteps') {
      const keys = new Set(cells.map(rowKey));
      return columnCells(
        project,
        rows.filter((r) => keys.has(rowKey(r))),
        project.steps.map((s) => s.id),
      );
    }
    return columnCells(
      project,
      editScope === 'device'
        ? rows.filter((r) => r.device.id === (selection?.deviceId ?? deviceId))
        : rows,
      [selectedStepId],
    );
  };
  const applyQuick = (field: EditField, value: number | null, cells = scopedCells()) => {
    const result = applyEdit(latest.current.draft, cells, field, value);
    if (result.count) setWorkspace((w) => ({ ...w, draft: result.project }));
    setToast(
      result.count ? `Изменено ячеек: ${result.count}` : 'Нет подходящих параметров в выделении',
    );
    setQuickEdit(null);
  };
  const openQuick = (field: EditField) => {
    const cells = scopedCells();
    if (cells.length) setQuickEdit({ field, cells });
    else setToast('Сначала выдели ячейки или шаги');
  };
  const moveRow = (source: string, target: string) =>
    setWorkspace((w) => ({ ...w, draft: moveEditorRow(w.draft, source, target) }), true);
  useEffect(() => {
    const keydown = (e: KeyboardEvent) => {
      if (!backendReady) return;
      const el = e.target instanceof Element ? e.target : null;
      if (
        el?.closest('input,textarea,select,[contenteditable="true"]') ||
        document.querySelector('[role="dialog"]')
      )
        return;
      if ((e.ctrlKey || e.metaKey) && e.code === 'KeyZ') {
        e.preventDefault();
        if (e.shiftKey) redo();
        else undo();
        return;
      }
      if (e.key === '?' || e.code === 'F1') {
        e.preventDefault();
        setShortcutHelp(true);
        return;
      }
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
        if (e.altKey && dy && selection) {
          const i = visibleRows.findIndex((r) => rowKey(r) === rowKey(selection)),
            target = visibleRows[i + dy];
          if (target) moveRow(rowKey(selection), rowKey(target));
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
      if (e.ctrlKey || e.metaKey || e.altKey) return;
      if (e.key === 'Enter' && el?.closest('.matrix-cell')) {
        e.preventDefault();
        openQuick('level');
        return;
      }
      if (/^Digit[0-9]$/.test(e.code) || /^Numpad[0-9]$/.test(e.code)) {
        e.preventDefault();
        applyQuick('level', Number(e.code.slice(-1)) * 10);
        return;
      }
      if (e.code === 'Equal' || e.code === 'NumpadAdd') {
        e.preventDefault();
        applyQuick('level', 100);
        return;
      }
      const fields: Record<string, EditField> = {
        KeyI: 'fadeIn',
        KeyO: 'fadeOut',
        KeyD: 'startDelay',
        KeyV: 'level',
      };
      if (fields[e.code]) {
        e.preventDefault();
        openQuick(fields[e.code]);
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
      project={project}
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
        setWorkspace((w) => ({
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
  return (
    <div className="app-shell">
      <header className="app-header">
        <div className="brand">
          <span className="brand-mark">
            <Layers3 size={20} />
          </span>
          <div>
            <h1>
              {PRODUCT_NAME} <small>{INTERFACE_VERSION}</small>
            </h1>
            <input
              className="project-name"
              disabled={!backendReady}
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
              disabled={selectedIndex <= 0}
              onClick={() => reorder(selectedStepId, selectedIndex - 1)}
            >
              <ChevronLeft size={14} />
            </button>
            <button
              className="icon-button"
              aria-label="Шаг правее"
              title="Переместить шаг вправо"
              disabled={selectedIndex === project.steps.length - 1}
              onClick={() => reorder(selectedStepId, selectedIndex + 1)}
            >
              <ChevronRight size={14} />
            </button>
            <button
              className="icon-button delete-button"
              aria-label="Удалить шаг"
              title="Удалить шаг"
              disabled={project.steps.length === 1}
              onClick={remove}
            >
              <Trash2 size={14} />
            </button>
          </div>
        </div>
        <div className="header-actions">
          <label
            className="preview-toggle"
            title="Включён: выбор и правки сразу исполняются. Выключен: синхронизация памяти без автозапуска."
          >
            <input
              type="checkbox"
              role="switch"
              aria-label="Предпросмотр"
              checked={!showMode}
              disabled={publishing || !backendReady}
              onChange={(e) => void changePreviewMode(e.target.checked)}
            />
            Предпросмотр
          </label>
          {useEspBackend && (
            <span className="sync-chip" role="status">
              {!backendReady
                ? 'Чтение партитуры…'
                : publishing || editorSyncing
                  ? 'Синхронизация…'
                  : preload.pending
                    ? `Память ${preload.stored}/${preload.total}`
                    : !showMode
                      ? 'Черновик · LIVE'
                      : 'Память контроллеров'}
            </span>
          )}
          <button
            className="icon-button"
            aria-label="Приборы"
            disabled={!backendReady}
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
            disabled={!backendReady}
            title="Эффекты ленты"
            onClick={() => setDialog('effects')}
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
            disabled={exporting}
          >
            <Upload size={17} />
          </button>
          {!useEspBackend && (
            <button
              className="icon-button publish-button"
              aria-label="Опубликовать"
              title="Опубликовать"
              disabled={publishing || !dirty}
              onClick={publish}
            >
              <Radio size={18} />
            </button>
          )}
          {published && (
            <span className="version-chip" title="Сохранённая версия">
              v{published.version}
              {dirty ? '*' : ''}
            </span>
          )}
          <span
            className={`save-dot ${saveState}`}
            title={
              saveState === 'saved'
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
          <QuickToolbar
            count={selectedCells.length || (selection ? 1 : 0)}
            scope={editScope}
            onScope={setEditScope}
            onEdit={openQuick}
            onLevel={(n) => applyQuick('level', n)}
            onHelp={() => setShortcutHelp(true)}
            custom={project.layout?.mode === 'custom'}
            onLayout={() =>
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
              )
            }
            canUndo={!!history.past.length}
            canRedo={!!history.future.length}
            onUndo={undo}
            onRedo={redo}
          />
          <Matrix
            network={network}
            project={project}
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
      {dialog === 'helmet' && helmetDevice && (
        <HelmetDialog
          device={helmetDevice}
          network={network}
          onClose={() => setDialog(null)}
          onChange={(device) =>
            setWorkspace((w) => ({
              ...w,
              draft: touch(w.draft, {
                devices: w.draft.devices.map((d) => (d.id === device.id ? device : d)),
              }),
            }))
          }
        />
      )}
      {dialog === 'network' && (
        <NetworkDialog project={project} transport={network} onClose={() => setDialog(null)} />
      )}
      {shortcutHelp && <ShortcutHelp onClose={() => setShortcutHelp(false)} />}
      {quickEdit && (
        <QuickValueDialog
          field={quickEdit.field}
          count={quickEdit.cells.length}
          devices={new Set(quickEdit.cells.map((c) => c.deviceId)).size}
          steps={new Set(quickEdit.cells.map((c) => c.stepId)).size}
          onApply={(n) => applyQuick(quickEdit.field, n, quickEdit.cells)}
          onClose={() => setQuickEdit(null)}
        />
      )}
      {toast && (
        <div className="toast" role="status" onClick={() => setToast('')}>
          {toast}
        </div>
      )}
    </div>
  );
}
