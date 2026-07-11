import { useState } from "react";
import { useMessages } from "../../utils/api";
import { fmtValue, magnitude } from "../../utils/format";
import { Spectrometer } from "./Spectrometer";

type Sample = Record<string, unknown>;
const GAINS = ["1x", "3.7x", "16x", "64x"];

function Tiles({ items }: { items: [string, string][] }) {
  if (items.length === 0) return <div className="muted">no data yet</div>;
  return (
    <div className="tiles">
      {items.map(([l, v]) => (
        <div className="tile" key={l}><div className="n">{v}</div><div className="l">{l}</div></div>
      ))}
    </div>
  );
}

export function Experiment() {
  const [source, setSource] = useState("");
  const [byKind, setByKind] = useState<Record<string, Sample>>({});
  const [specA, setSpecA] = useState<Sample>();
  const [specB, setSpecB] = useState<Sample>();

  useMessages((m) => {
    if (m.type === "experiment") setSource(m.source);
    else if (m.type === "reset") { setByKind({}); setSpecA(undefined); setSpecB(undefined); }
    else if (m.type === "frame") {
      const f = m.frame;
      const s = f.sample as Sample;
      if (f.name.endsWith("spectrum_a")) setSpecA(s);
      else if (f.name.endsWith("spectrum_b")) setSpecB(s);
      else setByKind((prev) => ({ ...prev, [String(s.kind)]: s }));
    }
  });

  const env = byKind.env;
  const status = byKind.status;
  const imu = byKind.imu;
  const ber = byKind.ber;

  const envTiles: [string, string][] = [];
  if (env?.temp_c != null) envTiles.push(["temp", `${Number(env.temp_c).toFixed(1)} °C`]);
  if (env?.pressure_mbar != null) envTiles.push(["pressure", `${Number(env.pressure_mbar).toFixed(1)} mbar`]);
  if (status?.data_ready_fails != null) envTiles.push(["dataRdy fails", fmtValue(status.data_ready_fails)]);

  return (
    <>
      <h1>{source.toUpperCase() || "Experiment"}</h1>

      {source === "exp1" && (
        <>
          <h2>Spectrometer · current cycle</h2>
          <Spectrometer a={specA?.channels as number[]} b={specB?.channels as number[]} valid={specA?.measurement_valid !== 0} />
          <div className="muted" style={{ marginTop: 6 }}>
            {specA
              ? `gain ${GAINS[Number(specA.gain)] ?? specA.gain} · led ${specA.led_mask} · integ ${specA.integration_cycles} · ${specA.measurement_valid === 0 ? "INVALID (mask)" : "valid"}`
              : "waiting…"}
          </div>
        </>
      )}

      {ber && (
        <>
          <h2>Bit error rate</h2>
          <Tiles items={[["BER", `${(Number(ber.ber) * 100).toFixed(2)} %`], ["bit errors", fmtValue(ber.bit_errors)], ["rate idx", fmtValue(ber.rate_index)]]} />
        </>
      )}

      {imu && (
        <>
          <h2>IMU</h2>
          <Tiles items={[
            ["|accel| m/s²", magnitude(imu.accel_ms2).toFixed(2)],
            ["|gyro| dps", magnitude(imu.gyro_dps).toFixed(1)],
          ]} />
        </>
      )}

      <h2>Environment</h2>
      <Tiles items={envTiles} />
    </>
  );
}
