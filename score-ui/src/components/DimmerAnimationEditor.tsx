import { useEffect, useState } from 'react';
import type { DimmerAnimation, DimmerCue } from '../domain/schema.ts';
import { defaultDimmerAnimation, dimmerFactor } from '../domain/dimmer.ts';
import { Section } from './Fields.tsx';
import { EFFECT_MIN_SECONDS, EFFECT_MAX_SECONDS, effectPeriod } from '../domain/effect-range.ts';
function Slider({
  label,
  value,
  min = 0,
  max = 100,
  step = 1,
  logarithmic = false,
  suffix = '',
  onChange,
}: {
  label: string;
  value: number;
  min?: number;
  max?: number;
  step?: number;
  logarithmic?: boolean;
  suffix?: string;
  onChange: (value: number) => void;
}) {
  const [text, setText] = useState(String(value));
  useEffect(() => setText(String(value)), [value]);
  const commit = () => {
    const n = Number(text.replace(',', '.'));
    if (Number.isFinite(n) && text.trim()) {
      const next = Math.min(max, Math.max(min, n));
      setText(String(next));
      onChange(next);
    } else setText(String(value));
  };
  const position = logarithmic
    ? (min > 0
        ? Math.log(value / min) / Math.log(max / min)
        : Math.log1p(value) / Math.log1p(max)) * 1000
    : value;
  return (
    <div className="animation-slider">
      <label className="field">
        <span>{label}</span>
        <div className="number-field">
          <input
            aria-label={label}
            type="text"
            inputMode="decimal"
            value={text}
            onChange={(e) => {
              const s = e.target.value;
              setText(s);
              const n = Number(s.replace(',', '.'));
              if (s.trim() && Number.isFinite(n) && n >= min && n <= max) onChange(n);
            }}
            onBlur={commit}
            onKeyDown={(e) => {
              if (e.key === 'Enter') e.currentTarget.blur();
            }}
          />
          {suffix && <span>{suffix}</span>}
        </div>
      </label>
      <input
        aria-label={`Ползунок: ${label}`}
        className="full-slider"
        type="range"
        min={logarithmic ? 0 : min}
        max={logarithmic ? 1000 : max}
        step={logarithmic ? 1 : step}
        value={position}
        onWheel={(e) => e.currentTarget.blur()}
        onChange={(e) =>
          onChange(
            logarithmic
              ? Math.round(
                  (min > 0
                    ? min * Math.pow(max / min, Number(e.target.value) / 1000)
                    : Math.expm1((Number(e.target.value) / 1000) * Math.log1p(max))) * 1000,
                ) / 1000
              : Number(e.target.value),
          )
        }
      />
    </div>
  );
}
export { Slider as SliderField };
export function DimmerAnimationEditor({
  cue,
  onChange,
  minPeriod = EFFECT_MIN_SECONDS,
}: {
  cue: DimmerCue;
  minPeriod?: number;
  onChange: (patch: Record<string, unknown>) => void;
}) {
  const a = cue.animation ?? defaultDimmerAnimation();
  const [seedText, setSeedText] = useState(String(a.seed));
  useEffect(() => setSeedText(String(a.seed)), [a.seed]);
  const patch = (changes: Partial<DimmerAnimation>) =>
    onChange({ animation: { ...a, ...changes } });
  const points = Array.from({ length: 121 }, (_, i) => {
    const cycles = a.mode === 'random' ? i / 30 : i / 120;
    const time = cycles + (a.mode === 'random' ? 0 : a.phase / 360);
    const factor = dimmerFactor(a, {
      at: 0,
      cycle: Math.floor(time),
      position: (time % 1) * Math.round(a.period * 1000),
    });
    return `${i * 2},${46 - (cue.value / 100) * factor * 40}`;
  }).join(' ');
  return (
    <Section title="Анимация DIM">
      <label className="animation-enable">
        <input
          type="checkbox"
          checked={a.enabled}
          onChange={(e) => patch({ enabled: e.target.checked })}
        />
        Включить анимацию
      </label>
      <fieldset className="animation-settings" disabled={!a.enabled}>
        <label className="field">
          <span>Функция</span>
          <select
            aria-label="Функция DIM"
            value={a.mode}
            onChange={(e) => patch({ mode: e.target.value as DimmerAnimation['mode'] })}
          >
            <option value="static">Статика</option>
            <option value="sine">Синус</option>
            <option value="square">Квадрат</option>
            <option value="ramp">Рамп</option>
            <option value="random">Рандом</option>
          </select>
        </label>
        {a.mode !== 'static' && (
          <>
            <Slider
              label="Период анимации"
              value={a.period}
              min={Math.max(minPeriod, EFFECT_MIN_SECONDS)}
              max={EFFECT_MAX_SECONDS}
              logarithmic
              suffix="с"
              onChange={(period) => patch({ period: effectPeriod(period) })}
            />
            <p className="muted">
              {1 / a.period >= 1 ? (1 / a.period).toFixed(2) : (1 / a.period).toPrecision(2)} Гц ·
              яркость ячейки задаёт максимум
            </p>
            {a.mode === 'random' ? (
              <label className="field">
                <span>Seed рандома</span>
                <div className="animation-seed">
                  <input
                    aria-label="Seed рандома"
                    type="text"
                    inputMode="numeric"
                    value={seedText}
                    onBlur={() => setSeedText(String(a.seed))}
                    onChange={(e) => {
                      setSeedText(e.target.value);
                      if (/^\d+$/.test(e.target.value) && Number(e.target.value) <= 0xffffffff)
                        patch({ seed: Number(e.target.value) });
                    }}
                  />
                  <button
                    type="button"
                    title="Новый seed"
                    onClick={() => patch({ seed: crypto.getRandomValues(new Uint32Array(1))[0] })}
                  >
                    ↻
                  </button>
                </div>
              </label>
            ) : (
              <Slider
                label="Фаза"
                value={a.phase}
                min={0}
                max={360}
                suffix="°"
                onChange={(phase) => patch({ phase })}
              />
            )}
            <Slider
              label="Нижний уровень"
              value={a.floor}
              suffix="%"
              onChange={(floor) => patch({ floor })}
            />
            <p className="muted">Доля от максимума: 25% при яркости40% даёт минимум10%.</p>
            {(a.mode === 'square' || a.mode === 'ramp') && (
              <Slider
                label={a.mode === 'square' ? 'Скважность · время ВКЛ' : 'Вершина рампы'}
                value={a.duty}
                suffix="%"
                onChange={(duty) => patch({ duty })}
              />
            )}
            {a.mode === 'ramp' && (
              <p className="muted">0% — спад, 50% — треугольник, 100% — подъём.</p>
            )}
            {a.mode === 'random' && (
              <Slider
                label="Плавность рандома"
                value={a.smooth}
                suffix="%"
                onChange={(smooth) => patch({ smooth })}
              />
            )}
            {a.mode === 'square' && (
              <p className="animation-warning">
                Стробоскоп: риск фотосенситивных реакций. Импульсы квантуются с шагом около1мс; это
                расчётный шаг, реальная точность зависит от загрузки и проверяется на нагрузке.
              </p>
            )}
          </>
        )}
        <svg
          className="dimmer-wave"
          viewBox="0 0 240 52"
          role="img"
          aria-label="Форма анимации DIM"
        >
          <polyline points={points} fill="none" stroke="currentColor" strokeWidth="2" />
        </svg>
      </fieldset>
      <p className="muted">
        Настройка только этого DIM в этом шаге прибора. Новая сцена начинает цикл с фазы;
        LIVE-правка яркости/скорости сохраняет ход. При Предпросмотр OFF правки только сохраняются;
        уже принятая анимация продолжает работать.
      </p>
    </Section>
  );
}
