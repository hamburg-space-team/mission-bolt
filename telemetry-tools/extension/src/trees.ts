import * as vscode from "vscode";

import { SessionManager } from "./session";
import { UPLINK_COMMANDS, PACKET_WIRE_BYTES } from "./protocol";
import { errorName, faultName, faultNode, gapNode } from "./codes";

class Node extends vscode.TreeItem {
  constructor(
    label: string,
    collapsible: vscode.TreeItemCollapsibleState = vscode.TreeItemCollapsibleState.None,
  ) {
    super(label, collapsible);
  }
}

abstract class BaseTree implements vscode.TreeDataProvider<Node> {
  private readonly _onDidChange = new vscode.EventEmitter<void>();
  readonly onDidChangeTreeData = this._onDidChange.event;
  constructor(protected readonly session: SessionManager) {
    session.onChange(() => this._onDidChange.fire());
  }
  getTreeItem(el: Node): vscode.TreeItem {
    return el;
  }
  abstract getChildren(el?: Node): Node[];
}

// Session view: current mode/source + entry actions
export class SessionTree extends BaseTree {
  getChildren(): Node[] {
    const s = this.session.state;
    const mode = new Node(`Mode: ${s.mode}`);
    mode.iconPath = new vscode.ThemeIcon(s.connected ? "broadcast" : "circle-outline");
    const nodes = [mode];
    if (s.source) {
      const src = new Node(s.connected ? `Live: ${s.source}` : `Capture: ${s.source}`);
      src.iconPath = new vscode.ThemeIcon(s.connected ? "rss" : "file-binary");
      nodes.push(src);
    }
    if (s.mode === "idle") {
      const connect = new Node("Connect serial…");
      connect.command = { command: "bolt.connect", title: "Connect" };
      connect.iconPath = new vscode.ThemeIcon("plug");
      const open = new Node("Open .raw capture…");
      open.command = { command: "bolt.openCapture", title: "Open" };
      open.iconPath = new vscode.ThemeIcon("folder-opened");
      nodes.push(connect, open);
    }
    return nodes;
  }
}

// Health view: signals (LO/SOE/SODS), CRC, experiments, fail counters
export class StatsTree extends BaseTree {
  getChildren(): Node[] {
    return [this.headline(), ...this.health()];
  }

  private health(): Node[] {
    const st = this.session.stats;
    const man = this.session.manifest;
    const total = st?.total ?? man?.total_frames ?? 0;
    const crcFail = st?.crc_fail ?? man?.crc_fail_total ?? 0;
    const lo = st?.lo ?? (man ? man.lo_rtc_s !== 0 : false);
    const rate = total ? ((crcFail / total) * 100).toFixed(2) : "0.00";

    const signal = (label: string, on: boolean, icon: string) => {
      const n = new Node(`${label}: ${on ? "active" : "—"}`);
      n.iconPath = new vscode.ThemeIcon(on ? icon : "circle-outline");
      return n;
    };

    const nodes: Node[] = [signal("LO", lo, "rocket")];
    // SODS/SOE only known live (manifest keeps LO only). Listed in the order the
    // REXUS control interface is specified in (RXSM manual 3.2), not in
    // signal_mask's bit order (1=SOE, 2=SODS)
    if (st) {
      nodes.push(signal("SODS", st.sods, "database"), signal("SOE", st.soe, "play-circle"));
    }
    const crcNode = new Node(`CRC fails: ${crcFail} (${rate}%)`);
    crcNode.iconPath = new vscode.ThemeIcon(crcFail ? "warning" : "check");
    // Post-flight: from the manifest. Live: derive from the packet names
    let exps = man?.experiments ?? [];
    if (!exps.length && st) {
      const set = new Set<string>();
      for (const name of Object.keys(st.counts)) {
        const src = ["btc", "exp1", "exp2", "exp3"].find((s) => name.startsWith(s));
        if (src) set.add(src);
      }
      exps = [...set].sort();
    }
    const expNode = new Node(`Experiments: ${exps.join(", ") || "—"}`);
    expNode.iconPath = new vscode.ThemeIcon("beaker");
    nodes.push(crcNode, expNode);

    // Per-source failure counters (why data may be invalid)
    for (const [src, c] of Object.entries(this.session.counters)) {
      if (c.led || c.spec || c.ready) {
        const n = new Node(`${src} fails: led ${c.led} · spec ${c.spec} · dataRdy ${c.ready}`);
        n.iconPath = new vscode.ThemeIcon("warning");
        nodes.push(n);
      }
    }
    return nodes;
  }

  private headline(): Node {
    const total = this.session.stats?.total ?? this.session.manifest?.total_frames ?? 0;
    const n = new Node(`${total} frames`);
    n.iconPath = new vscode.ThemeIcon("pulse");
    return n;
  }
}

// Packets view: per-type counts. Clicking filters the live feed
export class PacketsTree extends BaseTree {
  getChildren(): Node[] {
    const counts = this.session.stats?.counts ?? this.session.manifest?.packet_counts ?? {};
    const entries = Object.entries(counts).sort((a, b) => b[1] - a[1]);
    if (!entries.length) {
      const n = new Node("no packets yet");
      n.iconPath = new vscode.ThemeIcon("circle-outline");
      return [n];
    }
    // Elapsed window for the throughput estimate: live stats carry elapsed_ms,
    // a post-flight manifest carries the LO..end timestamp span.
    const man = this.session.manifest;
    const elapsedSec = this.session.stats?.elapsed_ms
      ? this.session.stats.elapsed_ms / 1000
      : man && man.t_end_us > man.t_start_us
        ? (man.t_end_us - man.t_start_us) / 1e6
        : 0;
    return entries.map(([name, n]) => {
      const node = new Node(name);
      // Estimated bandwidth: count x on-wire bytes over the elapsed window.
      const wire = PACKET_WIRE_BYTES[name];
      const kbps = elapsedSec > 0 && wire ? (n * wire * 8) / (elapsedSec * 1000) : null;
      const rate = kbps == null ? "" : `  ·  ${kbps < 10 ? kbps.toFixed(1) : Math.round(kbps)} kbit/s`;
      node.description = `${n}${rate}`;
      node.iconPath = new vscode.ThemeIcon("symbol-numeric");
      node.tooltip =
        kbps == null
          ? "Show all packets of this type, all fields, formatted"
          : `${n} packets · ~${kbps.toFixed(2)} kbit/s (${wire} B/pkt over ${elapsedSec.toFixed(1)} s)\nShow all packets of this type, all fields, formatted`;
      node.command = { command: "bolt.showPackets", title: "Show packets", arguments: [name] };
      return node;
    });
  }
}

// Uplink view: command buttons + arm state
export class UplinkTree extends BaseTree {
  getChildren(): Node[] {
    const connected = this.session.state.connected;
    const armed = this.session.state.armed;
    // The RXSM kills the uplink the moment LO asserts and drops everything the
    // ground sends (ICD-001 / RXSM SIM manual 3.3). Commands are pad-only, so
    // say so instead of letting the operator click into the void
    const flown = this.session.stats?.lo ?? false;

    // No standalone "Uplink: safe/ARMED" row - the arm state is shown by the
    // toolbar shield button (view/title) and the status bar, and the
    // dangerous commands already render "disabled - arm first" until armed
    const cmds = UPLINK_COMMANDS.map((c) => {
      const n = new Node(c.label);
      n.contextValue = `bolt.cmd.${c.id}`;
      if (!connected) {
        n.iconPath = new vscode.ThemeIcon("circle-slash");
        n.description = "disabled — not connected";
        n.tooltip = "Connect a live serial session to enable uplink";
      } else if (flown) {
        n.iconPath = new vscode.ThemeIcon("rocket");
        n.description = "disabled — LO: uplink is dead";
        n.tooltip =
          "The RXSM disables the uplink and drops all ground data once LO is asserted (ICD-001). " +
          "Telecommands are pad-only — nothing sent now can reach the experiment.";
      } else if (c.dangerous && !armed) {
        // Dangerous + safe mode: shown, clearly disabled until armed
        n.iconPath = new vscode.ThemeIcon("lock");
        n.description = "disabled — arm first";
        n.tooltip = "Consequential command: toggle the guard (Arm) above to enable";
        n.command = { command: `bolt.send.${camel(c.id)}`, title: `Send ${c.label}` };
      } else {
        n.iconPath = new vscode.ThemeIcon(c.dangerous ? "flame" : "send");
        n.command = { command: `bolt.send.${camel(c.id)}`, title: `Send ${c.label}` };
        n.tooltip = c.dangerous ? "Armed — click to send (asks for confirmation)" : undefined;
      }
      return n;
    });
    return cmds;
  }
}

// Export view: capture-derived outputs
export class ExportTree extends BaseTree {
  getChildren(): Node[] {
    const items: Array<[string, string, string]> = [
      ["Export HDF5", "bolt.export.hdf5", "database"],
      ["Export manifest.json", "bolt.export.manifest", "json"],
      ["Export packet type as CSV…", "bolt.export.csv", "export"],
    ];
    return items.map(([label, cmd, icon]) => {
      const n = new Node(label);
      n.command = { command: cmd, title: label };
      n.contextValue = cmd;
      n.iconPath = new vscode.ThemeIcon(icon);
      return n;
    });
  }
}

function camel(id: string): string {
  return id.replace(/_([a-z])/g, (_, c) => c.toUpperCase());
}

// Panel view (bottom): error inspection + packet filter; manifest when a
// capture is loaded, else the live faults/gaps/counts
export class InspectorTree extends BaseTree {
  getChildren(el?: Node): Node[] {
    if (!el) {
      return [
        this.section(`Faults`, "faults", this.faults().length, "error"),
        this.section(`Gaps`, "gaps", this.gaps().length, "warning"),
        this.section(`CRC fails`, "crc", this.crcCount(), "warning"),
        this.section(`Packet types`, "packets", Object.keys(this.counts()).length, "package"),
      ];
    }
    switch (el.contextValue) {
      case "faults": return this.faultNodes();
      case "gaps": return this.gapNodes();
      case "crc": return this.crcNodes();
      case "packets": return this.packetNodes();
      default: return [];
    }
  }

  private counts(): Record<string, number> {
    return this.session.stats?.counts ?? this.session.manifest?.packet_counts ?? {};
  }
  private faults() {
    return this.session.manifest?.faults ?? this.session.liveFaults;
  }
  private gaps() {
    return this.session.manifest?.gaps ?? this.session.liveGaps;
  }
  private crcCount(): number {
    return this.session.manifest?.crc_fail_total ?? this.session.stats?.crc_fail ?? 0;
  }

  private faultNodes(): Node[] {
    const faults = this.faults();
    if (!faults.length) return [empty("no faults")];
    return faults.map((f) => {
      const n = new Node(`[${faultNode(f.fault_code)}] #${f.fault_code} ${faultName(f.fault_code)} · ${errorName(f.error_code)} · line ${f.line}`);
      n.iconPath = new vscode.ThemeIcon("error");
      n.tooltip = `origin: ${faultNode(f.fault_code)}\nsteps: ${f.steps.join(" → ") || "—"}${f.truncated ? " (truncated)" : ""}`;
      n.command = { command: "bolt.inspectFault", title: "Inspect fault", arguments: [f] };
      return n;
    });
  }
  private gapNodes(): Node[] {
    const gaps = this.gaps();
    if (!gaps.length) return [empty("no gaps")];
    return gaps.map((g) => {
      const n = new Node(`[${gapNode(g.reason)}] tick ${g.first_missing_tick} ×${g.count} · ${g.reason}`);
      n.iconPath = new vscode.ThemeIcon("warning");
      n.tooltip = `likely origin: ${gapNode(g.reason)} (gaps carry no explicit node)`;
      return n;
    });
  }
  private crcNodes(): Node[] {
    const list = this.session.manifest?.crc_fails;
    if (list && list.length) {
      return list.slice(0, 200).map((c) => {
        const n = new Node(`#${c.index} · type 0x${c.ty.toString(16).padStart(2, "0")} · tick ${c.tick}`);
        n.iconPath = new vscode.ThemeIcon("warning");
        return n;
      });
    }
    const total = this.crcCount();
    return [empty(total ? `${total} CRC fails — open a capture for the list` : "no CRC fails")];
  }
  private packetNodes(): Node[] {
    return Object.entries(this.counts())
      .sort((a, b) => b[1] - a[1])
      .map(([name, c]) => {
        const n = new Node(name);
        n.description = String(c);
        n.iconPath = new vscode.ThemeIcon("package");
        n.tooltip = "Filter the live feed to this packet type";
        n.command = { command: "bolt.filterFeed", title: "Filter feed", arguments: [name] };
        return n;
      });
  }

  private section(label: string, ctx: string, count: number, icon: string): Node {
    const n = new Node(`${label} (${count})`, vscode.TreeItemCollapsibleState.Collapsed);
    n.contextValue = ctx;
    n.iconPath = new vscode.ThemeIcon(icon);
    return n;
  }
}

function empty(text: string): Node {
  const n = new Node(text);
  n.iconPath = new vscode.ThemeIcon("check");
  return n;
}
