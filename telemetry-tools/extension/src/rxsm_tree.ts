import * as vscode from "vscode";

import { RxsmMonitor } from "./rxsm";

class Node extends vscode.TreeItem {}

// Labels as the simulator actually prints them (the scanned manual abbreviates
// "Dropout Duration" to "Duration"). Always rendered, in this order, even before
// any data arrives: the error generator's settings are what this view is for, so
// their absence must be visible rather than silently missing.
const FIELDS: ReadonlyArray<{ key: string; icon: string }> = [
  { key: "Error Inhibit", icon: "shield" },
  { key: "Dropout Duration", icon: "watch" },
  { key: "Byte Dropout Rate", icon: "circle-slash" },
  { key: "Bit Error Rate", icon: "zap" },
];

const UNKNOWN = "—";

/**
 * RXSM simulator view: the error-generator settings read off the simulator's
 * debug port. Ground equipment - unrelated to the flight telemetry session.
 */
export class RxsmTree implements vscode.TreeDataProvider<Node> {
  private readonly _onDidChange = new vscode.EventEmitter<void>();
  readonly onDidChangeTreeData = this._onDidChange.event;

  constructor(private readonly rxsm: RxsmMonitor) {
    rxsm.onChange(() => this._onDidChange.fire());
  }

  getTreeItem(el: Node): vscode.TreeItem {
    return el;
  }

  getChildren(): Node[] {
    const s = this.rxsm.state;
    const nodes: Node[] = [this.head()];

    // Disconnected: just the connect action. Rows of "—" would only be noise.
    if (!s.connected) {
      return nodes;
    }

    // A fixed set, never data-driven: a label the simulator mangles (line noise,
    // a fragment) must not be able to invent a row. Unknown labels are still
    // logged raw to the output channel, which is where they belong.
    for (const f of FIELDS) {
      nodes.push(this.field(f.key, f.icon, s.fields[f.key]));
    }
    return nodes;
  }

  private head(): Node {
    const s = this.rxsm.state;
    if (!s.connected) {
      const n = new Node("Connect RXSM debug port…");
      n.command = { command: "bolt.rxsm.connect", title: "Connect" };
      n.iconPath = new vscode.ThemeIcon("plug");
      n.tooltip = "The simulator's own D-SUB 9 at 115200 8N1 — not the RS-422 flight link.";
      return n;
    }
    if (s.silent) {
      const n = new Node(`No data on ${s.port ?? "?"}`);
      n.iconPath = new vscode.ThemeIcon("warning");
      n.description = "wrong port?";
      n.command = { command: "bolt.rxsm.pickPort", title: "Pick port" };
      n.tooltip =
        "Nothing arrived in 5 s. The simulator's debug interface is its own D-SUB 9 (115200 8N1, ASCII) — " +
        "not the RS-422 telemetry link. Raw lines are logged to the Bolt output channel as 'rxsm < …'. " +
        "Click to pick a different port.";
      return n;
    }
    const n = new Node(`Debug port: ${s.port ?? "?"}`);
    n.iconPath = new vscode.ThemeIcon("broadcast");
    n.description = "115200 8N1";
    n.command = { command: "bolt.rxsm.disconnect", title: "Disconnect" };
    return n;
  }

  private field(key: string, icon: string, value?: string): Node {
    const n = new Node(key);
    n.description = value ?? UNKNOWN;
    n.iconPath = new vscode.ThemeIcon(value === undefined ? "circle-outline" : icon);

    if (key === "Error Inhibit" && value !== undefined) {
      // "Inhibited" means the potentiometers are ignored and nothing is
      // injected - i.e. the link is clean and NOT being stress-tested.
      const on = /^on$/i.test(value);
      n.description = on ? `${value} — no errors injected` : `${value} — errors active`;
      n.iconPath = new vscode.ThemeIcon(on ? "shield" : "zap");
    }
    return n;
  }
}
