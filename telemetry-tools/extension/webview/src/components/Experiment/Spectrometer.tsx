// AS7265x 18 channels (one atomic packet); UV/NIR bands approximated
const WL = [410, 435, 460, 485, 510, 535, 560, 365, 340, 585, 610, 645, 680, 705, 730, 760, 810, 940];

function wlRGB(wl: number): [number, number, number] {
  let r = 0, g = 0, b = 0;
  if (wl >= 380 && wl < 440) { r = -(wl - 440) / 60; b = 1; }
  else if (wl < 490) { g = (wl - 440) / 50; b = 1; }
  else if (wl < 510) { g = 1; b = -(wl - 510) / 20; }
  else if (wl < 580) { r = (wl - 510) / 70; g = 1; }
  else if (wl < 645) { r = 1; g = -(wl - 645) / 65; }
  else if (wl <= 780) { r = 1; }
  else if (wl < 380) { r = 0.4; b = 0.6; }
  else { r = 0.5; }
  const f = wl < 420 ? 0.3 + 0.7 * (wl - 380) / 40 : wl > 700 ? 0.3 + 0.7 * (780 - wl) / 80 : 1;
  const c = (x: number) => Math.round(255 * Math.max(0, Math.min(1, x)) * Math.max(0.25, f));
  return [c(r), c(g), c(b)];
}

export function Spectrometer({ channels, valid }: { channels?: number[]; valid?: boolean }) {
  const chans = channels ?? Array(18).fill(0);
  const order = WL.map((_, i) => i).sort((x, y) => WL[x] - WL[y]);
  const max = Math.max(1, ...chans);
  return (
    <div className="spectro">
      {order.map((i) => {
        const h = Math.max(1, (chans[i] / max) * 100);
        const [r, g, bl] = wlRGB(WL[i]);
        return (
          <div className="spectro-bar" key={i} title={`${WL[i]} nm: ${chans[i]}`}>
            <div className="spectro-fill" style={{ height: `${h}%`, background: `rgb(${r},${g},${bl})`, opacity: valid === false ? 0.35 : 1 }} />
            <div className="spectro-wl">{WL[i] >= 380 && WL[i] <= 780 ? WL[i] : WL[i] < 380 ? "UV" : "NIR"}</div>
          </div>
        );
      })}
    </div>
  );
}
