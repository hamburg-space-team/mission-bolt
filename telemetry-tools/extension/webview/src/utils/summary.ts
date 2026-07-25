import { faultName, errorName, faultNode, stepName, gapNode } from "../../../src/codes";
import { fmtValue } from "./format";
import { nodeOf, type Row } from "./feed";

/** Origin node: firmware-stamped if present, else inferred from the code. */
export function rowNode(r: Row): string {
  const s = r.sample;
  const node = nodeOf(r);
  // Anything but the "system" type-level placeholder is a real, firmware-stamped
  // node (see nodeOf) - use it
  if (node !== "system") return node;
  // No source_node in the payload: fall back to what the code itself implies
  if (r.name === "fault") return `${faultNode(Number(s.fault_code))} (inferred)`;
  if (r.name === "gap_marker") return gapNode(String(s.reason ?? ""));
  return node;
}

/** One-line summary of a row's payload. */
export function summary(r: Row): string {
  const s = r.sample;
  // key off the Sample kind - the per-node families (timing, test) share
  // one kind but have four names each
  switch (String(s.kind)) {
    case "timing": {
      const tick = Number(s.tick_us);
      const over = tick > 40000 ? " ⚠ OVER 40 ms" : "";
      return `tick ${(tick / 1000).toFixed(1)} ms${over} · read ${s.read_us} · cfg ${s.cfg_us} · drive ${s.drive_us} · send ${s.send_us} · store ${s.store_us} µs`;
    }
    case "test": {
      const done = s.last ? " · run complete" : "";
      const data = Number(s.data);
      // hex reads well for IDs/registers, harmless for counts
      return `self-test step ${s.test_id}: ${s.result_name} · data 0x${data.toString(16).toUpperCase()}${done}`;
    }
    default:
      break;
  }
  switch (r.name) {
    case "fault":
      return `${faultName(Number(s.fault_code))} · ${errorName(Number(s.error_code))} · line ${s.line}`;
    case "gap_marker":
      return `gap tick ${s.first_missing_tick} ×${s.count} · ${s.reason}`;
    case "cmd_ack":
      return `${s.command} (seq ${s.seq}) · ${s.result}`;
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
