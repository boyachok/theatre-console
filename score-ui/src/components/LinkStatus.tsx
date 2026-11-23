import { Lightbulb } from 'lucide-react';
import type { NetworkDeviceStatus } from '../transport/network.ts';
import { displayedLinkHealth, LINK_LABELS } from '../domain/link-health.ts';
const metric = (v: number | null | undefined, unit: string) =>
  typeof v === 'number' && Number.isFinite(v) ? `${Math.round(v * 10) / 10} ${unit}` : '—';
export function LinkStatus({
  report,
  connected = true,
}: {
  report?: Pick<NetworkDeviceStatus, 'online' | 'rttMs' | 'rssi' | 'health'>;
  connected?: boolean;
}) {
  const health = displayedLinkHealth(report, connected);
  const fresh = connected && report?.online === true;
  return (
    <div className="link-status" data-link-health={health}>
      <span
        className={`link-led link-led-${health}`}
        role="img"
        aria-label={LINK_LABELS[health]}
        title={LINK_LABELS[health]}
      />
      <span className="link-readings">
        <span>
          RTT <b>{metric(fresh ? report?.rttMs : null, 'мс')}</b>
        </span>
        <span>
          RSSI <b>{metric(fresh ? report?.rssi : null, 'dBm')}</b>
        </span>
      </span>
    </div>
  );
}
export function IdentifyButton({
  disabled,
  busy,
  onClick,
  name,
}: {
  disabled: boolean;
  busy?: boolean;
  onClick: () => void;
  name: string;
}) {
  return (
    <button
      type="button"
      className="icon-button identify-button"
      aria-label={`Мигать ${name}`}
      title="Мигать"
      disabled={disabled || busy}
      onClick={onClick}
    >
      <Lightbulb size={17} />
    </button>
  );
}
