import { Fragment, useState, useSyncExternalStore } from 'react';
import { Check, Plus, Settings2, Trash2 } from 'lucide-react';
import type { Device, Parameter, Project } from '../domain/schema.ts';
import { COLORS } from '../domain/schema.ts';
import { clamp, clone, createDevice, parameterCode } from '../domain/project.ts';
import { Modal } from './Modal.tsx';
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
  const status = useSyncExternalStore(network.subscribe, network.getSnapshot);
  const [devices, setDevices] = useState(() => clone(project.devices));
  const [selected, setSelected] = useState<string | null>(initialId ?? null);
  const update = (id: string, patch: Partial<Device>) =>
    setDevices((list) => list.map((d) => (d.id === id ? { ...d, ...patch } : d)));
  const parameterChange = (device: Device, parameter: Parameter, patch: Partial<Parameter>) => {
    const transform = (p: Parameter) => (p.id === parameter.id ? { ...p, ...patch } : p);
    update(device.id, {
      dimmers: device.dimmers.map(transform),
      relays: device.relays.map(transform),
      audio: transform(device.audio),
      strip: transform(device.strip),
    });
  };
  const add = () => {
    if (devices.length >= 10) return;
    let n = 1;
    while (devices.some((d) => d.name === `Прибор ${n}`)) n++;
    const device = createDevice(n - 1);
    setDevices((list) => [...list, device]);
    setSelected(device.id);
  };
  const remove = (device: Device) => {
    if (devices.length <= 1 || !confirm(`Удалить прибор «${device.name}» из партитуры?`)) return;
    setDevices((list) => list.filter((d) => d.id !== device.id));
    if (selected === device.id) setSelected(null);
  };
  const parameters = (d: Device) => [...d.dimmers, ...d.relays, d.strip, d.audio];
  const save = () => {
    onSave(
      devices.map((device, index) => {
        const original = project.devices.find((d) => d.id === device.id);
        const normalize = (p: Parameter) => ({
          ...p,
          name:
            p.name.trim() ||
            (original ? parameters(original).find((item) => item.id === p.id)?.name : null) ||
            parameterCode(device, p),
        });
        return {
          ...device,
          name: device.name.trim() || original?.name || `Прибор ${index + 1}`,
          dimmers: device.dimmers.map(normalize),
          relays: device.relays.map(normalize),
          strip: normalize(device.strip),
          audio: normalize(device.audio),
        };
      }),
    );
    onClose();
  };
  return (
    <Modal title="Приборы" onClose={onClose} className="devices-modal">
      <div className="devices-toolbar">
        <button
          className="icon-button"
          aria-label="Добавить прибор"
          title={devices.length >= 10 ? 'Максимум 10 приборов' : 'Добавить прибор'}
          disabled={devices.length >= 10}
          onClick={add}
        >
          <Plus size={19} />
        </button>
      </div>
      <div className="feature-matrix-scroll">
        <table className="feature-matrix" aria-label="Состав приборов">
          <thead>
            <tr>
              <th>Прибор</th>
              {[
                'DIM1',
                'DIM2',
                'DIM3',
                'DIM4',
                'DIM5',
                'DIM6',
                'DIM7',
                'DIM8',
                'REL1',
                'REL2',
                'SPI',
                'SOU1',
              ].map((code) => (
                <th key={code}>{code}</th>
              ))}
              <th />
            </tr>
          </thead>
          <tbody>
            {devices.map((device) => (
              <Fragment key={device.id}>
                <tr data-config-device={device.id}>
                  <th>
                    <button
                      className="device-name"
                      onClick={() => setSelected(selected === device.id ? null : device.id)}
                    >
                      {device.name || '—'}
                    </button>
                    <code>
                      {status.devices[device.mac?.toUpperCase() ?? '']?.ip ?? device.ip ?? '—'}
                    </code>
                  </th>
                  {parameters(device).map((parameter) => (
                    <td key={parameter.id}>
                      <input
                        type="checkbox"
                        aria-label={`${device.name} · ${parameterCode(device, parameter)}`}
                        checked={parameter.enabled}
                        onChange={(e) =>
                          parameterChange(device, parameter, { enabled: e.target.checked })
                        }
                      />
                    </td>
                  ))}
                  <td className="device-actions">
                    <button
                      className="icon-button"
                      aria-label={`Настроить ${device.name}`}
                      onClick={() => setSelected(selected === device.id ? null : device.id)}
                    >
                      <Settings2 size={15} />
                    </button>
                    <button
                      className="icon-button delete-button"
                      aria-label={`Удалить ${device.name}`}
                      disabled={devices.length <= 1}
                      onClick={() => remove(device)}
                    >
                      <Trash2 size={15} />
                    </button>
                  </td>
                </tr>
                {selected === device.id && (
                  <tr className="device-details">
                    <td colSpan={14}>
                      <div className="device-detail-form">
                        <div className="device-detail-top">
                          <label className="field">
                            <span>Название прибора</span>
                            <input
                              aria-label="Название прибора"
                              value={device.name}
                              maxLength={120}
                              onChange={(e) => update(device.id, { name: e.target.value })}
                            />
                          </label>
                          <label className="field">
                            <span>IP</span>
                            <input
                              aria-label="IP прибора"
                              readOnly
                              value={
                                status.devices[device.mac?.toUpperCase() ?? '']?.ip ??
                                device.ip ??
                                '—'
                              }
                            />
                          </label>
                          <label className="field">
                            <span>Светодиодов</span>
                            <input
                              aria-label="Светодиодов в ленте"
                              type="number"
                              min="1"
                              max="10000"
                              value={device.ledCount}
                              onChange={(e) =>
                                update(device.id, {
                                  ledCount: Math.round(clamp(Number(e.target.value), 1, 10000)),
                                })
                              }
                            />
                          </label>
                        </div>
                        <label className="field">
                          <span>Контроллер</span>
                          <select
                            aria-label="Контроллер прибора"
                            value={device.mac ?? ''}
                            onChange={(e) =>
                              update(device.id, {
                                mac: e.target.value || null,
                                ip: status.devices[e.target.value]?.ip ?? null,
                              })
                            }
                          >
                            <option value="">Не назначен</option>
                            {device.mac && !status.devices[device.mac] && (
                              <option value={device.mac}>{device.mac}</option>
                            )}
                            {Object.entries(status.devices).map(([mac, info]) => (
                              <option key={mac} value={mac}>
                                {info.name ?? mac} · {info.ip ?? mac}
                              </option>
                            ))}
                          </select>
                        </label>
                        <div className="parameter-fine-grid">
                          {parameters(device).map((p) => {
                            const code = parameterCode(device, p);
                            return (
                              <div className="parameter-config" key={p.id}>
                                <code>{code}</code>
                                <input
                                  aria-label={`Название ${code}`}
                                  value={p.name}
                                  maxLength={120}
                                  onChange={(e) =>
                                    parameterChange(device, p, { name: e.target.value })
                                  }
                                />
                                <select
                                  aria-label={`Цвет ${code}`}
                                  value={p.color}
                                  onChange={(e) =>
                                    parameterChange(device, p, {
                                      color: e.target.value as Parameter['color'],
                                    })
                                  }
                                  style={{ color: COLORS[p.color].hex }}
                                >
                                  {Object.entries(COLORS).map(([key, color]) => (
                                    <option key={key} value={key}>
                                      {color.name}
                                    </option>
                                  ))}
                                </select>
                                {device.relays.some((item) => item.id === p.id) && (
                                  <select
                                    aria-label={`Уровень ${code}`}
                                    value={p.relayMode ?? 0}
                                    onChange={(e) =>
                                      parameterChange(device, p, {
                                        relayMode: Number(e.target.value) as 0 | 1 | 2,
                                      })
                                    }
                                  >
                                    <option value="0">Не настроено</option>
                                    <option value="1">HIGH включает</option>
                                    <option value="2">LOW включает</option>
                                  </select>
                                )}
                              </div>
                            );
                          })}
                        </div>
                      </div>
                    </td>
                  </tr>
                )}
              </Fragment>
            ))}
          </tbody>
        </table>
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
