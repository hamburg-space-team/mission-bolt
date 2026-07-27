import { useCallback, useEffect, useState } from "react";
import {
  getNoInit,
  getRun,
  getRuns,
  getStatus,
  getSteps,
  resetWindow,
  runSelfTest,
  NODES,
  type NoInit,
  type RunDetail,
  type RunSummary,
  type Status,
} from "./api";

/** Poll `fn` every `ms`, keeping the last good value when a request fails. */
function usePoll<T>(fn: () => Promise<T | null>, ms: number, deps: unknown[] = []) {
  const [value, setValue] = useState<T | null>(null);
  const stable = useCallback(fn, deps); // eslint-disable-line react-hooks/exhaustive-deps
  useEffect(() => {
    let alive = true;
    const tick = async () => {
      const v = await stable();
      if (alive && v !== null) setValue(v);
    };
    void tick();
    const h = setInterval(tick, ms);
    return () => {
      alive = false;
      clearInterval(h);
    };
  }, [stable, ms]);
  return value;
}

const clock = (iso: string | null) => {
  if (!iso) return "";
  const d = new Date(iso);
  const p = (n: number) => String(n).padStart(2, "0");
  return `${p(d.getDate())}.${p(d.getMonth() + 1)} ${p(d.getHours())}:${p(d.getMinutes())}`;
};

const ago = (iso: string | null) => {
  if (!iso) return "";
  const s = Math.max(0, (Date.now() - new Date(iso).getTime()) / 1000);
  if (s < 60) return `${Math.round(s)}s ago`;
  if (s < 3600) return `${Math.round(s / 60)}m ago`;
  if (s < 86400) return `${Math.round(s / 3600)}h ago`;
  return `${Math.round(s / 86400)}d ago`;
};

function Header({ status }: { status: Status | null }) {
  const probeBoard = status?.probe?.target?.board ?? null;
  const mode = NODES.map((n) => status?.boards?.[n]?.mode).find((m) => m != null) ?? null;
  return (
    <header>
      <div className="boards">
        {NODES.map((n) => {
          const sending = !!status?.boards?.[n]?.sending;
          const boot = status?.boards?.[n]?.boot;
          const probe = probeBoard === n;
          // green: sending and the probe sits on it. yellow: sending, no
          // probe. dark: nothing arriving at all
          const cls = !sending ? "dead" : probe ? "ok" : "warn";
          return (
            <div className={`board ${cls}`} key={n}>
              <div className="name">{n.toUpperCase()}</div>
              <div className="meta">
                <i className="dot" />
                {/* from the BOOT packet, no probe needed */}
                {boot?.reason === "watchdog" ? (
                  <span className="wdg">WDG</span>
                ) : sending ? (
                  "live"
                ) : (
                  "dark"
                )}
              </div>
              {probe && <span className="dbg">DBG</span>}
            </div>
          );
        })}
      </div>
      <div className="signals">
        <div className={`sig ${status?.signals?.lo ? "on" : ""}`}>LO</div>
        <div className={`sig ${status?.signals?.sods ? "on" : ""}`}>SODS</div>
        <div className={`sig ${status?.signals?.soe ? "on" : ""}`}>SOE</div>
        <div className={`mode ${mode ?? ""}`}>{(mode ?? "no sync").toUpperCase()}</div>
      </div>
    </header>
  );
}

function Side({ status, noinit }: { status: Status | null; noinit: NoInit | null }) {
  const env = NODES.map((n) => [n, status?.boards?.[n]?.env] as const).filter(
    ([, e]) => e != null,
  );
  const link = status?.link;
  const d = noinit?.decoded;
  const win = link?.window;
  const clean = (win?.crc_fails ?? 0) === 0;
  // share of the link budget in use
  const load = win && link?.budget_bps ? (win.bytes_per_s / link.budget_bps) * 100 : 0;

  return (
    <div className="side">
      <div className="card">
        <span className="label">Environment</span>
        {env.length === 0 && (
          <div className="kv">
            <span>no data</span>
          </div>
        )}
        {env.map(([n, e]) => (
          <div className="kv" key={n}>
            <span>{n}</span>
            <span className="v">
              {e!.temp_c?.toFixed(1)}° {e!.pressure_mbar?.toFixed(0)}
            </span>
          </div>
        ))}
      </div>

      <div className="card">
        <div className="cardhead">
          <span className="label">Link · {win?.seconds ?? 0}s</span>
          <button className="mini" onClick={() => void resetWindow()} title="reset window">
            ⟲
          </button>
        </div>
        <div className="kv">
          <span>packets</span>
          <span className="v">
            {win?.frames ?? "—"}
            <span className="unit"> {win ? `${win.frames_per_s}/s` : ""}</span>
          </span>
        </div>
        <div className="kv">
          <span>downlink</span>
          <span className={`v ${load > 90 ? "bad" : ""}`}>
            {win ? (win.bytes_per_s / 1000).toFixed(2) : "—"}
            <span className="unit"> kB/s</span>
          </span>
        </div>
        {/* at 100 % the link drops packets */}
        <div className="kv">
          <span>of {link ? (link.budget_bps / 1000).toFixed(1) : "—"} kB/s budget</span>
          <span className={`v ${load > 90 ? "bad" : "good"}`}>{load.toFixed(0)}%</span>
        </div>
        <div className={`bar ${load > 90 ? "bad" : ""}`}>
          <i style={{ width: `${Math.min(100, load)}%` }} />
        </div>
        <div className="kv">
          <span>crc fails</span>
          <span className={`v ${clean ? "good" : "bad"}`}>{win?.crc_fails ?? "—"}</span>
        </div>
        <div className="kv">
          <span>total</span>
          <span className="v">
            {link?.total_frames ?? "—"}
            <span className="unit"> pkt · {link?.total_crc_fails ?? "—"} bad</span>
          </span>
        </div>
      </div>

      <div className="card grow">
        <div className="cardhead">
          <span className="label">.noinit</span>
          <span className="unit">{noinit?.address ?? ""}</span>
        </div>
        {d ? (
          <>
            <div className="kv">
              <span>tick</span>
              <span className="v">{d.tick}</span>
            </div>
            <div className="kv">
              <span>reboots</span>
              <span className={`v ${d.reboot_count > 0 ? "bad" : "good"}`}>
                {d.reboot_count}
              </span>
            </div>
            <div className="kv">
              <span>reason</span>
              <span className={`v ${d.reason_name === "watchdog" ? "bad" : ""}`}>
                {d.reason_name}
              </span>
            </div>
            <div className="kv">
              <span>mode</span>
              <span className="v">{d.mode}</span>
            </div>
            <div className="kv">
              <span>LO latch</span>
              <span className={`v ${d.lo_latched ? "bad" : ""}`}>
                {d.lo_latched ? "set" : "clear"}
              </span>
            </div>
            <div className="kv">
              <span>LO rtc</span>
              <span className="v">{d.lo_rtc_s || "—"}</span>
            </div>
          </>
        ) : (
          <div className="kv">
            <span>
              {!noinit
                ? "unreachable"
                : noinit.probe_reachable === false
                  ? "no probe"
                  : "cold - no state"}
            </span>
          </div>
        )}
      </div>
    </div>
  );
}

function RunList({
  runs,
  activeRun,
  onOpen,
}: {
  runs: RunSummary[];
  activeRun: number | null;
  onOpen: (id: number) => void;
}) {
  return (
    <div className="runs scroll">
      <div className="title">
        <span className="label">Self-tests</span>
        <span className="label">{runs.length}</span>
      </div>
      {runs.length === 0 && <div className="empty">no runs yet</div>}
      {runs.map((r) => {
        const running = r.id === activeRun;
        const cls = running ? "active" : r.fail > 0 ? "fail" : "pass";
        return (
          <button className={`run ${cls}`} key={r.id} onClick={() => onOpen(r.id)}>
            <div className="top">
              <span className="id">#{r.id}</span>
              <span className="verdict">
                {running ? "RUNNING" : r.fail > 0 ? "FAIL" : "PASS"}
              </span>
            </div>
            <div className="top">
              <span className="when">{clock(r.started_utc)}</span>
              <span className="counts">
                <span className="p">{r.pass}✓</span>
                {r.fail > 0 && <span className="f"> {r.fail}✗</span>}
              </span>
            </div>
            <div className="top">
              <span className="when">{ago(r.started_utc)}</span>
            </div>
          </button>
        );
      })}
    </div>
  );
}

function Detail({
  id,
  steps,
  onBack,
}: {
  id: number;
  steps: Record<string, string[]> | null;
  onBack: () => void;
}) {
  const detail = usePoll<RunDetail>(() => getRun(id), 2000, [id]);
  const label = (r: string) =>
    r === "no_report" ? "LOST" : r === "node_dark" ? "DARK" : r.toUpperCase().slice(0, 4);

  return (
    <div className="detail">
      <div className="head">
        <button className="back" onClick={onBack}>
          ‹ BACK
        </button>
        <span className="label">RUN #{id}</span>
        <span className="counts" style={{ marginLeft: "auto" }}>
          {detail && (
            <>
              <span className="p" style={{ color: "var(--green)" }}>
                {detail.pass}✓
              </span>
              {detail.fail > 0 && (
                <span style={{ color: "var(--red)" }}> {detail.fail}✗</span>
              )}
            </>
          )}
        </span>
      </div>
      <div className="scroll" style={{ flex: "1 1 auto", minHeight: 0 }}>
        {!detail && <div className="empty">loading…</div>}
        {detail &&
          NODES.map((node) => {
            const rows = detail.nodes[node] ?? [];
            // the contract decides which steps exist; a run only says
            // what happened to them
            const names = steps?.[node] ?? [];
            const total = Math.max(names.length, rows.length);
            if (total === 0) return null;
            const passed = rows.filter((r) => r.result === "pass").length;
            return (
              <div key={node}>
                <div className="node-head">
                  <span>{node.toUpperCase()}</span>
                  <span className="frac">
                    {passed}/{total}
                  </span>
                </div>
                {Array.from({ length: total }, (_, i) => {
                  const row = rows.find((r) => r.test_id === i);
                  const name = names[i] ?? row?.name ?? `test ${i}`;
                  const result = row?.result ?? "waiting";
                  return (
                    <div className="step" key={i}>
                      <span className="idx">{i}</span>
                      <span className="nm">{name}</span>
                      <span className="val">
                        {row?.data != null ? `0x${row.data.toString(16)}` : ""}
                        {row?.duration_ms != null ? ` ${row.duration_ms.toFixed(0)}ms` : ""}
                      </span>
                      <span className={`rs ${result}`}>{label(result)}</span>
                    </div>
                  );
                })}
              </div>
            );
          })}
      </div>
    </div>
  );
}

export default function App() {
  const [openRun, setOpenRun] = useState<number | null>(null);
  const [armed, setArmed] = useState(false);
  const [sending, setSending] = useState(false);

  const status = usePoll<Status>(getStatus, 1000);
  const runs = usePoll(getRuns, 2000);
  const noinit = usePoll<NoInit>(getNoInit, 5000);
  const steps = usePoll<Record<string, string[]>>(getSteps, 60000);

  // flagged dangerous in the contract, so a stray touch must not fire it
  useEffect(() => {
    if (!armed) return;
    const h = setTimeout(() => setArmed(false), 4000);
    return () => clearTimeout(h);
  }, [armed]);

  const press = async () => {
    if (!armed) {
      setArmed(true);
      return;
    }
    setArmed(false);
    setSending(true);
    await runSelfTest();
    setTimeout(() => setSending(false), 1500);
  };

  const iface = status?.station?.interfaces?.[0];

  return (
    <>
      <Header status={status} />
      <main>
        {openRun === null ? (
          <>
            <RunList
              runs={runs?.runs ?? []}
              activeRun={runs?.active_run ?? null}
              onOpen={setOpenRun}
            />
            <Side status={status} noinit={noinit} />
          </>
        ) : (
          <Detail id={openRun} steps={steps} onBack={() => setOpenRun(null)} />
        )}
      </main>
      <footer>
        <button
          className={`run-test ${armed ? "arm" : ""}`}
          onClick={press}
          disabled={sending}
        >
          {sending ? "SENT" : armed ? "CONFIRM?" : "FULL SYSTEM TEST"}
        </button>
        <div className="ip">
          <div className="a">{status?.station?.ip ?? "…"}</div>
          <div className="b">{iface ? `${iface.interface} · ${iface.source}` : ""}</div>
        </div>
      </footer>
    </>
  );
}
