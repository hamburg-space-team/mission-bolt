import { useState } from "react";
import { SELF_TEST_STEPS } from "../../../../src/protocol";
import { post, useMessages } from "../../utils/api";

type Sample = Record<string, unknown>;

const NODES = ["btc", "exp1", "exp2", "exp3"] as const;

// test_id -> name from the wire contract; the firmware asserts against
// the same source
const STEPS = SELF_TEST_STEPS;

// Ground-side verdict bridge: judge the raw value a step shipped - the board
// verdict only proves the transaction worked, the VALUE is ground's call.
// Expected ranges land here (per node + test_id); null = no judgement yet
export function groundVerdict(node: string, testId: number, data: number): "pass" | "fail" | null {
  void node;
  void testId;
  void data;
  return null;
}

type RowState = "waiting" | "pass" | "fail" | "skip" | "no report" | "node dark";

interface Row {
  state: RowState;
  data?: number;
  durationMs?: number;
}

interface NodeRun {
  rows: Row[];
  lastTs?: number; // previous report's timestamp_us, duration base
  done: boolean;
  dark: boolean; // BTC skipped this node (GAP_MARKER self_test_skipped)
}

const freshRun = (node: string): NodeRun => ({
  rows: STEPS[node].map(() => ({ state: "waiting" as RowState })),
  done: false,
  dark: false,
});
const freshAll = (): Record<string, NodeRun> =>
  Object.fromEntries(NODES.map((n) => [n, freshRun(n)])) as Record<string, NodeRun>;

const COLORS: Record<RowState, string> = {
  waiting: "var(--vscode-testing-iconQueued)",
  pass: "var(--vscode-testing-iconPassed)",
  fail: "var(--vscode-testing-iconErrored)",
  skip: "var(--vscode-testing-iconSkipped)",
  "no report": "var(--vscode-editorWarning-foreground)",
  "node dark": "var(--vscode-editorWarning-foreground)",
};

function Chip({ state }: { state: RowState }) {
  return (
    <span className="pill" style={{ color: COLORS[state], fontWeight: 600 }}>
      {state.toUpperCase()}
    </span>
  );
}

export function SelfTest() {
  const [runs, setRuns] = useState<Record<string, NodeRun>>(freshAll);
  const [runSeen, setRunSeen] = useState(false);

  useMessages((m) => {
    if (m.type !== "frame") return;
    const f = m.frame;
    const s = f.sample as Sample;

    // the run starts over when the vehicle acknowledges the command
    if (s.kind === "cmd_ack" && s.command === "full_system_test") {
      setRuns(freshAll());
      setRunSeen(true);
      return;
    }

    if (s.kind === "test") {
      const node = String(s.node);
      if (!(node in STEPS)) return;
      const id = Number(s.test_id);
      setRuns((prev) => {
        const run = { ...prev[node], rows: [...prev[node].rows] };
        // duration = gap to the node's previous report (wrap-safe uint32 us)
        const dur = run.lastTs != null ? ((f.timestamp_us - run.lastTs) >>> 0) / 1000 : undefined;
        run.lastTs = f.timestamp_us;
        if (id < run.rows.length) {
          const name = String(s.result_name);
          const state: RowState = name === "pass" ? "pass" : name === "fail" ? "fail" : "skip";
          run.rows[id] = { state, data: Number(s.data), durationMs: dur };
        }
        // an arrived id proves every lower id already ran: what is still
        // "waiting" below it was sent and lost on the downlink
        for (let i = 0; i < Math.min(id, run.rows.length); i++) {
          if (run.rows[i].state === "waiting") run.rows[i] = { state: "no report" };
        }
        if (s.last === true) {
          run.done = true;
          for (const [i, r] of run.rows.entries()) {
            if (r.state === "waiting") run.rows[i] = { state: "no report" };
          }
        }
        return { ...prev, [node]: run };
      });
      setRunSeen(true);
      return;
    }

    // the BTC skipped a node that never answered
    if (s.kind === "gap" && s.reason === "self_test_skipped") {
      const node = String(s.node);
      if (!(node in STEPS)) return;
      setRuns((prev) => {
        const run = { ...prev[node], rows: [...prev[node].rows], dark: true, done: true };
        for (const [i, r] of run.rows.entries()) {
          if (r.state === "waiting") run.rows[i] = { state: "node dark" };
        }
        return { ...prev, [node]: run };
      });
    }
  });

  const runNow = () => {
    setRuns(freshAll()); // reset on click; the ack resets again (harmless)
    setRunSeen(true);
    post({ type: "runSelfTest" });
  };

  return (
    <>
      <h1>
        Self-Test{" "}
        <button className="secondary" onClick={runNow}>
          Run Full System Test
        </button>
      </h1>
      {!runSeen && <div className="muted">No run yet - start one, or wait for a FULL_SYSTEM_TEST ack.</div>}

      {NODES.map((node) => {
        const run = runs[node];
        const pass = run.rows.filter((r) => r.state === "pass").length;
        const fail = run.rows.filter((r) => r.state === "fail").length;
        return (
          <div key={node} style={{ marginTop: 14 }}>
            <h2>
              {node.toUpperCase()} <span className="pill">{pass}/{run.rows.length} pass</span>{" "}
              {fail > 0 && (
                <span className="pill" style={{ color: COLORS.fail }}>
                  {fail} fail
                </span>
              )}{" "}
              {run.dark && (
                <span className="pill" style={{ color: COLORS["node dark"] }}>
                  skipped - node dark
                </span>
              )}
            </h2>
            <table>
              <thead>
                <tr>
                  <th>#</th>
                  <th>test</th>
                  <th>board</th>
                  <th>value</th>
                  <th>value check</th>
                  <th>duration</th>
                </tr>
              </thead>
              <tbody>
                {run.rows.map((r, i) => {
                  const gv = r.data != null ? groundVerdict(node, i, r.data) : null;
                  return (
                    <tr key={i}>
                      <td>{i}</td>
                      <td>{STEPS[node][i]}</td>
                      <td>
                        <Chip state={r.state} />
                      </td>
                      <td>{r.data != null ? `${r.data} (0x${r.data.toString(16)})` : "–"}</td>
                      <td>{gv ? <Chip state={gv} /> : <span className="muted">–</span>}</td>
                      <td>{r.durationMs != null ? `${r.durationMs.toFixed(1)} ms` : "–"}</td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
        );
      })}
    </>
  );
}
