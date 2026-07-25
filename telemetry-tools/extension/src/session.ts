import * as vscode from "vscode";
import { ChildProcessWithoutNullStreams, spawn } from "child_process";
import * as readline from "readline";
import * as path from "path";
import * as fs from "fs";

import { BridgeMsg, FrameMsg, Manifest, PortInfo, StatsMsg } from "./protocol";
import { CaptureCache, CacheEntry } from "./cache";

// Filters/pagination for a cache DB query
export interface PacketQuery {
  type?: string;
  source?: string;
  sinceUs?: number;
  untilUs?: number;
  limit?: number;
  offset?: number;
}

export type Mode = "idle" | "live" | "postflight";

// Keep only the most recent `max` entries of an array in place
function cap<T>(arr: T[], max: number): void {
  if (arr.length > max) arr.splice(0, arr.length - max);
}

interface State {
  mode: Mode;
  source?: string; // port or file path
  connected: boolean;
  armed: boolean;
}

// Owns the bridge/postflight child processes + decoded state; live and
// post-flight flow through here so they share one model
export class SessionManager implements vscode.Disposable {
  readonly state: State = { mode: "idle", connected: false, armed: false };
  stats?: StatsMsg;
  manifest?: Manifest;
  // Rolling buffer of the most recent frames for the live overview
  readonly recentFrames: FrameMsg[] = [];
  private static RECENT_CAP = 5000;
  // Latest per-source failure counters from status frames
  readonly counters: Record<string, { led: number; spec: number; ready: number }> = {};
  // Faults/gaps seen live (post-flight reads them from the manifest)
  readonly liveFaults: Array<{
    fault_code: number; error_code: number; line: number; depth: number;
    truncated: boolean; steps: number[]; timestamp_us: number; tick: number;
  }> = [];
  readonly liveGaps: Array<{ first_missing_tick: number; count: number; reason: string; tick: number }> = [];
  private static EVENT_CAP = 500;

  private bridge?: ChildProcessWithoutNullStreams;
  private sessionRaw?: string;

  // Decoded-capture cache (raw -> indexed SQLite, built once)
  private readonly cache = new CaptureCache(() => this.capturesDir);
  // Cache slot for the currently loaded post-flight capture
  private currentEntry?: CacheEntry;

  private readonly _onChange = new vscode.EventEmitter<void>();
  readonly onChange = this._onChange.event;
  private readonly _onFrame = new vscode.EventEmitter<FrameMsg>();
  readonly onFrame = this._onFrame.event;
  private readonly _onStats = new vscode.EventEmitter<StatsMsg>();
  readonly onStats = this._onStats.event;
  private readonly _onFilter = new vscode.EventEmitter<string | undefined>();
  readonly onFilter = this._onFilter.event;
  // New data source (live connect / capture open): views drop stale data
  private readonly _onReset = new vscode.EventEmitter<void>();
  readonly onReset = this._onReset.event;
  // Cache DB built (e.g. after Live->Idle); carries the raw so the UI reloads from it
  private readonly _onCaptureReady = new vscode.EventEmitter<string>();
  readonly onCaptureReady = this._onCaptureReady.event;

  // Active live-feed packet-type filter (undefined = all)
  feedFilter: string | undefined;

  setFeedFilter(name: string | undefined): void {
    this.feedFilter = name;
    this._onFilter.fire(name);
  }

  constructor(private readonly log: vscode.LogOutputChannel) {}

  // Public: the RXSM debug monitor spawns the same bridge binary and must not
  // duplicate this search (workspace subfolders + debug/release fallback)
  bin(name: string): string {
    const cfg = vscode.workspace.getConfiguration("bolt");
    const dir = cfg.get<string>("binDir", "telemetry-tools/target/debug");
    if (path.isAbsolute(dir)) {
      return path.join(dir, name);
    }
    // The workspace may be opened at any subfolder (e.g. flight-software/),
    // so search each folder AND its parent, debug + release, for the binary
    const folders = vscode.workspace.workspaceFolders ?? [];
    const roots = folders.flatMap((f) => [f.uri.fsPath, path.dirname(f.uri.fsPath)]);
    const rels = [dir, "telemetry-tools/target/release", "telemetry-tools/target/debug"];
    for (const root of roots) {
      for (const rel of rels) {
        const candidate = path.join(root, rel, name);
        if (fs.existsSync(candidate)) {
          return candidate;
        }
      }
    }
    // Not found: return the primary guess so the caller's existsSync check
    // surfaces a clear, actionable error
    return path.join(folders[0]?.uri.fsPath ?? ".", dir, name);
  }

  private mission(): string {
    return vscode.workspace.getConfiguration("bolt").get<string>("mission", "bolt");
  }

  // Repo root (walk up for .git) so captures land in mission-bolt/
  private repoRoot(): string {
    const start = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath ?? ".";
    let dir = start;
    for (let i = 0; i < 8; i++) {
      if (fs.existsSync(path.join(dir, ".git"))) return dir;
      const up = path.dirname(dir);
      if (up === dir) break;
      dir = up;
    }
    return start;
  }

  // Captures + Bolt scratch live in mission-bolt/captures (gitignored)
  get capturesDir(): string {
    return path.join(this.repoRoot(), "captures");
  }
  ensureTmp(): void {
    fs.mkdirSync(this.capturesDir, { recursive: true });
  }

  // List serial ports via the bridge --list-ports (USB first); [] on failure
  async listPorts(): Promise<PortInfo[]> {
    return new Promise((resolve) => {
      let out = "";
      let proc: ChildProcessWithoutNullStreams;
      try {
        proc = spawn(this.bin("bolt-serial-bridge"), ["--list-ports"]);
      } catch {
        return resolve([]);
      }
      proc.stdout.on("data", (d) => (out += d.toString()));
      proc.on("error", () => resolve([]));
      proc.on("exit", () => {
        for (const line of out.split("\n")) {
          try {
            const m = JSON.parse(line);
            if (m?.t === "ports") return resolve(m.ports as PortInfo[]);
          } catch {
            /* not our line */
          }
        }
        resolve([]);
      });
    });
  }

  // Start a live session on `port`, recording to a timestamped .raw
  async connect(port: string): Promise<void> {
    if (this.bridge) {
      await this.disconnect();
    }
    this._onReset.fire(); // new live source -> views drop any loaded capture
    const cfg = vscode.workspace.getConfiguration("bolt");
    const baud = cfg.get<number>("baud", 38400);
    const statsMs = cfg.get<number>("statsIntervalMs", 500);
    // Captures live in mission-bolt/tmp/captures (repo root, gitignored)
    const stamp = new Date().toISOString().replace(/[:.]/g, "-");
    this.ensureTmp();
    this.sessionRaw = path.join(this.capturesDir, `session-${stamp}.raw`);

    const binPath = this.bin("bolt-serial-bridge");
    if (!fs.existsSync(binPath)) {
      void vscode.window.showErrorMessage(
        `Bolt: bridge binary not found at ${binPath}. Run 'cargo build' in telemetry-tools/ or fix 'bolt.binDir'.`,
      );
      return;
    }
    const args = [
      "--port", port,
      "--baud", String(baud),
      "--raw", this.sessionRaw,
      "--mission", this.mission(),
      "--stats-ms", String(statsMs),
    ];
    this.log.info(`spawning bridge: ${binPath} ${args.join(" ")}`);
    this.bridge = spawn(binPath, args);
    this.wireBridge();

    this.state.mode = "live";
    this.state.source = port;
    this.state.connected = true;
    this.recentFrames.length = 0;
    this.fire();
  }

  private wireBridge(): void {
    if (!this.bridge) return;
    const rl = readline.createInterface({ input: this.bridge.stdout });
    rl.on("line", (line) => this.onLine(line));
    this.bridge.stderr.on("data", (d) => this.log.warn(`[bridge] ${d}`));
    this.bridge.on("error", (e: Error) => {
      this.log.error(`bridge failed to start: ${e.message}`);
      void vscode.window.showErrorMessage(`Bolt: bridge could not start - ${e.message}`);
      this.state.connected = false;
      this.state.mode = "idle";
      this.bridge = undefined;
      this.fire();
    });
    this.bridge.on("exit", (code) => {
      this.log.info(`bridge exited (${code})`);
      const wasRaw = this.sessionRaw;
      this.state.connected = false;
      this.state.mode = "idle";
      this.bridge = undefined;
      this.fire();
      if (wasRaw && vscode.workspace.getConfiguration("bolt").get<boolean>("autoPostflight", true)) {
        // Build the cache DB for the just-recorded session, then reload the UI
        // from that persisted DB (Live -> Idle now shows the DB-backed capture,
        // not the volatile in-memory buffer). Skip empty recordings
        void this.ensureCache(wasRaw)
          .then((entry) => {
            const m: Manifest = JSON.parse(fs.readFileSync(entry.manifestPath, "utf8"));
            this.manifest = m;
            this.currentEntry = entry;
            // Only reload into post-flight if still idle - if the user already
            // reconnected (mode === "live"), don't clobber the new session
            if ((m.total_frames ?? 0) > 0 && this.state.mode === "idle") {
              this.state.mode = "postflight";
              this.state.source = wasRaw;
              this._onCaptureReady.fire(wasRaw);
            }
            this.fire();
          })
          .catch((e: Error) => this.log.info(`[postflight] auto-decode skipped: ${e.message}`));
      }
    });
  }

  private onLine(line: string): void {
    let msg: BridgeMsg;
    try {
      msg = JSON.parse(line);
    } catch {
      return;
    }
    switch (msg.t) {
      case "frame":
        this.recentFrames.push(msg);
        if (this.recentFrames.length > SessionManager.RECENT_CAP) {
          this.recentFrames.splice(0, this.recentFrames.length - SessionManager.RECENT_CAP);
        }
        this.captureFrame(msg);
        this._onFrame.fire(msg);
        break;
      case "stats":
        this.stats = msg;
        this._onStats.fire(msg);
        this.fire();
        break;
      case "log":
        this.log[msg.level === "error" ? "error" : msg.level === "warn" ? "warn" : "info"](
          `[bridge] ${msg.msg}`,
        );
        break;
      case "resync":
        break;
    }
  }

  // Capture per-frame side channels: status counters, faults, gaps
  private captureFrame(msg: FrameMsg): void {
    const s = msg.sample as Record<string, unknown>;
    if (s.kind === "status") {
      if (s.led_write_fails != null || s.spec_start_fails != null || s.data_ready_fails != null) {
        this.counters[msg.source] = {
          led: Number(s.led_write_fails ?? 0),
          spec: Number(s.spec_start_fails ?? 0),
          ready: Number(s.data_ready_fails ?? 0),
        };
      }
    } else if (s.kind === "fault") {
      this.liveFaults.push({
        fault_code: Number(s.fault_code ?? 0),
        error_code: Number(s.error_code ?? 0),
        line: Number(s.line ?? 0),
        depth: Number(s.depth ?? 0),
        truncated: Boolean(s.truncated),
        steps: Array.isArray(s.steps) ? (s.steps as number[]) : [],
        timestamp_us: msg.timestamp_us,
        tick: msg.tick,
      });
      cap(this.liveFaults, SessionManager.EVENT_CAP);
    } else if (s.kind === "gap") {
      this.liveGaps.push({
        first_missing_tick: Number(s.first_missing_tick ?? 0),
        count: Number(s.count ?? 0),
        reason: String(s.reason ?? "unknown"),
        tick: msg.tick,
      });
      cap(this.liveGaps, SessionManager.EVENT_CAP);
    } else if (s.kind === "cmd_ack") {
      // The vehicle acknowledged an uplink command - surface it so the
      // operator sees the round trip. Only fires live (captureFrame is only
      // called on the live bridge stream)
      const cmd = String(s.command ?? `opcode 0x${Number(s.opcode ?? 0).toString(16)}`);
      const result = String(s.result ?? `status ${Number(s.status ?? 0)}`);
      const ok = Number(s.status ?? 0) === 0;
      const line = `Bolt: uplink "${cmd}" acknowledged (seq ${Number(s.seq ?? 0)}) — ${result}`;
      if (ok) void vscode.window.showInformationMessage(line);
      else void vscode.window.showWarningMessage(line);
    }
  }

  async disconnect(): Promise<void> {
    const b = this.bridge;
    this.bridge = undefined;
    if (b) {
      // Ask the bridge to stop so it flushes the raw tee cleanly (no lost
      // tail), then force it if it doesn't exit within the grace period
      try {
        b.stdin.write(JSON.stringify({ cmd: "stop" }) + "\n");
      } catch {
        /* stdin may already be closed */
      }
      await new Promise<void>((resolve) => {
        const t = setTimeout(() => {
          b.kill("SIGTERM");
          resolve();
        }, 1500);
        b.once("exit", () => {
          clearTimeout(t);
          resolve();
        });
      });
    }
    this.state.connected = false;
    this.state.mode = "idle";
    this.fire();
  }

  // Decode a recorded .raw (once, cached) and load it for post-flight review
  async openCapture(rawPath: string): Promise<Manifest> {
    const entry = await this.ensureCache(rawPath);
    const manifest: Manifest = JSON.parse(fs.readFileSync(entry.manifestPath, "utf8"));
    this.currentEntry = entry;
    this.state.mode = "postflight";
    this.state.source = rawPath;
    this.manifest = manifest;
    this.fire();
    return manifest;
  }

  // Ensure the cache DB exists (decode once); re-opens reuse the size+mtime slot
  async ensureCache(rawPath: string): Promise<CacheEntry> {
    const entry = this.cache.entryFor(rawPath);
    if (this.cache.isReady(entry)) {
      this.cache.touch(entry);
      return entry;
    }
    const args = [
      rawPath,
      "--manifest", entry.manifestPath,
      "--db", entry.dbPath,
      "--mission", this.mission(),
    ];
    await vscode.window.withProgress(
      { location: vscode.ProgressLocation.Notification, title: "Bolt: decoding capture (building cache)", cancellable: true },
      (_progress, token) =>
        new Promise<void>((resolve, reject) => {
          this.cache.prepareDir(entry);
          const proc = spawn(this.bin("bolt-postflight"), args);
          token.onCancellationRequested(() => proc.kill("SIGTERM"));
          proc.stderr.on("data", (d) => this.log.info(`[postflight] ${d}`));
          proc.on("error", reject);
          proc.on("exit", (code) => {
            if (code !== 0) return reject(new Error(`postflight exited ${code}`));
            this.cache.markReady(entry);
            resolve();
          });
        }),
    );
    return entry;
  }

  // Query the cache DB (newest first, indexed) - ms regardless of size
  async queryPackets(entry: CacheEntry, q: PacketQuery = {}): Promise<Record<string, unknown>[]> {
    const args = ["--query", "--db", entry.dbPath];
    if (q.type) args.push("--type", q.type);
    if (q.source) args.push("--source", q.source);
    if (q.sinceUs != null) args.push("--since-us", String(q.sinceUs));
    if (q.untilUs != null) args.push("--until-us", String(q.untilUs));
    if (q.limit != null) args.push("--limit", String(q.limit));
    if (q.offset != null) args.push("--offset", String(q.offset));
    return new Promise((resolve, reject) => {
      const proc = spawn(this.bin("bolt-postflight"), args);
      let out = "";
      let err = "";
      proc.stdout.on("data", (d: Buffer) => (out += d.toString()));
      proc.stderr.on("data", (d: Buffer) => (err += d.toString()));
      proc.on("error", reject);
      proc.on("exit", (code) => {
        if (code !== 0) return reject(new Error(err.trim() || `query exited ${code}`));
        const recs: Record<string, unknown>[] = [];
        for (const line of out.split("\n")) {
          if (!line) continue;
          try {
            recs.push(JSON.parse(line));
          } catch {
            /* skip malformed line */
          }
        }
        resolve(recs);
      });
    });
  }

  // Ensure the raw's cache and query it in one call (used by the UI reads)
  async queryCapture(rawPath: string, q: PacketQuery = {}): Promise<Record<string, unknown>[]> {
    const entry = await this.ensureCache(rawPath);
    return this.queryPackets(entry, q);
  }

  // Send an uplink command over the live link
  sendCommand(cmd: string): void {
    if (!this.bridge || !this.state.connected) {
      void vscode.window.showWarningMessage("Bolt: not connected - cannot send uplink.");
      return;
    }
    this.bridge.stdin.write(JSON.stringify({ cmd }) + "\n");
    this.log.info(`uplink queued: ${cmd}`);
  }

  setArmed(armed: boolean): void {
    this.state.armed = armed;
    this.fire();
  }

  // Force views/context to re-evaluate
  refresh(): void {
    this.fire();
  }

  // Most recent raw: live recording or loaded capture; undefined if neither
  get lastRaw(): string | undefined {
    if (this.sessionRaw) return this.sessionRaw;
    if (this.state.source && this.state.source.endsWith(".raw")) return this.state.source;
    return undefined;
  }

  // Run bolt-postflight on a raw with arbitrary args (export paths etc.)
  runPostflight(rawPath: string, extraArgs: string[]): Promise<void> {
    return new Promise((resolve, reject) => {
      const proc = spawn(this.bin("bolt-postflight"), [rawPath, "--mission", this.mission(), ...extraArgs]);
      proc.stderr.on("data", (d: Buffer) => this.log.info(`[postflight] ${d}`));
      proc.on("error", reject);
      proc.on("exit", (code: number | null) =>
        code === 0 ? resolve() : reject(new Error(`postflight exited ${code}`)),
      );
    });
  }

  private fire(): void {
    this._onChange.fire();
  }

  dispose(): void {
    this.bridge?.kill("SIGTERM");
    this._onChange.dispose();
    this._onFrame.dispose();
    this._onStats.dispose();
    this._onFilter.dispose();
    this._onReset.dispose();
    this._onCaptureReady.dispose();
  }
}
