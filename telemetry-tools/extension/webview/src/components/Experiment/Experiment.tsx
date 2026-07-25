import { useEffect, useMemo, useRef, useState } from "react";
import { useMessages, saveState } from "../../utils/api";
import { Spectrometer } from "./Spectrometer";
import { TimeSeriesChart, type Series } from "./TimeSeriesChart";
import { RocketAttitude } from "./RocketAttitude";

type Sample = Record<string, unknown>;
type Pt = [number, number];
const GAINS = ["1x", "3.7x", "16x", "64x"];
const XYZ = ["#e06c75", "#98c379", "#61afef"];
const CAP = 8000; // per-series live buffer cap
const TICK_S = 0.04; // 25 Hz tick period
const SPEEDS = [1, 2, 4, 10, 25];

const num = (v: unknown): number | null => (v == null ? null : Number(v));
const arr3 = (v: unknown): number[] => (Array.isArray(v) ? v.map(Number) : [0, 0, 0]);
const baro = (p: number, p0: number) => 44330 * (1 - Math.pow(p / p0, 0.190_284));

function emptyBuf() {
  return { accel: [[], [], []] as Pt[][], gyro: [[], [], []] as Pt[][], temp: [] as Pt[], press: [] as Pt[], ber: [] as Pt[] };
}
type Buf = ReturnType<typeof emptyBuf>;

function push(a: Pt[], p: Pt, cap: boolean) {
  a.push(p);
  if (cap && a.length > CAP) a.shift();
}

// Index of the last point at time <= t (arrays are time-sorted).
function idxAt(xs: Pt[], t: number): number {
  let lo = 0, hi = xs.length - 1, idx = 0;
  while (lo <= hi) {
    const m = (lo + hi) >> 1;
    if (xs[m][0] <= t) { idx = m; lo = m + 1; } else hi = m - 1;
  }
  return idx;
}

function timeRange(b: Buf): [number, number] {
  const arrs = [b.accel[0], b.temp, b.press, b.ber].filter((a) => a.length);
  if (!arrs.length) return [0, 0];
  return [Math.min(...arrs.map((a) => a[0][0])), Math.max(...arrs.map((a) => a[a.length - 1][0]))];
}

export function Experiment() {
  const [source, setSource] = useState("");
  const [rev, setRev] = useState(0); // data changed
  const [, setPlayRev] = useState(0); // playback frame
  const [mode, setMode] = useState<"live" | "post">("live");
  const [playing, setPlaying] = useState(false);
  const [speed, setSpeed] = useState(1);
  const [spec, setSpec] = useState<Sample>();
  const buf = useRef<Buf>(emptyBuf());
  const lastRender = useRef(0);
  const lastImu = useRef<{ accel: number[]; gyro: number[] } | null>(null);
  const ground = useRef<number | null>(null);
  const lastP = useRef<number | null>(null);
  const playT = useRef(0);
  const prevTick = useRef<number | null>(null);
  const tickBase = useRef(0);

  const bump = (force: boolean) => {
    const now = Date.now();
    if (force || now - lastRender.current > 120) {
      lastRender.current = now;
      setRev((r) => r + 1);
    }
  };

  const clearBuffers = () => {
    buf.current = emptyBuf();
    lastImu.current = null;
    ground.current = null;
    lastP.current = null;
    prevTick.current = null;
    tickBase.current = 0;
  };

  // X axis in tick seconds - tick is the timeline. timestamp_us wraps every
  // ~54 s (ICD-006) and would fold the charts back onto themselves.
  const xOf = (tick: number): number => {
    const p = prevTick.current;
    if (p != null && tick < p) {
      if (p - tick > 32768) {
        tickBase.current += 65536; // uint16 wrap on long bench runs
      } else if (p - tick > 250) {
        clearBuffers(); // tick restarted (LO): a new timeline, not a wrap
      }
    }
    prevTick.current = tick;
    return (tickBase.current + tick) * TICK_S;
  };

  const addSample = (name: string, s: Sample, tick: number, suspect: boolean, cap: boolean) => {
    if (suspect && s.kind !== "status") return; // plot only real values
    const t = xOf(tick);
    if (s.kind === "imu") {
      const a = arr3(s.accel_ms2), g = arr3(s.gyro_dps);
      lastImu.current = { accel: a, gyro: g };
      for (let i = 0; i < 3; i++) {
        push(buf.current.accel[i], [t, a[i]], cap);
        push(buf.current.gyro[i], [t, g[i]], cap);
      }
    } else if (s.kind === "env") {
      const tc = num(s.temp_c), pr = num(s.pressure_mbar);
      if (tc != null) push(buf.current.temp, [t, tc], cap);
      if (pr != null) {
        if (ground.current == null) ground.current = pr;
        lastP.current = pr;
        push(buf.current.press, [t, pr], cap);
      }
    } else if (s.kind === "ber") {
      push(buf.current.ber, [t, Number(s.ber) * 100], cap);
    } else if (name.endsWith("spectrum")) setSpec(s);
  };

  const reset = () => {
    clearBuffers();
    setSpec(undefined);
    setMode("live");
    setPlaying(false);
  };

  useMessages((m) => {
    if (m.type === "experiment") { setSource(m.source); saveState({ source: m.source }); }
    else if (m.type === "reset") { reset(); bump(true); }
    else if (m.type === "frame") {
      setMode("live");
      addSample(m.frame.name, m.frame.sample as Sample, m.frame.tick, m.frame.suspect, true);
      bump(false);
    } else if (m.type === "load") {
      reset();
      for (const r of m.records) addSample(String(r.name ?? ""), r.sample, r.tick, Boolean(r.suspect), false);
      playT.current = timeRange(buf.current)[0];
      setMode("post");
      bump(true);
    }
  });

  // Post-flight playback loop: advance playT and re-render at ~30 Hz.
  useEffect(() => {
    if (mode !== "post" || !playing) return;
    const [t0, t1] = timeRange(buf.current);
    if (playT.current >= t1) playT.current = t0;
    let raf = 0, last = 0, lastRev = 0;
    const loop = (t: number) => {
      const dt = last ? (t - last) / 1000 : 0;
      last = t;
      playT.current = Math.min(t1, playT.current + dt * speed);
      if (t - lastRev > 33) { lastRev = t; setPlayRev((r) => r + 1); }
      if (playT.current >= t1) { setPlaying(false); return; }
      raf = requestAnimationFrame(loop);
    };
    raf = requestAnimationFrame(loop);
    return () => cancelAnimationFrame(raf);
  }, [mode, playing, speed]);

  const b = buf.current;
  const xyzSeries = (data: Pt[][]): Series[] => ["x", "y", "z"].map((n, i) => ({ name: n, color: XYZ[i], data: data[i] }));
  // Series only change when data changes (rev), NOT on every playback frame,
  // so playback stays cheap (charts only move the cursor markLine).
  const s = useMemo(
    () => ({
      accel: xyzSeries(b.accel),
      gyro: xyzSeries(b.gyro),
      ber: [{ name: "BER", color: "#e5c07b", data: b.ber }] as Series[],
      temp: [{ name: "temp", color: "#e5c07b", data: b.temp }] as Series[],
      press: [{ name: "pressure", color: "#61afef", data: b.press }] as Series[],
    }),
    [rev],
  );

  const hasImu = b.accel[0].length > 0;
  const hasEnv = b.temp.length > 0 || b.press.length > 0;
  const [t0, t1] = timeRange(b);
  const cursor = mode === "post" ? playT.current : null;

  // Rocket state: from the playback cursor in post-flight, else the latest live sample.
  let rAccel = lastImu.current?.accel;
  let rGyro = lastImu.current?.gyro;
  let pNow = lastP.current;
  if (mode === "post") {
    if (hasImu) {
      const i = idxAt(b.accel[0], playT.current);
      rAccel = [b.accel[0][i][1], b.accel[1][i][1], b.accel[2][i][1]];
      rGyro = [b.gyro[0][i][1], b.gyro[1][i][1], b.gyro[2][i][1]];
    }
    pNow = b.press.length ? b.press[idxAt(b.press, playT.current)][1] : null;
  }
  const altM = ground.current != null && pNow != null ? baro(pNow, ground.current) : null;

  return (
    <>
      <h1>{source.toUpperCase() || "Experiment"}</h1>

      {mode === "post" && t1 > t0 && (
        <div className="bar">
          <button onClick={() => setPlaying((p) => !p)}>{playing ? "⏸ Pause" : "▶ Play"}</button>
          <input
            type="range"
            min={t0}
            max={t1}
            step={(t1 - t0) / 1000 || 0.01}
            value={playT.current}
            onChange={(e) => { playT.current = Number(e.target.value); setPlaying(false); setPlayRev((r) => r + 1); }}
            style={{ flex: 1 }}
          />
          <span className="muted">{(playT.current - t0).toFixed(1)} / {(t1 - t0).toFixed(1)} s</span>
          <select value={speed} onChange={(e) => setSpeed(Number(e.target.value))}>
            {SPEEDS.map((v) => <option key={v} value={v}>{v}×</option>)}
          </select>
        </div>
      )}

      {source === "exp1" && (
        <>
          <h2>Spectrometer · current cycle</h2>
          <Spectrometer channels={spec?.channels as number[]} valid={spec?.measurement_valid !== 0} />
          <div className="muted" style={{ marginTop: 6 }}>
            {spec
              ? `gain ${GAINS[Number(spec.gain)] ?? spec.gain} · led ${spec.led_mask} · integ ${spec.integration_cycles} · ${spec.measurement_valid === 0 ? "INVALID (mask)" : "valid"}`
              : "waiting…"}
          </div>
        </>
      )}

      {hasImu && (
        <>
          <h2>Attitude</h2>
          <RocketAttitude accel={rAccel} gyro={rGyro} altM={altM} />
          <h2>Accel (m/s²)</h2>
          <TimeSeriesChart series={s.accel} yLabel="m/s²" cursor={cursor} />
          <h2>Gyro (dps)</h2>
          <TimeSeriesChart series={s.gyro} yLabel="dps" cursor={cursor} />
        </>
      )}

      {b.ber.length > 0 && (
        <>
          <h2>Bit error rate (%)</h2>
          <TimeSeriesChart series={s.ber} yLabel="%" cursor={cursor} />
        </>
      )}

      {hasEnv && (
        <>
          {b.temp.length > 0 && (
            <>
              <h2>Temperature (°C)</h2>
              <TimeSeriesChart series={s.temp} yLabel="°C" height={180} cursor={cursor} />
            </>
          )}
          {b.press.length > 0 && (
            <>
              <h2>Pressure (mbar)</h2>
              <TimeSeriesChart series={s.press} yLabel="mbar" height={180} cursor={cursor} />
            </>
          )}
        </>
      )}

      {!hasImu && !hasEnv && b.ber.length === 0 && source !== "exp1" && <div className="muted">no data yet</div>}
    </>
  );
}
