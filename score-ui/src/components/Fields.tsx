import type { ReactNode } from 'react';
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
  return (
    <label className="field">
      <span>{label}</span>
      <div className="number-field">
        <input
          aria-label={label}
          type="number"
          inputMode={step === 1 ? 'numeric' : 'decimal'}
          min={min}
          max={max}
          step={step}
          value={value ?? ''}
          placeholder={placeholder}
          onChange={(e) =>
            onChange(e.target.value === '' ? null : clamp(Number(e.target.value), min, max))
          }
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
