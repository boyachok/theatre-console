import { createContext, useCallback, useContext, useEffect, useRef, useState } from 'react';
import type { ReactNode } from 'react';
import { Modal } from './Modal.tsx';
interface Options {
  message: string;
  accept?: string;
  remember?: 'delete-step';
}
interface Request extends Options {
  resolve: (answer: boolean) => void;
}
const Context = createContext<((options: Options) => Promise<boolean>) | null>(null);
export function useConfirm() {
  const ask = useContext(Context);
  if (!ask) throw new Error('ConfirmProvider missing');
  return ask;
}
export function ConfirmProvider({ children }: { children: ReactNode }) {
  const queue = useRef<Request[]>([]),
    skipped = useRef(new Set<string>()),
    [current, setCurrent] = useState<Request | null>(null),
    [remember, setRemember] = useState(false);
  const next = useCallback(() => {
    while (queue.current[0]?.remember && skipped.current.has(queue.current[0].remember!)) {
      queue.current.shift()!.resolve(true);
    }
    setRemember(false);
    setCurrent(queue.current[0] ?? null);
  }, []);
  const ask = useCallback(
    (options: Options) => {
      if (options.remember && skipped.current.has(options.remember)) return Promise.resolve(true);
      return new Promise<boolean>((resolve) => {
        queue.current.push({ ...options, resolve });
        if (queue.current.length === 1) next();
      });
    },
    [next],
  );
  const finish = (answer: boolean) => {
    const request = queue.current.shift();
    if (!request) return;
    if (answer && remember && request.remember) skipped.current.add(request.remember);
    request.resolve(answer);
    next();
  };
  useEffect(
    () => () => {
      for (const request of queue.current) request.resolve(false);
      queue.current = [];
    },
    [],
  );
  return (
    <Context.Provider value={ask}>
      {children}
      {current && (
        <Modal title="Подтверждение" className="confirm-modal" onClose={() => finish(false)}>
          <div className="confirm-content">
            <p>{current.message}</p>
            {current.remember && (
              <label className="confirm-remember">
                <input
                  type="checkbox"
                  checked={remember}
                  onChange={(e) => setRemember(e.target.checked)}
                />
                Не спрашивать до перезагрузки страницы
              </label>
            )}
          </div>
          <footer className="modal-footer">
            <button onClick={() => finish(false)}>Отмена</button>
            <button className="primary-button" onClick={() => finish(true)}>
              {current.accept ?? 'Продолжить'}
            </button>
          </footer>
        </Modal>
      )}
    </Context.Provider>
  );
}
