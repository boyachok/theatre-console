// Palette/status names match console/network_led.h. This is link telemetry, not load feedback.
export const LINK_HEALTH = ['warmup', 'excellent', 'good', 'marginal', 'bad', 'offline'] as const;
export type LinkHealth = (typeof LINK_HEALTH)[number];
export const LINK_LABELS: Record<LinkHealth | 'unknown', string> = {
  warmup: 'Поиск',
  excellent: 'Отличная связь',
  good: 'Хорошая связь',
  marginal: 'Слабая связь',
  bad: 'Плохая связь',
  offline: 'Нет связи',
  unknown: 'Нет данных',
};
export function parseLinkHealth(value: unknown): LinkHealth | undefined {
  const name =
    typeof value === 'number' && Number.isInteger(value)
      ? LINK_HEALTH[value]
      : typeof value === 'string'
        ? value.toLowerCase()
        : undefined;
  return LINK_HEALTH.includes(name as LinkHealth) ? (name as LinkHealth) : undefined;
}
export function displayedLinkHealth(
  report?: { online?: boolean | null; health?: LinkHealth },
  connected = true,
): LinkHealth | 'unknown' {
  if (!connected || report?.online == null) return 'unknown';
  if (!report.online) return 'offline';
  return report.health ?? 'unknown';
}
