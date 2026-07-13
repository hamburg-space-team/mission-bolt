import { r as reactExports, j as jsxRuntimeExports, u as useMessages, p as post, c as createRoot } from "./chunk-DHH7KV4w.js";
import { f as fmtValue } from "./chunk-CG4_lz18.js";
const num = (v) => v === "" ? null : Number(v);
function FilterBar(p) {
  const [t0, setT0] = reactExports.useState("");
  const [t1, setT1] = reactExports.useState("");
  const [k0, setK0] = reactExports.useState("");
  const [k1, setK1] = reactExports.useState("");
  const apply = () => p.onRange({ tMin: num(t0), tMax: num(t1), kMin: num(k0), kMax: num(k1) });
  const clear = () => {
    setT0("");
    setT1("");
    setK0("");
    setK1("");
    p.onRange({ tMin: null, tMax: null, kMin: null, kMax: null });
  };
  const onKey = (e) => e.key === "Enter" && apply();
  return /* @__PURE__ */ jsxRuntimeExports.jsxs("div", { className: "bar", children: [
    /* @__PURE__ */ jsxRuntimeExports.jsxs("select", { value: p.filter, onChange: (e) => p.onFilter(e.target.value), children: [
      /* @__PURE__ */ jsxRuntimeExports.jsx("option", { value: "", children: "All" }),
      /* @__PURE__ */ jsxRuntimeExports.jsx("option", { value: "__fault", children: "⚠ Faults" }),
      /* @__PURE__ */ jsxRuntimeExports.jsx("option", { value: "__crc", children: "⚡ CRC fails" }),
      /* @__PURE__ */ jsxRuntimeExports.jsx("option", { value: "__gap", children: "▸ Gaps" }),
      /* @__PURE__ */ jsxRuntimeExports.jsx("option", { value: "__suspect", children: "⚠ Invalid (mask)" }),
      /* @__PURE__ */ jsxRuntimeExports.jsx("option", { disabled: true, children: "── source ──" }),
      /* @__PURE__ */ jsxRuntimeExports.jsx("option", { value: "__src:btc", children: "BTC" }),
      /* @__PURE__ */ jsxRuntimeExports.jsx("option", { value: "__src:exp1", children: "EXP1" }),
      /* @__PURE__ */ jsxRuntimeExports.jsx("option", { value: "__src:exp2", children: "EXP2" }),
      /* @__PURE__ */ jsxRuntimeExports.jsx("option", { value: "__src:exp3", children: "EXP3" }),
      /* @__PURE__ */ jsxRuntimeExports.jsx("option", { value: "__src:system", children: "System" }),
      p.types.length > 0 && /* @__PURE__ */ jsxRuntimeExports.jsx("option", { disabled: true, children: "── type ──" }),
      p.types.map((t) => /* @__PURE__ */ jsxRuntimeExports.jsx("option", { value: t, children: t }, t))
    ] }),
    /* @__PURE__ */ jsxRuntimeExports.jsx("input", { className: "num", placeholder: "t≥ µs", value: t0, onChange: (e) => setT0(e.target.value), onKeyDown: onKey }),
    /* @__PURE__ */ jsxRuntimeExports.jsx("input", { className: "num", placeholder: "t≤ µs", value: t1, onChange: (e) => setT1(e.target.value), onKeyDown: onKey }),
    /* @__PURE__ */ jsxRuntimeExports.jsx("input", { className: "num", placeholder: "tick≥", value: k0, onChange: (e) => setK0(e.target.value), onKeyDown: onKey }),
    /* @__PURE__ */ jsxRuntimeExports.jsx("input", { className: "num", placeholder: "tick≤", value: k1, onChange: (e) => setK1(e.target.value), onKeyDown: onKey }),
    /* @__PURE__ */ jsxRuntimeExports.jsx("button", { className: "secondary", onClick: apply, children: "Apply" }),
    /* @__PURE__ */ jsxRuntimeExports.jsx("button", { className: "secondary", onClick: clear, children: "✕" }),
    /* @__PURE__ */ jsxRuntimeExports.jsx("button", { className: p.paused ? "on" : "secondary", onClick: p.onPause, children: p.paused ? "▶ Resume" : "⏸ Pause" }),
    /* @__PURE__ */ jsxRuntimeExports.jsx("button", { className: "secondary", onClick: p.onReset, children: "Reset" }),
    /* @__PURE__ */ jsxRuntimeExports.jsx("button", { className: "secondary", onClick: p.onReload, children: "Reload" }),
    /* @__PURE__ */ jsxRuntimeExports.jsx("span", { style: { flex: 1 } }),
    /* @__PURE__ */ jsxRuntimeExports.jsx("button", { className: "secondary", disabled: p.page === 0, onClick: () => p.onPage(0), children: "⏮" }),
    /* @__PURE__ */ jsxRuntimeExports.jsx("button", { className: "secondary", disabled: p.page === 0, onClick: () => p.onPage(p.page - 1), children: "◀" }),
    /* @__PURE__ */ jsxRuntimeExports.jsx("span", { className: "muted", children: p.info }),
    /* @__PURE__ */ jsxRuntimeExports.jsx("button", { className: "secondary", disabled: p.page >= p.pages - 1, onClick: () => p.onPage(p.page + 1), children: "▶" }),
    /* @__PURE__ */ jsxRuntimeExports.jsx("button", { className: "secondary", disabled: p.page >= p.pages - 1, onClick: () => p.onPage(p.pages - 1), children: "⏭" })
  ] });
}
const ERROR_CODES = {
  1: "BUS_ERROR",
  2: "TIMEOUT",
  3: "BAD_ARGUMENT",
  4: "DISABLED",
  5: "PROTOCOL_ERROR",
  6: "IO_ERROR",
  7: "OUTPUT_TOO_LARGE"
};
const FAULT_CODES = {
  1: "I²C bus",
  2: "MS5611 barometer",
  3: "TMP117 temperature",
  4: "SD / storage",
  5: "CAN bus",
  6: "RS-422 downlink",
  7: "ICM-42686 IMU",
  8: "AS7265X spectrometer",
  9: "RGB LED driver (LP5810C)",
  10: "UV/IR LED driver (LP5810D)"
};
const FAULT_NODES = {
  1: "all",
  2: "all",
  3: "all",
  4: "all",
  5: "EXPs",
  6: "BTC",
  7: "BTC/EXP3",
  8: "EXP1",
  9: "EXP1",
  10: "EXP1"
};
const GAP_NODE_HINT = {
  lifi_timeout: "EXP3",
  can_crc_fail: "BTC←EXP",
  no_data: "BTC/EXP",
  sensor_failed: "device"
};
const STEP_CODES = {
  0: "NONE",
  16: "I2C_INIT",
  17: "I2C_RESET",
  18: "I2C_WRITE",
  19: "I2C_READ",
  20: "I2C_WRITE_READ",
  21: "I2C_WAIT_COMPLETE",
  32: "BARO_RESET",
  33: "BARO_PROM_READ",
  34: "BARO_PROM_CRC",
  35: "BARO_START_CONV",
  36: "BARO_READ_ADC",
  37: "BARO_COLLECT",
  38: "BARO_READ",
  48: "TMP_INIT",
  49: "TMP_ID_CHECK",
  50: "TMP_CONFIG",
  51: "TMP_READ",
  64: "SPEC_INIT",
  65: "SPEC_START_MEAS",
  66: "SPEC_SET_INTEGRATION",
  67: "SPEC_WAIT_STATUS",
  68: "SPEC_VREG_WRITE",
  69: "SPEC_VREG_READ",
  70: "SPEC_DEV_SEL",
  71: "SPEC_READ_DIES",
  80: "LED_INIT",
  81: "LED_CONFIGURE",
  82: "LED_ENABLE_CHIP",
  83: "LED_SET_CHANNELS",
  84: "LED_APPLY",
  85: "LED_DISABLE_ALL",
  86: "LED_RECOVER",
  96: "IMU_WHOAMI",
  97: "IMU_CONFIG_ACCEL",
  98: "IMU_CONFIG_GYRO",
  99: "IMU_POWER_ON",
  100: "IMU_READ",
  101: "IMU_CONFIG_INT",
  112: "SD_INIT",
  113: "SD_MOUNT",
  114: "SD_OPEN",
  115: "SD_WRITE",
  116: "SD_FLUSH",
  128: "PKT_BUILD",
  129: "CAN_TX_RING",
  130: "UART_TX_RING"
};
function stepName(code) {
  return STEP_CODES[code] ?? `0x${code.toString(16)}`;
}
function errorName(code) {
  return ERROR_CODES[code] ?? `err ${code}`;
}
function faultName(code) {
  return FAULT_CODES[code] ?? `fault ${code}`;
}
function faultNode(code) {
  return FAULT_NODES[code] ?? "?";
}
function gapNode(reason) {
  return GAP_NODE_HINT[reason] ?? "?";
}
function rowNode(r) {
  const s = r.sample;
  const node = s.node;
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
function summary(r) {
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
      const over = tick > 4e4 ? " ⚠ OVER 40 ms" : "";
      return `tick ${(tick / 1e3).toFixed(1)} ms${over} · read ${s.read_us} · cfg ${s.cfg_us} · drive ${s.drive_us} · send ${s.send_us} · store ${s.store_us} µs`;
    }
    default: {
      const keys = Object.keys(s).filter((k) => k !== "kind");
      return keys.slice(0, 4).map((k) => `${k}=${fmtValue(s[k])}`).join("  ");
    }
  }
}
function faultTrace(r) {
  const s = r.sample;
  const steps = s.steps ?? [];
  const depth = Number(s.depth ?? steps.length);
  const trace = steps.slice(0, depth).map(stepName).join("  →  ");
  return [
    `device: ${faultName(Number(s.fault_code))}   node: ${rowNode(r)}`,
    `error: ${errorName(Number(s.error_code))}   line: ${s.line}   depth: ${depth}`,
    `trace: ${trace || "—"}`
  ].join("\n");
}
function FeedTable({ rows, onShowType }) {
  const [open, setOpen] = reactExports.useState(null);
  return /* @__PURE__ */ jsxRuntimeExports.jsxs("table", { children: [
    /* @__PURE__ */ jsxRuntimeExports.jsx("thead", { children: /* @__PURE__ */ jsxRuntimeExports.jsxs("tr", { children: [
      /* @__PURE__ */ jsxRuntimeExports.jsx("th", { children: "#" }),
      /* @__PURE__ */ jsxRuntimeExports.jsx("th", { children: "tick" }),
      /* @__PURE__ */ jsxRuntimeExports.jsx("th", { children: "t (µs)" }),
      /* @__PURE__ */ jsxRuntimeExports.jsx("th", { children: "source" }),
      /* @__PURE__ */ jsxRuntimeExports.jsx("th", { children: "packet" }),
      /* @__PURE__ */ jsxRuntimeExports.jsx("th", { children: "crc" }),
      /* @__PURE__ */ jsxRuntimeExports.jsx("th", { children: "valid" }),
      /* @__PURE__ */ jsxRuntimeExports.jsx("th", { children: "summary" })
    ] }) }),
    /* @__PURE__ */ jsxRuntimeExports.jsx("tbody", { children: rows.map((r, i) => {
      const bad = !r.crc_ok || r.suspect;
      const key = `${r.timestamp_us}-${r.seq ?? i}`;
      return /* @__PURE__ */ jsxRuntimeExports.jsxs(reactExports.Fragment, { children: [
        /* @__PURE__ */ jsxRuntimeExports.jsxs("tr", { className: bad ? "bad" : "", onClick: () => setOpen(open === i ? null : i), style: { cursor: "pointer" }, children: [
          /* @__PURE__ */ jsxRuntimeExports.jsx("td", { children: r.seq ?? "" }),
          /* @__PURE__ */ jsxRuntimeExports.jsx("td", { children: r.tick }),
          /* @__PURE__ */ jsxRuntimeExports.jsx("td", { children: r.timestamp_us }),
          /* @__PURE__ */ jsxRuntimeExports.jsx("td", { children: /* @__PURE__ */ jsxRuntimeExports.jsx("span", { className: "pill", children: rowNode(r) }) }),
          /* @__PURE__ */ jsxRuntimeExports.jsx("td", { onClick: (e) => {
            e.stopPropagation();
            onShowType(r.name);
          }, style: { textDecoration: "underline" }, children: r.name }),
          /* @__PURE__ */ jsxRuntimeExports.jsx("td", { children: r.crc_ok ? "✓" : "✗" }),
          /* @__PURE__ */ jsxRuntimeExports.jsx("td", { children: r.suspect ? "⚠" : "✓" }),
          /* @__PURE__ */ jsxRuntimeExports.jsx("td", { children: summary(r) })
        ] }),
        open === i && /* @__PURE__ */ jsxRuntimeExports.jsx("tr", { children: /* @__PURE__ */ jsxRuntimeExports.jsx("td", { colSpan: 8, style: { whiteSpace: "pre-wrap", fontSize: 11, background: "var(--vscode-editorWidget-background)" }, children: r.name === "fault" ? faultTrace(r) : JSON.stringify(r.sample, null, 2) }) })
      ] }, key);
    }) })
  ] });
}
const emptyRange = { tMin: null, tMax: null, kMin: null, kMax: null };
function matches(r, filter, range) {
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
const PAGE = 100;
const CAP = 1e5;
function Feed() {
  const [rows, setRows] = reactExports.useState([]);
  const [counts, setCounts] = reactExports.useState({});
  const [filter, setFilter] = reactExports.useState("");
  const [range, setRange] = reactExports.useState(emptyRange);
  const [paused, setPaused] = reactExports.useState(false);
  const [page, setPage] = reactExports.useState(0);
  const [live, setLive] = reactExports.useState(true);
  useMessages((m) => {
    switch (m.type) {
      case "frame":
        setLive(true);
        setRows((prev) => {
          const next = prev.length >= CAP ? prev.slice(prev.length - CAP + 1) : prev.slice();
          next.push(m.frame);
          return next;
        });
        if (!paused) setPage(0);
        break;
      case "counts":
        setCounts(m.counts);
        break;
      case "load":
        setLive(false);
        setRows(m.records.slice().reverse());
        setPage(0);
        break;
      case "reset":
        setRows([]);
        setPage(0);
        break;
    }
  });
  const filtered = reactExports.useMemo(() => rows.filter((r) => matches(r, filter, range)), [rows, filter, range]);
  const pages = Math.max(1, Math.ceil(filtered.length / PAGE));
  const clamped = Math.min(page, pages - 1);
  const view = [];
  for (let i = 0; i < PAGE; i++) {
    const idx = filtered.length - 1 - clamped * PAGE - i;
    if (idx < 0) break;
    view.push(filtered[idx]);
  }
  const types = reactExports.useMemo(() => {
    const set = new Set(Object.keys(counts));
    for (const r of rows) set.add(r.name);
    return [...set].sort();
  }, [counts, rows]);
  const info = `p ${clamped + 1}/${pages} · ${filtered.length.toLocaleString()}${live ? " live" : ""}`;
  return /* @__PURE__ */ jsxRuntimeExports.jsxs(jsxRuntimeExports.Fragment, { children: [
    /* @__PURE__ */ jsxRuntimeExports.jsx(
      FilterBar,
      {
        filter,
        onFilter: (f) => {
          setFilter(f);
          setPage(0);
        },
        types,
        onRange: (r) => {
          setRange(r);
          setPage(0);
        },
        paused,
        onPause: () => {
          setPaused((p) => !p);
          setPage(0);
        },
        onReset: () => {
          setRows([]);
          setPage(0);
        },
        onReload: () => post({ type: "reload" }),
        page: clamped,
        pages,
        onPage: setPage,
        info
      }
    ),
    /* @__PURE__ */ jsxRuntimeExports.jsx(FeedTable, { rows: view, onShowType: (name) => post({ type: "showType", name }) })
  ] });
}
createRoot(document.getElementById("root")).render(/* @__PURE__ */ jsxRuntimeExports.jsx(Feed, {}));
//# sourceMappingURL=feed.js.map
