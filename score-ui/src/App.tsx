import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
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
  updateCell,
  updateStep,
} from './domain/project.ts';
import { PreviewEngine } from './domain/playback.ts';
import type { TransitionReason } from './domain/playback.ts';
import { parseBackup } from './domain/validation.ts';
import {
  downloadWorkspace,
  emptyWorkspace,
  loadWorkspace,
  saveWorkspace,
} from './storage/workspace.ts';
import { LocalPreviewTransport } from './transport/preview.ts';
import type { ScoreTransport } from './transport/preview.ts';
import { EspConsoleTransport, useEspBackend } from './transport/esp.ts';
import { DisconnectedNetworkTransport } from './transport/network.ts';
import { Matrix } from './components/Matrix.tsx';
import { Inspector } from './components/Inspector.tsx';
import { DevicesDialog } from './components/DevicesDialog.tsx';
import { EffectsDialog } from './components/EffectsDialog.tsx';
import { NetworkDialog } from './components/NetworkDialog.tsx';
import { Modal } from './components/Modal.tsx';
import { StepPlayer } from './components/StepPlayer.tsx';
import { INTERFACE_VERSION } from './version.ts';
const previewSignature = (project: Project, step: Step) =>
  JSON.stringify([step.devices, step.timings, project.devices, project.effects]);
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
  const [initial] = useState(startup),
    [workspace, setWorkspace] = useState<Workspace>(initial.workspace);
  const project = workspace.draft,
    latest = useRef(workspace);
  latest.current = workspace;
  const [engine] = useState(() => new PreviewEngine(initial.workspace.draft));
  const [network] = useState(() =>
    useEspBackend ? new EspConsoleTransport() : new DisconnectedNetworkTransport(),
  );
  const [backendReady, setBackendReady] = useState(!useEspBackend);
  const requestBusy = useRef(false),
    appliedSignature = useRef('');
  const transition = useRef(0),
    [activeId, setActiveId] = useState<string | null>(null);
  const [selectedStepId, setSelectedStepId] = useState(project.steps[0].id),
    [selection, setSelection] = useState<CellSelection | null>(null);
  const [deviceId, setDeviceId] = useState(project.devices[0].id),
    [compact, setCompact] = useState(() => window.matchMedia('(max-width: 960px)').matches);
  const [inspectorOpen, setInspectorOpen] = useState(false),
    [dialog, setDialog] = useState<'devices' | 'effects' | 'network' | null>(null);
  const [deviceDialogId, setDeviceDialogId] = useState<string | undefined>();
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
      if (!mounted || requestBusy.current) return;
      const state = network.getSnapshot();
      if (!state.connected) return;
      const source = state.preview ? latest.current.draft : latest.current.published?.project;
      const target = state.activeStep == null ? null : source?.steps[state.activeStep - 1];
      if (!target) {
        setActiveId(null);
        return;
      }
      setActiveId(target.id);
      if (engine.getSnapshot().stepId !== target.id && source) {
        engine.apply(source, target, 'recover', ++transition.current);
        appliedSignature.current = previewSignature(source, target);
      }
    });
    network
      .loadWorkspace()
      .then((saved) => {
        if (!mounted) return;
        if (saved) {
          setWorkspace(saved);
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
  const changeCell = useCallback(
    (cell: CellSelection, changes: Record<string, unknown>) =>
      setWorkspace((w) => ({ ...w, draft: updateCell(w.draft, cell, changes) })),
    [],
  );
  const changeStep = useCallback(
    (id: string, changes: Partial<Step>) =>
      setWorkspace((w) => ({
        ...w,
        draft: updateStep(w.draft, id, (step) => ({ ...step, ...changes })),
      })),
    [],
  );
  const selectStep = useCallback((id: string) => {
    setSelectedStepId(id);
    setSelection(null);
    setInspectorOpen(true);
  }, []);
  const selectCell = useCallback((cell: CellSelection) => {
    setSelectedStepId(cell.stepId);
    setDeviceId(cell.deviceId);
    setSelection(cell);
    setInspectorOpen(true);
  }, []);
  const reorder = useCallback(
    (id: string, index: number) =>
      setWorkspace((w) => ({ ...w, draft: moveStep(w.draft, id, index) })),
    [],
  );
  const duplicate = useCallback(() => {
    const w = latest.current,
      result = duplicateStep(w.draft, selectedRef.current);
    setWorkspace({ ...w, draft: result.project });
    setSelectedStepId(result.id);
    setSelection(null);
    requestAnimationFrame(() =>
      document
        .getElementById(`step-${result.id}`)
        ?.scrollIntoView({ block: 'nearest', inline: 'nearest' }),
    );
  }, []);
  const remove = () => {
    const index = project.steps.findIndex((s) => s.id === selectedStepId);
    if (project.steps.length <= 1 || !confirm(`Удалить шаг ${index + 1}?`)) return;
    const steps = project.steps.filter((s) => s.id !== selectedStepId);
    setWorkspace((w) => ({ ...w, draft: touch(w.draft, { steps }) }));
    setSelectedStepId(steps[Math.min(index, steps.length - 1)].id);
    setSelection(null);
  };
  const run = useCallback(
    async (id: string, reason: TransitionReason) => {
      const p = latest.current.draft,
        step = p.steps.find((s) => s.id === id);
      if (!step || requestBusy.current) return;
      const signature = previewSignature(p, step);
      if (engine.getSnapshot().stepId === id && appliedSignature.current === signature) return;
      if (network instanceof EspConsoleTransport) {
        try {
          requestBusy.current = true;
          await network.preview(p, step, reason === 'back' || reason === 'first');
        } catch (error) {
          setToast(error instanceof Error ? error.message : 'Нет подтверждения мастера');
          return;
        } finally {
          requestBusy.current = false;
        }
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
    [engine, network],
  );
  const navigate = useCallback(
    (action: 'back' | 'forward' | 'first' | 'last') => {
      const steps = latest.current.draft.steps,
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
    [run, engine, network],
  );
  const choosePlaybackStep = (id: string) => {
    const before = project.steps.findIndex((s) => s.id === engine.getSnapshot().stepId),
      after = project.steps.findIndex((s) => s.id === id);
    run(id, after < before ? 'back' : 'select');
  };
  const publish = async () => {
    setPublishing(true);
    try {
      const published = await transport.current.publish(workspace.draft, workspace.published);
      setWorkspace((w) => ({ ...w, published }));
      if (network instanceof EspConsoleTransport) appliedSignature.current = '';
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
      if (!confirm('Заменить текущую партитуру из JSON?')) return;
      setWorkspace({ ...next, interfaceVersion: INTERFACE_VERSION });
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
    if (!devices.some((d) => d.id === deviceId)) setDeviceId(devices[0].id);
    setSelection(null);
  };
  const saveEffects = (effects: StripEffect[]) =>
    setWorkspace((w) => ({ ...w, draft: touch(w.draft, { effects }) }));
  const openDevices = useCallback((id?: string) => {
    setDeviceDialogId(id);
    setDialog('devices');
  }, []);
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
              Консоль <small>{INTERFACE_VERSION}</small>
            </h1>
            <input
              className="project-name"
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
        <div className="step-control-strip">
          <StepPlayer
            steps={project.steps}
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
          <button
            className="icon-button"
            aria-label="Приборы"
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
            title="Эффекты ленты"
            onClick={() => setDialog('effects')}
          >
            <Waves size={18} />
          </button>
          <span className="header-divider" />
          <button
            className="icon-button"
            aria-label="Скачать JSON"
            title="Скачать JSON"
            onClick={() => downloadWorkspace(workspace)}
          >
            <Download size={17} />
          </button>
          <button
            className="icon-button"
            aria-label="Загрузить JSON"
            title="Загрузить JSON"
            onClick={() => importInput.current?.click()}
          >
            <Upload size={17} />
          </button>
          <button
            className="icon-button publish-button"
            aria-label="Опубликовать"
            title="Опубликовать"
            disabled={publishing || !dirty}
            onClick={publish}
          >
            <Radio size={18} />
          </button>
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
      <main className="workspace">
        <section className="matrix-area">
          <Matrix
            network={network}
            project={project}
            selectedStepId={selectedStepId}
            activeStepId={activeId}
            selection={selection}
            compact={compact}
            onSelectStep={selectStep}
            onSelectCell={selectCell}
            onChangeCell={changeCell}
            onMove={reorder}
            onDevices={openDevices}
          />
        </section>
        {!compact && <aside className="inspector">{inspector}</aside>}
      </main>
      {compact && inspectorOpen && (
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
      {dialog === 'network' && (
        <NetworkDialog project={project} transport={network} onClose={() => setDialog(null)} />
      )}
      {toast && (
        <div className="toast" role="status" onClick={() => setToast('')}>
          {toast}
        </div>
      )}
    </div>
  );
}
