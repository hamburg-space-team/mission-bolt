import { r as reactExports, u as useMessages, j as jsxRuntimeExports, f as fmtValue, c as createRoot } from "./chunk-myRcYhpq.js";
const SOURCES = ["btc", "exp1", "exp2", "exp3"];
function Tile({ n, label, bad }) {
  return /* @__PURE__ */ jsxRuntimeExports.jsxs("div", { className: "tile", children: [
    /* @__PURE__ */ jsxRuntimeExports.jsx("div", { className: `n ${bad ? "bad" : ""}`, children: n }),
    /* @__PURE__ */ jsxRuntimeExports.jsx("div", { className: "l", children: label })
  ] });
}
function Signal({ on, label }) {
  return /* @__PURE__ */ jsxRuntimeExports.jsxs("span", { className: "pill", children: [
    label,
    " ",
    on ? "✓" : "✗"
  ] });
}
function Overview() {
  const [live, setLive] = reactExports.useState(true);
  const [stats, setStats] = reactExports.useState();
  const [manifest, setManifest] = reactExports.useState();
  const [latest, setLatest] = reactExports.useState({});
  useMessages((m) => {
    if (m.type === "init") {
      setLive(m.live);
      setManifest(m.manifest);
    } else if (m.type === "stats") setStats(m.stats);
    else if (m.type === "frame") {
      const f = m.frame;
      const kind = String(f.sample.kind);
      setLatest((prev) => ({ ...prev, [f.source]: { ...prev[f.source], [kind]: f.sample } }));
    }
  });
  const total = stats?.total ?? manifest?.total_frames ?? 0;
  const crcFail = stats?.crc_fail ?? manifest?.crc_fail_total ?? 0;
  const crcPct = total ? (crcFail / total * 100).toFixed(2) : "0.00";
  const lo = stats?.lo ?? (manifest ? manifest.lo_rtc_s > 0 : false);
  const soe = stats?.soe ?? false;
  const sods = stats?.sods ?? false;
  const sources = reactExports.useMemo(() => {
    const seen = /* @__PURE__ */ new Set([...Object.keys(latest), ...Object.keys(manifest?.packet_counts ?? {}).map((n) => n.split("_")[0])]);
    return SOURCES.filter((s) => seen.has(s) || live);
  }, [latest, manifest, live]);
  return /* @__PURE__ */ jsxRuntimeExports.jsxs(jsxRuntimeExports.Fragment, { children: [
    /* @__PURE__ */ jsxRuntimeExports.jsx("h1", { children: manifest?.mission ?? (live ? "Live" : "Overview") }),
    /* @__PURE__ */ jsxRuntimeExports.jsxs("div", { className: "row", children: [
      /* @__PURE__ */ jsxRuntimeExports.jsx(Signal, { on: lo, label: "LO" }),
      /* @__PURE__ */ jsxRuntimeExports.jsx(Signal, { on: soe, label: "SOE" }),
      /* @__PURE__ */ jsxRuntimeExports.jsx(Signal, { on: sods, label: "SODS" })
    ] }),
    /* @__PURE__ */ jsxRuntimeExports.jsxs("div", { className: "tiles", style: { marginTop: 8 }, children: [
      /* @__PURE__ */ jsxRuntimeExports.jsx(Tile, { n: total.toLocaleString(), label: "frames" }),
      /* @__PURE__ */ jsxRuntimeExports.jsx(Tile, { n: `${crcPct}%`, label: "CRC fails", bad: crcFail > 0 }),
      /* @__PURE__ */ jsxRuntimeExports.jsx(Tile, { n: String(crcFail), label: "crc-fail count", bad: crcFail > 0 }),
      /* @__PURE__ */ jsxRuntimeExports.jsx(Tile, { n: live ? "live" : "post-flight", label: "mode" })
    ] }),
    /* @__PURE__ */ jsxRuntimeExports.jsx("h2", { children: "Experiments" }),
    /* @__PURE__ */ jsxRuntimeExports.jsx("div", { className: "tiles", children: sources.map((src) => {
      const env = latest[src]?.env;
      const status = latest[src]?.status;
      const metrics = [];
      if (env?.temp_c != null) metrics.push(["temp", `${Number(env.temp_c).toFixed(1)} °C`]);
      if (env?.pressure_mbar != null) metrics.push(["pressure", `${Number(env.pressure_mbar).toFixed(1)} mbar`]);
      if (status?.data_ready_fails != null) metrics.push(["dataRdy fails", fmtValue(status.data_ready_fails)]);
      return /* @__PURE__ */ jsxRuntimeExports.jsxs("div", { className: "tile", children: [
        /* @__PURE__ */ jsxRuntimeExports.jsx("div", { className: "n", children: src.toUpperCase() }),
        metrics.length === 0 && /* @__PURE__ */ jsxRuntimeExports.jsx("div", { className: "l", children: "no data yet" }),
        metrics.map(([l, v]) => /* @__PURE__ */ jsxRuntimeExports.jsxs("div", { className: "l", children: [
          l,
          ": ",
          v
        ] }, l))
      ] }, src);
    }) })
  ] });
}
createRoot(document.getElementById("root")).render(/* @__PURE__ */ jsxRuntimeExports.jsx(Overview, {}));
//# sourceMappingURL=overview.js.map
