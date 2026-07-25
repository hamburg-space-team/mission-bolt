export interface Row {
  /**
   * Webview-local unique id, stamped on ingestion.
   *
   * The wire carries no globally unique packet id, so it cannot supply a React
   * key: `seq` is a PER-PAYLOAD-TYPE counter and only a uint8 (wraps every
   * 256), and BTC_TIMING / BTC_ENV / BTC_STATUS are all stamped with the same
   * tick_start_us. TIMING and BTC_ENV ship every tick, so their counters run in
   * lockstep - (timestamp_us, seq) collides on every single tick.
   */
  uid: number;
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

/**
 * Origin node of a row.
 *
 * `r.source` comes from the payload TYPE alone, which only names a node for the
 * per-node types (BTC_ENV, EXP1_ENV, ...). The generic types - TIMING, FAULT,
 * BOOT, GAP_MARKER - are annotated `.node = "SYSTEM"` because they can
 * originate on ANY node, so the type byte cannot say which one. Their real
 * origin travels in the payload's `source_node`, which the codec decodes to
 * `sample.node`. Prefer that; "system" is only a type-level placeholder and
 * must never be shown as if it were a node.
 */
export function nodeOf(r: Row): string {
  const node = r.sample?.node as string | undefined;
  if (node && node !== "unknown") return node;
  return r.source;
}

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
  // nodeOf, not r.source: filtering by BTC must also catch the BTC's own
  // timing/fault/boot/gap packets, whose type byte only says "system"
  if (filter.startsWith("__src:")) return nodeOf(r) === filter.slice(6);
  return r.name === filter;
}
