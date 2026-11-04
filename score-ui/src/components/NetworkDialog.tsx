import { useState, useSyncExternalStore } from 'react';
import { Lightbulb, RotateCcw } from 'lucide-react';
import type { Project } from '../domain/schema.ts';
import type { NetworkSample, NetworkTransport } from '../transport/network.ts';
import { Modal } from './Modal.tsx';
const metric = (v: number | null | undefined, suffix = '') =>
  v == null ? '—' : `${Number(v.toFixed(1))}${suffix}`;
function HistoryChart({ samples, mode }: { samples: NetworkSample[]; mode: 'rtt' | 'rssi' }) {
  const visible = samples.slice(-120),
    values = visible.map((s) => (mode === 'rtt' ? s.rttMs : s.rssi)),
    known = values.filter((v): v is number => v !== null);
  const first = visible[0]?.at ?? 0,
    elapsed = Math.max(0, (visible.at(-1)?.at ?? 0) - first),
    max = mode === 'rtt' ? Math.max(100, ...known) : Math.max(-20, ...known),
    min = mode === 'rtt' ? 0 : Math.min(-100, ...known);
  const segments: string[] = [];
  let segment: string[] = [];
  values.forEach((v, i) => {
    if (v == null) {
      if (segment.length) segments.push(segment.join(' '));
      segment = [];
      return;
    }
    segment.push(
      `${45 + ((visible[i].at - first) / Math.max(1, elapsed)) * 630},${145 - ((v - min) / (max - min)) * 125}`,
    );
  });
  if (segment.length) segments.push(segment.join(' '));
  return (
    <svg
      className="network-chart"
      viewBox="0 0 700 178"
      role="img"
      aria-label={mode === 'rtt' ? 'График задержки' : 'График RSSI'}
    >
      {[0, 0.5, 1].map((r) => (
        <g key={r}>
          <line x1="45" x2="675" y1={20 + r * 125} y2={20 + r * 125} />
          <text x="36" y={24 + r * 125} textAnchor="end">
            {Math.round(max - r * (max - min))}
          </text>
        </g>
      ))}
      <text x="45" y="170">
        {values.length ? `−${Math.round(elapsed / 1000)} с` : '—'}
      </text>
      <text x="675" y="170" textAnchor="end">
        {values.length ? '0 с' : '—'}
      </text>
      {segments.map((points, i) => (
        <polyline key={i} points={points} />
      ))}
      {values.every((v) => v === null) && (
        <text className="empty-chart" x="360" y="90" textAnchor="middle">
          —
        </text>
      )}
    </svg>
  );
}
export function NetworkDialog({
  project,
  transport,
  onClose,
}: {
  project: Project;
  transport: NetworkTransport;
  onClose: () => void;
}) {
  const snapshot = useSyncExternalStore(transport.subscribe, transport.getSnapshot),
    [selected, setSelected] = useState(''),
    [mode, setMode] = useState<'rtt' | 'rssi'>('rtt'),
    [error, setError] = useState('');
  const rows =
    transport.kind === 'esp'
      ? Object.entries(snapshot.devices).map(([id, report]) => ({
          id,
          name: project.devices.find((d) => d.mac?.toUpperCase() === id)?.name ?? report.name ?? id,
          ip: report.ip,
          report,
        }))
      : project.devices.map((d) => ({
          id: d.id,
          name: d.name,
          ip: d.ip,
          report: snapshot.devices[d.mac ?? d.id],
        }));
  const device = rows.find((d) => d.id === selected) ?? rows[0],
    report = device?.report;
  const action = async (fn: () => unknown) => {
    try {
      setError('');
      await fn();
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Ошибка сети');
    }
  };
  return (
    <Modal title="Сеть" onClose={onClose} className="network-modal">
      <div className="network-content">
        <div className="network-toolbar">
          <label className="test-switch">
            <input
              type="checkbox"
              role="switch"
              aria-label="Пинг-тест"
              checked={snapshot.testEnabled}
              onChange={(e) => void action(() => transport.setMeasurement(e.target.checked))}
            />
            <span>Тест</span>
          </label>
          <span className={`network-connection ${snapshot.connected ? 'connected' : ''}`}>
            {snapshot.connected ? 'Подключено' : 'Нет подключения'}
          </span>
          <button
            className="icon-button"
            aria-label="Мигать мастером"
            title="Мигать мастером"
            disabled={!snapshot.connected || !transport.identify || !snapshot.masterMac}
            onClick={() => void action(() => transport.identify!(snapshot.masterMac!))}
          >
            <Lightbulb size={16} />
          </button>
          <button
            className="icon-button"
            aria-label="Сбросить сеть"
            title="Забыть контроллеры и запустить поиск"
            disabled={!snapshot.connected || !transport.resetNetwork}
            onClick={() => {
              if (
                confirm('Забыть список контроллеров и начать поиск заново? Партитура сохраняется.')
              )
                void action(() => transport.resetNetwork!());
            }}
          >
            <RotateCcw size={16} />
          </button>
          <div className="segmented">
            <button className={mode === 'rtt' ? 'selected' : ''} onClick={() => setMode('rtt')}>
              RTT
            </button>
            <button className={mode === 'rssi' ? 'selected' : ''} onClick={() => setMode('rssi')}>
              RSSI
            </button>
          </div>
        </div>
        {error && (
          <p role="alert" className="network-error">
            {error}
          </p>
        )}
        <div className="network-chart-heading">
          <select
            aria-label="Прибор на графике"
            value={device?.id ?? ''}
            onChange={(e) => setSelected(e.target.value)}
          >
            {!rows.length && <option value="">—</option>}
            {rows.map((d) => (
              <option key={d.id} value={d.id}>
                {d.name}
              </option>
            ))}
          </select>
          <b>{mode === 'rtt' ? metric(report?.rttMs, ' мс') : metric(report?.rssi, ' dBm')}</b>
        </div>
        <HistoryChart samples={report?.history ?? []} mode={mode} />
        <div className="network-table-scroll">
          <table className="network-table">
            <thead>
              <tr>
                {[
                  'Прибор',
                  'IP',
                  'Связь',
                  'RTT, мс',
                  'p95, мс',
                  'TX',
                  'RX',
                  'ACK',
                  'Потери',
                  'RSSI, dBm',
                  'Шаг',
                  'Версия',
                  '',
                ].map((label, i) => (
                  <th key={i}>{label}</th>
                ))}
              </tr>
            </thead>
            <tbody>
              {rows.map((d) => {
                const r = d.report;
                return (
                  <tr
                    key={d.id}
                    className={d.id === device?.id ? 'selected' : ''}
                    onClick={() => setSelected(d.id)}
                  >
                    <th>{d.name}</th>
                    <td>{d.ip ?? '—'}</td>
                    <td>
                      <span
                        className={`network-dot ${r?.online === true ? 'online' : r?.online === false ? 'offline' : ''}`}
                      />
                      {r?.online === true ? 'OK' : r?.online === false ? 'OFF' : '—'}
                    </td>
                    <td>{metric(r?.rttMs)}</td>
                    <td>{metric(r?.p95Ms)}</td>
                    <td>{r?.sent ?? '—'}</td>
                    <td>{r?.received ?? '—'}</td>
                    <td>{r?.acknowledged ?? '—'}</td>
                    <td>{r?.lost ?? '—'}</td>
                    <td>{metric(r?.rssi)}</td>
                    <td>{r?.confirmedStep ?? '—'}</td>
                    <td>{r?.scoreVersion ?? '—'}</td>
                    <td>
                      <button
                        className="icon-button"
                        aria-label={`Мигать ${d.name}`}
                        title="Мигать"
                        disabled={!snapshot.connected || !transport.identify}
                        onClick={() => void action(() => transport.identify!(r?.mac ?? d.id))}
                      >
                        <Lightbulb size={15} />
                      </button>
                    </td>
                  </tr>
                );
              })}
            </tbody>
          </table>
        </div>
      </div>
    </Modal>
  );
}
