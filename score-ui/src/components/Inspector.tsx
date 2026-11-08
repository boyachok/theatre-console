import { useEffect, useState } from 'react';
import { RotateCcw } from 'lucide-react';
import type { CellSelection, Project, Step, Timings } from '../domain/schema.ts';
import type { PreviewEngine } from '../domain/playback.ts';
import { FieldPair, NumberField, Section } from './Fields.tsx';
import { DevicePreview } from './DevicePreview.tsx';
import { DimmerAnimationEditor } from './DimmerAnimationEditor.tsx';
interface Props {
  project: Project;
  stepId: string;
  selection: CellSelection | null;
  deviceId: string;
  engine: PreviewEngine;
  onChangeCell: (cell: CellSelection, changes: Record<string, unknown>) => void;
  onStep: (id: string, changes: Partial<Step>) => void;
}
export function Inspector({
  project,
  stepId,
  selection,
  deviceId,
  engine,
  onChangeCell,
  onStep,
}: Props) {
  const [tab, setTab] = useState<'step' | 'cell'>(selection ? 'cell' : 'step');
  useEffect(
    () => setTab(selection ? 'cell' : 'step'),
    [selection?.stepId, selection?.deviceId, selection?.parameterId, stepId],
  );
  const index = project.steps.findIndex((s) => s.id === stepId),
    step = project.steps[index] ?? project.steps[0];
  const device =
    project.devices.find((d) => d.id === (selection?.deviceId ?? deviceId)) ?? project.devices[0];
  const parameter = selection
    ? [...device.dimmers, ...device.relays, device.audio, device.strip].find(
        (p) => p.id === selection.parameterId,
      )
    : null;
  const cue = step.devices[device.id];
  const change = (patch: Record<string, unknown>) => {
    if (selection) onChangeCell(selection, patch);
  };
  const timing = (key: keyof Timings, n: number | null) =>
    onStep(step.id, { timings: { ...step.timings, [key]: n ?? 0 } });
  const fadeFields = (kind: 'light' | 'audio') => {
    const cell = kind === 'light' ? cue.dimmers[selection!.parameterId] : cue.audio;
    return (
      <Section title="Фейд">
        <FieldPair>
          <NumberField
            label="Вход"
            value={cell.fadeIn}
            placeholder={String(step.timings[kind === 'light' ? 'lightIn' : 'audioIn'])}
            suffix="с"
            onChange={(n) => change({ fadeIn: n })}
          />
          <NumberField
            label="Выход"
            value={cell.fadeOut}
            placeholder={String(step.timings[kind === 'light' ? 'lightOut' : 'audioOut'])}
            suffix="с"
            onChange={(n) => change({ fadeOut: n })}
          />
        </FieldPair>
        <button className="text-button" onClick={() => change({ fadeIn: null, fadeOut: null })}>
          <RotateCcw size={12} />
          Как в шаге
        </button>
      </Section>
    );
  };
  return (
    <div className="inspector-content">
      <div className="inspector-title">
        <h2>{index + 1}</h2>
      </div>
      <div className="inspector-tabs">
        <button className={tab === 'step' ? 'selected' : ''} onClick={() => setTab('step')}>
          Шаг
        </button>
        <button
          className={tab === 'cell' ? 'selected' : ''}
          disabled={!selection}
          onClick={() => setTab('cell')}
        >
          Ячейка
        </button>
      </div>
      {tab === 'step' || !selection ? (
        <>
          <Section title="Свет">
            <FieldPair>
              <NumberField
                label="Свет · вход"
                suffix="с"
                value={step.timings.lightIn}
                onChange={(n) => timing('lightIn', n)}
              />
              <NumberField
                label="Свет · выход"
                suffix="с"
                value={step.timings.lightOut}
                onChange={(n) => timing('lightOut', n)}
              />
            </FieldPair>
          </Section>
          <Section title="Звук">
            <FieldPair>
              <NumberField
                label="Звук · вход"
                suffix="с"
                value={step.timings.audioIn}
                onChange={(n) => timing('audioIn', n)}
              />
              <NumberField
                label="Звук · выход"
                suffix="с"
                value={step.timings.audioOut}
                onChange={(n) => timing('audioOut', n)}
              />
            </FieldPair>
          </Section>
          <Section title="Задержка реле">
            <FieldPair>
              <NumberField
                label="Реле · включение"
                suffix="с"
                value={step.timings.relayOn}
                onChange={(n) => timing('relayOn', n)}
              />
              <NumberField
                label="Реле · выключение"
                suffix="с"
                value={step.timings.relayOff}
                onChange={(n) => timing('relayOff', n)}
              />
            </FieldPair>
          </Section>
        </>
      ) : (
        <>
          <div className="cell-context">
            <span>{device.name}</span>
            <h3>{parameter?.name}</h3>
          </div>
          {selection.kind === 'dimmer' && (
            <>
              <NumberField
                label="Яркость"
                value={cue.dimmers[selection.parameterId].value}
                suffix="%"
                min={0}
                max={100}
                step={1}
                onChange={(n) => change({ value: n ?? 0 })}
              />
              <input
                className="full-slider"
                aria-label="Ползунок яркости"
                type="range"
                min="0"
                max="100"
                value={cue.dimmers[selection.parameterId].value}
                onChange={(e) => change({ value: Number(e.target.value) })}
              />
              <DimmerAnimationEditor
                key={`${step.id}/${device.id}/${selection.parameterId}`}
                cue={cue.dimmers[selection.parameterId]}
                onChange={change}
              />
              {fadeFields('light')}
            </>
          )}
          {selection.kind === 'audio' && device.profile === 'helmet' && (
            <Section title="Обработка голоса">
              <div className="segmented">
                <button
                  className={cue.audio.voiceOn ? 'selected' : ''}
                  onClick={() => change({ voiceOn: true })}
                >
                  ВКЛ
                </button>
                <button
                  className={!cue.audio.voiceOn ? 'selected' : ''}
                  onClick={() => change({ voiceOn: false })}
                >
                  ВЫКЛ
                </button>
              </div>
              <p className="muted">
                Только запрос состояния. Аудиомодуль пока не подключён; реального звука нет.
              </p>
            </Section>
          )}
          {selection.kind === 'audio' && device.profile !== 'helmet' && (
            <>
              <NumberField
                label="Номер трека"
                min={1}
                max={65535}
                step={1}
                value={cue.audio.track}
                placeholder="—"
                onChange={(n) => change({ track: n === null ? null : Math.round(n) })}
              />
              <button className="text-button" onClick={() => change({ track: null })}>
                Очистить трек
              </button>
              <NumberField
                label="Громкость"
                value={cue.audio.volume}
                suffix="%"
                max={100}
                step={1}
                onChange={(n) => change({ volume: n ?? 0 })}
              />
              {fadeFields('audio')}
            </>
          )}
          {selection.kind === 'relay' && (
            <>
              <div className="segmented">
                <button
                  className={cue.relays[selection.parameterId].on ? 'selected' : ''}
                  onClick={() => change({ on: true })}
                >
                  ВКЛ
                </button>
                <button
                  className={!cue.relays[selection.parameterId].on ? 'selected' : ''}
                  onClick={() => change({ on: false })}
                >
                  ВЫКЛ
                </button>
              </div>
              <Section title="Задержка">
                <FieldPair>
                  <NumberField
                    label="Задержка включения"
                    suffix="с"
                    value={cue.relays[selection.parameterId].delayOn}
                    placeholder={String(step.timings.relayOn)}
                    onChange={(n) => change({ delayOn: n })}
                  />
                  <NumberField
                    label="Задержка выключения"
                    suffix="с"
                    value={cue.relays[selection.parameterId].delayOff}
                    placeholder={String(step.timings.relayOff)}
                    onChange={(n) => change({ delayOff: n })}
                  />
                </FieldPair>
                <button
                  className="text-button"
                  onClick={() => change({ delayOn: null, delayOff: null })}
                >
                  <RotateCcw size={12} />
                  Как в шаге
                </button>
              </Section>
            </>
          )}
          {selection.kind === 'strip' && (
            <label className="field">
              <span>Событие ленты</span>
              <select
                aria-label="Событие ленты"
                value={
                  cue.strip.action === 'play' ? (cue.strip.effectId ?? 'hold') : cue.strip.action
                }
                onChange={(e) =>
                  change(
                    e.target.value === 'hold' || e.target.value === 'stop'
                      ? { action: e.target.value, effectId: null }
                      : { action: 'play', effectId: e.target.value },
                  )
                }
              >
                <option value="hold">Продолжать</option>
                <option value="stop">Стоп</option>
                {project.effects.map((f) => (
                  <option key={f.id} value={f.id}>
                    {f.name}
                  </option>
                ))}
              </select>
            </label>
          )}
        </>
      )}
      <DevicePreview device={device} engine={engine} />
    </div>
  );
}
