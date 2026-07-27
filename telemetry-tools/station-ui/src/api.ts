// Shapes the station serves, deliberately partial: an API field the
// display does not render cannot break the build.

export interface Interface {
  interface: string;
  cidr: string;
  source: string;
}

export interface Boot {
  reason: string;
  reboot_count: number;
  node: string;
  seen_utc: string;
}

export interface Board {
  sending: boolean;
  last_seen_ms_ago: number | null;
  mode: string | null;
  env: Record<string, number> | null;
  status: Record<string, unknown> | null;
  boot: Boot | null;
}

export interface Status {
  station: { ip: string | null; hostname: string | null; interfaces: Interface[] };
  signals: { lo: boolean; sods: boolean; soe: boolean };
  link: {
    total_frames: number;
    total_crc_fails: number;
    total_bytes: number;
    baud: number;
    budget_bps: number;
    window: {
      seconds: number;
      frames: number;
      crc_fails: number;
      bytes: number;
      frames_per_s: number;
      bytes_per_s: number;
    };
  };
  boards: Record<string, Board>;
  probe: { stlink_on_usb: boolean; target: { uid: string; board: string | null } | null };
  active_run: number | null;
}

export interface RunSummary {
  id: number;
  started_utc: string;
  finished_utc: string | null;
  pass: number;
  fail: number;
}

export interface Step {
  test_id: number;
  name: string;
  result: string;
  data: number | null;
  timestamp_us: number | null;
  recv_utc: string;
  duration_ms: number | null;
}

export interface RunDetail {
  run: number;
  pass: number;
  fail: number;
  nodes: Record<string, Step[]>;
}

export interface NoInit {
  address?: string;
  magic?: string;
  valid: boolean;
  note?: string;
  /** only present when no marker was found: could the probe read at all */
  probe_reachable?: boolean;
  decoded?: {
    tick: number;
    reboot_count: number;
    mode: string;
    lo_latched: boolean;
    reason: number;
    reason_name: string;
    lo_rtc_s: number;
  };
}

export const NODES = ["btc", "exp1", "exp2", "exp3"] as const;

async function get<T>(path: string): Promise<T | null> {
  try {
    const r = await fetch(path, { cache: "no-store" });
    if (!r.ok) return null;
    return (await r.json()) as T;
  } catch {
    return null; // the kiosk keeps its last frame rather than blanking
  }
}

export const getStatus = () => get<Status>("/api/status");
export const getRuns = () => get<{ active_run: number | null; runs: RunSummary[] }>("/api/tests");
export const getRun = (id: number) => get<RunDetail>(`/api/tests/${id}`);
export const getNoInit = () => get<NoInit>("/api/debugger/noinit");
export const getSteps = () => get<Record<string, string[]>>("/api/selftest/steps");

/** Clears packets, CRC fails and throughput together - one window. */
export async function resetWindow(): Promise<void> {
  try {
    await fetch("/api/window/reset", { method: "POST" });
  } catch {
    /* the next poll shows whether it took */
  }
}

export async function runSelfTest(): Promise<boolean> {
  try {
    const r = await fetch("/api/command/full_system_test", { method: "POST" });
    const body = (await r.json()) as { sent?: string; error?: string };
    return !!body.sent;
  } catch {
    return false;
  }
}
