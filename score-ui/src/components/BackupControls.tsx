import { useEffect, useState } from 'react';
import type { BackupInfo } from '../domain/schema.ts';
export function BackupSeries({
  info,
  onChange,
  disabled = false,
}: {
  info: BackupInfo;
  disabled?: boolean;
  onChange: (major: number) => void;
}) {
  const [text, setText] = useState(String(info.major));
  useEffect(() => setText(String(info.major)), [info.major]);
  return (
    <label
      className="backup-series"
      title="Серия JSON задаётся вручную; номер файла после точки растёт при каждом скачивании"
    >
      <span>JSON</span>
      <input
        aria-label="Серия JSON"
        inputMode="numeric"
        type="text"
        value={text}
        disabled={disabled}
        onChange={(e) => {
          const v = e.target.value;
          setText(v);
          if (/^\d+$/.test(v) && Number(v) >= 1 && Number(v) <= 999999) onChange(Number(v));
        }}
        onBlur={() => setText(String(info.major))}
      />
      <small>.{String(info.minor).padStart(3, '0')}</small>
    </label>
  );
}
export function BackupPrompt({
  onDownload,
  onLater,
  disabled = false,
}: {
  disabled?: boolean;
  onDownload: () => void;
  onLater: () => void;
}) {
  return (
    <aside
      className="backup-reminder"
      role="dialog"
      aria-modal="false"
      aria-label="Резервная копия JSON"
    >
      <strong>Скачать резервную копию?</strong>
      <p>Есть изменения после последнего JSON. Исполнение сцен продолжается.</p>
      <div>
        <button className="primary-button" onClick={onDownload} disabled={disabled}>
          Скачать JSON
        </button>
        <button onClick={onLater} disabled={disabled}>
          Через 10 минут
        </button>
      </div>
    </aside>
  );
}
