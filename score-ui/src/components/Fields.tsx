import { useEffect, useRef, useState, type ReactNode } from 'react';
import { clamp } from '../domain/project.ts';
export function NumberField({
  label,
  value,
  onChange,
  min = 0,
  max = 3600,
  step = 0.2,
  suffix,
  placeholder,
}: {
  label: string;
  value: number | null;
  onChange: (value: number | null) => void;
  min?: number;
  max?: number;
  step?: number;
  suffix?: string;
  placeholder?: string;
}) {
  const [text, setText] = useState(value == null ? '' : String(value));
  const focused = useRef(false),
    cancelled = useRef(false),
    original = useRef(value);
  useEffect(() => {
    if (!focused.current) setText(value == null ? '' : String(value));
  }, [value]);
  const commit = () => {
    const n = Number(text.replace(',', '.'));
    const next = text.trim() ? (Number.isFinite(n) ? clamp(n, min, max) : value) : null;
    setText(next == null ? '' : String(next));
    onChange(next);
  };
  return (
    <label className="field">
      <span>{label}</span>
      <div className="number-field">
        <input
          aria-label={label}
          type="text"
          inputMode={step === 1 ? 'numeric' : 'decimal'}
          min={min}
          max={max}
          step={step}
          value={text}
          placeholder={placeholder}
          onFocus={() => {
            focused.current = true;
            cancelled.current = false;
            original.current = value;
          }}
          onChange={(e) => {
            const s = e.target.value;
            setText(s);
            const n = Number(s.replace(',', '.'));
            if (s.trim() && Number.isFinite(n) && n >= min && n <= max) onChange(n);
          }}
          onBlur={() => {
            focused.current = false;
            if (!cancelled.current) commit();
          }}
          onKeyDown={(e) => {
            if (e.key === 'Enter') {
              e.preventDefault();
              e.currentTarget.blur();
            }
            if (e.key === 'Escape') {
              e.stopPropagation();
              cancelled.current = true;
              setText(original.current == null ? '' : String(original.current));
              onChange(original.current);
              e.currentTarget.blur();
            }
          }}
        />
        {suffix && <span>{suffix}</span>}
      </div>
    </label>
  );
}
export function FieldPair({ children }: { children: ReactNode }) {
  return <div className="field-pair">{children}</div>;
}
export function Section({ title, children }: { title: string; children: ReactNode }) {
  return (
    <section className="form-section">
      <h3>{title}</h3>
      {children}
    </section>
  );
}
