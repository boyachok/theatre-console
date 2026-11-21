import { useEffect, useRef, useState } from 'react';
import { MoreHorizontal } from 'lucide-react';
import type { Device } from '../domain/schema.ts';
import { voiceBank } from '../domain/voice.ts';
export function VoiceLauncher({
  helmets,
  ready,
  busy,
  onPreset,
  onSound,
  onScore,
}: {
  helmets: Device[];
  ready: boolean;
  busy: boolean;
  onPreset: (device: Device, id: string) => void;
  onSound: (id: string) => void;
  onScore: () => void;
}) {
  const [open, setOpen] = useState(false),
    [selected, setSelected] = useState<string>();
  const box = useRef<HTMLElement>(null),
    button = useRef<HTMLButtonElement>(null),
    menu = useRef<HTMLDivElement>(null);
  const target = helmets.find((d) => d.id === selected) ?? helmets[0];
  useEffect(() => {
    box.current?.querySelector<HTMLSelectElement>('select')?.focus();
  }, []);
  useEffect(() => {
    if (!open) return;
    menu.current?.querySelector<HTMLButtonElement>('button:not(:disabled)')?.focus();
    const outside = (e: PointerEvent) => {
      if (!box.current?.contains(e.target as Node)) setOpen(false);
    };
    const key = (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        e.preventDefault();
        e.stopPropagation();
        setOpen(false);
        button.current?.focus();
      }
    };
    document.addEventListener('pointerdown', outside);
    document.addEventListener('keydown', key);
    return () => {
      document.removeEventListener('pointerdown', outside);
      document.removeEventListener('keydown', key);
    };
  }, [open]);
  return (
    <main className="voice-launcher">
      <section ref={box} className="voice-launcher-card" aria-label="Стартовое окно голоса шлема">
        {helmets.length ? (
          helmets.map((device) => (
            <label className="field voice-launcher-field" key={device.id}>
              <span>Голос шлема{helmets.length > 1 ? ` · ${device.name}` : ''}</span>
              <select
                aria-label={helmets.length > 1 ? `Голос шлема · ${device.name}` : 'Голос шлема'}
                value={voiceBank(device).selected}
                disabled={!ready || busy}
                onFocus={() => setSelected(device.id)}
                onChange={(e) => {
                  setSelected(device.id);
                  onPreset(device, e.target.value);
                }}
              >
                {voiceBank(device).presets.map((p) => (
                  <option key={p.id} value={p.id}>
                    {p.name}
                  </option>
                ))}
              </select>
            </label>
          ))
        ) : (
          <label className="field voice-launcher-field">
            <span>Голос шлема</span>
            <select aria-label="Голос шлема" disabled value="">
              <option value="">{ready ? 'Шлем не назначен' : 'Загрузка…'}</option>
            </select>
          </label>
        )}
        <div className="voice-launcher-footer">
          <button
            ref={button}
            type="button"
            className="icon-button voice-launcher-more"
            aria-label="Меню голоса шлема"
            aria-haspopup="menu"
            aria-expanded={open}
            disabled={busy}
            onClick={() => setOpen((v) => !v)}
            onKeyDown={(e) => {
              if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
                e.preventDefault();
                setOpen(true);
              }
            }}
          >
            <MoreHorizontal size={22} />
          </button>
          {open && (
            <div
              ref={menu}
              className="voice-launcher-menu"
              role="menu"
              aria-label="Редактирование"
              onKeyDown={(e) => {
                if (e.key === 'Tab') {
                  setOpen(false);
                  button.current?.focus();
                  return;
                }
                if (!['ArrowDown', 'ArrowUp', 'Home', 'End'].includes(e.key)) return;
                e.preventDefault();
                const items = Array.from(
                  menu.current?.querySelectorAll<HTMLButtonElement>('button:not(:disabled)') ?? [],
                );
                const index = items.indexOf(document.activeElement as HTMLButtonElement);
                items[
                  e.key === 'Home'
                    ? 0
                    : e.key === 'End'
                      ? items.length - 1
                      : (index + (e.key === 'ArrowDown' ? 1 : -1) + items.length) % items.length
                ]?.focus();
              }}
            >
              <button
                role="menuitem"
                disabled={!ready || !target}
                onClick={() => {
                  setOpen(false);
                  if (target) onSound(target.id);
                }}
              >
                Редактирование звука
              </button>
              <button
                role="menuitem"
                onClick={() => {
                  setOpen(false);
                  onScore();
                }}
              >
                Редактирование партитуры
              </button>
            </div>
          )}
        </div>
      </section>
    </main>
  );
}
