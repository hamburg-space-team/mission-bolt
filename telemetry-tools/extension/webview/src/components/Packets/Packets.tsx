import { useEffect, useRef, useState } from "react";
import { TabulatorFull as Tabulator } from "tabulator-tables";
import { useMessages } from "../../utils/api";
import { fmtValue } from "../../utils/format";
import type { PacketRecord } from "../../../../src/messages";

const LIST = new Set([
  "source", "node", "reason", "command", "result", "device", "error", "crc", "valid",
  "fault_code", "error_code", "opcode", "status", "gain", "led_mask", "measurement_valid",
  "valid_mask", "rate_index", "sd_status", "signal_mask",
]);
const NUM = new Set(["seq", "tick", "timestamp_us", "fault_code", "error_code", "line", "depth", "opcode", "status", "reboot_count", "bit_errors", "bits_sent"]);
const TITLE: Record<string, string> = { seq: "#", timestamp_us: "t (µs)" };
const FIXED = ["seq", "tick", "timestamp_us", "source", "crc", "valid"];

type Flat = Record<string, unknown>;

function flatten(records: PacketRecord[]): Flat[] {
  return records.map((r) => ({
    ...r.sample,
    kind: undefined,
    seq: r.seq, tick: r.tick, timestamp_us: r.timestamp_us,
    source: r.source ?? "", crc: r.crc_ok ? "ok" : "FAIL", valid: r.suspect ? "suspect" : "ok",
  }));
}

function columns(rows: Flat[]): unknown[] {
  const fields = FIXED.slice();
  for (const row of rows) for (const k of Object.keys(row)) {
    if (k !== "kind" && row[k] !== undefined && !fields.includes(k)) fields.push(k);
  }
  return fields.map((f) => {
    const list = LIST.has(f);
    return {
      title: TITLE[f] ?? f, field: f,
      headerFilter: list ? "list" : "input",
      headerFilterParams: list ? { valuesLookup: true, clearable: true } : undefined,
      headerFilterLiveFilter: !list,
      sorter: NUM.has(f) ? "number" : "string",
      formatter: (cell: { getValue(): unknown }) => fmtValue(cell.getValue()),
      minWidth: 70,
    };
  });
}

export function Packets() {
  const host = useRef<HTMLDivElement>(null);
  // Tabulator ships no bundler-resolvable types here; treat the instance as any
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const table = useRef<any>(null);
  const [name, setName] = useState("");
  const [rows, setRows] = useState<Flat[]>([]);
  const [t0, setT0] = useState(""); const [t1, setT1] = useState("");
  const [k0, setK0] = useState(""); const [k1, setK1] = useState("");

  useMessages((m) => {
    if (m.type === "packets") { setName(m.name); setRows(flatten(m.records)); }
  });

  useEffect(() => {
    if (!host.current) return;
    const t = new Tabulator(host.current, {
      data: rows,
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      columns: columns(rows) as any,
      columnDefaults: { headerSortTristate: true },
      layout: "fitDataStretch",
      height: "82vh",
      pagination: true,
      paginationSize: 100,
      paginationSizeSelector: [50, 100, 250, 1000],
      paginationCounter: "rows",
      initialSort: [{ column: "tick", dir: "desc" }],
      rowFormatter: (row: { getData(): Flat; getElement(): HTMLElement }) => {
        const d = row.getData();
        if (d.crc === "FAIL" || d.valid === "suspect") row.getElement().classList.add("bad");
      },
    });
    table.current = t;
    return () => { t.destroy(); table.current = null; };
  }, [rows]);

  const applyRange = () => {
    const t = table.current;
    if (!t) return;
    const f: { field: string; type: string; value: number }[] = [];
    const add = (field: string, type: string, v: string) => v !== "" && f.push({ field, type, value: Number(v) });
    add("timestamp_us", ">=", t0); add("timestamp_us", "<=", t1);
    add("tick", ">=", k0); add("tick", "<=", k1);
    t.clearFilter(false);
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    if (f.length) t.setFilter(f as any);
  };
  const clearAll = () => {
    setT0(""); setT1(""); setK0(""); setK1("");
    table.current?.clearFilter(true);
  };

  return (
    <>
      <div className="bar">
        <strong>{name}</strong>
        <span className="muted">— filter any column header (node, fault_code, source, …)</span>
        <span style={{ flex: 1 }} />
        <input className="num" placeholder="t≥ µs" value={t0} onChange={(e) => setT0(e.target.value)} />
        <input className="num" placeholder="t≤ µs" value={t1} onChange={(e) => setT1(e.target.value)} />
        <input className="num" placeholder="tick≥" value={k0} onChange={(e) => setK0(e.target.value)} />
        <input className="num" placeholder="tick≤" value={k1} onChange={(e) => setK1(e.target.value)} />
        <button className="secondary" onClick={applyRange}>Apply range</button>
        <button className="secondary" onClick={clearAll}>Clear</button>
      </div>
      <div ref={host} />
    </>
  );
}
