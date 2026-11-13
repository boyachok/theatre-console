import { useEffect, useRef, useState } from 'react';
import { Modal } from './Modal.tsx';
import type { EditField } from '../domain/editor.ts';
export type EditScope = 'selection' | 'device' | 'step' | 'allSteps';
export const fieldLabels: Record<EditField, string> = {
  level: 'Уровень',
  fadeIn: 'Fade in / включение',
  fadeOut: 'Fade out / выключение',
  startDelay: 'Задержка старта',
};
export function QuickToolbar(p: {
  count: number;
  scope: EditScope;
  onScope: (s: EditScope) => void;
  onEdit: (f: EditField) => void;
  onLevel: (n: number) => void;
  onHelp: () => void;
  custom: boolean;
  onLayout: () => void;
  canUndo: boolean;
  canRedo: boolean;
  onUndo: () => void;
  onRedo: () => void;
}) {
  return (
    <div className="quick-toolbar" aria-label="Быстрая правка">
      <button
        onClick={p.onLayout}
        aria-pressed={p.custom}
        title="Переключить группировку; строки можно перетаскивать"
      >
        {p.custom ? 'Своя партитура' : 'По приборам'}
      </button>
      <span className="selection-count">Выделено: {p.count}</span>
      <select
        aria-label="Область правки"
        value={p.scope}
        onChange={(e) => p.onScope(e.target.value as EditScope)}
      >
        <option value="selection">Выделение</option>
        <option value="device">Прибор · этот шаг</option>
        <option value="step">Все приборы · этот шаг</option>
        <option value="allSteps">Эти строки · все шаги</option>
      </select>
      <button
        disabled={!p.count}
        title="Уровень числом · V; 1…9 → 10…90%, 0 → 0%, = → 100%"
        onClick={() => p.onEdit('level')}
      >
        Уровень
      </button>
      <button disabled={!p.count} title="Ноль · 0" onClick={() => p.onLevel(0)}>
        0%
      </button>
      <button disabled={!p.count} title="Полный уровень · =" onClick={() => p.onLevel(100)}>
        100%
      </button>
      <button
        disabled={!p.count}
        title="Время входа / задержка реле · I"
        onClick={() => p.onEdit('fadeIn')}
      >
        Fade in <kbd>I</kbd>
      </button>
      <button
        disabled={!p.count}
        title="Время выхода / задержка реле · O"
        onClick={() => p.onEdit('fadeOut')}
      >
        Fade out <kbd>O</kbd>
      </button>
      <button
        disabled={!p.count}
        title="Задержка старта · D"
        onClick={() => p.onEdit('startDelay')}
      >
        Задержка
      </button>
      <button disabled={!p.canUndo} title="Отменить правку · ⌘/Ctrl+Z" onClick={p.onUndo}>
        ↶
      </button>
      <button disabled={!p.canRedo} title="Повторить · ⌘/Ctrl+Shift+Z" onClick={p.onRedo}>
        ↷
      </button>
      <button onClick={p.onHelp} title="Горячие клавиши · ?" aria-label="Горячие клавиши">
        Клавиши ?
      </button>
    </div>
  );
}
export function QuickValueDialog(p: {
  field: EditField;
  count: number;
  steps: number;
  devices: number;
  onApply: (n: number | null) => void;
  onClose: () => void;
}) {
  const [text, setText] = useState('');
  const input = useRef<HTMLInputElement>(null);
  useEffect(() => {
    input.current?.focus();
  }, []);
  const n = Number(text.replace(',', '.')),
    valid =
      !!text.trim() && Number.isFinite(n) && n >= 0 && n <= (p.field === 'level' ? 100 : 3600);
  return (
    <Modal title={fieldLabels[p.field]} onClose={p.onClose} className="quick-value-modal">
      <p>
        До {p.count} ячеек · {p.devices} приборов · {p.steps} шагов. Неподходящие параметры
        пропускаются.
      </p>
      <form
        onSubmit={(e) => {
          e.preventDefault();
          if (valid) p.onApply(n);
        }}
      >
        <label className="field">
          <span>{p.field === 'level' ? 'Проценты' : 'Секунды; 0 — без времени'}</span>
          <input
            ref={input}
            type="text"
            inputMode="decimal"
            aria-label="Быстрое значение"
            value={text}
            onChange={(e) => setText(e.target.value)}
          />
        </label>
        {p.field === 'level' && (
          <input
            type="range"
            aria-label="Ползунок быстрого уровня"
            min={0}
            max={100}
            step={1}
            value={valid ? n : 0}
            onChange={(e) => setText(e.target.value)}
          />
        )}
        <div className="segmented">
          <button type="button" onClick={() => p.onApply(0)}>
            Ноль
          </button>
          <button type="submit" disabled={!valid}>
            Применить
          </button>
          {p.field !== 'level' && p.field !== 'startDelay' && (
            <button type="button" onClick={() => p.onApply(null)}>
              Время шага
            </button>
          )}
          <button type="button" onClick={p.onClose}>
            Отмена
          </button>
        </div>
      </form>
    </Modal>
  );
}
export function ShortcutHelp({ onClose }: { onClose: () => void }) {
  const rows = [
    ['Стрелки', 'Перемещение курсора без изменения значений и без GO'],
    ['Shift + стрелки / клик', 'Прямоугольное выделение; в заголовке — диапазон шагов'],
    ['⌘ / Ctrl + клик', 'Добавить / убрать отдельную ячейку или шаг'],
    ['Клик / двойной клик', 'Выделить / переключить значение'],
    ['1…9 / 0 / =', '10…90% / 0% / 100%'],
    ['V / I / O / D', 'Ввести уровень / fade in / fade out / задержку'],
    ['Alt + ↑ / ↓', 'Переставить строку в своей раскладке или внутри прибора'],
    ['⌘ / Ctrl + Z', 'Отменить; Shift + Z — повторить'],
    ['Esc / Enter', 'Отменить ввод / подтвердить'],
    ['?', 'Эта подсказка'],
  ];
  return (
    <Modal title="Горячие клавиши" onClose={onClose}>
      <p>
        Клавиши редактируют выделение. В текстовых полях, списках и открытых окнах они не
        перехватываются. LIVE применяет правки текущей исполняемой сцены обычным способом.
      </p>
      <dl className="shortcut-list">
        {rows.map(([key, text]) => (
          <div key={key}>
            <dt>
              <kbd>{key}</kbd>
            </dt>
            <dd>{text}</dd>
          </div>
        ))}
      </dl>
    </Modal>
  );
}
