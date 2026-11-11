import { useSyncExternalStore } from 'react';
import type { Device } from '../domain/schema.ts';
import { acParameter } from '../domain/project.ts';
import { COLORS } from '../domain/schema.ts';
import type { PreviewEngine } from '../domain/playback.ts';
import { StripCanvas } from './StripCanvas.tsx';
export function DevicePreview({ device, engine }: { device: Device; engine: PreviewEngine }) {
  const snapshot = useSyncExternalStore(engine.subscribe, engine.getSnapshot),
    runtime = snapshot.devices[device.id];
  if (!runtime) return null;
  return (
    <section className="device-preview">
      <div className="section-heading">
        <h3>{device.name}</h3>
      </div>
      <div className="preview-lights">
        {device.dimmers
          .filter((p) => p.enabled)
          .map((p, i) => (
            <div
              className="preview-light"
              key={p.id}
              title={p.name}
              data-live-parameter={p.id}
              data-live-value={Math.round(runtime.dimmers[p.id]?.value ?? 0)}
            >
              <div>
                <span>
                  {i + 1}
                  {runtime.dimmers[p.id]?.startAt != null ? ' ◷' : ''}
                </span>
                <b>
                  {Math.round(runtime.dimmers[p.id]?.value ?? 0)}
                  <small>%</small>
                </b>
              </div>
              <div className="level-track">
                <i
                  style={{
                    width: `${runtime.dimmers[p.id]?.value ?? 0}%`,
                    background: COLORS[p.color].hex,
                  }}
                />
              </div>
            </div>
          ))}
        {device.profile !== 'helmet' && acParameter(device).enabled && (
          <div
            className="preview-light"
            data-live-parameter="ac-220"
            data-live-value={Math.round(runtime.ac.value)}
            title="Расчёт команды Диммер220; не измерение сети"
          >
            <div>
              <span>220{runtime.ac.startAt != null ? ' ◷' : ''}</span>
              <b>
                {Math.round(runtime.ac.value)}
                <small>%</small>
              </b>
            </div>
            <div className="level-track">
              <i style={{ width: `${runtime.ac.value}%`, background: COLORS.warm.hex }} />
            </div>
          </div>
        )}
      </div>
      <div className="preview-relays">
        {device.relays
          .filter((p) => p.enabled)
          .map((p) => (
            <span key={p.id} className={runtime.relays[p.id]?.on ? 'is-on' : ''}>
              <i />
              {p.name}
              <b>
                {runtime.relays[p.id]?.due !== null
                  ? '…'
                  : runtime.relays[p.id]?.on
                    ? 'ВКЛ'
                    : 'ВЫКЛ'}
              </b>
            </span>
          ))}
      </div>
      {device.profile === 'helmet' && device.audio.enabled && (
        <p className="muted">Голос: модуль не подключён</p>
      )}
      {device.audio.enabled && device.profile !== 'helmet' && (
        <div className="preview-audio">
          <span>{device.audio.name}</span>
          <b>
            {runtime.audio.status === 'waiting'
              ? `№ ${runtime.audio.targetTrack} · ожидание`
              : runtime.audio.track === null
                ? '—'
                : `№ ${runtime.audio.track}`}
          </b>
          <span className="muted">
            {runtime.audio.status === 'waiting'
              ? '◷'
              : runtime.audio.status === 'playing'
                ? '▶'
                : runtime.audio.status === 'paused'
                  ? 'Ⅱ'
                  : '■'}
          </span>
          <span>
            {Math.round(runtime.audio.volume)}%{runtime.audio.loop ? ' ↻' : ''}
          </span>
        </div>
      )}
      {device.strip.enabled && (
        <div className="preview-strip">
          <div>
            <span>{device.strip.name}</span>
            <small>{runtime.strip?.effect.name ?? '—'}</small>
          </div>
          <StripCanvas
            effect={runtime.strip?.effect ?? null}
            startedAt={runtime.strip?.startedAt}
            count={device.ledCount}
          />
        </div>
      )}
    </section>
  );
}
