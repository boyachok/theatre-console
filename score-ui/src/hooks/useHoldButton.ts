import { useEffect, useRef, useState } from 'react';
import type { KeyboardEvent, PointerEvent } from 'react';
export function useHoldButton(shortPress: () => void, longPress: () => void) {
  const callbacks = useRef({ shortPress, longPress });
  callbacks.current = { shortPress, longPress };
  const state = useRef<{
    timer: ReturnType<typeof setTimeout> | null;
    pressed: boolean;
    fired: boolean;
  }>({ timer: null, pressed: false, fired: false });
  const [holding, setHolding] = useState(false);
  const cancel = () => {
    if (state.current.timer) clearTimeout(state.current.timer);
    state.current.pressed = false;
    state.current.timer = null;
    setHolding(false);
  };
  const start = () => {
    if (state.current.pressed) return;
    state.current.pressed = true;
    state.current.fired = false;
    setHolding(true);
    state.current.timer = setTimeout(() => {
      if (!state.current.pressed) return;
      state.current.fired = true;
      callbacks.current.longPress();
    }, 3000);
  };
  const finish = () => {
    if (!state.current.pressed) return;
    const fire = !state.current.fired;
    cancel();
    if (fire) callbacks.current.shortPress();
  };
  useEffect(() => {
    const blur = () => cancel(),
      visibility = () => {
        if (document.hidden) cancel();
      };
    window.addEventListener('blur', blur);
    document.addEventListener('visibilitychange', visibility);
    return () => {
      if (state.current.timer) clearTimeout(state.current.timer);
      window.removeEventListener('blur', blur);
      document.removeEventListener('visibilitychange', visibility);
    };
  }, []);
  return {
    holding,
    handlers: {
      onPointerDown: (event: PointerEvent<HTMLButtonElement>) => {
        if (event.button !== 0) return;
        event.currentTarget.setPointerCapture(event.pointerId);
        start();
      },
      onPointerUp: (event: PointerEvent<HTMLButtonElement>) => {
        const r = event.currentTarget.getBoundingClientRect();
        if (
          event.clientX < r.left ||
          event.clientX > r.right ||
          event.clientY < r.top ||
          event.clientY > r.bottom
        )
          cancel();
        else finish();
      },
      onPointerCancel: cancel,
      onLostPointerCapture: cancel,
      onContextMenu: (event: React.MouseEvent) => event.preventDefault(),
      onKeyDown: (event: KeyboardEvent<HTMLButtonElement>) => {
        if (event.key === ' ' || event.key === 'Enter') {
          event.preventDefault();
          if (!event.repeat) start();
        }
      },
      onKeyUp: (event: KeyboardEvent<HTMLButtonElement>) => {
        if (event.key === ' ' || event.key === 'Enter') {
          event.preventDefault();
          finish();
        }
      },
      onBlur: cancel,
      onClick: (event: React.MouseEvent) => {
        if (event.detail === 0 && !state.current.pressed) callbacks.current.shortPress();
      },
    },
  };
}
