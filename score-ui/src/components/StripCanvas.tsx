import { useEffect, useRef } from 'react';
import type { StripEffect } from '../domain/schema.ts';
function rgb(hex: string) {
  return [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16));
}
export function StripCanvas({
  effect,
  startedAt,
  count = 60,
  tall = false,
}: {
  effect: StripEffect | null;
  startedAt?: number;
  count?: number;
  tall?: boolean;
}) {
  const ref = useRef<HTMLCanvasElement>(null),
    initial = useRef(performance.now());
  useEffect(() => {
    const canvas = ref.current;
    if (!canvas) return;
    let frame = 0;
    const draw = () => {
      const width = Math.max(1, canvas.clientWidth),
        height = tall ? 72 : 36,
        scale = window.devicePixelRatio || 1;
      if (
        canvas.width !== Math.round(width * scale) ||
        canvas.height !== Math.round(height * scale)
      ) {
        canvas.width = Math.round(width * scale);
        canvas.height = Math.round(height * scale);
      }
      const ctx = canvas.getContext('2d');
      if (!ctx) return;
      ctx.setTransform(scale, 0, 0, scale, 0, 0);
      ctx.clearRect(0, 0, width, height);
      const pixels = Math.min(count, 180),
        gap = pixels > 100 ? 1 : 2,
        size = width / pixels;
      const elapsed = Math.max(0, (performance.now() - (startedAt ?? initial.current)) / 1000);
      const phase = effect ? elapsed / effect.duration : 0;
      const fg = rgb(effect?.color ?? '#000000'),
        bg = rgb(effect?.background ?? '#000000');
      for (let i = 0; i < pixels; i++) {
        let x = pixels === 1 ? 0 : i / (pixels - 1);
        if (effect?.direction === 'reverse') x = 1 - x;
        let weight = effect?.mode === 'static' ? 1 : 0;
        if (effect?.mode === 'wave') {
          const center = (phase % 1) * (1 + effect.width / 100) - effect.width / 200;
          weight = Math.max(0, 1 - Math.abs(x - center) / (effect.width / 200));
        }
        if (effect?.mode === 'fill') {
          const front = Math.min(1 + effect.width / 100, phase * (1 + effect.width / 100));
          weight = Math.max(0, Math.min(1, (front - x) / (effect.width / 100)));
        }
        const color = fg.map((v, channel) =>
          Math.round(
            ((bg[channel] + (v - bg[channel]) * weight) * (effect?.brightness ?? 0)) / 100,
          ),
        );
        ctx.fillStyle = `rgb(${color.join(',')})`;
        ctx.fillRect(i * size, height * 0.23, Math.max(1, size - gap), height * 0.54);
        ctx.strokeStyle = '#ffffff0b';
        ctx.strokeRect(i * size, height * 0.23, Math.max(1, size - gap), height * 0.54);
      }
      if (effect && effect.mode !== 'static') frame = requestAnimationFrame(draw);
    };
    draw();
    const resize = new ResizeObserver(() => {
      if (!effect || effect.mode === 'static') draw();
    });
    resize.observe(canvas);
    return () => {
      cancelAnimationFrame(frame);
      resize.disconnect();
    };
  }, [effect, startedAt, count, tall]);
  return (
    <canvas
      ref={ref}
      className={`strip-canvas ${tall ? 'tall' : ''}`}
      aria-label="Предпросмотр ленты"
    />
  );
}
