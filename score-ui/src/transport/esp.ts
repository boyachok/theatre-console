import type { Project, PublishedScore, Step, Workspace } from '../domain/schema.ts';
import { clone, fingerprint, uid } from '../domain/project.ts';
import { compileDevices } from '../domain/compile.ts';
import { parseBackup } from '../domain/validation.ts';
import type { ScoreTransport } from './preview.ts';
import type { NetworkSnapshot, NetworkTransport, NetworkDeviceStatus } from './network.ts';
import { INTERFACE_VERSION } from '../version.ts';
export class EspConsoleTransport implements ScoreTransport, NetworkTransport {
  readonly kind = 'esp' as const;
  private snapshot: NetworkSnapshot = { connected: false, testEnabled: false, devices: {} };
  private listeners = new Set<() => void>();
  private token = '';
  private manifest = { version: 0 };
  private clock: { server: number; at: number } | null = null;
  private timer: ReturnType<typeof setTimeout> | null = null;
  private generation = 0;
  private sequence = 0;
  private client = uid();
  getSnapshot = () => this.snapshot;
  subscribe = (listener: () => void) => {
    this.listeners.add(listener);
    return () => {
      this.listeners.delete(listener);
    };
  };
  private emit() {
    this.listeners.forEach((fn) => fn());
  }
  private async request(path: string, options: RequestInit = {}, timeout = 2000) {
    const controller = new AbortController(),
      timer = setTimeout(() => controller.abort(), timeout);
    try {
      const r = await fetch(path, { ...options, signal: controller.signal, cache: 'no-store' });
      if (!r.ok) throw new Error((await r.text()) || `HTTP ${r.status}`);
      return r;
    } finally {
      clearTimeout(timer);
    }
  }
  private async status() {
    const r = await this.request('/api/console/status'),
      data = await r.json();
    if (data.role !== 'master' || !data.console || data.version !== INTERFACE_VERSION)
      throw new Error(
        `Обновите страницу и мастер: нужна одинаковая версия Theatre console ${INTERFACE_VERSION}`,
      );
    this.token = data.token;
    this.manifest = data.console;
    this.clock = { server: data.serverMs, at: performance.now() };
    const devices: Record<string, NetworkDeviceStatus> = {};
    for (const n of [...data.nodes, ...(data.remotes ?? [])]) {
      const key = n.mac.toUpperCase(),
        prior = this.snapshot.devices[key];
      const rtt = n.p50 ?? null;
      const fresh = n.received > (prior?.received ?? 0),
        missed = n.timeouts > (prior?.lost ?? 0);
      const history = prior && n.sent >= prior.sent ? prior.history : [];
      devices[key] = {
        mac: key,
        role: n.role === 'remote' ? 'remote' : 'slave',
        name: n.name,
        ip: n.ip,
        online: n.ageMs !== null && n.ageMs < 3000,
        sent: n.sent,
        received: n.received,
        acknowledged: n.received,
        lost: n.timeouts,
        rttMs: rtt,
        p95Ms: n.p95,
        rssi: n.worstRssi ?? n.rssiSlave,
        confirmedStep: n.confirmedStep ?? null,
        scoreSupported: !!n.scoreSupported,
        scoreVersion: n.role === 'remote' ? undefined : (n.scoreVersion ?? 0),
        history:
          fresh || missed
            ? [
                ...history,
                { at: Date.now(), rttMs: fresh ? rtt : null, rssi: fresh ? n.rssiSlave : null },
              ].slice(-120)
            : history,
      };
    }
    this.snapshot = {
      connected: true,
      testEnabled: data.measuring,
      devices,
      masterMac: data.mac,
      activeStep: data.console.step,
      preview: data.console.preview,
    };
    this.emit();
    return data;
  }
  start() {
    const generation = ++this.generation;
    const poll = async () => {
      try {
        await this.status();
      } catch {
        this.snapshot = { ...this.snapshot, connected: false };
        this.emit();
      } finally {
        if (generation === this.generation) this.timer = setTimeout(poll, 500);
      }
    };
    void poll();
    return () => {
      this.generation++;
      if (this.timer) clearTimeout(this.timer);
    };
  }
  private async post(path: string, values: Record<string, string | number>) {
    if (!this.token) await this.status();
    return this.request(path, {
      method: 'POST',
      body: new URLSearchParams({
        ...Object.fromEntries(Object.entries(values).map(([k, v]) => [k, String(v)])),
        token: this.token,
      }),
    });
  }
  async setMeasurement(enabled: boolean) {
    await this.post('/api/console/measure', { enabled: Number(enabled) });
    await this.status();
  }
  async identify(mac: string) {
    await this.post('/api/console/identify', { mac });
  }
  async resetNetwork() {
    await this.post('/api/console/network/reset', {});
    this.snapshot = { ...this.snapshot, devices: {} };
    this.emit();
  }
  async loadWorkspace(): Promise<Workspace | null> {
    await this.status();
    const r = await this.request('/api/console/workspace', {}, 7000),
      text = await r.text();
    return text === 'null' ? null : parseBackup(text);
  }
  async saveWorkspace(workspace: Workspace) {
    if (!this.token) await this.status();
    await this.request(
      `/api/console/workspace?token=${this.token}`,
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ...workspace, interfaceVersion: INTERFACE_VERSION }),
      },
      10000,
    );
  }
  private checkDevices(project: Project, steps: Step[]) {
    const devices = compileDevices(project, steps);
    for (const device of devices) {
      const state = this.snapshot.devices[device.mac];
      if (state?.role === 'remote') throw new Error('remote — пульт: его нельзя назначить прибору');
      if (state?.online && !state.scoreSupported)
        throw new Error(`Обновите прошивку ${state.name ?? device.mac} до 0.7.1`);
    }
    return devices;
  }
  async publish(project: Project, _previous: PublishedScore | null): Promise<PublishedScore> {
    await this.status();
    if (project.steps.length > 512) throw new Error('Не более 512 шагов для этой прошивки');
    const devices = this.checkDevices(project, project.steps),
      version = this.manifest.version + 1;
    const published: PublishedScore = {
      version,
      fingerprint: fingerprint(project),
      createdAt: new Date().toISOString(),
      project: clone(project),
      mode: 'esp',
    };
    const workspace: Workspace = {
      format: 'theatre-score',
      schemaVersion: 1,
      interfaceVersion: INTERFACE_VERSION,
      draft: project,
      published,
    };
    await this.request(
      `/api/console/publish?token=${this.token}`,
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          version,
          fingerprint: published.fingerprint,
          steps: project.steps.map((s) => s.id),
          devices,
          workspaceJson: JSON.stringify(workspace),
        }),
      },
      20000,
    );
    await this.status();
    return published;
  }
  async preview(project: Project, step: Step, restart: boolean) {
    if (!this.clock || performance.now() - this.clock.at > 10000) await this.status();
    const devices = this.checkDevices(project, [step]),
      deadline = Math.floor(this.clock!.server + performance.now() - this.clock!.at + 700) >>> 0;
    const query = new URLSearchParams({
      token: this.token,
      deadline: String(deadline),
      client: this.client,
      seq: String(++this.sequence),
    });
    await this.request(
      `/api/console/preview?${query}`,
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          devices,
          step: project.steps.findIndex((s) => s.id === step.id),
          total: project.steps.length,
          reason: restart ? 1 : 0,
        }),
      },
      1200,
    );
  }
}
export const useEspBackend = import.meta.env?.VITE_CONSOLE_BACKEND === '1';
