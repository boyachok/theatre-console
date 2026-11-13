import type { StripEffect } from '../domain/schema.ts';
import { stripModes, stripLabels } from '../domain/strip.ts';
import { SliderField } from './DimmerAnimationEditor.tsx';
export function StripEditor({
  effect: e,
  onChange: change,
}: {
  effect: StripEffect;
  onChange: (p: Partial<StripEffect>) => void;
}) {
  const n = (
    key: keyof StripEffect,
    label: string,
    min: number,
    max: number,
    defaultValue: number,
    step = 1,
    suffix = '',
  ) => (
    <SliderField
      key={key}
      label={label}
      value={(e[key] as number) ?? defaultValue}
      min={min}
      max={max}
      step={step}
      suffix={suffix}
      onChange={(v) => change({ [key]: v })}
    />
  );
  return (
    <>
      <div className="segmented" style={{ flexWrap: 'wrap' }}>
        {stripModes.map((m, i) => (
          <button
            key={m}
            className={e.mode === m ? 'selected' : ''}
            onClick={() => change({ mode: m })}
          >
            {stripLabels[i]}
          </button>
        ))}
      </div>
      <label className="field">
        <span>Цвет</span>
        <input
          aria-label="Цвет эффекта"
          type="color"
          value={e.color}
          onChange={(v) => change({ color: v.target.value })}
        />
      </label>
      {n('brightness', 'Яркость эффекта', 0, 100, 100, 1, '%')}
      {e.mode !== 'static' && (
        <>
          {n(
            'duration',
            e.mode === 'waiting' ? 'Полный цикл туда-обратно' : 'Время прохода',
            0.04,
            3600,
            2,
            0.01,
            'с',
          )}
          {n('width', 'Ширина', 1, 100, 20, 1, '%')}
        </>
      )}
      {['wave', 'fill'].includes(e.mode) && (
        <label className="field">
          <span>Направление</span>
          <select
            aria-label="Направление"
            value={e.direction}
            onChange={(v) => change({ direction: v.target.value as StripEffect['direction'] })}
          >
            <option value="forward">Слева направо</option>
            <option value="reverse">Справа налево</option>
          </select>
        </label>
      )}
      {['disco', 'loading'].includes(e.mode) && n('density', 'Плотность полос', 0, 100, 50, 1, '%')}
      {e.mode === 'disco' && (
        <>
          <label className="field">
            <span>Палитра</span>
            <select
              aria-label="Палитра дискотеки"
              value={e.palette ?? 0}
              onChange={(v) => change({ palette: Number(v.target.value) as 0 | 1 | 2 })}
            >
              <option value={0}>Радуга</option>
              <option value={1}>Тёплая</option>
              <option value={2}>Холодная</option>
            </select>
          </label>
          {n('sparkles', 'Искры', 0, 100, 25, 1, '%')}
        </>
      )}
      {e.mode === 'alarm' && (
        <>
          <p className="muted">
            Красный строб. Шум меняет яркость пикселей и рисунок вспышек. Осторожно:
            фотосенситивность.
          </p>
          {n('noise', 'Шум тревоги', 0, 100, 30, 1, '%')}
        </>
      )}
      <label>
        <input
          aria-label="Аудиореактивность"
          type="checkbox"
          checked={e.audioReactive ?? false}
          onChange={(v) => change({ audioReactive: v.target.checked })}
        />{' '}
        Аудиореактивность
      </label>
      {e.audioReactive && (
        <>
          <p className="muted">
            Микрофон шлема открывает анимацию. У обычного slave без микрофона эффект воспроизводится
            обычно. В браузере без телеметрии голос не имитируется.
          </p>
          {n('sensitivity', 'Порог реакции (вправо — громче)', 0, 100, 50, 1, '%')}
          {n('release', 'Фейд после голоса', 0, 30, 0.4, 0.01, 'с')}
        </>
      )}
      <label>
        <input
          aria-label="Фоновая подсветка"
          type="checkbox"
          checked={e.backgroundEnabled ?? true}
          onChange={(v) => change({ backgroundEnabled: v.target.checked })}
        />{' '}
        Фоновая подсветка
      </label>
      {(e.backgroundEnabled ?? true) && (
        <>
          <label className="field">
            <span>Фон</span>
            <input
              aria-label="Цвет фона"
              type="color"
              value={e.background}
              onChange={(v) => change({ background: v.target.value })}
            />
          </label>
          {n('backgroundBrightness', 'Яркость фона', 0, 100, 100, 1, '%')}
        </>
      )}
    </>
  );
}
