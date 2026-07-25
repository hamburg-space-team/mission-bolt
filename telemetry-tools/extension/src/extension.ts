import * as vscode from "vscode";
import * as fs from "fs";
import * as path from "path";

import { SessionManager } from "./session";
import { RxsmMonitor } from "./rxsm";
import { RxsmTree } from "./rxsm_tree";
import { SessionTree, StatsTree, PacketsTree, UplinkTree, ExportTree } from "./trees";
import { OverviewProvider, PacketRecord } from "./overview";
import { PanelFeedProvider } from "./panel";
import { ExperimentView } from "./experiment";
import { UPLINK_COMMANDS } from "./protocol";
import { errorName, faultName, faultNode } from "./codes";

export function activate(ctx: vscode.ExtensionContext): void {
  const log = vscode.window.createOutputChannel("Bolt", { log: true });
  const session = new SessionManager(log);
  // Ground equipment, own serial port + process: independent of the session
  const rxsm = new RxsmMonitor(log, session);
  ctx.subscriptions.push(log, session, rxsm);

  // --- sidebar views ---
  const feed = new PanelFeedProvider(ctx, session);
  const overview = new OverviewProvider(ctx, session, feed);
  const experiments = new ExperimentView(ctx, session);
  ctx.subscriptions.push(
    vscode.window.registerTreeDataProvider("bolt.session", new SessionTree(session)),
    vscode.window.registerTreeDataProvider("bolt.health", new StatsTree(session)),
    vscode.window.registerTreeDataProvider("bolt.rxsm", new RxsmTree(rxsm)),
    vscode.window.registerTreeDataProvider("bolt.packets", new PacketsTree(session)),
    vscode.window.registerTreeDataProvider("bolt.uplink", new UplinkTree(session)),
    vscode.window.registerTreeDataProvider("bolt.export", new ExportTree(session)),
    vscode.window.registerWebviewViewProvider("bolt.feed", feed, {
      webviewOptions: { retainContextWhenHidden: true },
    }),
    vscode.window.registerCustomEditorProvider("bolt.overview", overview, {
      supportsMultipleEditorsPerDocument: false,
      webviewOptions: { retainContextWhenHidden: true },
    }),
  );

  // --- status bar ---
  const status = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 100);
  status.command = "bolt.connect";
  ctx.subscriptions.push(status);
  const refreshStatus = () => {
    const s = session.state;
    const st = session.stats;
    if (s.connected) {
      const rate = st && st.total ? ((st.crc_fail / st.total) * 100).toFixed(2) : "0.00";
      status.text = `$(broadcast) BOLT ${st?.total ?? 0} · LO ${st?.lo ? "✓" : "✗"} · CRC ${rate}%${s.armed ? " · $(shield-x) ARMED" : ""}`;
      status.command = "bolt.disconnect";
    } else {
      status.text = "$(plug) BOLT: connect";
      status.command = "bolt.connect";
    }
    status.show();
    void vscode.commands.executeCommand("setContext", "bolt.connected", s.connected);
    void vscode.commands.executeCommand("setContext", "bolt.armed", s.armed);
    void vscode.commands.executeCommand("setContext", "bolt.hasCapture", s.mode === "postflight" || !!session.manifest);
  };
  ctx.subscriptions.push(session.onChange(refreshStatus));
  refreshStatus();

  // Live -> Idle: once the session's cache DB is built, reload the whole
  // capture from it (opens the overview, which loads panel + views from the
  // persisted DB rather than the volatile live buffer)
  ctx.subscriptions.push(
    session.onCaptureReady((raw) => {
      void vscode.commands.executeCommand("vscode.openWith", vscode.Uri.file(raw), "bolt.overview");
    }),
  );

  // --- commands ---
  const reg = (id: string, fn: (...a: any[]) => any) =>
    ctx.subscriptions.push(vscode.commands.registerCommand(id, fn));

  reg("bolt.connect", async () => {
    const port = await pickSerialPort(session);
    if (!port) return;
    // Mirror of RxsmMonitor.connect: one adapter, one owner. Whoever is asked
    // for the port takes it, and the other side lets go
    if (rxsm.state.connected && rxsm.state.port === port) {
      log.info(`session: taking ${port} - stopping the rxsm debug reader`);
      rxsm.disconnect();
    }
    await session.connect(port);
    overview.openLive();
  });

  reg("bolt.disconnect", () => session.disconnect());

  // RXSM simulator debug port (ground equipment, separate cable + baud)
  reg("bolt.rxsm.connect", () => rxsm.connect());
  // Always asks, so a wrong remembered port is never a dead end
  reg("bolt.rxsm.pickPort", () => rxsm.connect(undefined, true));
  reg("bolt.rxsm.disconnect", () => rxsm.disconnect());

  reg("bolt.openCapture", async () => {
    session.ensureTmp();
    const picks = await vscode.window.showOpenDialog({
      canSelectMany: false,
      filters: { "Raw capture": ["raw"] },
      openLabel: "Open capture",
      defaultUri: vscode.Uri.file(session.capturesDir),
    });
    if (picks?.[0]) {
      await vscode.commands.executeCommand("vscode.openWith", picks[0], "bolt.overview");
    }
  });

  reg("bolt.openOverview", () => overview.openLive());

  reg("bolt.openExperiment", async (name?: unknown) => {
    // A toolbar/menu invocation passes a non-string context arg; ignore it.
    let src = typeof name === "string" ? name : undefined;
    if (!src) {
      const counts = session.stats?.counts ?? session.manifest?.packet_counts ?? {};
      const seen = new Set(
        Object.keys(counts)
          .map((n) => ["btc", "exp1", "exp2", "exp3"].find((s) => n.startsWith(s)))
          .filter((s): s is string => !!s),
      );
      const list = seen.size ? [...seen] : ["btc", "exp1", "exp2", "exp3"];
      src = await vscode.window.showQuickPick(list, { placeHolder: "Experiment view" });
    }
    if (src) experiments.open(src);
  });

  reg("bolt.uplink.arm", () => session.setArmed(!session.state.armed));

  for (const c of UPLINK_COMMANDS) {
    reg(`bolt.send.${camel(c.id)}`, () => sendUplink(session, c.id, c.label, c.dangerous));
  }

  reg("bolt.export.manifest", () => exportManifest(session));
  reg("bolt.export.hdf5", () => exportHdf5(session));
  reg("bolt.export.csv", () => exportCsv(session));

  reg("bolt.refreshInspector", () => session.refresh());
  reg("bolt.filterFeed", (name?: string) => filterFeed(session, name));
  reg("bolt.inspectFault", (fault?: FaultLike) => inspectFault(fault));
  reg("bolt.showPackets", (name?: string) => showPackets(session, overview, name));

  log.info("Bolt activated");
}

interface FaultLike {
  fault_code: number;
  error_code: number;
  line: number;
  steps?: number[];
  truncated?: boolean;
}

async function filterFeed(session: SessionManager, name?: string): Promise<void> {
  if (typeof name === "string") {
    session.setFeedFilter(name);
    return;
  }
  const counts = session.stats?.counts ?? session.manifest?.packet_counts ?? {};
  const items = ["(all packets)", ...Object.keys(counts).sort()];
  const pick = await vscode.window.showQuickPick(items, { placeHolder: "Filter live feed by packet type" });
  if (!pick) return;
  session.setFeedFilter(pick === "(all packets)" ? undefined : pick);
}

async function showPackets(
  session: SessionManager,
  overview: OverviewProvider,
  name?: string,
): Promise<void> {
  if (!name) {
    const counts = session.stats?.counts ?? session.manifest?.packet_counts ?? {};
    name = await vscode.window.showQuickPick(Object.keys(counts).sort(), { placeHolder: "Packet type to inspect" });
  }
  if (!name) return;
  const raw = await resolveRaw(session);
  if (!raw) return;
  const rowLimit = vscode.workspace.getConfiguration("bolt").get<number>("tableRowLimit", 100000);
  let records: PacketRecord[] = [];
  try {
    // Answered from the indexed cache DB - no re-decode of the raw
    records = (await vscode.window.withProgress(
      { location: vscode.ProgressLocation.Notification, title: `Bolt: loading ${name} packets` },
      () => session.queryCapture(raw, { type: name, limit: rowLimit }),
    )) as unknown as PacketRecord[];
  } catch (e) {
    void vscode.window.showErrorMessage(`Bolt: ${(e as Error).message}`);
    return;
  }
  if (!records.length) {
    void vscode.window.showInformationMessage(`Bolt: no "${name}" packets in this capture.`);
    return;
  }
  // Enrich faults with decoded device/origin/error for the table
  for (const r of records) {
    const s = r.sample as Record<string, unknown>;
    if (s.kind === "fault") {
      s.device = faultName(Number(s.fault_code));
      s.error = errorName(Number(s.error_code));
      // `node` is the authoritative source_node the firmware stamps. Only fall
      // back to the fault_code-inferred origin when it's missing (old captures)
      // - no separate `origin` column, which just duplicated `node`
      if (s.node == null || s.node === "unknown") {
        s.node = `${faultNode(Number(s.fault_code))} (inferred)`;
      }
    }
  }
  overview.showPackets(name, records);
}

async function inspectFault(fault?: FaultLike): Promise<void> {
  const summary = fault
    ? `[${faultNode(fault.fault_code)}] Fault #${fault.fault_code} ${faultName(fault.fault_code)} · ${errorName(fault.error_code)} · line ${fault.line} · steps ${(fault.steps ?? []).join(" → ") || "—"}`
    : "Bolt: fault";
  const handbook = await findHandbook("fault-trace-codes.md");
  const action = await vscode.window.showInformationMessage(summary, ...(handbook ? ["Open trace handbook"] : []));
  if (action && handbook) {
    await vscode.window.showTextDocument(handbook);
  }
}

// Search workspace folders and their parents for docs/handbook/<name>
async function findHandbook(name: string): Promise<vscode.Uri | undefined> {
  const folders = vscode.workspace.workspaceFolders ?? [];
  const roots = folders.flatMap((f) => [f.uri.fsPath, path.dirname(f.uri.fsPath)]);
  for (const root of roots) {
    const candidate = path.join(root, "docs", "handbook", name);
    if (fs.existsSync(candidate)) return vscode.Uri.file(candidate);
  }
  return undefined;
}

async function sendUplink(
  session: SessionManager,
  id: string,
  label: string,
  dangerous: boolean,
): Promise<void> {
  if (!session.state.connected) {
    void vscode.window.showWarningMessage("Bolt: not connected.");
    return;
  }
  if (dangerous) {
    if (!session.state.armed) {
      void vscode.window.showWarningMessage(`Bolt: arm uplink before sending "${label}".`);
      return;
    }
    const ok = await vscode.window.showWarningMessage(
      `Send "${label}" to the live vehicle?`,
      { modal: true },
      "Send",
    );
    if (ok !== "Send") return;
  }
  session.sendCommand(id);
  void vscode.window.showInformationMessage(`Bolt: uplink "${label}" sent.`);
}

// A .raw to export from: live/last session, or a file the user picks (works idle)
async function resolveRaw(session: SessionManager): Promise<string | undefined> {
  if (session.lastRaw && fs.existsSync(session.lastRaw)) return session.lastRaw;
  session.ensureTmp();
  const picks = await vscode.window.showOpenDialog({
    canSelectMany: false,
    filters: { "Raw capture": ["raw"] },
    openLabel: "Choose capture to export",
    defaultUri: vscode.Uri.file(session.capturesDir),
  });
  return picks?.[0]?.fsPath;
}

// Default save location, pre-filled: alongside the capture, sane filename
function suggest(raw: string, ext: string): vscode.Uri {
  const base = path.basename(raw).replace(/\.raw$/, "");
  return vscode.Uri.file(path.join(path.dirname(raw), `${base}.${ext}`));
}

async function exportManifest(session: SessionManager): Promise<void> {
  const raw = await resolveRaw(session);
  if (!raw) return;
  const dest = await vscode.window.showSaveDialog({
    filters: { JSON: ["json"] },
    saveLabel: "Export manifest",
    defaultUri: suggest(raw, "manifest.json"),
  });
  if (!dest) return;
  await vscode.window.withProgress(
    { location: vscode.ProgressLocation.Notification, title: "Bolt: exporting manifest" },
    () => session.runPostflight(raw, ["--manifest", dest.fsPath]),
  );
  const doc = await vscode.workspace.openTextDocument(dest);
  void vscode.window.showTextDocument(doc);
}

async function exportCsv(session: SessionManager): Promise<void> {
  const raw = await resolveRaw(session);
  if (!raw) return;
  const counts = session.stats?.counts ?? session.manifest?.packet_counts ?? {};
  const type =
    (await vscode.window.showQuickPick(Object.keys(counts).sort(), { placeHolder: "Packet type to export as CSV" })) ??
    (await vscode.window.showInputBox({ prompt: "Packet type (e.g. btc_env, exp1_spectrum_a)" }));
  if (!type) return;
  const dest = await vscode.window.showSaveDialog({
    filters: { CSV: ["csv"] },
    saveLabel: "Export CSV",
    defaultUri: vscode.Uri.file(path.join(path.dirname(raw), `${type}.csv`)),
  });
  if (!dest) return;
  session.ensureTmp();
  const tmpManifest = path.join(session.capturesDir, ".bolt.manifest.json");
  await vscode.window.withProgress(
    { location: vscode.ProgressLocation.Notification, title: `Bolt: exporting ${type} CSV` },
    () => session.runPostflight(raw, ["--manifest", tmpManifest, "--csv", type, "--csv-out", dest.fsPath]),
  );
  if (!fs.existsSync(dest.fsPath)) {
    void vscode.window.showWarningMessage(`Bolt: no "${type}" packets in this capture.`);
    return;
  }
  const doc = await vscode.workspace.openTextDocument(dest);
  void vscode.window.showTextDocument(doc);
}

async function exportHdf5(session: SessionManager): Promise<void> {
  const raw = await resolveRaw(session);
  if (!raw) return;
  const dest = await vscode.window.showSaveDialog({
    filters: { HDF5: ["h5"] },
    saveLabel: "Export HDF5",
    defaultUri: suggest(raw, "h5"),
  });
  if (!dest) return;
  session.ensureTmp();
  const tmpManifest = path.join(session.capturesDir, ".bolt.manifest.json");
  await vscode.window.withProgress(
    { location: vscode.ProgressLocation.Notification, title: "Bolt: exporting HDF5" },
    () => session.runPostflight(raw, ["--manifest", tmpManifest, "--hdf5", dest.fsPath]),
  );
  if (fs.existsSync(dest.fsPath)) {
    void vscode.window.showInformationMessage(`Bolt: HDF5 exported to ${dest.fsPath}`);
  } else {
    void vscode.window.showWarningMessage(
      "Bolt: HDF5 needs a build with `--features hdf5` (libhdf5). Manifest/CSV export work without it.",
    );
  }
}

async function pickSerialPort(session: SessionManager): Promise<string | undefined> {
  const ports = await session.listPorts();
  const manualEntry = async () =>
    vscode.window.showInputBox({ prompt: "Serial device path", value: "/dev/ttyUSB0" });

  if (ports.length === 0) {
    void vscode.window.showWarningMessage("Bolt: no serial ports detected (is the adapter passed through?).");
    return manualEntry();
  }

  type Item = vscode.QuickPickItem & { port?: string };
  const items: Item[] = ports.map((p) => {
    const name = [p.product, p.manufacturer].filter(Boolean).join(" · ");
    const ids = p.vid != null ? `${hex4(p.vid)}:${hex4(p.pid ?? 0)} (${p.kind})` : p.kind;
    return {
      label: `$(plug) ${p.port}`,
      description: name || ids,
      detail: name ? ids : undefined,
      port: p.port,
    };
  });
  items.push({ label: "$(edit) Enter manually…" });

  const pick = await vscode.window.showQuickPick(items, {
    placeHolder:
      ports.length === 1 ? `Detected ${ports[0].port} — Enter to connect` : "Select serial adapter",
    matchOnDescription: true,
  });
  if (!pick) return undefined;
  return pick.port ?? manualEntry();
}

function hex4(n: number): string {
  return "0x" + n.toString(16).padStart(4, "0");
}

function camel(id: string): string {
  return id.replace(/_([a-z])/g, (_, c) => c.toUpperCase());
}

export function deactivate(): void {}
