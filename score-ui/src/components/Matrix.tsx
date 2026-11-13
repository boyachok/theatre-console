import { Fragment, memo, useCallback, useMemo, useRef, useSyncExternalStore } from 'react';
import type { CSSProperties } from 'react';
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
import { cellKey, editorRows, rowKey, type Modifiers } from '../domain/editor.ts';
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
        className="parameter-row"
        data-device={row.device.id}
        data-parameter={row.parameter.id}
        onDragOver={(e) => {
          if (e.dataTransfer.types.includes('application/x-theatre-row')) e.preventDefault();
        }}
        onDrop={(e) => {
          const source = e.dataTransfer.getData('application/x-theatre-row');
          if (source) {
            e.preventDefault();
            e.stopPropagation();
            p.onMoveRow(source, key);
          }
        }}
      >
        <th className="parameter-column">
          <div className="row-header">
            <button
              className="drag-handle"
              draggable
              aria-label={`Переместить параметр ${row.device.name} · ${row.parameter.name}`}
              title="Перетащи строку; Alt+↑/↓"
              onDragStart={(e) => {
                e.dataTransfer.setData('application/x-theatre-row', key);
                e.dataTransfer.effectAllowed = 'move';
              }}
            >
              <GripVertical size={12} />
            </button>
            <span className="channel-dot" style={{ background: COLORS[row.parameter.color].hex }} />
            <code>{parameterCode(row.device, row.parameter)}</code>
            <span className="row-name" title={`${row.device.name} · ${row.parameter.name}`}>
              {custom ? `${row.device.name} · ` : ''}
              {row.parameter.name}
            </span>
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
    <div className="matrix-scroll">
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
            : project.devices.map((device, index) => (
                <Fragment key={device.id}>
                  <tr
                    className="device-group"
                    data-device-group={device.id}
                    onDragOver={(e) => {
                      if (e.dataTransfer.types.includes('application/x-theatre-device'))
                        e.preventDefault();
                    }}
                    onDrop={(e) => {
                      const id = e.dataTransfer.getData('application/x-theatre-device');
                      if (id) {
                        e.preventDefault();
                        p.onMoveDevice(id, index);
                      }
                    }}
                  >
                    <th className="parameter-column">
                      <div>
                        <button
                          className="drag-handle"
                          draggable
                          aria-label={`Переместить прибор ${device.name}`}
                          title="Перетащи прибор целиком"
                          onDragStart={(e) => {
                            e.dataTransfer.setData('application/x-theatre-device', device.id);
                            e.dataTransfer.effectAllowed = 'move';
                          }}
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
                        <button className="device-name" onClick={() => p.onDevices(device.id)}>
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
