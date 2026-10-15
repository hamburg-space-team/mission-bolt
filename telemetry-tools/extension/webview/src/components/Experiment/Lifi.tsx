import { useEffect, useRef } from "react";
import * as echarts from "echarts/core";
import { BarChart } from "echarts/charts";
import { GridComponent, TooltipComponent } from "echarts/components";
import { CanvasRenderer } from "echarts/renderers";
import { BAUD, BITS, BLOCK, type LifiState, type Round } from "../../utils/lifi";

echarts.use([BarChart, GridComponent, TooltipComponent, CanvasRenderer]);

const hex = (b: number | undefined) => (b === undefined ? "··" : b.toString(16).padStart(2, "0"));
const pct = (e: number, bits: number) => (bits ? (100 * e) / bits : 0);
const isDark = () => document.body.classList.contains("vscode-dark") || document.body.classList.contains("vscode-high-contrast");
const cssVar = (name: string, fallback: string) => getComputedStyle(document.body).getPropertyValue(name).trim() || fallback;

// Sequential (one hue, blue): BER magnitude. Near zero recedes toward the
// surface in both themes, so the dark ramp runs the other way.
const SEQ_LIGHT = ["#cde2fb", "#9ec5f4", "#6da7ec", "#3987e5", "#256abf", "#184f95", "#0d366b"];
const SEQ_DARK = ["#0d366b", "#104281", "#184f95", "#1c5cab", "#2a78d6", "#5598e7", "#86b6ef"];
const BINS = [0, 1, 5, 10, 25, 40]; // % lower edges of steps 1..6; step 0 = exactly 0
const BIN_LABEL = ["0", "≤1", "1–5", "5–10", "10–25", "25–40", ">40"];

function step(berPct: number): number {
  if (berPct === 0) return 0;
  let s = 1;
  for (let i = 1; i < BINS.length; i++) if (berPct > BINS[i]) s = i + 1;
  return s;
}

function Tiles({ r }: { r?: Round }) {
  if (!r) return <div className="muted">waiting for the first round…</div>;
  const ok = r.valid && r.bitErrors === 0;
  const status = !r.valid ? "⊘ no measurement" : ok ? "✓ intact" : `✕ ${r.bitErrors} bit errors`;
  return (
    <div className="tiles">
      <div className="tile"><div className="n">{status}</div><div className="l">round {r.seq}</div></div>
      <div className="tile"><div className="n">{BAUD[r.rateIndex]?.toLocaleString() ?? `#${r.rateIndex}`}</div><div className="l">optical baud</div></div>
      <div className="tile"><div className="n">{r.threshold}</div><div className="l">threshold (DAC)</div></div>
      <div className="tile"><div className="n">{r.valid ? `${pct(r.bitErrors, BITS).toFixed(1)} %` : "–"}</div><div className="l">BER</div></div>
      <div className="tile"><div className="n">{r.rxCount}</div><div className="l">bytes received</div></div>
      <div className="tile"><div className="n">{r.valid ? r.offset : "–"}</div><div className="l">block offset</div></div>
      <div className="tile"><div className="n">{r.valid ? `${(r.latencyUs / 1000).toFixed(2)} ms` : "–"}</div><div className="l">\S → ACK</div></div>
      <div className="tile"><div className="n">0x{r.seed.toString(16).padStart(4, "0")}</div><div className="l">PRBS-15 seed</div></div>
    </div>
  );
}

// 64 bytes across, 8 bits down (MSB on top): one cell per sent bit
function BitMap({ r }: { r?: Round }) {
  if (!r?.expected || !r.errorBits) return <div className="muted">no received block in the latest round</div>;
  const err = new Set(r.errorBits);
  const cells = [];
  for (let bit = 0; bit < 8; bit++) {
    for (let i = 0; i < BLOCK; i++) {
      const pos = i * 8 + bit;
      const got = r.received?.[r.offset + i];
      const state = got === undefined ? "missing" : err.has(pos) ? "error" : "ok";
      cells.push(
        <div
          key={pos}
          className={`lifi-bit ${state}`}
          title={`bit ${pos} (byte ${i}, bit ${7 - bit})\nsent 0x${hex(r.expected[i])} · got 0x${hex(got)}\n${state}`}
        />,
      );
    }
  }
  return (
    <>
      <div className="lifi-bitmap" role="img" aria-label={`${r.errorBits.length} of ${BITS} bits wrong`}>{cells}</div>
      <div className="lifi-axis"><span>byte 0</span><span>byte 32</span><span>byte 63</span></div>
      <div className="lifi-legend">
        <span><i className="lifi-bit ok" /> correct</span>
        <span><i className="lifi-bit error" /> ✕ wrong</span>
        <span><i className="lifi-bit missing" /> not received</span>
        <span className="muted">rows: bit 7 (top) … bit 0 · hover a cell for sent/received</span>
      </div>
    </>
  );
}

// the received bytes, 32 per row; the scored 64-byte window is outlined and
// bytes that differ from the sent block are marked
function RxBytes({ r }: { r?: Round }) {
  if (!r?.received?.length) return null;
  const exp = r.expected ?? [];
  const rows = [];
  for (let row = 0; row < r.received.length; row += 32) {
    rows.push(
      <div className="lifi-hexrow" key={row}>
        <span className="lifi-hexaddr">{row.toString(16).padStart(3, "0")}</span>
        {r.received.slice(row, row + 32).map((b, k) => {
          const at = row + k;
          const inBlock = at >= r.offset && at < r.offset + BLOCK;
          const bad = inBlock && b !== exp[at - r.offset];
          return (
            <span key={at} className={`lifi-hex${inBlock ? " in" : ""}${bad ? " bad" : ""}`}
              title={inBlock ? `rx[${at}] = block byte ${at - r.offset}: sent 0x${hex(exp[at - r.offset])}` : `rx[${at}], outside the block`}>
              {hex(b)}
            </span>
          );
        })}
      </div>,
    );
  }
  return (
    <>
      <div className="lifi-hex-wrap">{rows}</div>
      <div className="lifi-legend">
        <span><i className="lifi-hex-key in" /> scored block (offset {r.offset})</span>
        <span><i className="lifi-hex-key bad" /> ✕ differs from the sent byte</span>
        {r.rxCount > r.received.length && <span className="muted">first {r.received.length} of {r.rxCount} bytes</span>}
      </div>
    </>
  );
}

function Heatmap({ s }: { s: LifiState }) {
  const rates = [...new Set([...s.cells.keys()].map((k) => Number(k.split(":")[0])))].sort((a, b) => a - b);
  const thrs = [...new Set([...s.cells.keys()].map((k) => Number(k.split(":")[1])))].sort((a, b) => b - a);
  if (!rates.length) return null;
  const ramp = isDark() ? SEQ_DARK : SEQ_LIGHT;
  // ink by step: light ramp gets white text from step 4 on, the dark ramp from step 0 to 3
  const ink = (st: number) => ((isDark() ? st <= 3 : st >= 4) ? "#ffffff" : "#0b0b0b");
  return (
    <>
      <table className="lifi-heat">
        <thead>
          <tr><th>baud \ threshold</th>{thrs.map((t) => <th key={t}>{t}</th>)}</tr>
        </thead>
        <tbody>
          {rates.map((ri) => (
            <tr key={ri}>
              <th>{BAUD[ri]?.toLocaleString() ?? ri}</th>
              {thrs.map((t) => {
                const c = s.cells.get(`${ri}:${t}`);
                if (!c || c.rounds === 0) {
                  return <td key={t} className="lifi-heat-empty" title={c ? `${c.invalid} rounds, none valid` : "no round yet"}>
                    {c ? `– · ${c.invalid}✕` : "–"}</td>;
                }
                const p = pct(c.errBits, c.bits);
                const st = step(p);
                return (
                  <td key={t} style={{ background: ramp[st], color: ink(st) }}
                    title={`${BAUD[ri]} Bd, threshold ${t}\n${c.errBits} of ${c.bits} bits wrong over ${c.rounds} rounds${c.invalid ? `\n${c.invalid} rounds without measurement` : ""}`}>
                    <b>{p.toFixed(p > 0 && p < 1 ? 2 : 1)} %</b>
                    <span className="lifi-heat-n">n={c.rounds}{c.invalid ? ` · ${c.invalid}⊘` : ""}</span>
                  </td>
                );
              })}
            </tr>
          ))}
        </tbody>
      </table>
      <div className="lifi-legend">
        <span className="muted">BER %</span>
        {BIN_LABEL.map((l, i) => <span key={l}><i className="lifi-swatch" style={{ background: ramp[i] }} /> {l}</span>)}
        <span className="muted">⊘ rounds without a measurement</span>
      </div>
    </>
  );
}

function PositionHistogram({ s, rev }: { s: LifiState; rev: number }) {
  const host = useRef<HTMLDivElement>(null);
  const chart = useRef<echarts.ECharts | null>(null);

  useEffect(() => {
    if (!host.current) return;
    chart.current = echarts.init(host.current, null, { renderer: "canvas" });
    const onResize = () => chart.current?.resize();
    window.addEventListener("resize", onResize);
    return () => {
      window.removeEventListener("resize", onResize);
      chart.current?.dispose();
      chart.current = null;
    };
  }, []);

  useEffect(() => {
    const fg = cssVar("--vscode-foreground", "#ccc");
    const border = cssVar("--vscode-widget-border", "#8884");
    const bar = isDark() ? "#3987e5" : "#2a78d6";
    chart.current?.setOption({
      backgroundColor: "transparent",
      textStyle: { color: fg },
      animation: false,
      grid: { left: 48, right: 12, top: 12, bottom: 36 },
      tooltip: {
        trigger: "axis",
        axisPointer: { type: "shadow" },
        formatter: (ps: { dataIndex: number; value: number }[]) => {
          const p = ps[0];
          return `bit ${p.dataIndex} (byte ${p.dataIndex >> 3}, bit ${7 - (p.dataIndex & 7)})<br/>${p.value} errors in ${s.histRounds} rounds`;
        },
      },
      xAxis: {
        type: "category",
        data: Array.from({ length: BITS }, (_, i) => i),
        name: "bit position in the block",
        nameLocation: "middle",
        nameGap: 22,
        axisLabel: { color: fg, interval: 63 },
        axisLine: { lineStyle: { color: border } },
        axisTick: { show: false },
      },
      yAxis: {
        type: "value",
        minInterval: 1,
        axisLabel: { color: fg },
        splitLine: { lineStyle: { color: border, opacity: 0.4 } },
      },
      series: [{ type: "bar", data: s.hist, itemStyle: { color: bar, borderRadius: [2, 2, 0, 0] }, barCategoryGap: "20%" }],
    });
  }, [rev]);

  return <div ref={host} style={{ width: "100%", height: 200 }} />;
}

export function LifiPanel({ state, rev }: { state: LifiState; rev: number }) {
  const r = state.latest;
  const errRounds = state.hist.some((v) => v > 0);
  return (
    <>
      <h2>LiFi · latest round</h2>
      <Tiles r={r} />
      <h2>Bit map · latest round</h2>
      <BitMap r={r} />
      <h2>Received bytes · latest round</h2>
      {r?.received?.length ? <RxBytes r={r} /> : <div className="muted">nothing received in the latest round</div>}
      <h2>BER by baud and threshold · since the view opened</h2>
      <Heatmap s={state} />
      <h2>Error positions · {state.histRounds} valid rounds</h2>
      {errRounds ? <PositionHistogram s={state} rev={rev} /> : <div className="muted">no bit error yet</div>}
    </>
  );
}
