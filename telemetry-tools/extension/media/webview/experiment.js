import { j as jsxRuntimeExports, r as reactExports, u as useMessages, f as fmtValue, m as magnitude, c as createRoot } from "./chunk-myRcYhpq.js";
const WL = [410, 435, 460, 485, 510, 535, 560, 365, 340, 585, 610, 645, 680, 705, 730, 760, 810, 940];
function wlRGB(wl) {
  let r = 0, g = 0, b = 0;
  if (wl >= 380 && wl < 440) {
    r = -(wl - 440) / 60;
    b = 1;
  } else if (wl < 490) {
    g = (wl - 440) / 50;
    b = 1;
  } else if (wl < 510) {
    g = 1;
    b = -(wl - 510) / 20;
  } else if (wl < 580) {
    r = (wl - 510) / 70;
    g = 1;
  } else if (wl < 645) {
    r = 1;
    g = -(wl - 645) / 65;
  } else if (wl <= 780) {
    r = 1;
  } else if (wl < 380) {
    r = 0.4;
    b = 0.6;
  } else {
    r = 0.5;
  }
  const f = wl < 420 ? 0.3 + 0.7 * (wl - 380) / 40 : wl > 700 ? 0.3 + 0.7 * (780 - wl) / 80 : 1;
  const c = (x) => Math.round(255 * Math.max(0, Math.min(1, x)) * Math.max(0.25, f));
  return [c(r), c(g), c(b)];
}
function Spectrometer({ a, b, valid }) {
  const chans = [...a ?? Array(9).fill(0), ...b ?? Array(9).fill(0)];
  const order = WL.map((_, i) => i).sort((x, y) => WL[x] - WL[y]);
  const max = Math.max(1, ...chans);
  return /* @__PURE__ */ jsxRuntimeExports.jsx("div", { className: "spectro", children: order.map((i) => {
    const h = Math.max(1, chans[i] / max * 100);
    const [r, g, bl] = wlRGB(WL[i]);
    return /* @__PURE__ */ jsxRuntimeExports.jsxs("div", { className: "spectro-bar", title: `${WL[i]} nm: ${chans[i]}`, children: [
      /* @__PURE__ */ jsxRuntimeExports.jsx("div", { className: "spectro-fill", style: { height: `${h}%`, background: `rgb(${r},${g},${bl})`, opacity: valid === false ? 0.35 : 1 } }),
      /* @__PURE__ */ jsxRuntimeExports.jsx("div", { className: "spectro-wl", children: WL[i] >= 380 && WL[i] <= 780 ? WL[i] : WL[i] < 380 ? "UV" : "NIR" })
    ] }, i);
  }) });
}
const GAINS = ["1x", "3.7x", "16x", "64x"];
function Tiles({ items }) {
  if (items.length === 0) return /* @__PURE__ */ jsxRuntimeExports.jsx("div", { className: "muted", children: "no data yet" });
  return /* @__PURE__ */ jsxRuntimeExports.jsx("div", { className: "tiles", children: items.map(([l, v]) => /* @__PURE__ */ jsxRuntimeExports.jsxs("div", { className: "tile", children: [
    /* @__PURE__ */ jsxRuntimeExports.jsx("div", { className: "n", children: v }),
    /* @__PURE__ */ jsxRuntimeExports.jsx("div", { className: "l", children: l })
  ] }, l)) });
}
function Experiment() {
  const [source, setSource] = reactExports.useState("");
  const [byKind, setByKind] = reactExports.useState({});
  const [specA, setSpecA] = reactExports.useState();
  const [specB, setSpecB] = reactExports.useState();
  useMessages((m) => {
    if (m.type === "experiment") setSource(m.source);
    else if (m.type === "reset") {
      setByKind({});
      setSpecA(void 0);
      setSpecB(void 0);
    } else if (m.type === "frame") {
      const f = m.frame;
      const s = f.sample;
      if (f.name.endsWith("spectrum_a")) setSpecA(s);
      else if (f.name.endsWith("spectrum_b")) setSpecB(s);
      else setByKind((prev) => ({ ...prev, [String(s.kind)]: s }));
    }
  });
  const env = byKind.env;
  const status = byKind.status;
  const imu = byKind.imu;
  const ber = byKind.ber;
  const envTiles = [];
  if (env?.temp_c != null) envTiles.push(["temp", `${Number(env.temp_c).toFixed(1)} °C`]);
  if (env?.pressure_mbar != null) envTiles.push(["pressure", `${Number(env.pressure_mbar).toFixed(1)} mbar`]);
  if (status?.data_ready_fails != null) envTiles.push(["dataRdy fails", fmtValue(status.data_ready_fails)]);
  return /* @__PURE__ */ jsxRuntimeExports.jsxs(jsxRuntimeExports.Fragment, { children: [
    /* @__PURE__ */ jsxRuntimeExports.jsx("h1", { children: source.toUpperCase() || "Experiment" }),
    source === "exp1" && /* @__PURE__ */ jsxRuntimeExports.jsxs(jsxRuntimeExports.Fragment, { children: [
      /* @__PURE__ */ jsxRuntimeExports.jsx("h2", { children: "Spectrometer · current cycle" }),
      /* @__PURE__ */ jsxRuntimeExports.jsx(Spectrometer, { a: specA?.channels, b: specB?.channels, valid: specA?.measurement_valid !== 0 }),
      /* @__PURE__ */ jsxRuntimeExports.jsx("div", { className: "muted", style: { marginTop: 6 }, children: specA ? `gain ${GAINS[Number(specA.gain)] ?? specA.gain} · led ${specA.led_mask} · integ ${specA.integration_cycles} · ${specA.measurement_valid === 0 ? "INVALID (mask)" : "valid"}` : "waiting…" })
    ] }),
    ber && /* @__PURE__ */ jsxRuntimeExports.jsxs(jsxRuntimeExports.Fragment, { children: [
      /* @__PURE__ */ jsxRuntimeExports.jsx("h2", { children: "Bit error rate" }),
      /* @__PURE__ */ jsxRuntimeExports.jsx(Tiles, { items: [["BER", `${(Number(ber.ber) * 100).toFixed(2)} %`], ["bit errors", fmtValue(ber.bit_errors)], ["rate idx", fmtValue(ber.rate_index)]] })
    ] }),
    imu && /* @__PURE__ */ jsxRuntimeExports.jsxs(jsxRuntimeExports.Fragment, { children: [
      /* @__PURE__ */ jsxRuntimeExports.jsx("h2", { children: "IMU" }),
      /* @__PURE__ */ jsxRuntimeExports.jsx(Tiles, { items: [
        ["|accel| m/s²", magnitude(imu.accel_ms2).toFixed(2)],
        ["|gyro| dps", magnitude(imu.gyro_dps).toFixed(1)]
      ] })
    ] }),
    /* @__PURE__ */ jsxRuntimeExports.jsx("h2", { children: "Environment" }),
    /* @__PURE__ */ jsxRuntimeExports.jsx(Tiles, { items: envTiles })
  ] });
}
createRoot(document.getElementById("root")).render(/* @__PURE__ */ jsxRuntimeExports.jsx(Experiment, {}));
//# sourceMappingURL=experiment.js.map
