import { Fragment, useState } from "react";
import type { Row } from "../../utils/feed";
import { summary, rowNode, faultTrace } from "../../utils/summary";

export function FeedTable({ rows, onShowType }: { rows: Row[]; onShowType: (name: string) => void }) {
  const [open, setOpen] = useState<number | null>(null);

  return (
    <table>
      <thead>
        <tr>
          <th>#</th><th>tick</th><th>t (µs)</th><th>source</th><th>packet</th><th>crc</th><th>valid</th><th>summary</th>
        </tr>
      </thead>
      <tbody>
        {rows.map((r, i) => {
          const bad = !r.crc_ok || r.suspect;
          const key = `${r.timestamp_us}-${r.seq ?? i}`;
          return (
            <Fragment key={key}>
              <tr className={bad ? "bad" : ""} onClick={() => setOpen(open === i ? null : i)} style={{ cursor: "pointer" }}>
                <td>{r.seq ?? ""}</td>
                <td>{r.tick}</td>
                <td>{r.timestamp_us}</td>
                <td><span className="pill">{rowNode(r)}</span></td>
                <td onClick={(e) => { e.stopPropagation(); onShowType(r.name); }} style={{ textDecoration: "underline" }}>{r.name}</td>
                <td>{r.crc_ok ? "✓" : "✗"}</td>
                <td>{r.suspect ? "⚠" : "✓"}</td>
                <td>{summary(r)}</td>
              </tr>
              {open === i && (
                <tr>
                  <td colSpan={8} style={{ whiteSpace: "pre-wrap", fontSize: 11, background: "var(--vscode-editorWidget-background)" }}>
                    {r.name === "fault" ? faultTrace(r) : JSON.stringify(r.sample, null, 2)}
                  </td>
                </tr>
              )}
            </Fragment>
          );
        })}
      </tbody>
    </table>
  );
}
