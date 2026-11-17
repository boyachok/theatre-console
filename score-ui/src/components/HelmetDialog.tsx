import { useRef, useSyncExternalStore } from 'react';
import type { Device } from '../domain/schema.ts';
import {
  voiceBank,
  voiceSettings,
  voiceAlgorithms,
  type VoiceSettings,
  type VoiceAlgorithm,
} from '../domain/voice.ts';
import { uid } from '../domain/project.ts';
import type { NetworkTransport } from '../transport/network.ts';
import { Modal } from './Modal.tsx';
import { SliderField } from './DimmerAnimationEditor.tsx';
import { useConfirm } from './Confirm.tsx';
const labels = [
  'Кольцевая модуляция',
  'Вокодер',
  'Автотюн',
  'Робот без интонации',
  'Голосовой чип',
];
export function HelmetDialog({
  device,
  onChange,
  onClose,
  network,
}: {
  device: Device;
  onChange: (d: Device) => void;
  onClose: () => void;
  network: NetworkTransport;
}) {
  const bank = voiceBank(device),
    sound = voiceSettings(device),
    preset = bank.presets.find((p) => p.id === bank.selected)!;
  const ask = useConfirm(),
    frequencies = useRef<Partial<Record<VoiceAlgorithm, number>>>({});
  const snapshot = useSyncExternalStore(network.subscribe, network.getSnapshot),
    node = snapshot.devices[(device.mac ?? '').toUpperCase()],
    v = node?.voice;
  const edit = (patch: Partial<VoiceSettings>) =>
    onChange({ ...device, voice: { ...bank, working: { ...sound, ...patch } } });
  const slider = (
    key: keyof VoiceSettings,
    label: string,
    min: number,
    max: number,
    step = 1,
    suffix = '',
  ) => (
    <SliderField
      key={key}
      label={label}
      min={min}
      max={max}
      step={step}
      value={sound[key] as number}
      suffix={suffix}
      onChange={(n) => edit({ [key]: n })}
    />
  );
  const selectPreset = async (id: string) => {
    if (
      bank.working &&
      !(await ask({
        message: 'Загрузить другой пресет вместо несохранённых звуковых параметров?',
        accept: 'Загрузить',
      }))
    )
      return;
    onChange({ ...device, voice: { ...bank, selected: id, working: undefined } });
  };
  return (
    <Modal title={`Звук шлема · ${device.name}`} onClose={onClose} className="helmet-modal">
      <div className="helmet-content">
        <p className="muted helmet-note">
          Общие настройки этого шлема для ВСЕХ сцен. LIVE — прослушивание сразу; OFF —
          редактирование. Включение голоса и свет задаются отдельно в каждой сцене.
        </p>
        <div className="helmet-preset-fields">
          <label className="field">
            <span>Звуковой пресет</span>
            <select
              aria-label="Звуковой пресет шлема"
              value={bank.selected}
              onChange={(e) => void selectPreset(e.target.value)}
            >
              {bank.presets.map((p) => (
                <option key={p.id} value={p.id}>
                  {p.name}
                </option>
              ))}
            </select>
          </label>
          <label className="field">
            <span>Название пресета</span>
            <input
              aria-label="Название голосового пресета"
              maxLength={28}
              value={preset.name}
              onChange={(e) =>
                onChange({
                  ...device,
                  voice: {
                    ...bank,
                    presets: bank.presets.map((p) =>
                      p.id === preset.id
                        ? { ...p, name: e.target.value.trim().length ? e.target.value : 'Голос' }
                        : p,
                    ),
                  },
                })
              }
            />
          </label>
        </div>
        <div className="segmented helmet-preset-actions">
          <button
            onClick={() =>
              onChange({
                ...device,
                voice: {
                  ...bank,
                  working: undefined,
                  presets: bank.presets.map((p) =>
                    p.id === preset.id
                      ? { ...p, name: p.name.trim() || 'Голос', settings: { ...sound } }
                      : p,
                  ),
                },
              })
            }
          >
            Сохранить пресет
          </button>
          <button
            disabled={bank.presets.length >= 8}
            onClick={() => {
              const id = uid();
              onChange({
                ...device,
                voice: {
                  selected: id,
                  presets: [
                    ...bank.presets,
                    { id, name: `Голос ${bank.presets.length + 1}`, settings: { ...sound } },
                  ],
                },
              });
            }}
          >
            Сохранить как новый
          </button>
          <button
            disabled={bank.presets.length <= 1}
            onClick={async () => {
              if (await ask({ message: 'Удалить голосовой пресет?', accept: 'Удалить' })) {
                const list = bank.presets.filter((p) => p.id !== preset.id);
                onChange({ ...device, voice: { selected: list[0].id, presets: list } });
              }
            }}
          >
            Удалить пресет
          </button>
        </div>
        <p className="muted helmet-note">
          {bank.working
            ? 'Параметры отличаются от сохранённого пресета.'
            : 'Выбран сохранённый пресет.'}{' '}
          Пресеты и текущие параметры входят в JSON-бэкап.
        </p>
        <p role="status" className="helmet-status">
          {!node?.online
            ? 'Шлем не подтверждён в сети'
            : !v
              ? 'Нет аудиотелеметрии'
              : v.ageMs > 3000
                ? 'Аудиоданные устарели'
                : v.ready
                  ? `Аудиодвижок готов · DSP ${v.load.toFixed(1)}% · ошибок ${v.errors} · просрочек ${v.late}`
                  : 'Аудиодвижок недоступен'}
        </p>
        <label className="field">
          <span>Уровень микрофона (после усиления)</span>
          <meter
            aria-label="Уровень микрофона шлема"
            min={0}
            max={1}
            value={v && v.ageMs < 3000 ? v.inputLevel : 0}
          />
        </label>
        <details>
          <summary>Параметры звука</summary>
          <div className="helmet-fields">
            <div className="segmented helmet-wide">
              <button
                className={sound.mode === 'dry' ? 'selected' : ''}
                onClick={() => edit({ mode: 'dry' })}
              >
                Без эффекта
              </button>
              <button
                className={sound.mode === 'robot' ? 'selected' : ''}
                onClick={() => edit({ mode: 'robot' })}
              >
                С эффектом
              </button>
            </div>
            <label className="field helmet-wide">
              <span>Алгоритм голоса</span>
              <select
                aria-label="Алгоритм голоса"
                value={sound.algorithm}
                onChange={(e) => {
                  const algorithm = e.target.value as VoiceAlgorithm;
                  frequencies.current[sound.algorithm] = sound.frequency;
                  edit({
                    algorithm,
                    frequency: Math.min(
                      algorithm === 'chip' ? 1000 : 400,
                      Math.max(
                        algorithm === 'monotone' ? 65 : 20,
                        frequencies.current[algorithm] ?? sound.frequency,
                      ),
                    ),
                  });
                }}
              >
                {voiceAlgorithms.map((a, i) => (
                  <option key={a} value={a}>
                    {labels[i]}
                  </option>
                ))}
              </select>
            </label>
            {slider('gain', 'Усиление микрофона', 0.25, 64, 0.25, '×')}
            {slider('volume', 'Общая громкость', 0, 1, 0.01)}
            {slider('gateDb', 'Порог отсечения шума', -90, -25, 1, 'дБ')}
            {slider('mix', 'Оригинал / эффект', 0, 1, 0.01)}
            {sound.algorithm === 'autotune' ? (
              <>
                <label className="field">
                  Тональность
                  <select
                    aria-label="Тональность голоса"
                    value={sound.tuneKey}
                    onChange={(e) => edit({ tuneKey: Number(e.target.value) })}
                  >
                    {[
                      'До',
                      'До♯',
                      'Ре',
                      'Ми♭',
                      'Ми',
                      'Фа',
                      'Фа♯',
                      'Соль',
                      'Ля♭',
                      'Ля',
                      'Си♭',
                      'Си',
                    ].map((n, i) => (
                      <option key={n} value={i}>
                        {n}
                      </option>
                    ))}
                  </select>
                </label>
                <label className="field">
                  Лад
                  <select
                    aria-label="Лад голоса"
                    value={sound.tuneScale}
                    onChange={(e) => edit({ tuneScale: Number(e.target.value) })}
                  >
                    <option value={0}>Хроматический</option>
                    <option value={1}>Мажор</option>
                    <option value={2}>Минор</option>
                  </select>
                </label>
                {slider('retuneMs', 'Переход между нотами', 0, 250, 1, 'мс')}
                {slider('tuneStrength', 'Сила автотюна', 0, 1, 0.01)}
                {slider('transpose', 'Транспонирование', -12, 12, 1, 'полутонов')}
              </>
            ) : (
              slider(
                'frequency',
                'Высота голоса / частота',
                sound.algorithm === 'monotone' ? 65 : 20,
                sound.algorithm === 'chip' ? 1000 : 400,
                1,
                'Гц',
              )
            )}
            {sound.algorithm === 'ring' && slider('wave', 'Характер дрожания', 0, 1, 0.01)}
            {sound.algorithm === 'vocoder' && (
              <>
                {slider('formant', 'Окраска гласных', -6, 6, 0.25)}
                {slider('clarity', 'Чёткость согласных', 0, 1, 0.01)}
              </>
            )}
            {slider('drive', 'Хрип голоса', 0, 12, 0.1)}
            {slider('toneHz', 'Срез высоких частот (0 — выключен)', 0, 20000, 20, 'Гц')}
            {sound.algorithm === 'chip' &&
              slider('breathReduction', 'Приглушение дыхания', 0, 100, 1, '%')}
            <div className="helmet-checks helmet-wide">
              <label>
                <input
                  type="checkbox"
                  checked={sound.notch}
                  onChange={(e) => edit({ notch: e.target.checked })}
                />{' '}
                Приглушить гул 50 Гц
              </label>
              <label>
                <input
                  type="checkbox"
                  checked={sound.reduceThumps}
                  onChange={(e) => edit({ reduceThumps: e.target.checked })}
                />{' '}
                Срез глухих стуков
              </label>
            </div>
          </div>
          <p className="muted helmet-note">
            Порог сравнивается с микрофоном до усиления; ближе к −25 дБ — сильнее отсечение. −90…−89
            дБ — без отсечения. Изменения прослушиваются в LIVE.
          </p>
        </details>
      </div>
      <footer className="modal-footer">
        <button onClick={onClose}>Закрыть</button>
      </footer>
    </Modal>
  );
}
