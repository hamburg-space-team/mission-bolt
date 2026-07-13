import { useEffect, useRef } from "react";

interface Props {
  accel?: number[]; // m/s^2 (board frame)
  gyro?: number[]; // dps
  altM?: number | null;
}

// Live rocket: tilts from the gravity vector (accel), rolls from integrated
// gyro_z, and climbs from barometric altitude. Smoothed via rAF so it stays
// fluid even at a low sample-update rate.
export function RocketAttitude({ accel, gyro, altM }: Props) {
  const body = useRef<SVGGElement>(null);
  const roll = useRef<SVGGElement>(null);
  const st = useRef({ lean: 0, target: 0, roll: 0, rate: 0, climb: 0, targetClimb: 0, last: 0 });

  // Refresh targets from the latest sample (runs each parent render).
  if (accel && accel.length === 3) st.current.target = (Math.atan2(accel[0], -accel[2]) * 180) / Math.PI;
  st.current.rate = gyro && gyro.length === 3 ? gyro[2] : 0;
  st.current.targetClimb = altM != null ? Math.max(-40, Math.min(40, altM / 25)) : 0;

  useEffect(() => {
    let raf = 0;
    const tick = (t: number) => {
      const s = st.current;
      const dt = s.last ? Math.min(0.05, (t - s.last) / 1000) : 0;
      s.last = t;
      s.lean += (s.target - s.lean) * Math.min(1, dt * 6);
      s.climb += (s.targetClimb - s.climb) * Math.min(1, dt * 4);
      s.roll = (s.roll + s.rate * dt) % 360;
      body.current?.setAttribute("transform", `translate(0 ${-s.climb.toFixed(1)}) rotate(${s.lean.toFixed(2)} 100 130)`);
      roll.current?.setAttribute("transform", `rotate(${s.roll.toFixed(1)} 100 130)`);
      raf = requestAnimationFrame(tick);
    };
    raf = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(raf);
  }, []);

  return (
    <div className="rocket">
      <svg viewBox="0 0 200 260" width="220" height="260" aria-label="rocket attitude">
        <line x1="18" y1="130" x2="182" y2="130" stroke="var(--vscode-widget-border, #8884)" strokeDasharray="4 5" />
        <g ref={roll}>
          <circle cx="100" cy="130" r="72" fill="none" stroke="var(--vscode-widget-border, #8886)" strokeDasharray="2 9" />
          <circle cx="100" cy="58" r="4.5" fill="#61afef" />
        </g>
        <g ref={body}>
          <path d="M93 188 L100 232 L107 188 Z" fill="#e5c07b" opacity="0.9" />
          <path d="M100 40 L115 92 L85 92 Z" fill="#e06c75" />
          <rect x="85" y="92" width="30" height="98" rx="9" fill="#d6d9e1" />
          <path d="M85 150 L68 192 L85 180 Z" fill="#61afef" />
          <path d="M115 150 L132 192 L115 180 Z" fill="#61afef" />
          <circle cx="100" cy="122" r="7.5" fill="#98c379" />
        </g>
      </svg>
      <div className="rocket-readout">
        <div><span className="muted">tilt</span><b>{Math.abs(st.current.lean).toFixed(1)}°</b></div>
        <div><span className="muted">roll rate</span><b>{(gyro?.[2] ?? 0).toFixed(0)} dps</b></div>
        <div><span className="muted">altitude</span><b>{altM != null ? `${altM.toFixed(0)} m` : "—"}</b></div>
      </div>
    </div>
  );
}
