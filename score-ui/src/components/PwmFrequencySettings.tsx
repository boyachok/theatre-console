import { useEffect, useState } from 'react';
import type { NetworkSnapshot, NetworkTransport } from '../transport/network.ts';
import { useConfirm } from './Confirm.tsx';
export function PwmFrequencySettings({
  mac,
  network,
  status,
}: {
  mac: string | null;
  network: NetworkTransport;
  status: NetworkSnapshot;
}) {
  const ask = useConfirm();
  const report = status.devices[mac?.toUpperCase() ?? ''];
  const config = report?.pwmFrequency;
  const [text, setText] = useState('');
  const [dirty, setDirty] = useState(false);
  const [busy, setBusy] = useState(false);
  const [message, setMessage] = useState('');
  useEffect(() => {
    if (!dirty) setText(config ? String(config.savedHz) : '');
  }, [config?.savedHz, dirty]);
  const hz = Number(text);
  const valid =
    text.trim() !== '' &&
    Number.isInteger(hz) &&
    !!config &&
    hz >= config.minHz &&
    hz <= config.maxHz;
  const permitted =
    !!network.setPwmFrequency &&
    report?.online === true &&
    status.editorLocked === false &&
    !status.running &&
    !status.livePreview &&
    !status.startupPending;
  const save = async () => {
    if (!permitted || !valid || !mac || busy || !network.setPwmFrequency) return;
    if (
      !(await ask({
        message: `Сохранить ${hz} Гц для всех 12 DIM? Это не относится к диммеру220. Частота изменится только после отдельного перезапуска платы; автоматического перезапуска не будет. Более низкая частота не гарантирует отсутствие полос на камере. Это отдельная запись настроек: «Отмена» окна приборов её не откатит.`,
        accept: 'Сохранить частоту',
      }))
    )
      return;
    setBusy(true);
    setMessage('');
    try {
      await network.setPwmFrequency(mac, hz);
      setDirty(false);
      setMessage('Сохранено. Применится после отдельного перезапуска платы.');
    } catch (error) {
      setMessage(error instanceof Error ? error.message : String(error));
    } finally {
      setBusy(false);
    }
  };
  return (
    <fieldset className="pwm-frequency-settings">
      <legend>Частота PWM · DIM1–12</legend>
      <p className="field-help">
        Одна частота для всех 12 низковольтных каналов. Диммер220 работает по переходам сети через
        ноль и сюда не относится.
      </p>
      {config ? (
        <>
          <p>
            Действующая: {config.activeHz} Гц · сохранённая: {config.savedHz} Гц
            {config.rebootRequired ? ' · нужен перезапуск' : ''}
          </p>
          <p className="field-help">
            MCPWM расчёт: {config.mcpwmHzCalculated} Гц, точность {config.bits} бит. Это расчёт
            таймера, не измерение сигнала.
          </p>
          <label className="field">
            <span>
              Частота PWM, Гц ({config.minHz}–{config.maxHz})
            </span>
            <input
              aria-label="Частота PWM DIM1–12"
              type="number"
              inputMode="numeric"
              min={config.minHz}
              max={config.maxHz}
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
            disabled={!permitted || !valid || busy || hz === config.savedHz}
            onClick={() => void save()}
          >
            Сохранить частоту PWM
          </button>
          {!permitted && (
            <p className="field-help">
              Нужны связь с контроллером, STOP и разблокированный редактор. STOP/перезапуск
              автоматически не отправляются.
            </p>
          )}
        </>
      ) : (
        <p className="field-help">
          Нужен привязанный доступный slave0.8.6+. На старой прошивке частота не меняется.
        </p>
      )}
      {message && <p role="status">{message}</p>}
    </fieldset>
  );
}
