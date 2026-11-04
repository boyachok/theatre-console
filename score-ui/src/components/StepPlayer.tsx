import { ChevronLeft, ChevronRight, Copy } from 'lucide-react';
import type { Step } from '../domain/schema.ts';
import { useHoldButton } from '../hooks/useHoldButton.ts';
export function StepPlayer({
  steps,
  activeId,
  selectedIndex,
  onNavigate,
  onSelect,
  onDuplicate,
}: {
  steps: Step[];
  activeId: string | null;
  selectedIndex: number;
  onNavigate: (action: 'back' | 'forward' | 'first' | 'last') => void;
  onSelect: (id: string) => void;
  onDuplicate: () => void;
}) {
  const back = useHoldButton(
    () => onNavigate('back'),
    () => onNavigate('first'),
  );
  const forward = useHoldButton(
    () => onNavigate('forward'),
    () => onNavigate('last'),
  );
  const exists = steps.some((s) => s.id === activeId);
  return (
    <div className="step-player" role="group" aria-label="Плеер шагов">
      <button
        className={`player-button ${back.holding ? 'holding' : ''}`}
        aria-label="Предыдущий шаг"
        title="Назад · удержание 3 с — первый шаг"
        {...back.handlers}
      >
        <ChevronLeft size={19} />
      </button>
      <select
        id="playing-step"
        aria-label="Текущий шаг"
        value={exists ? activeId! : ''}
        onChange={(e) => onSelect(e.target.value)}
      >
        {!exists && <option value="">—</option>}
        {steps.map((s, i) => (
          <option key={s.id} value={s.id}>
            {i + 1}
          </option>
        ))}
      </select>
      <button
        className={`player-button forward-button ${forward.holding ? 'holding' : ''}`}
        aria-label="Следующий шаг"
        title="Вперёд · удержание 3 с — последний шаг"
        {...forward.handlers}
      >
        <ChevronRight size={19} />
      </button>
      <span className="player-divider" />
      <button
        className="icon-button duplicate-button"
        aria-label="Сдублировать шаг"
        title={`Сдублировать шаг ${selectedIndex + 1}`}
        onClick={onDuplicate}
      >
        <Copy size={16} />
      </button>
    </div>
  );
}
