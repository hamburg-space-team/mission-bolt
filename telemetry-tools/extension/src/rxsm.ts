import * as vscode from "vscode";
import * as fs from "fs";
import { spawn, ChildProcessWithoutNullStreams } from "child_process";

import { SessionManager } from "./session";

export interface RxsmState {
  connected: boolean;
  port?: string;
  /** Whatever the simulator prints, label -> value. Deliberately open. */
  fields: Record<string, string>;
  /** True once the port has been open a while with nothing readable on it. */
  silent: boolean;
}

// The simulator prints "regularly"; well past that with nothing at all means
// the port is wrong, not that we are early.
const SILENT_AFTER_MS = 5000;

/**
 * Watches the RXSM simulator's ASCII debug port via
 * `bolt-serial-bridge --rxsm-debug`.
 *
 * This is the simulator's own D-SUB 9 at 115.2 kBd (user manual 3.5) - a
 * different cable and baud from the 38.4 kBd RS-422 flight link, so it gets its
 * own port and its own process. It is ground equipment, deliberately kept
 * independent of the telemetry session: the error generator's settings are
 * worth seeing even when no flight data is flowing.
 */
export class RxsmMonitor {
  private proc?: ChildProcessWithoutNullStreams;
  private buf = "";
  private readonly _onChange = new vscode.EventEmitter<void>();
  readonly onChange = this._onChange.event;
  readonly state: RxsmState = { connected: false, fields: {}, silent: false };
  private silentTimer?: NodeJS.Timeout;

  constructor(
    private readonly log: vscode.LogOutputChannel,
    private readonly session: SessionManager,
  ) {}

  private fire(): void {
    this._onChange.fire();
  }

  /** Remembered port, so reconnecting is one click. */
  private savedPort(): string | undefined {
    return vscode.workspace.getConfiguration("bolt").get<string>("rxsmDebugPort") || undefined;
  }

  private async pickPort(): Promise<string | undefined> {
    const ports = await this.session.listPorts();
    if (ports.length === 0) {
      void vscode.window.showWarningMessage("Bolt: no serial ports found for the RXSM debug interface.");
      return undefined;
    }
    const pick = await vscode.window.showQuickPick(
      ports.map((p) => ({
        label: p.port,
        description: [p.manufacturer, p.product].filter(Boolean).join(" "),
        detail: p.kind,
      })),
      { title: "RXSM simulator debug port (D-SUB 9, 115200 8N1)" },
    );
    if (!pick) {
      return undefined;
    }
    await vscode.workspace
      .getConfiguration("bolt")
      .update("rxsmDebugPort", pick.label, vscode.ConfigurationTarget.Workspace);
    return pick.label;
  }

  /**
   * `forcePick` skips the remembered port and always asks. Without it a wrong
   * saved port wins forever and the picker can never be reached again.
   */
  async connect(port?: string, forcePick = false): Promise<void> {
    const target = port ?? (forcePick ? await this.pickPort() : this.savedPort() ?? (await this.pickPort()));
    if (!target) {
      return;
    }

    // With a single RS-232 adapter the cable gets physically moved between the
    // RS-422 link and the simulator's debug D-SUB 9, so both cannot be live at
    // once. Taking the port means the telemetry session has to let go of it -
    // otherwise the two processes fight over the device and neither reads.
    const sess = this.session.state;
    if (sess.connected && sess.source === target) {
      this.log.info(`rxsm: taking ${target} - disconnecting the telemetry session`);
      this.session.disconnect();
      void vscode.window.showInformationMessage(
        `Bolt: telemetry session disconnected to free ${target} for the RXSM debug port.`,
      );
    }

    this.disconnect();

    const bin = this.session.bin("bolt-serial-bridge");
    if (!fs.existsSync(bin)) {
      void vscode.window.showErrorMessage(
        `Bolt: bridge binary not found at ${bin}. Run 'cargo build' in telemetry-tools/ or fix 'bolt.binDir'.`,
      );
      return;
    }

    const args = ["--rxsm-debug", target];
    this.log.info(`spawning rxsm debug: ${bin} ${args.join(" ")}`);
    try {
      this.proc = spawn(bin, args);
    } catch (e) {
      void vscode.window.showErrorMessage(`Bolt: could not start the RXSM debug reader: ${String(e)}`);
      return;
    }

    this.buf = "";
    this.state.connected = true;
    this.state.port = target;
    this.state.fields = {};
    this.state.silent = false;
    clearTimeout(this.silentTimer);
    this.silentTimer = setTimeout(() => {
      if (Object.keys(this.state.fields).length === 0) {
        this.state.silent = true;
        this.fire();
      }
    }, SILENT_AFTER_MS);
    this.wire();
    this.fire();
  }

  private wire(): void {
    const proc = this.proc;
    if (!proc) {
      return;
    }
    proc.stdout.on("data", (d: Buffer) => this.onStdout(d.toString()));
    proc.stderr.on("data", (d: Buffer) => this.log.warn(`[rxsm] ${d.toString().trim()}`));
    proc.on("error", (e) => {
      this.log.error(`[rxsm] ${e.message}`);
      this.markDown();
    });
    proc.on("exit", (code) => {
      this.log.info(`[rxsm] reader exited (${code})`);
      this.markDown();
    });
  }

  private markDown(): void {
    this.state.connected = false;
    this.proc = undefined;
    this.fire();
  }

  private onStdout(chunk: string): void {
    this.buf += chunk;
    const lines = this.buf.split("\n");
    this.buf = lines.pop() ?? ""; // keep the partial line
    for (const line of lines) {
      const text = line.trim();
      if (!text) {
        continue;
      }
      let msg: { t?: string; fields?: Record<string, string>; level?: string; msg?: string };
      try {
        msg = JSON.parse(text);
      } catch {
        continue; // not our JSON - ignore rather than kill the reader
      }
      if (msg.t === "rxsm" && msg.fields) {
        this.state.fields = msg.fields;
        this.state.silent = false;
        this.fire();
      } else if (msg.t === "log" && msg.msg) {
        this.log.info(`[rxsm] ${msg.msg}`);
      }
    }
  }

  disconnect(): void {
    clearTimeout(this.silentTimer);
    this.state.silent = false;
    if (this.proc) {
      this.proc.kill();
      this.proc = undefined;
    }
    if (this.state.connected) {
      this.state.connected = false;
      this.fire();
    }
  }

  dispose(): void {
    this.disconnect();
    this._onChange.dispose();
  }
}
