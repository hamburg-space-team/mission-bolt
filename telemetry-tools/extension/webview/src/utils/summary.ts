import { faultName, errorName, faultNode, stepName, gapNode } from "../../../src/codes";
import { fmtValue } from "./format";
import type { Row } from "./feed";

/** Origin node: firmware-stamped if present, else inferred from the code. */
export function rowNode(r: Row): string {
  const s = r.sample;
  const node = s.node as string | undefined;
  if (r.name === "fault") {
    if (node && node !== "unknown") return node;
    return `${faultNode(Number(s.fault_code))} (inferred)`;
  }
  if (r.name === "gap_marker") {
    if (node && node !== "unknown") return node;
    return gapNode(String(s.reason ?? ""));
  }
  return r.source;
}

/** One-line summary of a row's payload. */
export function summary(r: Row): string {
  const s = r.sample;
  switch (r.name) {
    case "fault":
      return `${faultName(Number(s.fault_code))} · ${errorName(Number(s.error_code))} · line ${s.line}`;
    case "gap_marker":
      return `gap tick ${s.first_missing_tick} ×${s.count} · ${s.reason}`;
    case "cmd_ack":
      return `${s.command} (seq ${s.seq}) · ${s.result}`;
    case "timing": {
      const tick = Number(s.tick_us);
      const over = tick > 40000 ? " ⚠ OVER 40 ms" : "";
      return `tick ${(tick / 1000).toFixed(1)} ms${over} · read ${s.read_us} · cfg ${s.cfg_us} · drive ${s.drive_us} · send ${s.send_us} · store ${s.store_us} µs`;
    }
    default: {
      const keys = Object.keys(s).filter((k) => k !== "kind");
      return keys.slice(0, 4).map((k) => `${k}=${fmtValue(s[k])}`).join("  ");
    }
  }
}

/** Full fault trace for the detail row. */
export function faultTrace(r: Row): string {
  const s = r.sample;
  const steps = (s.steps as number[]) ?? [];
  const depth = Number(s.depth ?? steps.length);
  const trace = steps.slice(0, depth).map(stepName).join("  →  ");
  return [
    `device: ${faultName(Number(s.fault_code))}   node: ${rowNode(r)}`,
    `error: ${errorName(Number(s.error_code))}   line: ${s.line}   depth: ${depth}`,
    `trace: ${trace || "—"}`,
  ].join("\n");
}
