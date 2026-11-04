import { useState } from 'react';
import { Check, Copy, RotateCcw } from 'lucide-react';
import type { Project, StripEffect } from '../domain/schema.ts';
import { clone, uid } from '../domain/project.ts';
import { FieldPair, NumberField } from './Fields.tsx';
import { Modal } from './Modal.tsx';
import { StripCanvas } from './StripCanvas.tsx';
export function EffectsDialog({
  project,
  onSave,
  onClose,
}: {
  project: Project;
  onSave: (effects: StripEffect[]) => void;
  onClose: () => void;
}) {
  const [effects, setEffects] = useState<StripEffect[]>(() =>
    project.effects.length
      ? clone(project.effects)
      : [
          {
            id: uid(),
            name: 'Волна',
            mode: 'wave',
            color: '#638dff',
            background: '#000000',
            brightness: 100,
            width: 20,
            duration: 4,
            direction: 'forward',
          },
        ],
  );
  const [selected, setSelected] = useState(effects[0].id),
    [start, setStart] = useState(performance.now());
  const effect = effects.find((e) => e.id === selected) ?? effects[0];
  const update = (patch: Partial<StripEffect>) =>
    setEffects((list) => list.map((e) => (e.id === effect.id ? { ...e, ...patch } : e)));
  const duplicate = () => {
    const copy = {
      ...clone(effect),
      id: uid(),
      name: `${effect.name.replace(/ · копия$/, '')} · копия`,
    };
    setEffects((list) => [...list, copy]);
    setSelected(copy.id);
    setStart(performance.now());
  };
  return (
    <Modal title="Эффекты ленты" onClose={onClose} className="effects-modal">
      <div className="effect-layout">
        <nav className="effects-list" aria-label="Эффекты">
          {effects.map((e, i) => (
            <button
              key={e.id}
              className={e.id === effect.id ? 'selected' : ''}
              onClick={() => {
                setSelected(e.id);
                setStart(performance.now());
              }}
            >
              <small>FX{String(i + 1).padStart(2, '0')}</small>
              {e.name}
            </button>
          ))}
          <button className="effect-duplicate" onClick={duplicate}>
            <Copy size={14} />
            Дублировать
          </button>
        </nav>
        <div className="effect-fields">
          <label className="field">
            <span>Название эффекта</span>
            <input
              aria-label="Название эффекта"
              value={effect.name}
              maxLength={120}
              onChange={(e) => update({ name: e.target.value })}
            />
          </label>
          <div className="segmented">
            {(
              [
                ['wave', 'Волна'],
                ['fill', 'Заполнение'],
                ['static', 'Статика'],
              ] as const
            ).map(([mode, label]) => (
              <button
                key={mode}
                className={effect.mode === mode ? 'selected' : ''}
                onClick={() => {
                  update({ mode });
                  setStart(performance.now());
                }}
              >
                {label}
              </button>
            ))}
          </div>
          <div className="effect-canvas">
            <StripCanvas effect={effect} startedAt={start} tall />
            {effect.mode !== 'static' && (
              <button
                className="icon-button"
                aria-label="Повторить эффект"
                onClick={() => setStart(performance.now())}
              >
                <RotateCcw size={16} />
              </button>
            )}
          </div>
          <FieldPair>
            <label className="field">
              <span>Цвет</span>
              <input
                aria-label="Цвет эффекта"
                type="color"
                value={effect.color}
                onChange={(e) => update({ color: e.target.value })}
              />
            </label>
            <NumberField
              label="Яркость эффекта"
              value={effect.brightness}
              suffix="%"
              step={1}
              max={100}
              onChange={(n) => update({ brightness: n ?? 0 })}
            />
          </FieldPair>
          {effect.mode !== 'static' && (
            <>
              <FieldPair>
                <NumberField
                  label="Время прохода"
                  min={0.1}
                  value={effect.duration}
                  suffix="с"
                  onChange={(n) => update({ duration: n ?? 0.1 })}
                />
                <NumberField
                  label="Ширина"
                  min={1}
                  max={100}
                  value={effect.width}
                  suffix="%"
                  step={1}
                  onChange={(n) => update({ width: n ?? 1 })}
                />
              </FieldPair>
              <FieldPair>
                <label className="field">
                  <span>Фон</span>
                  <input
                    aria-label="Цвет фона"
                    type="color"
                    value={effect.background}
                    onChange={(e) => update({ background: e.target.value })}
                  />
                </label>
                <label className="field">
                  <span>Направление</span>
                  <select
                    aria-label="Направление"
                    value={effect.direction}
                    onChange={(e) =>
                      update({ direction: e.target.value as StripEffect['direction'] })
                    }
                  >
                    <option value="forward">Слева направо</option>
                    <option value="reverse">Справа налево</option>
                  </select>
                </label>
              </FieldPair>
            </>
          )}
        </div>
      </div>
      <footer className="modal-footer">
        <button onClick={onClose}>Отмена</button>
        <button
          className="primary-button"
          onClick={() => {
            onSave(effects.map((e) => ({ ...e, name: e.name.trim() || 'Эффект' })));
            onClose();
          }}
        >
          <Check size={15} />
          Сохранить
        </button>
      </footer>
    </Modal>
  );
}
