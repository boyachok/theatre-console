import { useEffect, useRef } from 'react';
import type { StripEffect } from '../domain/schema.ts';
import { stripColor } from '../domain/strip.ts';
export function StripCanvas({
  effect,
  startedAt,
  count = 60,
  tall = false,
  audioLevel,
}: {
  effect: StripEffect | null;
  startedAt?: number;
  count?: number;
  tall?: boolean;
  audioLevel?: number;
}) {
  const ref = useRef<HTMLCanvasElement>(null),
    initial = useRef(performance.now()),
    lastVoice = useRef<number | null>(null);
  useEffect(() => {
    const canvas = ref.current;
    if (!canvas) return;
    let frame = 0;
    const draw = () => {
      const w = Math.max(1, canvas.clientWidth),
        h = tall ? 72 : 36,
        scale = window.devicePixelRatio || 1;
      if (canvas.width !== Math.round(w * scale) || canvas.height !== Math.round(h * scale)) {
        canvas.width = Math.round(w * scale);
        canvas.height = Math.round(h * scale);
      }
      const ctx = canvas.getContext('2d');
      if (!ctx) return;
      ctx.setTransform(scale, 0, 0, scale, 0, 0);
      ctx.clearRect(0, 0, w, h);
      const pixels = Math.min(count, 180),
        gap = pixels > 100 ? 1 : 2,
        size = w / pixels,
        now = performance.now(),
        elapsed = Math.max(0, (now - (startedAt ?? initial.current)) / 1000);
      let gate = 1;
      if (effect?.audioReactive) {
        const threshold = 10 ** ((-60 + (effect.sensitivity ?? 50) * 0.4) / 20);
        if (audioLevel !== undefined && audioLevel >= threshold) lastVoice.current = now;
        gate =
          lastVoice.current === null
            ? 0
            : effect.release === 0
              ? Number(audioLevel !== undefined && audioLevel >= threshold)
              : Math.max(0, 1 - (now - lastVoice.current) / ((effect.release ?? 0.4) * 1000));
      }
      for (let i = 0; i < pixels; i++) {
        const color = effect ? stripColor(effect, i, pixels, elapsed, gate) : [0, 0, 0];
        ctx.fillStyle = `rgb(${color.join(',')})`;
        ctx.fillRect(i * size, h * 0.23, Math.max(1, size - gap), h * 0.54);
        ctx.strokeStyle = '#ffffff0b';
        ctx.strokeRect(i * size, h * 0.23, Math.max(1, size - gap), h * 0.54);
      }
      if (effect && (effect.mode !== 'static' || effect.audioReactive))
        frame = requestAnimationFrame(draw);
    };
    draw();
    const resize = new ResizeObserver(() => {
      if (!effect || (effect.mode === 'static' && !effect.audioReactive)) draw();
    });
    resize.observe(canvas);
    return () => {
      cancelAnimationFrame(frame);
      resize.disconnect();
    };
  }, [effect, startedAt, count, tall, audioLevel]);
  return (
    <canvas
      ref={ref}
      className={`strip-canvas ${tall ? 'tall' : ''}`}
      aria-label="Предпросмотр ленты"
    />
  );
}
