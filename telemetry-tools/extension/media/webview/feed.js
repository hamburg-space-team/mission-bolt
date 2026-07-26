import { a as require_client, i as useMessages, n as post, o as require_react, t as require_jsx_runtime } from "./chunk-19Q_QZ2X.js";
import { t as fmtValue } from "./chunk-DwebAtj3.js";
//#region webview/src/components/Feed/FilterBar.tsx
var import_client = require_client();
var import_react = require_react();
var import_jsx_runtime = require_jsx_runtime();
var num = (v) => v === "" ? null : Number(v);
function FilterBar(p) {
	const [t0, setT0] = (0, import_react.useState)("");
	const [t1, setT1] = (0, import_react.useState)("");
	const [k0, setK0] = (0, import_react.useState)("");
	const [k1, setK1] = (0, import_react.useState)("");
	const apply = () => p.onRange({
		tMin: num(t0),
		tMax: num(t1),
		kMin: num(k0),
		kMax: num(k1)
	});
	const clear = () => {
		setT0("");
		setT1("");
		setK0("");
		setK1("");
		p.onRange({
			tMin: null,
			tMax: null,
			kMin: null,
			kMax: null
		});
	};
	const onKey = (e) => e.key === "Enter" && apply();
	return /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
		className: "bar",
		children: [
			/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("select", {
				value: p.filter,
				onChange: (e) => p.onFilter(e.target.value),
				children: [
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("option", {
						value: "",
						children: "All"
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("option", {
						value: "__fault",
						children: "⚠ Faults"
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("option", {
						value: "__crc",
						children: "⚡ CRC fails"
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("option", {
						value: "__gap",
						children: "▸ Gaps"
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("option", {
						value: "__suspect",
						children: "⚠ Invalid (mask)"
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("option", {
						disabled: true,
						children: "── source ──"
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("option", {
						value: "__src:btc",
						children: "BTC"
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("option", {
						value: "__src:exp1",
						children: "EXP1"
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("option", {
						value: "__src:exp2",
						children: "EXP2"
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("option", {
						value: "__src:exp3",
						children: "EXP3"
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("option", {
						value: "__src:system",
						children: "System"
					}),
					p.types.length > 0 && /* @__PURE__ */ (0, import_jsx_runtime.jsx)("option", {
						disabled: true,
						children: "── type ──"
					}),
					p.types.map((t) => /* @__PURE__ */ (0, import_jsx_runtime.jsx)("option", {
						value: t,
						children: t
					}, t))
				]
			}),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("input", {
				className: "num",
				placeholder: "t≥ µs",
				value: t0,
				onChange: (e) => setT0(e.target.value),
				onKeyDown: onKey
			}),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("input", {
				className: "num",
				placeholder: "t≤ µs",
				value: t1,
				onChange: (e) => setT1(e.target.value),
				onKeyDown: onKey
			}),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("input", {
				className: "num",
				placeholder: "tick≥",
				value: k0,
				onChange: (e) => setK0(e.target.value),
				onKeyDown: onKey
			}),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("input", {
				className: "num",
				placeholder: "tick≤",
				value: k1,
				onChange: (e) => setK1(e.target.value),
				onKeyDown: onKey
			}),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("button", {
				className: "secondary",
				onClick: apply,
				children: "Apply"
			}),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("button", {
				className: "secondary",
				onClick: clear,
				children: "✕"
			}),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("button", {
				className: p.paused ? "on" : "secondary",
				onClick: p.onPause,
				children: p.paused ? "▶ Resume" : "⏸ Pause"
			}),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("button", {
				className: "secondary",
				onClick: p.onReset,
				children: "Reset"
			}),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("button", {
				className: "secondary",
				onClick: p.onReload,
				children: "Reload"
			}),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("span", { style: { flex: 1 } }),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("button", {
				className: "secondary",
				disabled: p.page === 0,
				onClick: () => p.onPage(0),
				children: "⏮"
			}),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("button", {
				className: "secondary",
				disabled: p.page === 0,
				onClick: () => p.onPage(p.page - 1),
				children: "◀"
			}),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("span", {
				className: "muted",
				children: p.info
			}),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("button", {
				className: "secondary",
				disabled: p.page >= p.pages - 1,
				onClick: () => p.onPage(p.page + 1),
				children: "▶"
			}),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("button", {
				className: "secondary",
				disabled: p.page >= p.pages - 1,
				onClick: () => p.onPage(p.pages - 1),
				children: "⏭"
			})
		]
	});
}
//#endregion
//#region src/codes.ts
var ERROR_CODES = {
	1: "BUS_ERROR",
	2: "TIMEOUT",
	3: "BAD_ARGUMENT",
	4: "DISABLED",
	5: "PROTOCOL_ERROR",
	6: "IO_ERROR",
	7: "OUTPUT_TOO_LARGE"
};
var FAULT_CODES = {
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
var FAULT_NODES = {
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
var GAP_NODE_HINT = {
	lifi_timeout: "EXP3",
	can_crc_fail: "BTC←EXP",
	no_data: "BTC/EXP",
	sensor_failed: "device"
};
var STEP_CODES = {
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
//#endregion
//#region webview/src/utils/feed.ts
var emptyRange = {
	tMin: null,
	tMax: null,
	kMin: null,
	kMax: null
};
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
function nodeOf(r) {
	const node = r.sample?.node;
	if (node && node !== "unknown") return node;
	return r.source;
}
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
	if (filter.startsWith("__src:")) return nodeOf(r) === filter.slice(6);
	return r.name === filter;
}
//#endregion
//#region webview/src/utils/summary.ts
/** Origin node: firmware-stamped if present, else inferred from the code. */
function rowNode(r) {
	const s = r.sample;
	const node = nodeOf(r);
	if (node !== "system") return node;
	if (r.name === "fault") return `${faultNode(Number(s.fault_code))} (inferred)`;
	if (r.name === "gap_marker") return gapNode(String(s.reason ?? ""));
	return node;
}
/** One-line summary of a row's payload. */
function summary(r) {
	const s = r.sample;
	switch (String(s.kind)) {
		case "timing": {
			const tick = Number(s.tick_us);
			const over = tick > 4e4 ? " ⚠ OVER 40 ms" : "";
			return `tick ${(tick / 1e3).toFixed(1)} ms${over} · read ${s.read_us} · cfg ${s.cfg_us} · drive ${s.drive_us} · send ${s.send_us} · store ${s.store_us} µs`;
		}
		case "test": {
			const done = s.last ? " · run complete" : "";
			const data = Number(s.data);
			return `self-test step ${s.test_id}: ${s.result_name} · data 0x${data.toString(16).toUpperCase()}${done}`;
		}
		default: break;
	}
	switch (r.name) {
		case "fault": return `${faultName(Number(s.fault_code))} · ${errorName(Number(s.error_code))} · line ${s.line}`;
		case "gap_marker": return `gap tick ${s.first_missing_tick} ×${s.count} · ${s.reason}`;
		case "cmd_ack": return `${s.command} (seq ${s.seq}) · ${s.result}`;
		default: return Object.keys(s).filter((k) => k !== "kind").slice(0, 4).map((k) => `${k}=${fmtValue(s[k])}`).join("  ");
	}
}
/** Full fault trace for the detail row. */
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
//#endregion
//#region webview/src/components/Feed/FeedTable.tsx
function FeedTable({ rows, onShowType }) {
	const [open, setOpen] = (0, import_react.useState)(null);
	return /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("table", { children: [/* @__PURE__ */ (0, import_jsx_runtime.jsx)("thead", { children: /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("tr", { children: [
		/* @__PURE__ */ (0, import_jsx_runtime.jsx)("th", { children: "#" }),
		/* @__PURE__ */ (0, import_jsx_runtime.jsx)("th", { children: "tick" }),
		/* @__PURE__ */ (0, import_jsx_runtime.jsx)("th", { children: "t (µs)" }),
		/* @__PURE__ */ (0, import_jsx_runtime.jsx)("th", { children: "source" }),
		/* @__PURE__ */ (0, import_jsx_runtime.jsx)("th", { children: "packet" }),
		/* @__PURE__ */ (0, import_jsx_runtime.jsx)("th", { children: "crc" }),
		/* @__PURE__ */ (0, import_jsx_runtime.jsx)("th", { children: "valid" }),
		/* @__PURE__ */ (0, import_jsx_runtime.jsx)("th", { children: "summary" })
	] }) }), /* @__PURE__ */ (0, import_jsx_runtime.jsx)("tbody", { children: rows.map((r) => {
		return /* @__PURE__ */ (0, import_jsx_runtime.jsxs)(import_react.Fragment, { children: [/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("tr", {
			className: !r.crc_ok || r.suspect ? "bad" : "",
			onClick: () => setOpen(open === r.uid ? null : r.uid),
			style: { cursor: "pointer" },
			children: [
				/* @__PURE__ */ (0, import_jsx_runtime.jsx)("td", { children: r.seq ?? "" }),
				/* @__PURE__ */ (0, import_jsx_runtime.jsx)("td", { children: r.tick }),
				/* @__PURE__ */ (0, import_jsx_runtime.jsx)("td", { children: r.timestamp_us }),
				/* @__PURE__ */ (0, import_jsx_runtime.jsx)("td", { children: /* @__PURE__ */ (0, import_jsx_runtime.jsx)("span", {
					className: "pill",
					children: rowNode(r)
				}) }),
				/* @__PURE__ */ (0, import_jsx_runtime.jsx)("td", {
					onClick: (e) => {
						e.stopPropagation();
						onShowType(r.name);
					},
					style: { textDecoration: "underline" },
					children: r.name
				}),
				/* @__PURE__ */ (0, import_jsx_runtime.jsx)("td", { children: r.crc_ok ? "✓" : "✗" }),
				/* @__PURE__ */ (0, import_jsx_runtime.jsx)("td", { children: r.suspect ? "⚠" : "✓" }),
				/* @__PURE__ */ (0, import_jsx_runtime.jsx)("td", { children: summary(r) })
			]
		}), open === r.uid && /* @__PURE__ */ (0, import_jsx_runtime.jsx)("tr", { children: /* @__PURE__ */ (0, import_jsx_runtime.jsx)("td", {
			colSpan: 8,
			style: {
				whiteSpace: "pre-wrap",
				fontSize: 11,
				background: "var(--vscode-editorWidget-background)"
			},
			children: r.name === "fault" ? faultTrace(r) : JSON.stringify(r.sample, null, 2)
		}) })] }, r.uid);
	}) })] });
}
//#endregion
//#region webview/src/components/Feed/Feed.tsx
var PAGE = 100;
var CAP = 1e5;
var FLUSH_MS = 100;
var BACKLOG_MS = 250;
function Feed() {
	const [rows, setRows] = (0, import_react.useState)([]);
	const [counts, setCounts] = (0, import_react.useState)({});
	const [filter, setFilter] = (0, import_react.useState)("");
	const [range, setRange] = (0, import_react.useState)(emptyRange);
	const [paused, setPaused] = (0, import_react.useState)(false);
	const [page, setPage] = (0, import_react.useState)(0);
	const [live, setLive] = (0, import_react.useState)(true);
	const [backlog, setBacklog] = (0, import_react.useState)(0);
	const pending = (0, import_react.useRef)([]);
	const nextUid = (0, import_react.useRef)(0);
	const following = !paused && page === 0;
	useMessages((m) => {
		switch (m.type) {
			case "frame":
				pending.current.push({
					...m.frame,
					uid: nextUid.current++
				});
				if (pending.current.length > CAP) pending.current.splice(0, pending.current.length - CAP);
				break;
			case "counts":
				setCounts(m.counts);
				break;
			case "load":
				pending.current = [];
				setLive(false);
				setRows(m.records.slice().reverse().map((r) => ({
					...r,
					uid: nextUid.current++
				})));
				setPage(0);
				break;
			case "reset":
				pending.current = [];
				setRows([]);
				setPage(0);
				break;
		}
	});
	(0, import_react.useEffect)(() => {
		if (!following) return;
		const id = setInterval(() => {
			if (pending.current.length === 0) return;
			const batch = pending.current;
			pending.current = [];
			setLive(true);
			setRows((prev) => {
				const next = prev.concat(batch);
				return next.length > CAP ? next.slice(next.length - CAP) : next;
			});
		}, FLUSH_MS);
		return () => clearInterval(id);
	}, [following]);
	(0, import_react.useEffect)(() => {
		if (following) {
			setBacklog(0);
			return;
		}
		const id = setInterval(() => setBacklog(pending.current.length), BACKLOG_MS);
		return () => clearInterval(id);
	}, [following]);
	const filtered = (0, import_react.useMemo)(() => rows.filter((r) => matches(r, filter, range)), [
		rows,
		filter,
		range
	]);
	const pages = Math.max(1, Math.ceil(filtered.length / PAGE));
	const clamped = Math.min(page, pages - 1);
	const view = (0, import_react.useMemo)(() => {
		const out = [];
		for (let i = 0; i < PAGE; i++) {
			const idx = filtered.length - 1 - clamped * PAGE - i;
			if (idx < 0) break;
			out.push(filtered[idx]);
		}
		return out;
	}, [filtered, clamped]);
	const types = (0, import_react.useMemo)(() => {
		const set = new Set(Object.keys(counts));
		for (const r of rows) if (r.name) set.add(r.name);
		return [...set].sort();
	}, [counts, rows]);
	const state = following ? live ? " · live" : "" : backlog > 0 ? ` · frozen +${backlog.toLocaleString()}` : " · frozen";
	const info = `p ${clamped + 1}/${pages} · ${filtered.length.toLocaleString()}${state}`;
	const resume = () => {
		setPaused(false);
		setPage(0);
	};
	return /* @__PURE__ */ (0, import_jsx_runtime.jsxs)(import_jsx_runtime.Fragment, { children: [/* @__PURE__ */ (0, import_jsx_runtime.jsx)(FilterBar, {
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
		onPause: () => paused ? resume() : setPaused(true),
		onReset: () => {
			pending.current = [];
			setRows([]);
			setPage(0);
		},
		onReload: () => post({ type: "reload" }),
		page: clamped,
		pages,
		onPage: setPage,
		info
	}), /* @__PURE__ */ (0, import_jsx_runtime.jsx)(FeedTable, {
		rows: view,
		onShowType: (name) => post({
			type: "showType",
			name
		})
	})] });
}
//#endregion
//#region webview/src/entries/feed.tsx
var errStyle = {
	whiteSpace: "pre-wrap",
	color: "var(--vscode-errorForeground)",
	fontSize: 11
};
var ErrorBoundary = class extends import_react.Component {
	constructor(..._args) {
		super(..._args);
		this.state = { error: null };
	}
	static getDerivedStateFromError(error) {
		return { error };
	}
	componentDidCatch(error, info) {
		console.error("[bolt/feed] render failed:", error, info.componentStack);
	}
	render() {
		const { error } = this.state;
		if (!error) return this.props.children;
		return /* @__PURE__ */ (0, import_jsx_runtime.jsx)("pre", {
			style: errStyle,
			children: `${error.name}: ${error.message}\n\n${error.stack ?? ""}`
		});
	}
};
var root = document.getElementById("root");
var showFatal = (label, e) => {
	console.error(`[bolt/feed] ${label}:`, e);
	const detail = e instanceof Error ? `${e.name}: ${e.message}\n\n${e.stack ?? ""}` : String(e);
	root.insertAdjacentHTML("afterbegin", `<pre style="white-space:pre-wrap;color:var(--vscode-errorForeground);font-size:11px"></pre>`);
	root.firstChild.textContent = `${label}\n\n${detail}`;
};
window.addEventListener("error", (e) => showFatal("uncaught error", e.error ?? e.message));
window.addEventListener("unhandledrejection", (e) => showFatal("unhandled rejection", e.reason));
(0, import_client.createRoot)(root).render(/* @__PURE__ */ (0, import_jsx_runtime.jsx)(ErrorBoundary, { children: /* @__PURE__ */ (0, import_jsx_runtime.jsx)(Feed, {}) }));
//#endregion

//# sourceMappingURL=feed.js.map