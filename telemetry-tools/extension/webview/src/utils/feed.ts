export interface Row {
  seq?: number;
  tick: number;
  timestamp_us: number;
  crc_ok: boolean;
  suspect: boolean;
  name: string;
  source: string;
  sample: Record<string, unknown>;
}

export interface Range {
  tMin: number | null;
  tMax: number | null;
  kMin: number | null;
  kMax: number | null;
}

export const emptyRange: Range = { tMin: null, tMax: null, kMin: null, kMax: null };

export function matches(r: Row, filter: string, range: Range): boolean {
  if (range.tMin != null && r.timestamp_us < range.tMin) return false;
  if (range.tMax != null && r.timestamp_us > range.tMax) return false;
  if (range.kMin != null && r.tick < range.kMin) return false;
  if (range.kMax != null && r.tick > range.kMax) return false;
  if (!filter) return true;
  if (filter === "__fault") return r.name === "fault";
  if (filter === "__crc") return !r.crc_ok;
  if (filter === "__gap") return r.name === "gap_marker";
  if (filter === "__suspect") return r.suspect;
  if (filter.startsWith("__src:")) return r.source === filter.slice(6);
  return r.name === filter;
}
