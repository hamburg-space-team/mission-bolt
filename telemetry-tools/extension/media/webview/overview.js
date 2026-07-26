import { a as require_client, i as useMessages, o as require_react, t as require_jsx_runtime } from "./chunk-19Q_QZ2X.js";
import { t as fmtValue } from "./chunk-DwebAtj3.js";
//#region webview/src/components/Overview/Overview.tsx
var import_client = require_client();
var import_react = require_react();
var import_jsx_runtime = require_jsx_runtime();
var SOURCES = [
	"btc",
	"exp1",
	"exp2",
	"exp3"
];
function Tile({ n, label, bad }) {
	return /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
		className: "tile",
		children: [/* @__PURE__ */ (0, import_jsx_runtime.jsx)("div", {
			className: `n ${bad ? "bad" : ""}`,
			children: n
		}), /* @__PURE__ */ (0, import_jsx_runtime.jsx)("div", {
			className: "l",
			children: label
		})]
	});
}
function Signal({ on, label }) {
	return /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("span", {
		className: "pill",
		children: [
			label,
			" ",
			on ? "✓" : "✗"
		]
	});
}
function Overview() {
	const [live, setLive] = (0, import_react.useState)(true);
	const [stats, setStats] = (0, import_react.useState)();
	const [manifest, setManifest] = (0, import_react.useState)();
	const [latest, setLatest] = (0, import_react.useState)({});
	useMessages((m) => {
		if (m.type === "init") {
			setLive(m.live);
			setManifest(m.manifest);
		} else if (m.type === "stats") setStats(m.stats);
		else if (m.type === "frame") {
			const f = m.frame;
			const kind = String(f.sample.kind);
			setLatest((prev) => ({
				...prev,
				[f.source]: {
					...prev[f.source],
					[kind]: f.sample
				}
			}));
		}
	});
	const total = stats?.total ?? manifest?.total_frames ?? 0;
	const crcFail = stats?.crc_fail ?? manifest?.crc_fail_total ?? 0;
	const crcPct = total ? (crcFail / total * 100).toFixed(2) : "0.00";
	const lo = stats?.lo ?? (manifest ? manifest.lo_rtc_s > 0 : false);
	const soe = stats?.soe ?? false;
	const sods = stats?.sods ?? false;
	const sources = (0, import_react.useMemo)(() => {
		const seen = /* @__PURE__ */ new Set([...Object.keys(latest), ...Object.keys(manifest?.packet_counts ?? {}).map((n) => n.split("_")[0])]);
		return SOURCES.filter((s) => seen.has(s) || live);
	}, [
		latest,
		manifest,
		live
	]);
	return /* @__PURE__ */ (0, import_jsx_runtime.jsxs)(import_jsx_runtime.Fragment, { children: [
		/* @__PURE__ */ (0, import_jsx_runtime.jsx)("h1", { children: manifest?.mission ?? (live ? "Live" : "Overview") }),
		/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
			className: "row",
			children: [
				/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Signal, {
					on: lo,
					label: "LO"
				}),
				/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Signal, {
					on: sods,
					label: "SODS"
				}),
				/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Signal, {
					on: soe,
					label: "SOE"
				})
			]
		}),
		/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
			className: "tiles",
			style: { marginTop: 8 },
			children: [
				/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Tile, {
					n: total.toLocaleString(),
					label: "frames"
				}),
				/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Tile, {
					n: `${crcPct}%`,
					label: "CRC fails",
					bad: crcFail > 0
				}),
				/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Tile, {
					n: String(crcFail),
					label: "crc-fail count",
					bad: crcFail > 0
				}),
				/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Tile, {
					n: live ? "live" : "post-flight",
					label: "mode"
				})
			]
		}),
		/* @__PURE__ */ (0, import_jsx_runtime.jsx)("h2", { children: "Experiments" }),
		/* @__PURE__ */ (0, import_jsx_runtime.jsx)("div", {
			className: "tiles",
			children: sources.map((src) => {
				const env = latest[src]?.env;
				const status = latest[src]?.status;
				const metrics = [];
				if (env?.temp_c != null) metrics.push(["temp", `${Number(env.temp_c).toFixed(1)} °C`]);
				if (env?.pressure_mbar != null) metrics.push(["pressure", `${Number(env.pressure_mbar).toFixed(1)} mbar`]);
				if (status?.data_ready_fails != null) metrics.push(["dataRdy fails", fmtValue(status.data_ready_fails)]);
				return /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
					className: "tile",
					children: [
						/* @__PURE__ */ (0, import_jsx_runtime.jsx)("div", {
							className: "n",
							children: src.toUpperCase()
						}),
						metrics.length === 0 && /* @__PURE__ */ (0, import_jsx_runtime.jsx)("div", {
							className: "l",
							children: "no data yet"
						}),
						metrics.map(([l, v]) => /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
							className: "l",
							children: [
								l,
								": ",
								v
							]
						}, l))
					]
				}, src);
			})
		})
	] });
}
//#endregion
//#region webview/src/entries/overview.tsx
(0, import_client.createRoot)(document.getElementById("root")).render(/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Overview, {}));
//#endregion

//# sourceMappingURL=overview.js.map