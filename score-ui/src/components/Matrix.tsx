import {
  Fragment,
  memo,
  useCallback,
  useMemo,
  useRef,
  useState,
  useSyncExternalStore,
} from 'react';
import type { CSSProperties, DragEvent } from 'react';
import {
  ChevronDown,
  ChevronRight,
  GripVertical,
  Link2,
  Music2,
  Clock3,
  Repeat2,
  Power,
  Settings2,
  Waves,
} from 'lucide-react';
import type { CellSelection, Column, DeviceCue, Project, StripEffect } from '../domain/schema.ts';
import { COLORS } from '../domain/schema.ts';
import { parameterCode, acCue } from '../domain/project.ts';
import {
  cellKey,
  editorRows,
  rowKey,
  type Modifiers,
  type RowDropPosition,
} from '../domain/editor.ts';
const ROW_MIME = 'application/x-theatre-row',
  ROWS_MIME = 'application/x-theatre-rows',
  DEVICE_MIME = 'application/x-theatre-device';
type Dragging = { kind: 'rows'; keys: string[] } | { kind: 'device'; id: string };
type DropTarget = { kind: 'rows' | 'device'; key: string; position: RowDropPosition };
import type { NetworkTransport } from '../transport/network.ts';
const DeviceIP = ({
  mac,
  fallback,
  network,
}: {
  mac?: string | null;
  fallback?: string | null;
  network: NetworkTransport;
}) => {
  const snapshot = useSyncExternalStore(network.subscribe, network.getSnapshot);
  return <code>{snapshot.devices[mac?.toUpperCase() ?? '']?.ip ?? fallback ?? '—'}</code>;
};
interface Props {
  network: NetworkTransport;
  project: Project;
  selectedStepId: string;
  activeStepId: string | null;
  selection: CellSelection | null;
  compact: boolean;
  selectedCells: CellSelection[];
  selectedStepIds: string[];
  collapsedDevices: string[];
  onToggleDevice: (id: string) => void;
  onMoveRow: (source: string, target: string) => void;
  onMoveRows: (sources: string[], target: string, position: RowDropPosition) => void;
  onSelectRow: (key: string, mods: Modifiers) => void;
  onMoveDevice: (id: string, index: number) => void;
  onSelectStep: (id: string, mods?: Modifiers) => void;
  onSelectCell: (c: CellSelection, mods?: Modifiers) => void;
  onChangeCell: (c: CellSelection, changes: Record<string, unknown>) => void;
  onMove: (id: string, destination: number) => void;
  onDevices: (id?: string) => void;
}
const ScoreCell = memo(function ScoreCell(p: {
  row: Column;
  cue: DeviceCue;
  previousTrack: number | null;
  stepId: string;
  index: number;
  effects: StripEffect[];
  selected: boolean;
  selectedColumn: boolean;
  active: boolean;
  onSelectCell: Props['onSelectCell'];
  onChangeCell: Props['onChangeCell'];
}) {
  const { device, parameter, kind } = p.row;
  const cell: CellSelection = {
    stepId: p.stepId,
    deviceId: device.id,
    parameterId: parameter.id,
    kind,
  };
  const label = `${device.name} · ${parameter.name} · шаг ${p.index + 1}`;
  let content;
  let lit = false;
  let patch: Record<string, unknown> | null = null;
  if (kind === 'dimmer' || kind === 'ac') {
    const light = kind === 'ac' ? acCue(p.cue) : p.cue.dimmers[parameter.id];
    lit = light.value > 0;
    patch = { value: lit ? 0 : 100 };
    content = (
      <>
        {light.animation?.enabled && light.animation.mode !== 'static' && (
          <Waves size={11} aria-label="Анимация DIM" />
        )}
        {!!light.startDelay && <Clock3 size={10} aria-label="Задержка света" />}
        <span>{light.value}</span>
        <small>%</small>
      </>
    );
  } else if (kind === 'relay') {
    lit = p.cue.relays[parameter.id].on;
    patch = { on: !lit };
    content = (
      <>
        <Power size={12} />
        {lit ? 'ВКЛ' : 'ВЫКЛ'}
      </>
    );
  } else if (kind === 'audio' && device.profile === 'helmet') {
    lit = !!p.cue.audio.voiceOn;
    patch = { voiceOn: !lit };
    content = (
      <>
        <Power size={12} />
        {lit ? 'ВКЛ' : 'ВЫКЛ'}
      </>
    );
  } else if (kind === 'audio') {
    const track = p.cue.audio.track;
    lit = track !== null;
    patch = {
      track: lit ? null : (p.cue.audio.lastTrack ?? 1),
      ...(!lit && p.cue.audio.volume === 0 ? { volume: 100 } : {}),
    };
    content = (
      <>
        {track && <Music2 size={12} />}{' '}
        {!!track && p.cue.audio.loop && <Repeat2 size={10} aria-label="Зациклить трек" />}
        <span>{track ?? '—'}</span>
        {track !== null && track === p.previousTrack && <Link2 size={11} />}
      </>
    );
  } else {
    const strip = p.cue.strip;
    lit = strip.action === 'play';
    content = (
      <>
        {lit && <Waves size={12} />}
        <span>
          {strip.action === 'hold'
            ? '—'
            : strip.action === 'stop'
              ? 'СТОП'
              : (p.effects.find((f) => f.id === strip.effectId)?.name ?? '—')}
        </span>
      </>
    );
  }
  return (
    <td
      data-step-id={p.stepId}
      data-device={device.id}
      data-parameter={parameter.id}
      aria-selected={p.selected}
      className={`matrix-cell ${p.selected ? 'selected-cell' : ''} ${p.selectedColumn ? 'selected-column' : ''} ${p.active ? 'active-column' : ''}`}
      style={{ '--channel-color': COLORS[parameter.color].hex } as CSSProperties}
    >
      <button
        className={`value-button ${lit ? 'lit' : 'zero'} ${kind === 'strip' ? 'strip-value' : ''}`}
        aria-label={label}
        aria-pressed={lit}
        title="Клик — выделить; двойной клик — включить/выключить. Shift — диапазон, ⌘/Ctrl — добавить."
        onClick={(e) => p.onSelectCell(cell, e)}
        onDoubleClick={() => {
          if (patch) p.onChangeCell(cell, patch);
        }}
      >
        {content}
      </button>
    </td>
  );
});
export function Matrix(p: Props) {
  const { project } = p;
  const custom = project.layout?.mode === 'custom';
  const rows = useMemo(
    () => editorRows(project, p.collapsedDevices),
    [project.devices, project.layout, p.collapsedDevices],
  );
  const selected = new Set(p.selectedCells.map(cellKey));
  const selectedRows = new Set(p.selectedCells.map(rowKey));
  const [dragging, setDragging] = useState<Dragging | null>(null);
  const [dropTarget, setDropTarget] = useState<DropTarget | null>(null);
  const endDrag = () => {
    setDragging(null);
    setDropTarget(null);
  };
  const startRow = (e: DragEvent, row: Column) => {
    e.stopPropagation();
    const key = rowKey(row),
      keys =
        custom && selectedRows.has(key)
          ? rows.filter((r) => selectedRows.has(rowKey(r))).map(rowKey)
          : [key];
    e.dataTransfer.setData(ROW_MIME, key);
    e.dataTransfer.setData(ROWS_MIME, JSON.stringify(keys));
    e.dataTransfer.effectAllowed = 'move';
    setDragging({ kind: 'rows', keys });
    if (!selectedRows.has(key)) p.onSelectRow(key, {});
  };
  const startDevice = (e: DragEvent, id: string) => {
    e.stopPropagation();
    e.dataTransfer.setData(DEVICE_MIME, id);
    e.dataTransfer.effectAllowed = 'move';
    setDragging({ kind: 'device', id });
  };
  const targetAt = (
    e: DragEvent<HTMLTableRowElement>,
    deviceId: string,
    key?: string,
  ): DropTarget | null => {
    if (!custom && e.dataTransfer.types.includes(DEVICE_MIME)) {
      if (dragging?.kind === 'device' && dragging.id === deviceId) return null;
      const body = e.currentTarget.parentElement,
        group = body?.querySelector(`[data-device-group="${CSS.escape(deviceId)}"]`),
        last = body?.querySelectorAll(`.parameter-row[data-device="${CSS.escape(deviceId)}"]`),
        top = (group ?? e.currentTarget).getBoundingClientRect().top,
        bottom = (
          last?.length ? last[last.length - 1] : (group ?? e.currentTarget)
        ).getBoundingClientRect().bottom;
      return {
        kind: 'device',
        key: deviceId,
        position: e.clientY > (top + bottom) / 2 ? 'after' : 'before',
      };
    }
    if (!key || !e.dataTransfer.types.includes(ROW_MIME)) return null;
    if (
      dragging?.kind === 'rows' &&
      (dragging.keys.includes(key) ||
        (!custom &&
          dragging.keys.some(
            (k) => !rows.some((r) => rowKey(r) === k && r.device.id === deviceId),
          )))
    )
      return null;
    const box = e.currentTarget.getBoundingClientRect();
    return {
      kind: 'rows',
      key,
      position: e.clientY > box.top + box.height / 2 ? 'after' : 'before',
    };
  };
  const dragOver = (e: DragEvent<HTMLTableRowElement>, deviceId: string, key?: string) => {
    const target = targetAt(e, deviceId, key);
    if (!target) {
      setDropTarget(null);
      return;
    }
    e.preventDefault();
    e.stopPropagation();
    e.dataTransfer.dropEffect = 'move';
    setDropTarget((old) =>
      old?.kind === target.kind && old.key === target.key && old.position === target.position
        ? old
        : target,
    );
  };
  const dropRows = (e: DragEvent<HTMLTableRowElement>, deviceId: string, key?: string) => {
    const target = targetAt(e, deviceId, key);
    if (target) {
      e.preventDefault();
      e.stopPropagation();
      if (target.kind === 'device') {
        const id = e.dataTransfer.getData(DEVICE_MIME),
          from = project.devices.findIndex((d) => d.id === id);
        let to =
          project.devices.findIndex((d) => d.id === target.key) +
          (target.position === 'after' ? 1 : 0);
        if (from >= 0) {
          if (from < to) to--;
          p.onMoveDevice(id, to);
        }
      } else {
        try {
          const keys: unknown = e.dataTransfer.getData(ROWS_MIME)
            ? JSON.parse(e.dataTransfer.getData(ROWS_MIME))
            : [e.dataTransfer.getData(ROW_MIME)];
          if (Array.isArray(keys) && keys.every((k) => typeof k === 'string'))
            p.onMoveRows(keys, target.key, target.position);
        } catch {
          /* Ignore an unrelated/malformed drag, never mutate cues. */
        }
      }
    }
    endDrag();
  };
  const indicator = (kind: DropTarget['kind'], key: string, position: RowDropPosition) =>
    dropTarget?.kind === kind && dropTarget.key === key && dropTarget.position === position;
  const latest = useRef({ steps: project.steps, onMove: p.onMove });
  latest.current = { steps: project.steps, onMove: p.onMove };
  const drop = useCallback((index: number, after: boolean, id: string) => {
    const { steps, onMove } = latest.current;
    if (!id || !steps.some((s) => s.id === id)) return;
    const from = steps.findIndex((s) => s.id === id);
    let to = index + (after ? 1 : 0);
    if (from < to) to--;
    onMove(id, to);
  }, []);
  const renderRow = (row: Column) => {
    const key = rowKey(row),
      i = rows.findIndex((r) => rowKey(r) === key),
      up = rows[i - 1],
      down = rows[i + 1];
    return (
      <tr
        key={key}
        className={`parameter-row ${selectedRows.has(key) ? 'selected-row' : ''} ${(dragging?.kind === 'rows' && dragging.keys.includes(key)) || (dragging?.kind === 'device' && dragging.id === row.device.id) ? 'drag-source' : ''} ${indicator('rows', key, 'before') ? 'drop-before' : ''} ${indicator('rows', key, 'after') || (!custom && down?.device.id !== row.device.id && indicator('device', row.device.id, 'after')) ? 'drop-after' : ''}`}
        aria-selected={selectedRows.has(key)}
        data-device={row.device.id}
        data-parameter={row.parameter.id}
        onDragOver={(e) => dragOver(e, row.device.id, key)}
        onDrop={(e) => dropRows(e, row.device.id, key)}
      >
        <th className="parameter-column" draggable onDragStart={(e) => startRow(e, row)}>
          <div className="row-header">
            <button
              className="drag-handle"
              draggable
              aria-label={`Переместить параметр ${row.device.name} · ${row.parameter.name}`}
              title={
                custom
                  ? 'Перетащи выделенные каналы группой; Alt+↑/↓ — одна строка'
                  : 'Перетащи канал внутри прибора; Alt+↑/↓'
              }
              onDragStart={(e) => startRow(e, row)}
            >
              <GripVertical size={12} />
            </button>
            <button
              className="row-select"
              draggable
              onDragStart={(e) => startRow(e, row)}
              aria-label={`Выделить канал ${row.device.name} · ${row.parameter.name}`}
              aria-pressed={selectedRows.has(key)}
              title="Выделить канал во всех шагах; Shift — диапазон, ⌘/Ctrl — несколько каналов. Перетаскивай за название."
              onClick={(e) => p.onSelectRow(key, e)}
            >
              <span
                className="channel-dot"
                style={{ background: COLORS[row.parameter.color].hex }}
              />
              <code>{parameterCode(row.device, row.parameter)}</code>
              <span className="row-name">
                {custom ? `${row.device.name} · ` : ''}
                {row.parameter.name}
              </span>
            </button>
            <span className="row-order">
              <button
                aria-label={`Строка выше: ${row.device.name} · ${row.parameter.name}`}
                disabled={!up || (!custom && up.device.id !== row.device.id)}
                onClick={() => up && p.onMoveRow(key, rowKey(up))}
              >
                ↑
              </button>
              <button
                aria-label={`Строка ниже: ${row.device.name} · ${row.parameter.name}`}
                disabled={!down || (!custom && down.device.id !== row.device.id)}
                onClick={() => down && p.onMoveRow(key, rowKey(down))}
              >
                ↓
              </button>
            </span>
          </div>
        </th>
        {project.steps.map((step, index) => {
          const cell = {
            stepId: step.id,
            deviceId: row.device.id,
            parameterId: row.parameter.id,
            kind: row.kind,
          };
          return (
            <ScoreCell
              key={step.id}
              row={row}
              cue={step.devices[row.device.id]}
              previousTrack={project.steps[index - 1]?.devices[row.device.id]?.audio.track ?? null}
              stepId={step.id}
              index={index}
              effects={project.effects}
              selected={
                selected.has(cellKey(cell)) ||
                (!selected.size && !!p.selection && cellKey(p.selection) === cellKey(cell))
              }
              selectedColumn={p.selectedStepIds.includes(step.id) || p.selectedStepId === step.id}
              active={p.activeStepId === step.id}
              onSelectCell={p.onSelectCell}
              onChangeCell={p.onChangeCell}
            />
          );
        })}
      </tr>
    );
  };
  return (
    <div
      className="matrix-scroll"
      onDragEnd={endDrag}
      onDragLeave={(e) => {
        if (!e.currentTarget.contains(e.relatedTarget as Node | null)) setDropTarget(null);
      }}
    >
      {dragging && (
        <div className="matrix-drag-status" role="status">
          {dragging.kind === 'rows'
            ? `Перенос каналов: ${dragging.keys.length}`
            : 'Перенос прибора целиком'}
        </div>
      )}
      <table className="score-matrix" aria-label="Партитура">
        <colgroup>
          <col className="parameter-column" />
          {project.steps.map((s) => (
            <col className="step-column" key={s.id} />
          ))}
        </colgroup>
        <thead>
          <tr>
            <th className="parameter-column corner-cell">{custom ? 'Своя партитура' : 'Прибор'}</th>
            {project.steps.map((step, index) => (
              <th
                key={step.id}
                id={`step-${step.id}`}
                data-step-id={step.id}
                className={`step-heading ${p.selectedStepId === step.id || p.selectedStepIds.includes(step.id) ? 'selected-column' : ''} ${p.activeStepId === step.id ? 'active-column' : ''}`}
                onDragOver={(e) => {
                  e.preventDefault();
                  e.dataTransfer.dropEffect = 'move';
                }}
                onDrop={(e) => {
                  e.preventDefault();
                  drop(
                    index,
                    e.clientX >
                      e.currentTarget.getBoundingClientRect().left +
                        e.currentTarget.getBoundingClientRect().width / 2,
                    e.dataTransfer.getData('text/plain'),
                  );
                }}
              >
                <button
                  className="drag-handle"
                  draggable
                  aria-label={`Переместить шаг ${index + 1}`}
                  title="Перетащить шаг"
                  onDragStart={(e) => {
                    e.dataTransfer.setData('text/plain', step.id);
                    e.dataTransfer.effectAllowed = 'move';
                  }}
                >
                  <GripVertical size={12} />
                </button>
                <button
                  className="step-select"
                  aria-label={`Редактировать шаг ${index + 1}`}
                  title="Shift — диапазон шагов; ⌘/Ctrl — добавить шаг"
                  onClick={(e) => p.onSelectStep(step.id, e)}
                >
                  <span className={`step-indicator ${p.activeStepId === step.id ? 'live' : ''}`} />
                  {index + 1}
                </button>
              </th>
            ))}
          </tr>
        </thead>
        <tbody>
          {custom
            ? rows.map(renderRow)
            : project.devices.map((device) => (
                <Fragment key={device.id}>
                  <tr
                    className={`device-group ${dragging?.kind === 'device' && dragging.id === device.id ? 'drag-source' : ''} ${indicator('device', device.id, 'before') ? 'drop-before' : ''} ${!rows.some((r) => r.device.id === device.id) && indicator('device', device.id, 'after') ? 'drop-after' : ''}`}
                    data-device-group={device.id}
                    onDragOver={(e) => dragOver(e, device.id)}
                    onDrop={(e) => dropRows(e, device.id)}
                  >
                    <th
                      className="parameter-column"
                      draggable
                      onDragStart={(e) => startDevice(e, device.id)}
                      title="Перетащи блок прибора целиком"
                    >
                      <div>
                        <button
                          className="drag-handle"
                          draggable
                          aria-label={`Переместить прибор ${device.name}`}
                          title="Перетащи прибор целиком"
                          onDragStart={(e) => startDevice(e, device.id)}
                        >
                          <GripVertical size={12} />
                        </button>
                        <button
                          className="icon-button collapse-device"
                          aria-label={`${p.collapsedDevices.includes(device.id) ? 'Развернуть' : 'Свернуть'} ${device.name}`}
                          aria-expanded={!p.collapsedDevices.includes(device.id)}
                          onClick={() => p.onToggleDevice(device.id)}
                        >
                          {p.collapsedDevices.includes(device.id) ? (
                            <ChevronRight size={14} />
                          ) : (
                            <ChevronDown size={14} />
                          )}
                        </button>
                        <button
                          className="device-name"
                          draggable
                          onDragStart={(e) => startDevice(e, device.id)}
                          onClick={() => p.onDevices(device.id)}
                        >
                          {device.name}
                        </button>
                        <button
                          className="icon-button"
                          aria-label={`Настройки ${device.name}`}
                          onClick={() => p.onDevices(device.id)}
                        >
                          <Settings2 size={14} />
                        </button>
                      </div>
                      <DeviceIP mac={device.mac} fallback={device.ip} network={p.network} />
                    </th>
                    {project.steps.map((step) => (
                      <td
                        key={step.id}
                        className={`${p.selectedStepId === step.id ? 'selected-column' : ''} ${p.activeStepId === step.id ? 'active-column' : ''}`}
                      />
                    ))}
                  </tr>
                  {rows.filter((row) => row.device.id === device.id).map(renderRow)}
                </Fragment>
              ))}
        </tbody>
      </table>
    </div>
  );
}
