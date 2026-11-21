import { useEffect, useState } from 'react';
import type { NetworkSnapshot, NetworkTransport } from '../transport/network.ts';
import { useConfirm } from './Confirm.tsx';
export function AcPulseSettings({
  mac,
  network,
  status,
}: {
  mac: string | null;
  network: NetworkTransport;
  status: NetworkSnapshot;
}) {
  const ask = useConfirm(),
    report = status.devices[mac?.toUpperCase() ?? ''],
    config = report?.acDimmer;
  const [text, setText] = useState(''),
    [dirty, setDirty] = useState(false),
    [busy, setBusy] = useState(false),
    [message, setMessage] = useState('');
  useEffect(() => {
    if (!dirty) setText(config ? String(config.pulseUs) : '');
  }, [config?.pulseUs, dirty]);
  const us = Number(text),
    valid =
      text.trim() !== '' &&
      Number.isInteger(us) &&
      !!config &&
      us >= config.minUs &&
      us <= config.maxUs;
  const permitted =
    !!network.setAcPulseUs &&
    report?.online === true &&
    !!config &&
    config.ageMs < 3000 &&
    !config.requested &&
    !config.paired &&
    status.editorLocked === false &&
    !status.running &&
    !status.livePreview &&
    !status.startupPending &&
    !status.syncing;
  const save = async () => {
    if (!permitted || !valid || !mac || busy || !network.setAcPulseUs) return;
    if (
      !(await ask({
        message: `Пробный импульс открытия220: ${us} мкс вместо ${config?.pulseUs} мкс. Исходный режим200 мкс. Допустимость импульса для модуля проверяется отдельно; устранение мерцания не гарантируется. Сохранится в NVS и применится в STOP без перезапуска, но лампы не включатся. При длинном импульсе нижний предел диммирования может немного подняться; 0 остаётся OFF. Запись отдельная: «Отмена» окна её не откатит.`,
        accept: 'Подтвердить импульс220',
      }))
    )
      return;
    setBusy(true);
    setMessage('');
    try {
      await network.setAcPulseUs(mac, us);
      setDirty(false);
      setMessage('Импульс сохранён. Для испытания нужен отдельный запуск сцены.');
    } catch (e) {
      setMessage(e instanceof Error ? e.message : String(e));
    } finally {
      setBusy(false);
    }
  };
  return (
    <fieldset className="pwm-frequency-settings">
      <legend>Диммер220 · пробный импульс открытия</legend>
      <p className="field-help">
        Не частота PWM и не изменение крутизны силового фронта. По умолчанию200 мкс; проверяем
        гипотезу неустойчивого открытия симистора.
      </p>
      {config ? (
        <>
          <p>
            Импульс: {config.pulseUs} мкс · синхронизация:{' '}
            {config.qualified ? 'захвачена' : 'ожидание'} · полупериод: {config.halfUs} мкс
          </p>
          <p className="field-help">
            Накопительные программные счётчики: Z-C {config.goodZero}, назначено {config.scheduled},
            пропущено {config.skippedPulses}, опоздания {config.late}, потери синхронизации{' '}
            {config.syncLosses}. Не измерение тока лампы.
          </p>
          {config.configFault && (
            <p role="alert">
              Ошибка CRC/NVS: AC заблокирован. Сохраните корректный импульс; запуск сцены отдельно.
            </p>
          )}
          <label className="field">
            <span>
              Импульс открытия220, мкс ({config.minUs}–{config.maxUs})
            </span>
            <input
              aria-label="Импульс открытия220, мкс"
              type="number"
              inputMode="numeric"
              min={config.minUs}
              max={config.maxUs}
              step="1"
              value={text}
              disabled={busy}
              onChange={(e) => {
                setText(e.target.value);
                setDirty(true);
                setMessage('');
              }}
            />
          </label>
          <button
            type="button"
            disabled={
              !permitted || !valid || busy || (us === config.pulseUs && !config.configFault)
            }
            onClick={() => void save()}
          >
            Сохранить импульс220
          </button>
          <button
            type="button"
            disabled={busy}
            onClick={() => {
              setText('200');
              setDirty(true);
              setMessage('');
            }}
          >
            Ввести исходные200 мкс
          </button>
          {!permitted && (
            <p className="field-help">
              Изменение только в STOP/LIVEoff при выключенной AC-нагрузке, свежей диагностике и
              unlocked-редакторе. Никаких автоматических STOP/GO.
            </p>
          )}
        </>
      ) : (
        <p className="field-help">
          Нужен привязанный slave0.8.7+ с диагностикой220. Старые прошивки остаются в прежнем
          режиме.
        </p>
      )}
      {message && <p role="status">{message}</p>}
    </fieldset>
  );
}
