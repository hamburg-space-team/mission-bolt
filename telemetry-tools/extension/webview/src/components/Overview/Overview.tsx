import { useMemo, useState } from "react";
import { useMessages } from "../../utils/api";
import type { Manifest, StatsMsg } from "../../../../src/messages";
import { fmtValue } from "../../utils/format";

type Sample = Record<string, unknown>;
type BySource = Record<string, Record<string, Sample>>;

const SOURCES = ["btc", "exp1", "exp2", "exp3"];

function Tile({ n, label, bad }: { n: string; label: string; bad?: boolean }) {
  return <div className="tile"><div className={`n ${bad ? "bad" : ""}`}>{n}</div><div className="l">{label}</div></div>;
}

function Signal({ on, label }: { on: boolean; label: string }) {
  return <span className="pill">{label} {on ? "✓" : "✗"}</span>;
}

export function Overview() {
  const [live, setLive] = useState(true);
  const [stats, setStats] = useState<StatsMsg>();
  const [manifest, setManifest] = useState<Manifest>();
  const [latest, setLatest] = useState<BySource>({});

  useMessages((m) => {
    if (m.type === "init") { setLive(m.live); setManifest(m.manifest); }
    else if (m.type === "stats") setStats(m.stats);
    else if (m.type === "frame") {
      const f = m.frame;
      const kind = String((f.sample as Sample).kind);
      setLatest((prev) => ({ ...prev, [f.source]: { ...prev[f.source], [kind]: f.sample as Sample } }));
    }
  });

  const total = stats?.total ?? manifest?.total_frames ?? 0;
  const crcFail = stats?.crc_fail ?? manifest?.crc_fail_total ?? 0;
  const crcPct = total ? ((crcFail / total) * 100).toFixed(2) : "0.00";
  const lo = stats?.lo ?? (manifest ? manifest.lo_rtc_s > 0 : false);
  const soe = stats?.soe ?? false;
  const sods = stats?.sods ?? false;

  // bus-wide mission mode as the BTC (the SYNC master) reports it
  const missionMode = latest.btc?.status?.mode as string | undefined;

  const sources = useMemo(() => {
    const seen = new Set([...Object.keys(latest), ...Object.keys(manifest?.packet_counts ?? {}).map((n) => n.split("_")[0])]);
    return SOURCES.filter((s) => seen.has(s) || live);
  }, [latest, manifest, live]);

  return (
    <>
      <h1>{manifest?.mission ?? (live ? "Live" : "Overview")}</h1>
      <div className="row">
        {/* LO, SODS, SOE - the order the REXUS control interface is specified
            in (RXSM manual 3.2), not signal_mask's bit order (1=SOE, 2=SODS). */}
        <Signal on={lo} label="LO" /><Signal on={sods} label="SODS" /><Signal on={soe} label="SOE" />
        {missionMode && <span className="pill">{missionMode.toUpperCase()}</span>}
      </div>
      <div className="tiles" style={{ marginTop: 8 }}>
        <Tile n={total.toLocaleString()} label="frames" />
        <Tile n={`${crcPct}%`} label="CRC fails" bad={crcFail > 0} />
        <Tile n={String(crcFail)} label="crc-fail count" bad={crcFail > 0} />
        <Tile n={live ? "live" : "post-flight"} label="session" />
      </div>

      <h2>Experiments</h2>
      <div className="tiles">
        {sources.map((src) => {
          const env = latest[src]?.env;
          // EXP3's status sample has its own kind
          const status = latest[src]?.status ?? latest[src]?.exp3_status;
          const metrics: [string, string][] = [];
          if (status?.mode != null) metrics.push(["mode", String(status.mode).toUpperCase()]);
          if (env?.temp_c != null) metrics.push(["temp", `${Number(env.temp_c).toFixed(1)} °C`]);
          if (env?.pressure_mbar != null) metrics.push(["pressure", `${Number(env.pressure_mbar).toFixed(1)} mbar`]);
          if (status?.data_ready_fails != null) metrics.push(["dataRdy fails", fmtValue(status.data_ready_fails)]);
          return (
            <div className="tile" key={src}>
              <div className="n">{src.toUpperCase()}</div>
              {metrics.length === 0 && <div className="l">no data yet</div>}
              {metrics.map(([l, v]) => <div className="l" key={l}>{l}: {v}</div>)}
            </div>
          );
        })}
      </div>
    </>
  );
}
