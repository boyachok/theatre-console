import { useEffect, useState, useSyncExternalStore } from 'react';
import { RotateCcw } from 'lucide-react';
import type { CellSelection, Project, Step, Timings } from '../domain/schema.ts';
import { StripEditor } from './StripEditor.tsx';
import { acParameter, acCue } from '../domain/project.ts';
import type { PreviewEngine } from '../domain/playback.ts';
import { FieldPair, NumberField, Section } from './Fields.tsx';
import { DevicePreview } from './DevicePreview.tsx';
import { DimmerAnimationEditor, SliderField } from './DimmerAnimationEditor.tsx';
import { voiceBank } from '../domain/voice.ts';
import type { Device } from '../domain/schema.ts';
import type { NetworkTransport } from '../transport/network.ts';
interface Props {
  readOnly?: boolean;
  onHelmet: (id: string) => void;
  onDevice: (d: Device) => void;
  network: NetworkTransport;
  project: Project;
  stepId: string;
  selection: CellSelection | null;
  deviceId: string;
  engine: PreviewEngine;
  onChangeCell: (cell: CellSelection, changes: Record<string, unknown>) => void;
  onStep: (id: string, changes: Partial<Step>) => void;
}
export function Inspector({
  readOnly = false,
  project,
  stepId,
  selection,
  deviceId,
  engine,
  onChangeCell,
  onStep,
  onDevice,
  onHelmet,
  network,
}: Props) {
  const networkState = useSyncExternalStore(network.subscribe, network.getSnapshot);
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
    ? [...device.dimmers, ...device.relays, device.audio, device.strip, acParameter(device)].find(
        (p) => p.id === selection.parameterId,
      )
    : null;
  const cue = step.devices[device.id];
  const stripEffect =
    cue.strip.action === 'play'
      ? (cue.strip.settings ?? project.effects.find((f) => f.id === cue.strip.effectId))
      : undefined;
  const light = selection?.kind === 'ac' ? acCue(cue) : cue.dimmers[selection?.parameterId ?? ''];
  const change = (patch: Record<string, unknown>) => {
    if (selection) onChangeCell(selection, patch);
  };
  const timing = (key: keyof Timings, n: number | null) =>
    onStep(step.id, { timings: { ...step.timings, [key]: n ?? 0 } });
  const fadeFields = (kind: 'light' | 'audio') => {
    const cell = kind === 'light' ? light : cue.audio;
    return (
      <Section title="Фейд">
        <FieldPair>
          <SliderField
            label="Вход"
            value={cell.fadeIn ?? step.timings[kind === 'light' ? 'lightIn' : 'audioIn']}
            min={0}
            max={3600}
            logarithmic
            suffix="с"
            onChange={(n) => change({ fadeIn: n })}
          />
          <SliderField
            label="Выход"
            value={cell.fadeOut ?? step.timings[kind === 'light' ? 'lightOut' : 'audioOut']}
            min={0}
            max={3600}
            logarithmic
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
      <fieldset className="score-edit-fieldset" disabled={readOnly}>
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
            {(selection.kind === 'dimmer' || selection.kind === 'ac') && (
              <>
                <NumberField
                  label="Яркость"
                  value={light.value}
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
                  value={light.value}
                  onChange={(e) => change({ value: Number(e.target.value) })}
                />
                <SliderField
                  label="Задержка включения света"
                  value={light.startDelay ?? 0}
                  min={0}
                  max={3600}
                  logarithmic
                  suffix="с"
                  onChange={(startDelay) => change({ startDelay })}
                />
                <DimmerAnimationEditor
                  key={`${step.id}/${device.id}/${selection.parameterId}`}
                  cue={light}
                  minPeriod={selection.kind === 'ac' ? 0.04 : 0.02}
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
                <label className="animation-enable">
                  <input
                    type="checkbox"
                    checked={cue.audio.loop ?? false}
                    onChange={(e) => change({ loop: e.target.checked })}
                  />
                  Зациклить трек
                </label>
                <SliderField
                  label="Задержка старта трека"
                  value={cue.audio.startDelay ?? 0}
                  min={0}
                  max={3600}
                  logarithmic
                  suffix="с"
                  onChange={(startDelay) => change({ startDelay })}
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
              <>
                <label className="field">
                  <span>Событие ленты</span>
                  <select
                    aria-label="Событие ленты"
                    value={
                      cue.strip.action === 'play'
                        ? (cue.strip.effectId ?? 'hold')
                        : cue.strip.action
                    }
                    onChange={(e) =>
                      change(
                        e.target.value === 'hold' || e.target.value === 'stop'
                          ? { action: e.target.value, effectId: null }
                          : {
                              action: 'play',
                              effectId: e.target.value,
                              settings: structuredClone(
                                project.effects.find((f) => f.id === e.target.value),
                              ),
                            },
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
                {stripEffect && (
                  <>
                    <StripEditor
                      effect={stripEffect}
                      onChange={(patch) => change({ settings: { ...stripEffect, ...patch } })}
                    />
                  </>
                )}
              </>
            )}
          </>
        )}
      </fieldset>
      {device.profile === 'helmet' && (
        <Section title="Звук шлема">
          <label className="field">
            <span>Пресет голоса</span>
            <select
              aria-label="Пресет голоса"
              value={voiceBank(device).selected}
              onChange={(e) =>
                onDevice({
                  ...device,
                  voice: { ...voiceBank(device), selected: e.target.value, working: undefined },
                })
              }
            >
              {voiceBank(device).presets.map((p) => (
                <option key={p.id} value={p.id}>
                  {p.name}
                </option>
              ))}
            </select>
          </label>
          <button onClick={() => onHelmet(device.id)}>Настройки шлема</button>
        </Section>
      )}
      <DevicePreview
        device={device}
        engine={engine}
        audioLevel={
          networkState.devices[(device.mac ?? '').toUpperCase()]?.voice?.ageMs! < 3000
            ? networkState.devices[(device.mac ?? '').toUpperCase()]?.voice?.inputLevel
            : undefined
        }
      />
    </div>
  );
}
