export interface NetworkSample {
  at: number;
  rttMs: number | null;
  rssi: number | null;
}
export interface NetworkDeviceStatus {
  role?: 'slave' | 'remote' | 'helmet';
  mac?: string;
  name?: string;
  scoreSupported?: boolean;
  scoreVersion?: number;
  ip: string | null;
  online: boolean | null;
  sent: number;
  received: number;
  acknowledged: number;
  lost: number;
  rttMs: number | null;
  p95Ms: number | null;
  rssi: number | null;
  confirmedStep: number | null;
  history: NetworkSample[];
}
export interface NetworkSnapshot {
  connected: boolean;
  testEnabled: boolean;
  devices: Record<string, NetworkDeviceStatus>;
  masterMac?: string;
  activeStep?: number | null;
  preview?: boolean;
}
export interface NetworkTransport {
  readonly kind: 'local' | 'esp';
  subscribe(listener: () => void): () => void;
  getSnapshot(): NetworkSnapshot;
  setMeasurement(enabled: boolean): void | Promise<void>;
  identify?(mac: string): Promise<void>;
  resetNetwork?(): Promise<void>;
}
// Editor-only adapter: changing the switch never invents packets or RF measurements.
export class DisconnectedNetworkTransport implements NetworkTransport {
  readonly kind = 'local' as const;
  private snapshot: NetworkSnapshot = { connected: false, testEnabled: false, devices: {} };
  private listeners = new Set<() => void>();
  getSnapshot = () => this.snapshot;
  subscribe = (listener: () => void) => {
    this.listeners.add(listener);
    return () => {
      this.listeners.delete(listener);
    };
  };
  setMeasurement(enabled: boolean) {
    if (this.snapshot.testEnabled === enabled) return;
    this.snapshot = { ...this.snapshot, testEnabled: enabled };
    this.listeners.forEach((listener) => listener());
  }
}
