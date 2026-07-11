export interface FrameMsg {
  t: "frame";
  ty: number;
  name: string;
  source: string;
  seq: number;
  tick: number;
  timestamp_us: number;
  crc_ok: boolean;
  // A validity mask says this frame's data is not real
  suspect: boolean;
  sample: Sample;
}

export interface ResyncMsg {
  t: "resync";
  skipped: number;
}

export interface StatsMsg {
  t: "stats";
  counts: Record<string, number>;
  total: number;
  crc_fail: number;
  resync_bytes: number;
  lo: boolean;
  soe: boolean;
  sods: boolean;
  elapsed_ms: number;
}

export interface LogMsg {
  t: "log";
  level: "info" | "warn" | "error";
  msg: string;
}

export type BridgeMsg = FrameMsg | ResyncMsg | StatsMsg | LogMsg;

// A normalized sample is a tagged union on `kind`; the fields present
// depend on the kind. The overview only needs `columns`-style scalars,
// which it derives generically, so we keep this loose
export type Sample = { kind: string } & Record<string, unknown>;

export interface Manifest {
  mission: string;
  protocol_version: number;
  codec_version: string;
  packet_counts: Record<string, number>;
  experiments: string[];
  total_frames: number;
  crc_fail_total: number;
  crc_fails: { index: number; ty: number; tick: number }[];
  resync_bytes: number;
  gaps: { first_missing_tick: number; count: number; reason: string }[];
  boots: { reason: string; reboot_count: number; timestamp_us: number }[];
  faults: {
    fault_code: number;
    error_code: number;
    line: number;
    depth: number;
    truncated: boolean;
    steps: number[];
    timestamp_us: number;
  }[];
  lo_rtc_s: number;
  t_start_us: number;
  t_end_us: number;
  timeline: { t_us: number; rows: Record<string, number> }[];
}

export interface PortInfo {
  port: string;
  kind: string;
  manufacturer: string | null;
  product: string | null;
  vid: number | null;
  pid: number | null;
  serial_number: string | null;
}

export const UPLINK_COMMANDS = [
  { id: "reset_tick", label: "Reset Tick", dangerous: true },
  { id: "start_experiment", label: "Start Experiment", dangerous: false },
  { id: "activate_camera", label: "Activate Camera", dangerous: false },
  { id: "full_system_test", label: "Full System Test", dangerous: true },
] as const;
