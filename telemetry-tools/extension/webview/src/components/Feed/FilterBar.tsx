import { useState } from "react";
import type { Range } from "../../utils/feed";

interface Props {
  filter: string;
  onFilter: (f: string) => void;
  types: string[];
  onRange: (r: Range) => void;
  paused: boolean;
  onPause: () => void;
  onReset: () => void;
  onReload: () => void;
  page: number;
  pages: number;
  onPage: (p: number) => void;
  info: string;
}

const num = (v: string): number | null => (v === "" ? null : Number(v));

export function FilterBar(p: Props) {
  const [t0, setT0] = useState("");
  const [t1, setT1] = useState("");
  const [k0, setK0] = useState("");
  const [k1, setK1] = useState("");

  const apply = () => p.onRange({ tMin: num(t0), tMax: num(t1), kMin: num(k0), kMax: num(k1) });
  const clear = () => {
    setT0(""); setT1(""); setK0(""); setK1("");
    p.onRange({ tMin: null, tMax: null, kMin: null, kMax: null });
  };
  const onKey = (e: React.KeyboardEvent) => e.key === "Enter" && apply();

  return (
    <div className="bar">
      <select value={p.filter} onChange={(e) => p.onFilter(e.target.value)}>
        <option value="">All</option>
        <option value="__fault">⚠ Faults</option>
        <option value="__crc">⚡ CRC fails</option>
        <option value="__gap">▸ Gaps</option>
        <option value="__suspect">⚠ Invalid (mask)</option>
        <option disabled>── source ──</option>
        <option value="__src:btc">BTC</option>
        <option value="__src:exp1">EXP1</option>
        <option value="__src:exp2">EXP2</option>
        <option value="__src:exp3">EXP3</option>
        <option value="__src:system">System</option>
        {p.types.length > 0 && <option disabled>── type ──</option>}
        {p.types.map((t) => <option key={t} value={t}>{t}</option>)}
      </select>
      <input className="num" placeholder="t≥ µs" value={t0} onChange={(e) => setT0(e.target.value)} onKeyDown={onKey} />
      <input className="num" placeholder="t≤ µs" value={t1} onChange={(e) => setT1(e.target.value)} onKeyDown={onKey} />
      <input className="num" placeholder="tick≥" value={k0} onChange={(e) => setK0(e.target.value)} onKeyDown={onKey} />
      <input className="num" placeholder="tick≤" value={k1} onChange={(e) => setK1(e.target.value)} onKeyDown={onKey} />
      <button className="secondary" onClick={apply}>Apply</button>
      <button className="secondary" onClick={clear}>✕</button>
      <button className={p.paused ? "on" : "secondary"} onClick={p.onPause}>{p.paused ? "▶ Resume" : "⏸ Pause"}</button>
      <button className="secondary" onClick={p.onReset}>Reset</button>
      <button className="secondary" onClick={p.onReload}>Reload</button>
      <span style={{ flex: 1 }} />
      <button className="secondary" disabled={p.page === 0} onClick={() => p.onPage(0)}>⏮</button>
      <button className="secondary" disabled={p.page === 0} onClick={() => p.onPage(p.page - 1)}>◀</button>
      <span className="muted">{p.info}</span>
      <button className="secondary" disabled={p.page >= p.pages - 1} onClick={() => p.onPage(p.page + 1)}>▶</button>
      <button className="secondary" disabled={p.page >= p.pages - 1} onClick={() => p.onPage(p.pages - 1)}>⏭</button>
    </div>
  );
}
