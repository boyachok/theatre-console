import {
  Fragment,
  memo,
  useCallback,
  useMemo,
  useRef,
  useState,
  useSyncExternalStore,
} from 'react';
import type { CSSProperties } from 'react';
import {
  ChevronDown,
  ChevronRight,
  GripVertical,
  Link2,
  Music2,
  Power,
  Settings2,
  Waves,
} from 'lucide-react';
import type { CellSelection, Column, DeviceCue, Project, StripEffect } from '../domain/schema.ts';
import { COLORS } from '../domain/schema.ts';
import { clamp, columnsFor, parameterCode } from '../domain/project.ts';
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
  onSelectStep: (id: string) => void;
  onSelectCell: (cell: CellSelection) => void;
  onChangeCell: (cell: CellSelection, changes: Record<string, unknown>) => void;
  onMove: (id: string, destination: number) => void;
  onDevices: (id?: string) => void;
}
const ScoreCell = memo(function ScoreCell({
  row,
  cue,
  previousTrack,
  stepId,
  index,
  effects,
  selected,
  selectedColumn,
  active,
  compact,
  onSelectCell,
  onChangeCell,
}: {
  row: Column;
  cue: DeviceCue;
  previousTrack: number | null;
  stepId: string;
  index: number;
  effects: StripEffect[];
  selected: boolean;
  selectedColumn: boolean;
  active: boolean;
  compact: boolean;
  onSelectCell: Props['onSelectCell'];
  onChangeCell: Props['onChangeCell'];
}) {
  const { device, parameter, kind } = row;
  const cell: CellSelection = { stepId, deviceId: device.id, parameterId: parameter.id, kind };
  const label = `${device.name} · ${parameter.name} · шаг ${index + 1}`;
  let content;
  if (kind === 'dimmer') {
    const value = cue.dimmers[parameter.id].value;
    content =
      selected && !compact ? (
        <input
          autoFocus
          className="matrix-input"
          aria-label={label}
          type="number"
          min="0"
          max="100"
          inputMode="numeric"
          value={value}
          onChange={(e) => onChangeCell(cell, { value: clamp(Number(e.target.value), 0, 100) })}
          onKeyDown={(e) => {
            if (e.key === 'Enter') e.currentTarget.blur();
          }}
        />
      ) : (
        <button
          className={`value-button ${value > 0 ? 'lit' : 'zero'}`}
          aria-label={label}
          onClick={() => {
            if (value === 0) onChangeCell(cell, { value: 100 });
            onSelectCell(cell);
          }}
        >
          <span>{value}</span>
          <small>%</small>
        </button>
      );
  } else if (kind === 'relay') {
    const on = cue.relays[parameter.id].on;
    content = (
      <button
        className={`value-button relay-value ${on ? 'lit' : 'zero'}`}
        aria-label={label}
        aria-pressed={on}
        onClick={() => {
          onSelectCell(cell);
          onChangeCell(cell, { on: !on });
        }}
      >
        <Power size={12} />
        {on ? 'ВКЛ' : 'ВЫКЛ'}
      </button>
    );
  } else if (kind === 'audio') {
    const track = cue.audio.track;
    content =
      selected && !compact ? (
        <input
          autoFocus
          className="matrix-input"
          aria-label={label}
          type="number"
          min="1"
          max="65535"
          placeholder="—"
          inputMode="numeric"
          value={track ?? ''}
          onChange={(e) =>
            onChangeCell(cell, {
              track:
                e.target.value === '' ? null : Math.round(clamp(Number(e.target.value), 1, 65535)),
            })
          }
          onKeyDown={(e) => {
            if (e.key === 'Enter') e.currentTarget.blur();
          }}
        />
      ) : (
        <button
          className={`value-button ${track ? 'lit' : 'zero'}`}
          aria-label={label}
          onClick={() => onSelectCell(cell)}
        >
          {track && <Music2 size={12} />}
          <span>{track ?? '—'}</span>
          {track !== null && track === previousTrack && <Link2 size={11} />}
        </button>
      );
  } else {
    const strip = cue.strip;
    content = (
      <button
        className={`value-button strip-value ${strip.action === 'play' ? 'lit' : 'zero'}`}
        aria-label={label}
        onClick={() => onSelectCell(cell)}
      >
        {strip.action === 'play' && <Waves size={12} />}
        <span>
          {strip.action === 'hold'
            ? '—'
            : strip.action === 'stop'
              ? 'СТОП'
              : (effects.find((f) => f.id === strip.effectId)?.name ?? '—')}
        </span>
      </button>
    );
  }
  return (
    <td
      data-step-id={stepId}
      data-device={device.id}
      data-parameter={parameter.id}
      className={`matrix-cell ${selected ? 'selected-cell' : ''} ${selectedColumn ? 'selected-column' : ''} ${active ? 'active-column' : ''}`}
      style={{ '--channel-color': COLORS[parameter.color].hex } as CSSProperties}
    >
      {content}
    </td>
  );
});
export function Matrix(props: Props) {
  const [collapsed, setCollapsed] = useState<Set<string>>(() => new Set());
  const toggleDevice = (id: string) =>
    setCollapsed((previous) => {
      const next = new Set(previous);
      if (next.has(id)) next.delete(id);
      else next.add(id);
      return next;
    });
  const { project } = props,
    rows = useMemo(() => columnsFor(project, 'all'), [project.devices]);
  const latest = useRef({ steps: project.steps, onMove: props.onMove });
  latest.current = { steps: project.steps, onMove: props.onMove };
  const drop = useCallback((index: number, after: boolean, id: string) => {
    const { steps, onMove } = latest.current;
    if (!id || !steps.some((s) => s.id === id)) return;
    const from = steps.findIndex((s) => s.id === id);
    let to = index + (after ? 1 : 0);
    if (from < to) to--;
    onMove(id, to);
  }, []);
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
            <th className="parameter-column corner-cell">Прибор</th>
            {project.steps.map((step, index) => (
              <th
                key={step.id}
                id={`step-${step.id}`}
                data-step-id={step.id}
                className={`step-heading ${props.selectedStepId === step.id ? 'selected-column' : ''} ${props.activeStepId === step.id ? 'active-column' : ''}`}
                onDragOver={(e) => {
                  e.preventDefault();
                  e.dataTransfer.dropEffect = 'move';
                }}
                onDrop={(e) => {
                  e.preventDefault();
                  const rect = e.currentTarget.getBoundingClientRect();
                  drop(
                    index,
                    e.clientX > rect.left + rect.width / 2,
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
                  onClick={() => props.onSelectStep(step.id)}
                >
                  <span
                    className={`step-indicator ${props.activeStepId === step.id ? 'live' : ''}`}
                  />
                  {index + 1}
                </button>
              </th>
            ))}
          </tr>
        </thead>
        <tbody>
          {project.devices.map((device) => {
            const deviceRows = rows.filter((row) => row.device.id === device.id);
            return (
              <Fragment key={device.id}>
                <tr className="device-group" data-device-group={device.id}>
                  <th className="parameter-column">
                    <div>
                      <button
                        className="icon-button collapse-device"
                        aria-label={`${collapsed.has(device.id) ? 'Развернуть' : 'Свернуть'} ${device.name}`}
                        aria-expanded={!collapsed.has(device.id)}
                        onClick={() => toggleDevice(device.id)}
                      >
                        {collapsed.has(device.id) ? (
                          <ChevronRight size={14} />
                        ) : (
                          <ChevronDown size={14} />
                        )}
                      </button>
                      <button className="device-name" onClick={() => props.onDevices(device.id)}>
                        {device.name}
                      </button>
                      <button
                        className="icon-button"
                        aria-label={`Настройки ${device.name}`}
                        onClick={() => props.onDevices(device.id)}
                      >
                        <Settings2 size={14} />
                      </button>
                    </div>
                    <DeviceIP mac={device.mac} fallback={device.ip} network={props.network} />
                  </th>
                  {project.steps.map((step) => (
                    <td
                      key={step.id}
                      className={`${props.selectedStepId === step.id ? 'selected-column' : ''} ${props.activeStepId === step.id ? 'active-column' : ''}`}
                    />
                  ))}
                </tr>
                {!collapsed.has(device.id) &&
                  deviceRows.map((row) => (
                    <tr
                      key={`${device.id}-${row.parameter.id}`}
                      className="parameter-row"
                      data-device={device.id}
                      data-parameter={row.parameter.id}
                    >
                      <th className="parameter-column">
                        <span
                          className="channel-dot"
                          style={{ background: COLORS[row.parameter.color].hex }}
                        />
                        <code>{parameterCode(device, row.parameter)}</code>
                        <span title={row.parameter.name}>{row.parameter.name}</span>
                      </th>
                      {project.steps.map((step, index) => (
                        <ScoreCell
                          key={step.id}
                          row={row}
                          cue={step.devices[device.id]}
                          previousTrack={
                            project.steps[index - 1]?.devices[device.id]?.audio.track ?? null
                          }
                          stepId={step.id}
                          index={index}
                          effects={project.effects}
                          selected={
                            props.selection?.stepId === step.id &&
                            props.selection.deviceId === device.id &&
                            props.selection.parameterId === row.parameter.id
                          }
                          selectedColumn={props.selectedStepId === step.id}
                          active={props.activeStepId === step.id}
                          compact={props.compact}
                          onSelectCell={props.onSelectCell}
                          onChangeCell={props.onChangeCell}
                        />
                      ))}
                    </tr>
                  ))}
              </Fragment>
            );
          })}
        </tbody>
      </table>
    </div>
  );
}
