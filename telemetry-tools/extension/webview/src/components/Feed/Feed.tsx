import { useMemo, useState } from "react";
import { useMessages, post } from "../../utils/api";
import { FilterBar } from "./FilterBar";
import { FeedTable } from "./FeedTable";
import { emptyRange, matches, type Range, type Row } from "../../utils/feed";

const PAGE = 100;
const CAP = 100_000;

export function Feed() {
  const [rows, setRows] = useState<Row[]>([]);
  const [counts, setCounts] = useState<Record<string, number>>({});
  const [filter, setFilter] = useState("");
  const [range, setRange] = useState<Range>(emptyRange);
  const [paused, setPaused] = useState(false);
  const [page, setPage] = useState(0);
  const [live, setLive] = useState(true);

  useMessages((m) => {
    switch (m.type) {
      case "frame":
        setLive(true);
        setRows((prev) => {
          const next = prev.length >= CAP ? prev.slice(prev.length - CAP + 1) : prev.slice();
          next.push(m.frame as unknown as Row);
          return next;
        });
        if (!paused) setPage(0);
        break;
      case "counts":
        setCounts(m.counts);
        break;
      case "load":
        setLive(false);
        setRows((m.records as Row[]).slice().reverse());
        setPage(0);
        break;
      case "reset":
        setRows([]);
        setPage(0);
        break;
    }
  });

  const filtered = useMemo(() => rows.filter((r) => matches(r, filter, range)), [rows, filter, range]);
  const pages = Math.max(1, Math.ceil(filtered.length / PAGE));
  const clamped = Math.min(page, pages - 1);

  const view: Row[] = [];
  for (let i = 0; i < PAGE; i++) {
    const idx = filtered.length - 1 - clamped * PAGE - i;
    if (idx < 0) break;
    view.push(filtered[idx]);
  }

  const types = useMemo(() => {
    const set = new Set(Object.keys(counts));
    for (const r of rows) set.add(r.name);
    return [...set].sort();
  }, [counts, rows]);

  const info = `p ${clamped + 1}/${pages} · ${filtered.length.toLocaleString()}${live ? " live" : ""}`;

  return (
    <>
      <FilterBar
        filter={filter} onFilter={(f) => { setFilter(f); setPage(0); }}
        types={types}
        onRange={(r) => { setRange(r); setPage(0); }}
        paused={paused} onPause={() => { setPaused((p) => !p); setPage(0); }}
        onReset={() => { setRows([]); setPage(0); }}
        onReload={() => post({ type: "reload" })}
        page={clamped} pages={pages} onPage={setPage}
        info={info}
      />
      <FeedTable rows={view} onShowType={(name) => post({ type: "showType", name })} />
    </>
  );
}
