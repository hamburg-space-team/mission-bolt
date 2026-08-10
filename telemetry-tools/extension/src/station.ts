import * as vscode from "vscode";

/**
 * The debug station's HTTP API (port 8080), as opposed to the raw telemetry
 * stream on 5000 the session reads.
 *
 * Ground equipment, deliberately independent of the session: the station knows
 * what the boards are doing whether or not this extension is connected to the
 * stream, and it is the only way to drive LO while the flight harness is not
 * attached.
 */

/** The REXUS LO line is faked on an ST-Link bridge GPIO the station drives. */
export type Level = "high" | "low";

export interface StationBoard {
  name: string;
  sending: boolean;
  mode?: string;
}

export interface StationState {
  url: string;
  reachable: boolean;
  /** why the last poll failed, for the tooltip */
  error?: string;
  /** level the station drives on the LO pin, undefined until read */
  loLevel?: Level;
  loGpio?: number;
  /** LO/SODS/SOE as the boards report them in their status packets */
  signals?: { lo: boolean; sods: boolean; soe: boolean };
  boards: StationBoard[];
  probePresent: boolean;
  probeBoard?: string;
}

const POLL_MS = 2000;
const TIMEOUT_MS = 3000;

/** The station answers errors as `{"error": …}` with a 500; surface that text. */
async function callJson(url: string, method: "GET" | "POST"): Promise<any> {
  const ctl = new AbortController();
  const timer = setTimeout(() => ctl.abort(), TIMEOUT_MS);
  try {
    const res = await fetch(url, { method, signal: ctl.signal });
    const body = (await res.json().catch(() => undefined)) as Record<string, any> | undefined;
    if (!res.ok || (body && typeof body.error === "string")) {
      throw new Error(body?.error ?? `HTTP ${res.status}`);
    }
    return body;
  } finally {
    clearTimeout(timer);
  }
}

export class StationClient {
  private readonly _onChange = new vscode.EventEmitter<void>();
  readonly onChange = this._onChange.event;
  readonly state: StationState = {
    url: "",
    reachable: false,
    boards: [],
    probePresent: false,
  };
  private timer?: NodeJS.Timeout;
  private inFlight = false;

  constructor(private readonly log: vscode.LogOutputChannel) {
    this.state.url = this.baseUrl();
  }

  private baseUrl(): string {
    const raw =
      vscode.workspace.getConfiguration("bolt").get<string>("stationUrl")?.trim() ||
      "http://bolt-station.local:8080";
    return raw.replace(/\/+$/, "");
  }

  /** Pin level that means "LO asserted". The GPIO drives a low-side switch,
   *  so high turns the transistor on and pulls the 28 V line to GND. */
  private assertedLevel(): Level {
    return vscode.workspace.getConfiguration("bolt").get<boolean>("loActiveLow", false) ? "low" : "high";
  }

  /** True when the driven level asserts LO, undefined while the level is unknown. */
  get loAsserted(): boolean | undefined {
    return this.state.loLevel === undefined ? undefined : this.state.loLevel === this.assertedLevel();
  }

  /** Poll only while someone is looking - this is a network round trip. */
  start(): void {
    if (this.timer) {
      return;
    }
    void this.refresh();
    this.timer = setInterval(() => void this.refresh(), POLL_MS);
  }

  stop(): void {
    clearInterval(this.timer);
    this.timer = undefined;
  }

  async refresh(): Promise<void> {
    if (this.inFlight) {
      return; // a slow station must not queue up polls
    }
    this.inFlight = true;
    const url = this.baseUrl();
    this.state.url = url;
    try {
      const [status, lo] = await Promise.all([
        callJson(`${url}/api/status`, "GET"),
        callJson(`${url}/api/lo`, "GET"),
      ]);
      this.state.reachable = true;
      this.state.error = undefined;
      this.state.loLevel = lo.level as Level;
      this.state.loGpio = lo.gpio as number;
      this.state.signals = status.signals;
      this.state.boards = Object.entries(status.boards ?? {}).map(([name, b]: [string, any]) => ({
        name,
        sending: Boolean(b.sending),
        mode: b.mode ?? undefined,
      }));
      this.state.probePresent = Boolean(status.probe?.stlink_on_usb);
      this.state.probeBoard = status.probe?.target?.board ?? undefined;
    } catch (e) {
      const msg = e instanceof Error ? e.message : String(e);
      // only log the transition; a station that is simply off would otherwise
      // fill the channel every two seconds
      if (this.state.reachable || this.state.error === undefined) {
        this.log.warn(`station: ${url} unreachable (${msg})`);
      }
      this.state.reachable = false;
      this.state.error = msg;
      this.state.loLevel = undefined;
      this.state.signals = undefined;
      this.state.boards = [];
      this.state.probePresent = false;
      this.state.probeBoard = undefined;
    } finally {
      this.inFlight = false;
      this._onChange.fire();
    }
  }

  /** Drive the LO line. `assert` is in REXUS terms; the level follows the polarity. */
  async setLo(assert: boolean): Promise<void> {
    const level: Level = assert === (this.assertedLevel() === "high") ? "high" : "low";
    const url = `${this.baseUrl()}/api/lo?level=${level}`;
    try {
      const body = await callJson(url, "POST");
      this.state.loLevel = body.level as Level;
      this.state.reachable = true;
      this.state.error = undefined;
      this.log.info(`station: LO ${assert ? "asserted" : "released"} (gpio ${body.gpio} = ${body.level})`);
      this._onChange.fire();
    } catch (e) {
      const msg = e instanceof Error ? e.message : String(e);
      void vscode.window.showErrorMessage(`Bolt: could not drive LO — ${msg}`);
      this.log.error(`station: LO drive failed (${msg})`);
      await this.refresh();
    }
  }

  async toggleLo(): Promise<void> {
    const now = this.loAsserted;
    if (now === undefined) {
      await this.refresh();
      if (this.loAsserted === undefined) {
        void vscode.window.showWarningMessage(
          `Bolt: the station at ${this.state.url} does not report a LO pin (${this.state.error ?? "no answer"}).`,
        );
        return;
      }
    }
    await this.setLo(!this.loAsserted);
  }

  dispose(): void {
    this.stop();
    this._onChange.dispose();
  }
}
