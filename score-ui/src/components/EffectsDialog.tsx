import { useState } from 'react';
import { Check, Copy, RotateCcw } from 'lucide-react';
import type { Project, StripEffect } from '../domain/schema.ts';
import { clone, uid } from '../domain/project.ts';
import { Modal } from './Modal.tsx';
import { StripCanvas } from './StripCanvas.tsx';
import { StripEditor } from './StripEditor.tsx';
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
          <button
            className="effect-duplicate"
            onClick={() => {
              const copy = {
                ...clone(effect),
                id: uid(),
                name: `${effect.name.replace(/ · копия$/, '')} · копия`,
              };
              setEffects((list) => [...list, copy]);
              setSelected(copy.id);
              setStart(performance.now());
            }}
          >
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
          <div className="effect-canvas">
            <StripCanvas effect={effect} startedAt={start} tall />
            <button
              className="icon-button"
              aria-label="Повторить эффект"
              onClick={() => setStart(performance.now())}
            >
              <RotateCcw size={16} />
            </button>
          </div>
          <StripEditor effect={effect} onChange={update} />
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
