import { FrameMsg, StatsMsg, Manifest } from "./protocol";

export type { FrameMsg, StatsMsg, Manifest };

export interface PacketRecord {
  seq: number;
  tick: number;
  timestamp_us: number;
  crc_ok: boolean;
  suspect: boolean;
  name?: string;
  source?: string;
  sample: Record<string, unknown>;
}

// Host -> webview
export type ToWebview =
  | { type: "init"; live: boolean; manifest: Manifest }
  | { type: "frame"; frame: FrameMsg }
  | { type: "stats"; stats: StatsMsg }
  | { type: "counts"; counts: Record<string, number> }
  | { type: "load"; records: PacketRecord[]; name?: string }
  | { type: "packets"; name: string; records: PacketRecord[] }
  | { type: "experiment"; source: string }
  | { type: "reset" };

// Webview -> host
export type FromWebview =
  | { type: "ready" }
  | { type: "reload" }
  | { type: "showType"; name: string };
