import { Fragment, useState } from "react";
import type { Row } from "../../utils/feed";
import { summary, rowNode, faultTrace } from "../../utils/summary";

export function FeedTable({ rows, onShowType }: { rows: Row[]; onShowType: (name: string) => void }) {
  // Keyed by Row.uid, not by row index: the window shifts as the tail moves, and
  // an index would keep the expander pinned to a slot while the packet under it
  // changes
  const [open, setOpen] = useState<number | null>(null);

  return (
    <table>
      <thead>
        <tr>
          <th>#</th><th>tick</th><th>t (µs)</th><th>source</th><th>packet</th><th>crc</th><th>valid</th><th>summary</th>
        </tr>
      </thead>
      <tbody>
        {rows.map((r) => {
          const bad = !r.crc_ok || r.suspect;
          return (
            <Fragment key={r.uid}>
              <tr className={bad ? "bad" : ""} onClick={() => setOpen(open === r.uid ? null : r.uid)} style={{ cursor: "pointer" }}>
                <td>{r.seq ?? ""}</td>
                <td>{r.tick}</td>
                <td>{r.timestamp_us}</td>
                <td><span className="pill">{rowNode(r)}</span></td>
                <td onClick={(e) => { e.stopPropagation(); onShowType(r.name); }} style={{ textDecoration: "underline" }}>{r.name}</td>
                <td>{r.crc_ok ? "✓" : "✗"}</td>
                <td>{r.suspect ? "⚠" : "✓"}</td>
                <td>{summary(r)}</td>
              </tr>
              {open === r.uid && (
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
