import { useEffect, useRef, useState, useSyncExternalStore } from 'react';
import { Check, Plus, Trash2 } from 'lucide-react';
import type { Device, Parameter, Project } from '../domain/schema.ts';
import { COLORS } from '../domain/schema.ts';
import {
  clone,
  createDevice,
  deviceProfile,
  parameterCode,
  acParameter,
  captureDevicePolarity,
} from '../domain/project.ts';
import { Modal } from './Modal.tsx';
import { useConfirm } from './Confirm.tsx';
import { IdentifyButton, LinkStatus } from './LinkStatus.tsx';
import type { NetworkTransport } from '../transport/network.ts';

export function DevicesDialog({
  project,
  initialId,
  onSave,
  onClose,
  network,
}: {
  project: Project;
  initialId?: string;
  onSave: (devices: Device[]) => void;
  onClose: () => void;
  network: NetworkTransport;
}) {
  const ask = useConfirm();
  const status = useSyncExternalStore(network.subscribe, network.getSnapshot);
  const [devices, setDevices] = useState(() => clone(project.devices));
  const [error, setError] = useState(''),
    [identifying, setIdentifying] = useState<string | null>(null);
  const cards = useRef(new Map<string, HTMLElement>());
  useEffect(() => {
    if (initialId) cards.current.get(initialId)?.scrollIntoView({ block: 'start' });
  }, [initialId]);
  const update = (id: string, patch: Partial<Device>) =>
    setDevices((list) => list.map((d) => (d.id === id ? { ...d, ...patch } : d)));
  const parameters = (d: Device) =>
    d.profile === 'helmet'
      ? [d.strip, d.audio]
      : [...d.dimmers.slice(0, 8), ...d.relays, acParameter(d), d.audio, d.strip];
  const parameterChange = (device: Device, parameter: Parameter, patch: Partial<Parameter>) => {
    const transform = (p: Parameter) => (p.id === parameter.id ? { ...p, ...patch } : p);
    update(device.id, {
      dimmers: device.dimmers.map(transform),
      relays: device.relays.map(transform),
      audio: transform(device.audio),
      strip: transform(device.strip),
      ...(parameter.id === acParameter(device).id
        ? { acDimmer: transform(acParameter(device)) }
        : {}),
    });
  };
  const add = () => {
    if (devices.length >= 10) return;
    let n = 1;
    while (devices.some((d) => d.name === `Прибор ${n}`)) n++;
    const fresh = createDevice(n - 1);
    const device = {
      ...fresh,
      dimmers: fresh.dimmers.map((p, i) => (i < 8 ? p : { ...p, enabled: false })),
    };
    setDevices((list) => [...list, device]);
    requestAnimationFrame(() => cards.current.get(device.id)?.scrollIntoView({ block: 'start' }));
  };
  const remove = async (device: Device) => {
    if (
      devices.length <= 1 ||
      !(await ask({ message: `Удалить прибор «${device.name}» из партитуры?`, accept: 'Удалить' }))
    )
      return;
    setDevices((list) => list.filter((d) => d.id !== device.id));
  };
  const identify = async (device: Device) => {
    if (!device.mac || !network.identify || identifying) return;
    setError('');
    setIdentifying(device.id);
    try {
      await network.identify(device.mac);
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Нет связи');
    } finally {
      setIdentifying(null);
    }
  };
  const save = () => {
    const next = devices.map((device, index) => {
      const original = project.devices.find((d) => d.id === device.id);
      const report = status.devices[device.mac?.toUpperCase() ?? ''];
      const measured = captureDevicePolarity(
        device,
        report?.online === true ? report.dimmerInverted : undefined,
      );
      const all = (d: Device) => [...d.dimmers, ...d.relays, d.strip, d.audio, acParameter(d)];
      const normalize = (p: Parameter) => ({
        ...p,
        name:
          p.name.trim() ||
          (original ? all(original).find((item) => item.id === p.id)?.name : null) ||
          parameterCode(device, p),
      });
      return {
        ...device,
        name: device.name.trim() || original?.name || `Прибор ${index + 1}`,
        // Hidden DIM9–12 and strip hardware settings are retained, not reset by this UI.
        dimmers: measured.dimmers.map((p, i) => (i < 8 ? normalize(p) : device.dimmers[i])),
        relays: device.relays.map(normalize),
        strip: normalize(device.strip),
        audio: normalize(device.audio),
        ...(device.acDimmer ? { acDimmer: normalize(device.acDimmer) } : {}),
      };
    });
    onSave(next);
    onClose();
  };
  return (
    <Modal title="Приборы" onClose={onClose} className="devices-modal operator-devices-modal">
      <div className="device-cards-scroll">
        {error && (
          <p className="network-error" role="alert">
            {error}
          </p>
        )}
        {devices.map((device) => {
          const report = status.devices[device.mac?.toUpperCase() ?? ''];
          const helmet = device.profile === 'helmet';
          const candidates = Object.entries(status.devices).filter(
            ([, info]) =>
              info.role !== 'remote' && (helmet ? info.role === 'helmet' : info.role !== 'helmet'),
          );
          return (
            <section
              key={device.id}
              className={`device-card ${helmet ? 'helmet-device-card' : ''}`}
              data-config-device={device.id}
              ref={(el) => {
                if (el) cards.current.set(device.id, el);
                else cards.current.delete(device.id);
              }}
            >
              <header className="device-card-heading">
                <input
                  aria-label="Название прибора"
                  value={device.name}
                  maxLength={120}
                  onChange={(e) => update(device.id, { name: e.target.value })}
                />
                <button
                  type="button"
                  className="icon-button delete-button"
                  aria-label={`Удалить ${device.name}`}
                  disabled={devices.length <= 1}
                  onClick={() => void remove(device)}
                >
                  <Trash2 size={16} />
                </button>
              </header>
              <div className="device-parameter-grid">
                {parameters(device).map((p) => {
                  const code = parameterCode(device, p),
                    dimmer = device.dimmers.findIndex((x) => x.id === p.id),
                    relay = device.relays.some((x) => x.id === p.id);
                  return (
                    <div
                      key={p.id}
                      className={`parameter-cell ${p.enabled ? '' : 'parameter-cell-off'}`}
                      data-parameter-cell={p.id}
                    >
                      <label className="parameter-cell-toggle">
                        <input
                          type="checkbox"
                          aria-label={`${device.name} · ${code}`}
                          checked={p.enabled}
                          onChange={(e) =>
                            parameterChange(device, p, { enabled: e.target.checked })
                          }
                        />
                        <strong>{code}</strong>
                      </label>
                      <fieldset disabled={!p.enabled} className="parameter-cell-fields">
                        <input
                          aria-label={`Название ${code}`}
                          value={p.name}
                          maxLength={120}
                          onChange={(e) => parameterChange(device, p, { name: e.target.value })}
                        />
                        <select
                          className="parameter-color-select"
                          aria-label={`Цвет ${code}`}
                          value={p.color}
                          style={{ borderLeftColor: COLORS[p.color].hex }}
                          onChange={(e) =>
                            parameterChange(device, p, {
                              color: e.target.value as Parameter['color'],
                            })
                          }
                        >
                          {Object.entries(COLORS).map(([key, color]) => (
                            <option key={key} value={key}>
                              {color.name}
                            </option>
                          ))}
                        </select>
                        {dimmer >= 0 && (
                          <label className="parameter-inversion">
                            <input
                              type="checkbox"
                              aria-label={`Инвертировать ${code}`}
                              checked={
                                p.inverted ?? !!((report?.dimmerInverted ?? 0) & (1 << dimmer))
                              }
                              onChange={(e) =>
                                parameterChange(device, p, { inverted: e.target.checked })
                              }
                            />
                            Инверсия
                          </label>
                        )}
                        {relay && (
                          <select
                            aria-label={`Уровень ${code}`}
                            value={p.relayMode ?? 0}
                            onChange={(e) =>
                              parameterChange(device, p, {
                                relayMode: Number(e.target.value) as 0 | 1 | 2,
                              })
                            }
                          >
                            <option value="0">—</option>
                            <option value="1">HIGH</option>
                            <option value="2">LOW</option>
                          </select>
                        )}
                      </fieldset>
                    </div>
                  );
                })}
                <div className="device-meta-cell">
                  <label className="field">
                    <span>Тип контроллера</span>
                    <select
                      aria-label="Тип прибора"
                      value={device.profile ?? 'standard'}
                      onChange={(e) =>
                        update(device.id, {
                          ...deviceProfile(device, e.target.value as 'standard' | 'helmet'),
                          mac: null,
                          ip: null,
                        })
                      }
                    >
                      <option value="standard">Слейв</option>
                      <option value="helmet">Шлем</option>
                    </select>
                  </label>
                </div>
                <div className="device-meta-cell controller-binding-cell">
                  <label className="field">
                    <span>Ведомый</span>
                    <select
                      aria-label="Контроллер прибора"
                      value={device.mac?.toUpperCase() ?? ''}
                      onChange={(e) =>
                        update(device.id, {
                          mac: e.target.value || null,
                          ip: status.devices[e.target.value]?.ip ?? null,
                        })
                      }
                    >
                      <option value="">Не назначен</option>
                      {device.mac &&
                        !candidates.some(([mac]) => mac === device.mac?.toUpperCase()) && (
                          <option value={device.mac.toUpperCase()}>{device.mac}</option>
                        )}
                      {candidates.map(([mac, info]) => (
                        <option
                          key={mac}
                          value={mac}
                          disabled={devices.some(
                            (d) => d.id !== device.id && d.mac?.toUpperCase() === mac,
                          )}
                        >
                          {info.name ? `${info.name} · ` : ''}
                          {mac}
                        </option>
                      ))}
                    </select>
                  </label>
                </div>
                <div className="device-meta-cell device-link-cell">
                  <div className="device-link-heading">
                    <span>Связь</span>
                    <IdentifyButton
                      name={device.name}
                      disabled={
                        !status.connected ||
                        !network.identify ||
                        !device.mac ||
                        report?.online !== true
                      }
                      busy={identifying === device.id}
                      onClick={() => void identify(device)}
                    />
                  </div>
                  <LinkStatus report={report} connected={status.connected} />
                </div>
              </div>
            </section>
          );
        })}
        <button
          type="button"
          className="add-device-button"
          disabled={devices.length >= 10}
          onClick={add}
        >
          <Plus size={17} />
          Добавить прибор
        </button>
      </div>
      <footer className="modal-footer">
        <button onClick={onClose}>Отмена</button>
        <button className="primary-button" onClick={save}>
          <Check size={15} />
          Сохранить
        </button>
      </footer>
    </Modal>
  );
}
