import { a as require_client, i as useMessages, n as post, o as require_react, t as require_jsx_runtime } from "./chunk-19Q_QZ2X.js";
//#region src/protocol.gen.ts
var SELF_TEST_STEPS = {
	btc: [
		"TMP117 WHO_AM_I",
		"TMP117 read",
		"MS5611 PROM CRC",
		"ICM-42686 WHO_AM_I",
		"ICM-42686 read",
		"SD mounted"
	],
	exp1: [
		"TMP117 WHO_AM_I",
		"TMP117 read",
		"MS5611 PROM CRC",
		"AS7265x HW version",
		"LP5810C responds",
		"LP5810D responds",
		"Spectrum dark",
		"Spectrum red",
		"Spectrum green",
		"Spectrum blue",
		"Spectrum white",
		"Spectrum IR 940nm",
		"Spectrum UV 400nm",
		"SD mounted"
	],
	exp2: [
		"TMP117 WHO_AM_I",
		"TMP117 read",
		"MS5611 PROM CRC",
		"SD mounted"
	],
	exp3: [
		"TMP117 WHO_AM_I",
		"TMP117 read",
		"MS5611 PROM CRC",
		"ICM-42686 WHO_AM_I",
		"ICM-42686 read",
		"SD mounted"
	]
};
//#endregion
//#region webview/src/components/SelfTest/SelfTest.tsx
var import_client = require_client();
var import_react = require_react();
var import_jsx_runtime = require_jsx_runtime();
var NODES = [
	"btc",
	"exp1",
	"exp2",
	"exp3"
];
var STEPS = SELF_TEST_STEPS;
function groundVerdict(node, testId, data) {
	return null;
}
var freshRun = (node) => ({
	rows: STEPS[node].map(() => ({ state: "waiting" })),
	done: false,
	dark: false
});
var freshAll = () => Object.fromEntries(NODES.map((n) => [n, freshRun(n)]));
var COLORS = {
	waiting: "var(--vscode-testing-iconQueued)",
	pass: "var(--vscode-testing-iconPassed)",
	fail: "var(--vscode-testing-iconErrored)",
	skip: "var(--vscode-testing-iconSkipped)",
	"no report": "var(--vscode-editorWarning-foreground)",
	"node dark": "var(--vscode-editorWarning-foreground)"
};
function Chip({ state }) {
	return /* @__PURE__ */ (0, import_jsx_runtime.jsx)("span", {
		className: "pill",
		style: {
			color: COLORS[state],
			fontWeight: 600
		},
		children: state.toUpperCase()
	});
}
function SelfTest() {
	const [runs, setRuns] = (0, import_react.useState)(freshAll);
	const [runSeen, setRunSeen] = (0, import_react.useState)(false);
	useMessages((m) => {
		if (m.type !== "frame") return;
		const f = m.frame;
		const s = f.sample;
		if (s.kind === "cmd_ack" && s.command === "full_system_test") {
			setRuns(freshAll());
			setRunSeen(true);
			return;
		}
		if (s.kind === "test") {
			const node = String(s.node);
			if (!(node in STEPS)) return;
			const id = Number(s.test_id);
			setRuns((prev) => {
				const run = {
					...prev[node],
					rows: [...prev[node].rows]
				};
				const dur = run.lastTs != null ? (f.timestamp_us - run.lastTs >>> 0) / 1e3 : void 0;
				run.lastTs = f.timestamp_us;
				if (id < run.rows.length) {
					const name = String(s.result_name);
					const state = name === "pass" ? "pass" : name === "fail" ? "fail" : "skip";
					run.rows[id] = {
						state,
						data: Number(s.data),
						durationMs: dur
					};
				}
				for (let i = 0; i < Math.min(id, run.rows.length); i++) if (run.rows[i].state === "waiting") run.rows[i] = { state: "no report" };
				if (s.last === true) {
					run.done = true;
					for (const [i, r] of run.rows.entries()) if (r.state === "waiting") run.rows[i] = { state: "no report" };
				}
				return {
					...prev,
					[node]: run
				};
			});
			setRunSeen(true);
			return;
		}
		if (s.kind === "gap" && s.reason === "self_test_skipped") {
			const node = String(s.node);
			if (!(node in STEPS)) return;
			setRuns((prev) => {
				const run = {
					...prev[node],
					rows: [...prev[node].rows],
					dark: true,
					done: true
				};
				for (const [i, r] of run.rows.entries()) if (r.state === "waiting") run.rows[i] = { state: "node dark" };
				return {
					...prev,
					[node]: run
				};
			});
		}
	});
	const runNow = () => {
		setRuns(freshAll());
		setRunSeen(true);
		post({ type: "runSelfTest" });
	};
	return /* @__PURE__ */ (0, import_jsx_runtime.jsxs)(import_jsx_runtime.Fragment, { children: [
		/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("h1", { children: [
			"Self-Test",
			" ",
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("button", {
				className: "secondary",
				onClick: runNow,
				children: "Run Full System Test"
			})
		] }),
		!runSeen && /* @__PURE__ */ (0, import_jsx_runtime.jsx)("div", {
			className: "muted",
			children: "No run yet - start one, or wait for a FULL_SYSTEM_TEST ack."
		}),
		NODES.map((node) => {
			const run = runs[node];
			const pass = run.rows.filter((r) => r.state === "pass").length;
			const fail = run.rows.filter((r) => r.state === "fail").length;
			return /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
				style: { marginTop: 14 },
				children: [/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("h2", { children: [
					node.toUpperCase(),
					" ",
					/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("span", {
						className: "pill",
						children: [
							pass,
							"/",
							run.rows.length,
							" pass"
						]
					}),
					" ",
					fail > 0 && /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("span", {
						className: "pill",
						style: { color: COLORS.fail },
						children: [fail, " fail"]
					}),
					" ",
					run.dark && /* @__PURE__ */ (0, import_jsx_runtime.jsx)("span", {
						className: "pill",
						style: { color: COLORS["node dark"] },
						children: "skipped - node dark"
					})
				] }), /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("table", { children: [/* @__PURE__ */ (0, import_jsx_runtime.jsx)("thead", { children: /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("tr", { children: [
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("th", { children: "#" }),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("th", { children: "test" }),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("th", { children: "board" }),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("th", { children: "value" }),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("th", { children: "value check" }),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("th", { children: "duration" })
				] }) }), /* @__PURE__ */ (0, import_jsx_runtime.jsx)("tbody", { children: run.rows.map((r, i) => {
					const gv = r.data != null ? groundVerdict(node, i, r.data) : null;
					return /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("tr", { children: [
						/* @__PURE__ */ (0, import_jsx_runtime.jsx)("td", { children: i }),
						/* @__PURE__ */ (0, import_jsx_runtime.jsx)("td", { children: STEPS[node][i] }),
						/* @__PURE__ */ (0, import_jsx_runtime.jsx)("td", { children: /* @__PURE__ */ (0, import_jsx_runtime.jsx)(Chip, { state: r.state }) }),
						/* @__PURE__ */ (0, import_jsx_runtime.jsx)("td", { children: r.data != null ? `${r.data} (0x${r.data.toString(16)})` : "–" }),
						/* @__PURE__ */ (0, import_jsx_runtime.jsx)("td", { children: gv ? /* @__PURE__ */ (0, import_jsx_runtime.jsx)(Chip, { state: gv }) : /* @__PURE__ */ (0, import_jsx_runtime.jsx)("span", {
							className: "muted",
							children: "–"
						}) }),
						/* @__PURE__ */ (0, import_jsx_runtime.jsx)("td", { children: r.durationMs != null ? `${r.durationMs.toFixed(1)} ms` : "–" })
					] }, i);
				}) })] })]
			}, node);
		})
	] });
}
//#endregion
//#region webview/src/entries/selftest.tsx
(0, import_client.createRoot)(document.getElementById("root")).render(/* @__PURE__ */ (0, import_jsx_runtime.jsx)(SelfTest, {}));
//#endregion

//# sourceMappingURL=selftest.js.map