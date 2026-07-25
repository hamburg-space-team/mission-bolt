import { useEffect, useMemo, useRef, useState } from "react";
import { useMessages, post } from "../../utils/api";
import { FilterBar } from "./FilterBar";
import { FeedTable } from "./FeedTable";
import { emptyRange, matches, type Range, type Row } from "../../utils/feed";

const PAGE = 100;
const CAP = 100_000;
// Frames arrive far faster than a re-render is ever useful (hundreds per
// second). One state update per frame re-copies and re-filters the whole
// buffer, which pegs the webview thread; collect them and apply one batch per
// interval instead
const FLUSH_MS = 100;
// While frozen, refresh the buffered-backlog counter at a human rate
const BACKLOG_MS = 250;

export function Feed() {
  const [rows, setRows] = useState<Row[]>([]);
  const [counts, setCounts] = useState<Record<string, number>>({});
  const [filter, setFilter] = useState("");
  const [range, setRange] = useState<Range>(emptyRange);
  const [paused, setPaused] = useState(false);
  const [page, setPage] = useState(0);
  const [live, setLive] = useState(true);
  const [backlog, setBacklog] = useState(0);

  // Frames land here first and are never dropped on the floor; the flush timer
  // moves them into `rows`
  const pending = useRef<Row[]>([]);
  // Stamps Row.uid - the wire has no id we can key rows by (see feed.ts)
  const nextUid = useRef(0);

  // Follow the live tail only while the newest page is showing and the user has
  // not paused. Paging back into history freezes the list exactly like an
  // explicit pause: frames keep buffering, so the rows under the cursor stay
  // put and nothing is lost
  const following = !paused && page === 0;

  useMessages((m) => {
    switch (m.type) {
      case "frame":
        pending.current.push({ ...(m.frame as unknown as Row), uid: nextUid.current++ });
        if (pending.current.length > CAP) {
          pending.current.splice(0, pending.current.length - CAP);
        }
        break;
      case "counts":
        setCounts(m.counts);
        break;
      case "load":
        pending.current = [];
        setLive(false);
        // Records arrive newest-first; `rows` is chronological
        setRows((m.records as Row[]).slice().reverse().map((r) => ({ ...r, uid: nextUid.current++ })));
        setPage(0);
        break;
      case "reset":
        pending.current = [];
        setRows([]);
        setPage(0);
        break;
    }
  });

  // Drain the buffer into the table, but only while following the tail
  useEffect(() => {
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

  // Show how far behind the frozen view has fallen (nothing re-renders while
  // frozen, so the count needs its own tick)
  useEffect(() => {
    if (following) {
      setBacklog(0);
      return;
    }
    const id = setInterval(() => setBacklog(pending.current.length), BACKLOG_MS);
    return () => clearInterval(id);
  }, [following]);

  const filtered = useMemo(() => rows.filter((r) => matches(r, filter, range)), [rows, filter, range]);
  const pages = Math.max(1, Math.ceil(filtered.length / PAGE));
  const clamped = Math.min(page, pages - 1);

  // Newest-first window: `rows` is chronological, so walk back from the end
  const view = useMemo(() => {
    const out: Row[] = [];
    for (let i = 0; i < PAGE; i++) {
      const idx = filtered.length - 1 - clamped * PAGE - i;
      if (idx < 0) break;
      out.push(filtered[idx]);
    }
    return out;
  }, [filtered, clamped]);

  const types = useMemo(() => {
    const set = new Set(Object.keys(counts));
    for (const r of rows) if (r.name) set.add(r.name);
    return [...set].sort();
  }, [counts, rows]);

  const state = following ? (live ? " · live" : "") : backlog > 0 ? ` · frozen +${backlog.toLocaleString()}` : " · frozen";
  const info = `p ${clamped + 1}/${pages} · ${filtered.length.toLocaleString()}${state}`;

  const resume = () => {
    setPaused(false);
    setPage(0);
  };

  return (
    <>
      <FilterBar
        filter={filter} onFilter={(f) => { setFilter(f); setPage(0); }}
        types={types}
        onRange={(r) => { setRange(r); setPage(0); }}
        paused={paused} onPause={() => (paused ? resume() : setPaused(true))}
        onReset={() => { pending.current = []; setRows([]); setPage(0); }}
        onReload={() => post({ type: "reload" })}
        page={clamped} pages={pages} onPage={setPage}
        info={info}
      />
      <FeedTable rows={view} onShowType={(name) => post({ type: "showType", name })} />
    </>
  );
}
