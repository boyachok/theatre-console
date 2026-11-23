import { useState, useSyncExternalStore } from 'react';
import type { Project } from '../domain/schema.ts';
import type { NetworkTransport } from '../transport/network.ts';
import { Modal } from './Modal.tsx';
import { IdentifyButton, LinkStatus } from './LinkStatus.tsx';

export function NetworkDialog({
  project,
  transport,
  onClose,
}: {
  project: Project;
  transport: NetworkTransport;
  onClose: () => void;
}) {
  const snapshot = useSyncExternalStore(transport.subscribe, transport.getSnapshot);
  const [error, setError] = useState(''),
    [busy, setBusy] = useState<string | null>(null);
  const rows =
    transport.kind === 'esp'
      ? Object.entries(snapshot.devices).map(([mac, report]) => ({
          mac,
          report,
          name:
            project.devices.find((d) => d.mac?.toUpperCase() === mac)?.name ??
            report.name ??
            (report.role === 'remote' ? 'Пульт' : mac),
        }))
      : project.devices.map((d) => ({
          mac: d.mac ?? d.id,
          report: snapshot.devices[d.mac ?? d.id],
          name: d.name,
        }));
  const identify = async (mac: string) => {
    if (!transport.identify || busy) return;
    setError('');
    setBusy(mac);
    try {
      await transport.identify(mac);
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Нет связи');
    } finally {
      setBusy(null);
    }
  };
  return (
    <Modal title="Сеть" onClose={onClose} className="network-modal operator-network-modal">
      <div className="operator-network-content">
        {error && (
          <p role="alert" className="network-error">
            {error}
          </p>
        )}
        <div className="operator-network-row" data-master-connection>
          <div className="network-device-label">
            <strong>Мастер</strong>
            <span>{snapshot.connected ? 'Подключено' : 'Нет связи'}</span>
          </div>
          <IdentifyButton
            name="мастером"
            disabled={!snapshot.connected || !transport.identify || !snapshot.masterMac}
            busy={busy === snapshot.masterMac}
            onClick={() => {
              if (snapshot.masterMac) void identify(snapshot.masterMac);
            }}
          />
        </div>
        {rows.map(({ mac, report, name }) => (
          <div key={mac} className="operator-network-row" data-network-device={mac}>
            <div className="network-device-label" title={mac}>
              <strong>{name}</strong>
            </div>
            <LinkStatus report={report} connected={snapshot.connected} />
            <IdentifyButton
              name={name}
              disabled={!snapshot.connected || !transport.identify || report?.online !== true}
              busy={busy === mac}
              onClick={() => void identify(mac)}
            />
          </div>
        ))}
      </div>
    </Modal>
  );
}
