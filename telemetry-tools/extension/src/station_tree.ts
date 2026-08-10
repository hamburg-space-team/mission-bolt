import * as vscode from "vscode";

import { StationClient } from "./station";

class Node extends vscode.TreeItem {}

const OFF = "—";

/**
 * Debug-station view: the same picture the kiosk dashboard shows in its
 * header - which boards are alive, the REXUS signals, where the probe sits -
 * plus the one thing only the station can do: drive the LO line.
 */
export class StationTree implements vscode.TreeDataProvider<Node> {
  private readonly _onDidChange = new vscode.EventEmitter<void>();
  readonly onDidChangeTreeData = this._onDidChange.event;

  constructor(private readonly station: StationClient) {
    station.onChange(() => this._onDidChange.fire());
  }

  getTreeItem(el: Node): vscode.TreeItem {
    return el;
  }

  getChildren(): Node[] {
    const s = this.station.state;
    const head = new Node(s.reachable ? "Station: online" : "Station: unreachable");
    head.description = s.url.replace(/^https?:\/\//, "");
    head.iconPath = new vscode.ThemeIcon(s.reachable ? "server-environment" : "circle-slash");
    head.tooltip = s.reachable
      ? `${s.url}\nHTTP API + kiosk dashboard`
      : `${s.url}\n${s.error ?? "no answer"}\n\nSet 'bolt.stationUrl' if the station moved.`;
    head.command = { command: "bolt.station.refresh", title: "Refresh" };
    if (!s.reachable) {
      return [head];
    }
    return [head, this.lo(), this.signals(), this.boards(), this.probe()];
  }

  /** The row that both shows and switches the line. */
  private lo(): Node {
    const s = this.station.state;
    const asserted = this.station.loAsserted;
    const n = new Node(`LO drive: ${asserted === undefined ? OFF : asserted ? "ASSERTED" : "released"}`);
    // both halves matter: "asserted" is REXUS, the level is what a probe sees
    n.description = s.loLevel ? `gpio${s.loGpio ?? 0} = ${s.loLevel}` : "unknown";
    n.iconPath = new vscode.ThemeIcon(asserted ? "rocket" : "circle-large-outline");
    n.contextValue = "bolt.lo";
    n.command = { command: "bolt.lo.toggle", title: "Toggle LO" };
    n.tooltip = new vscode.MarkdownString(
      `REXUS **LO**, faked on the ST-Link V3 bridge GPIO${s.loGpio ?? 0} while the flight harness is not connected.\n\n` +
        `Click to ${asserted ? "release" : "assert"} it. Asserting puts the BTC into FLIGHT and latches it, ` +
        `so it survives a reset (\`.noinit\`).\n\n` +
        `The GPIO drives a low-side MOSFET, so **high** pulls the 28 V line to GND and asserts LO; ` +
        `low leaves it released. Flip \`bolt.loActiveLow\` if that stage is ever bypassed.`,
    );
    return n;
  }

  private signals(): Node {
    const sig = this.station.state.signals;
    const mark = (on: boolean | undefined) => (on ? "✓" : OFF);
    const n = new Node("Signals");
    n.description = sig ? `LO ${mark(sig.lo)} · SODS ${mark(sig.sods)} · SOE ${mark(sig.soe)}` : OFF;
    n.iconPath = new vscode.ThemeIcon(sig?.lo ? "rocket" : "pulse");
    n.tooltip = "What the boards report in their status packets — not what the station drives.";
    return n;
  }

  private boards(): Node {
    const boards = this.station.state.boards;
    const live = boards.filter((b) => b.sending);
    const n = new Node("Boards");
    n.description = live.length ? live.map((b) => b.name).join(", ") : "none sending";
    n.iconPath = new vscode.ThemeIcon(live.length ? "circuit-board" : "warning");
    const modes = [...new Set(live.map((b) => b.mode).filter(Boolean))];
    n.tooltip = boards.length
      ? boards.map((b) => `${b.name}: ${b.sending ? b.mode ?? "sending" : "silent"}`).join("\n")
      : "no boards seen";
    if (modes.length) {
      n.description += `  ·  ${modes.join("/")}`;
    }
    return n;
  }

  private probe(): Node {
    const s = this.station.state;
    const n = new Node("Probe");
    n.description = !s.probePresent ? "not plugged in" : (s.probeBoard ?? "unmapped chip");
    n.iconPath = new vscode.ThemeIcon(s.probePresent ? "plug" : "circle-slash");
    n.tooltip = s.probeBoard
      ? `The ST-Link sits on ${s.probeBoard} (chip UID mapped in /etc/bolt-station/uids.conf).`
      : "Which board the ST-Link sits on, read from the chip's unique ID.";
    return n;
  }
}
