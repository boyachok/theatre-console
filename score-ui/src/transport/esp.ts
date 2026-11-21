import type { Project, PublishedScore, Step, Workspace } from '../domain/schema.ts';
import { clone, fingerprint, uid } from '../domain/project.ts';
import { compileDevices } from '../domain/compile.ts';
import { parseBackup } from '../domain/validation.ts';
import type { ScoreTransport } from './preview.ts';
import type { NetworkSnapshot, NetworkTransport, NetworkDeviceStatus } from './network.ts';
import { INTERFACE_VERSION } from '../version.ts';
import type { VoiceBank } from '../domain/voice.ts';
import { compileDraft, draftDelta, type DraftPlan } from '../domain/programming.ts';
// Retry only idempotent editor reconciliation; never automatically replay GO or I/O commands.
class TransientRequestError extends Error {}
export class EspConsoleTransport implements ScoreTransport, NetworkTransport {
  readonly kind = 'esp' as const;
  private snapshot: NetworkSnapshot = { connected: false, testEnabled: false, devices: {} };
  private listeners = new Set<() => void>();
  private token = '';
  private backupMeta: Workspace['backup'];
  rememberBackup(backup: Workspace['backup']) {
    this.backupMeta = backup;
  }
  private manifest: {
    version: number;
    fingerprint?: string;
    syncFingerprint?: string;
    draftFingerprint?: string;
    draftRevision?: number;
    livePreview?: boolean;
    editorLocked?: boolean;
    helmetVoiceRevision?: number;
  } = { version: 0 };
  private draftCache: DraftPlan | null = null;
  private voiceBanks: Record<string, VoiceBank> = {};
  private voiceRevision = 0;
  private ownVoiceRevision = 0;
  private voiceEditSerial: Record<string, number> = {};
  private editorQueue: Promise<unknown> = Promise.resolve();
  private editorRevision = 0;
  private statusRequest = 0;
  private acceptedStatus = 0;
  private enqueue<T>(job: () => Promise<T>): Promise<T> {
    const next = this.editorQueue.then(job, job);
    this.editorQueue = next.catch(() => {});
    return next;
  }
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
      if (!r.ok) {
        const message = (await r.text()) || `HTTP ${r.status}`;
        if (r.status === 408 || r.status === 429 || r.status >= 500)
          throw new TransientRequestError(message);
        throw new Error(message);
      }
      if (options.method === 'POST') this.acceptedStatus = ++this.statusRequest;
      return r;
    } catch (error) {
      if (controller.signal.aborted)
        throw new TransientRequestError(
          options.method === 'POST'
            ? 'HTTP: нет подтверждения, результат запроса неизвестен'
            : 'Истекло время ожидания ответа мастера',
        );
      if (error instanceof TypeError)
        throw new TransientRequestError(
          options.method === 'POST'
            ? 'Нет HTTP-связи с мастером; результат запроса неизвестен'
            : 'Нет HTTP-связи с мастером',
        );
      throw error;
    } finally {
      clearTimeout(timer);
    }
  }
  private async status() {
    const stamp = ++this.statusRequest;
    const r = await this.request('/api/console/status'),
      data = await r.json();
    if (data.role !== 'master' || !data.console || data.version !== INTERFACE_VERSION)
      throw new Error(
        `Обновите страницу и мастер: нужна одинаковая версия Theatre console ${INTERFACE_VERSION}`,
      );
    if (stamp < this.acceptedStatus) return data;
    this.acceptedStatus = stamp;
    if (this.token && this.token !== data.token) {
      this.draftCache = null;
      this.voiceRevision = this.ownVoiceRevision = 0;
    }
    this.token = data.token;
    this.manifest = data.console;
    this.clock = { server: data.serverMs, at: performance.now() };
    if (
      Number.isInteger(data.console.helmetVoiceRevision) &&
      data.console.helmetVoiceRevision !== this.voiceRevision
    ) {
      const voices = await (await this.request('/api/console/helmet-voices', {}, 7000)).json();
      if (voices.revision >= this.voiceRevision) {
        this.voiceRevision = voices.revision;
        this.voiceBanks = Object.fromEntries(
          Object.entries(voices.banks ?? {}).map(([mac, bank]) => [mac.toUpperCase(), bank]),
        ) as Record<string, VoiceBank>;
      }
      if (stamp < this.acceptedStatus) return data;
    }
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
        voice:
          n.role === 'helmet' && n.voice && Number.isFinite(n.voice.inputLevel) ? n.voice : null,
        acDimmer: n.acDimmer ?? null,
        pwmFrequency: n.pwmFrequency ?? null,
        firmwareVersion: typeof n.firmwareVersion === 'string' ? n.firmwareVersion : null,
        dimmerInverted: n.pwmConfig?.inverted12 ?? n.pwmConfig?.inverted,
        // Before first contact after a master restart, protocol/deviceBoot=0 and
        // the server's default "slave" is not an observed hardware role.
        role:
          n.role === 'remote'
            ? 'remote'
            : n.protocol === 0 && !n.deviceBoot
              ? undefined
              : n.role === 'helmet'
                ? 'helmet'
                : 'slave',
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
        localScoreSupported: !!n.localScoreSupported,
        sceneStatus: n.sceneStatus,
        sceneResponses: n.role === 'remote' ? undefined : n.scoreReceived,
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
      editorLocked: data.console.editorLocked,
      startupPending: data.console.startupPending,
      startupResetAllowed: data.console.startupResetAllowed,
      helmetVoiceRevision: this.voiceRevision,
      helmetVoices: Number.isInteger(data.console.helmetVoiceRevision)
        ? this.voiceBanks
        : undefined,
      helmetVoiceReady: data.console.helmetVoiceReady,
      helmetVoiceSaved: data.console.helmetVoiceSaved,
      testEnabled: data.measuring,
      devices,
      masterMac: data.mac,
      masterFirmwareVersion: data.version,
      activeStep: data.console.step,
      activeStepId: data.console.stepId,
      livePreview: data.console.livePreview,
      running: data.console.running,
      syncing: data.console.syncing,
      navigation: data.console.navigation,
      draftFingerprint: data.console.draftFingerprint,
      preview: data.console.preview,
      transition: data.console.transition,
      publicationReady: data.console.ready,
      storedDevices: data.console.storedDevices,
      plannedDevices: data.console.plannedDevices,
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
        this.snapshot = {
          ...this.snapshot,
          connected: false,
          devices: Object.fromEntries(
            Object.entries(this.snapshot.devices).map(([mac, info]) => [
              mac,
              { ...info, voice: null, online: null, confirmedStep: null, sceneStatus: undefined },
            ]),
          ),
        };
        this.emit();
      } finally {
        if (generation === this.generation) this.timer = setTimeout(poll, 200);
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
  setPwmFrequency(mac: string, hz: number) {
    return this.saveHardwareSetting(mac, hz, false);
  }
  setAcPulseUs(mac: string, us: number) {
    return this.saveHardwareSetting(mac, us, true);
  }
  private async saveHardwareSetting(mac: string, hz: number, ac: boolean) {
    const status = await this.status();
    const node = status.nodes.find(
      (n: { mac: string }) => n.mac.toUpperCase() === mac.toUpperCase(),
    );
    if (
      !node ||
      !(ac ? node.acDimmer : node.pwmFrequency) ||
      node.role !== 'slave' ||
      !Number.isFinite(node.ageMs) ||
      node.ageMs >= 3000
    )
      throw new Error('Нужен доступный slave с поддержкой этой настройки');
    if (
      ac &&
      (!Number.isFinite(node.acDimmer.ageMs) ||
        node.acDimmer.ageMs >= 3000 ||
        node.acDimmer.requested ||
        node.acDimmer.paired)
    )
      throw new Error('Нужны свежая диагностика220 и выключенная AC-нагрузка');
    if (
      status.console.editorLocked !== false ||
      status.console.running ||
      status.console.livePreview ||
      status.console.startupPending ||
      status.console.syncing
    )
      throw new Error('Сначала STOP и разблокировка редактора');
    const min = ac ? node.acDimmer.minUs : node.pwmFrequency.minHz,
      max = ac ? node.acDimmer.maxUs : node.pwmFrequency.maxHz;
    if (!Number.isInteger(hz) || hz < min || hz > max)
      throw new Error('Значение вне диапазона контроллера');
    const response = await this.post('/command', {
      id: node.id,
      mac: node.mac,
      op: ac ? 24 : 23,
      channel: 0,
      value: hz,
      client: this.client,
      seq: ++this.sequence,
      deadline: Math.floor(this.clock!.server + performance.now() - this.clock!.at + 950),
    });
    const { requestId } = await response.json();
    // Read-only ACK polling. Never resend an uncertain setting or reboot the device.
    for (let i = 0; i < 12; i++) {
      await new Promise((resolve) => setTimeout(resolve, 150));
      const data = await this.status();
      const current = data.nodes.find(
        (n: { mac: string }) => n.mac.toUpperCase() === mac.toUpperCase(),
      );
      if (!current || current.deviceBoot !== node.deviceBoot || current.requestId !== requestId)
        throw new Error('Сессия/команда изменилась; перечитайте настройки');
      if (current.commandPending) continue;
      if (!['accepted', 'applied'].includes(String(current.commandStatus).toLowerCase()))
        throw new Error(current.commandStatus || 'Нет подтверждения настройки');
      if (
        ac
          ? current.acDimmer?.pulseUs === hz && !current.acDimmer.configFault
          : current.pwmFrequency?.savedHz === hz
      )
        return;
    }
    throw new Error('Нет подтверждения настройки; перечитайте состояние, запрос не повторялся');
  }
  async setEditLock(locked: boolean) {
    ++this.editorRevision;
    await this.post('/api/console/edit-lock', { locked: Number(locked) });
    await this.status();
  }
  async saveHelmetBank(mac: string, bank: VoiceBank, baseRevision: number) {
    const key = mac.toUpperCase(),
      serial = (this.voiceEditSerial[key] ?? 0) + 1;
    this.voiceEditSerial[key] = serial;
    return this.enqueue(async () => {
      if (serial !== this.voiceEditSerial[key]) return null;
      await this.status();
      if (serial !== this.voiceEditSerial[key]) return null;
      const current = this.manifest.helmetVoiceRevision ?? 1;
      if (baseRevision !== current && this.ownVoiceRevision !== current)
        throw new Error('Звук изменён другим редактором: обновите страницу');
      const r = await this.request(
        `/api/console/helmet-voices?token=${this.token}`,
        {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ mac: mac.toUpperCase(), bank, revision: current }),
        },
        10000,
      );
      const result = await r.json();
      this.ownVoiceRevision = result.revision;
      this.manifest.helmetVoiceRevision = result.revision;
      await this.status();
      return result;
    });
  }
  async importHelmetBanks(workspace: Workspace) {
    ++this.editorRevision;
    for (const mac of Object.keys(this.voiceEditSerial)) ++this.voiceEditSerial[mac];
    return this.enqueue(async () => {
      await this.status();
      const banks =
        workspace.helmetVoices ??
        Object.fromEntries(
          workspace.draft.devices
            .filter((d) => d.profile === 'helmet' && d.mac && d.voice)
            .map((d) => [d.mac!.toUpperCase(), d.voice!]),
        );
      const r = await this.request(
        `/api/console/helmet-voices?token=${this.token}`,
        {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            import: true,
            banks,
            revision: this.manifest.helmetVoiceRevision ?? 1,
          }),
        },
        10000,
      );
      const result = await r.json();
      this.ownVoiceRevision = result.revision;
      await this.status();
      return result;
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
    const workspace = text === 'null' ? null : parseBackup(text);
    this.backupMeta = workspace?.backup;
    return workspace;
  }
  async reserveBackupSequence(workspace: Workspace, floor: number): Promise<number> {
    const major = workspace.backup?.major ?? 1,
      r = await this.post('/api/console/backup-sequence', {
        project: workspace.draft.id,
        major,
        floor,
      }),
      data = await r.json();
    if (
      data.major !== major ||
      !Number.isSafeInteger(data.minor) ||
      data.minor <= floor ||
      data.minor >= Number.MAX_SAFE_INTEGER
    )
      throw new Error('Некорректный номер JSON от мастера');
    return data.minor;
  }
  async saveWorkspace(workspace: Workspace) {
    this.backupMeta = workspace.backup;
    if (!this.token) await this.status();
    await this.request(
      `/api/console/${this.manifest.editorLocked ? 'workspace-metadata' : 'workspace'}?token=${this.token}`,
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(
          this.manifest.editorLocked
            ? { backup: workspace.backup, interfaceVersion: INTERFACE_VERSION }
            : { ...workspace, interfaceVersion: INTERFACE_VERSION },
        ),
      },
      10000,
    );
  }
  private checkDevices(project: Project, steps: Step[]) {
    const devices = compileDevices(project, steps);
    for (const device of devices) {
      const state = this.snapshot.devices[device.mac];
      if (state?.role === 'remote') throw new Error('remote — пульт: его нельзя назначить прибору');
      const authored = project.devices.find((d) => d.id === device.id)!;
      if (state?.role && (authored.profile === 'helmet') !== (state.role === 'helmet'))
        throw new Error(
          `Тип прибора «${authored.name}» (${device.mac}) и прошивки контроллера не совпадают: Helmet / стандартный`,
        );
      if (state?.online && !state.localScoreSupported)
        throw new Error(
          `Обновите прошивку ${state.name ?? device.mac}: нужен протокол сцен, совместимый с Theatre console ${INTERFACE_VERSION}`,
        );
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
      ...(this.backupMeta ? { backup: this.backupMeta } : {}),
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
  private async uploadDraft(project: Project) {
    const next = compileDraft(project),
      base = this.manifest.draftFingerprint ?? '';
    this.checkDevices(project, project.steps);
    if (
      next.fingerprint === base &&
      (!this.manifest.syncFingerprint ||
        this.manifest.syncFingerprint === next.fingerprint ||
        this.manifest.livePreview)
    ) {
      this.draftCache = next;
      return;
    }
    if (this.draftCache && this.draftCache.fingerprint !== base)
      throw new Error('Черновик в мастере уже изменён: обновите страницу');
    await this.request(
      `/api/console/draft?token=${this.token}`,
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          ...draftDelta(next, this.draftCache, base),
          revision: this.manifest.draftRevision ?? 0,
        }),
      },
      20000,
    );
    this.draftCache = next;
    this.manifest.draftFingerprint = next.fingerprint;
    this.manifest.draftRevision = (this.manifest.draftRevision ?? 0) + 1;
    if (!this.manifest.livePreview) this.manifest.syncFingerprint = next.fingerprint;
  }
  private navigationFields(navigation = this.snapshot.navigation ?? 0) {
    if (!this.clock) throw new Error('Нет свежего времени мастера');
    return {
      navigation,
      deadline: Math.floor(this.clock.server + performance.now() - this.clock.at + 700) >>> 0,
      client: this.client,
      seq: ++this.sequence,
    };
  }
  reconcile(project: Project): Promise<PublishedScore | null> {
    const revision = ++this.editorRevision;
    return this.enqueue(async () => {
      for (let attempt = 0; ; attempt++) {
        if (revision !== this.editorRevision) return null;
        try {
          await this.status();
          if (revision !== this.editorRevision) return null;
          if (this.manifest.editorLocked) return null;
          await this.uploadDraft(project);
          break;
        } catch (error) {
          if (!(error instanceof TransientRequestError) || attempt >= 3) throw error;
          // Fresh status resolves an uncertain committed POST before any retry.
          // A newer edit/STOP invalidates this revision while waiting.
          await new Promise((resolve) => setTimeout(resolve, 250 * 2 ** attempt));
        }
      }
      if (revision !== this.editorRevision) return null;
      // Do not automatically repeat persistent publication, GO or I/O commands.
      if (!this.manifest.livePreview && this.manifest.fingerprint !== fingerprint(project))
        return this.publish(project, null);
      await this.status();
      return null;
    });
  }
  changePreview(
    enabled: boolean,
    project: Project,
    stepId: string,
  ): Promise<PublishedScore | null> {
    ++this.editorRevision;
    const navigation = this.snapshot.navigation ?? 0;
    return this.enqueue(async () => {
      await this.status();
      if (!enabled) {
        // Suspend live execution BEFORE compilation: OFF must work even with an invalid draft.
        await this.post('/api/console/preview-mode', {
          enabled: 0,
          fingerprint: fingerprint(project),
          ...this.navigationFields(navigation),
        });
        await this.status();
        await this.uploadDraft(project);
        if (this.manifest.fingerprint !== fingerprint(project)) return this.publish(project, null);
        return null; // Flash sync never queues a delayed GO.
      }
      await this.uploadDraft(project);
      await this.post('/api/console/preview-mode', {
        enabled: 1,
        stepId,
        ...this.navigationFields(navigation),
      });
      await this.status();
      return null;
    });
  }
  selectDraft(project: Project, stepId: string, restart: boolean) {
    const navigation = this.snapshot.navigation ?? 0;
    return this.enqueue(async () => {
      await this.status();
      await this.uploadDraft(project);
      await this.post('/api/console/draft-go', {
        stepId,
        reason: Number(restart),
        ...this.navigationFields(navigation),
      });
      await this.status();
    });
  }
  async navigate(action: 'back' | 'forward' | 'first' | 'last') {
    const navigation = this.snapshot.navigation ?? 0;
    if (!this.clock || performance.now() - this.clock.at > 10000) await this.status();
    await this.post('/api/console/navigate', {
      action: { forward: 1, back: 2, first: 3, last: 4 }[action],
      ...this.navigationFields(navigation),
    });
    await this.status();
  }
  async go(version: number, step: number, restart: boolean) {
    const navigation = this.snapshot.navigation ?? 0;
    if (!this.clock || performance.now() - this.clock.at > 10000) await this.status();
    const deadline =
      Math.floor(this.clock!.server + performance.now() - this.clock!.at + 700) >>> 0;
    await this.post('/api/console/go', {
      navigation,
      version,
      step,
      reason: Number(restart),
      deadline,
      client: this.client,
      seq: ++this.sequence,
    });
    await this.status();
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
