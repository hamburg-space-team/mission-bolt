import * as vscode from "vscode";

import { SessionManager } from "./session";
import { FrameMsg, Manifest, StatsMsg } from "./protocol";
import { PanelFeedProvider } from "./panel";
import { PacketRecord } from "./messages";
import { webviewHtml, mediaRoots } from "./shell";
import { errorName, faultName, faultNode } from "./codes";

export { PacketRecord };

export class OverviewProvider implements vscode.CustomReadonlyEditorProvider {
  constructor(
    private readonly ctx: vscode.ExtensionContext,
    private readonly session: SessionManager,
    private readonly panel: PanelFeedProvider,
  ) {}

  openCustomDocument(uri: vscode.Uri): vscode.CustomDocument {
    return { uri, dispose: () => {} };
  }

  async resolveCustomEditor(document: vscode.CustomDocument, panel: vscode.WebviewPanel): Promise<void> {
    this.mount(panel.webview, "Bolt Overview");
    let manifest: Manifest | undefined;
    panel.webview.onDidReceiveMessage((m) => {
      if (m?.type === "ready" && manifest) panel.webview.postMessage({ type: "init", live: false, manifest });
    });
    try {
      manifest = await this.session.openCapture(document.uri.fsPath);
      panel.webview.postMessage({ type: "init", live: false, manifest });
      void this.panel.loadCapture(document.uri.fsPath);
    } catch (e) {
      void vscode.window.showErrorMessage(`Bolt: ${(e as Error).message}`);
    }
  }

  openLive(): void {
    this.attachLive(this.newPanel("bolt.overviewLive", "Bolt · Live"));
  }

  // Wire a new or restored (serializer) live overview panel.
  attachLive(panel: vscode.WebviewPanel): void {
    panel.title = "Bolt · Live";
    this.mount(panel.webview, panel.title, "overview");
    const subs: vscode.Disposable[] = [
      this.session.onFrame((f: FrameMsg) => panel.webview.postMessage({ type: "frame", frame: f })),
      this.session.onStats((s: StatsMsg) => panel.webview.postMessage({ type: "stats", stats: s })),
    ];
    panel.webview.onDidReceiveMessage((m) => {
      if (m?.type !== "ready") return;
      panel.webview.postMessage({ type: "init", live: true, manifest: emptyManifest(this.session) });
      if (this.session.stats) panel.webview.postMessage({ type: "stats", stats: this.session.stats });
    });
    panel.onDidDispose(() => subs.forEach((d) => d.dispose()));
  }

  showPackets(name: string, records: PacketRecord[]): void {
    this.wirePackets(this.newPanel("bolt.packets", `${name} · ${records.length}`), name, records);
  }

  // Restore a packets panel after a reload: re-query the capture for `name`.
  async attachPackets(panel: vscode.WebviewPanel, name: string): Promise<void> {
    const raw = this.session.lastRaw;
    let records: PacketRecord[] = [];
    if (raw && name) {
      const limit = vscode.workspace.getConfiguration("bolt").get<number>("tableRowLimit", 100000);
      try {
        records = (await this.session.queryCapture(raw, { type: name, limit })) as unknown as PacketRecord[];
        enrichFaults(records);
      } catch {
        /* leave empty */
      }
    }
    this.wirePackets(panel, name, records);
  }

  private wirePackets(panel: vscode.WebviewPanel, name: string, records: PacketRecord[]): void {
    panel.title = `${name} · ${records.length}`;
    this.mount(panel.webview, name, "packets");
    panel.webview.onDidReceiveMessage((m) => {
      if (m?.type === "ready") panel.webview.postMessage({ type: "packets", name, records });
    });
  }

  private newPanel(viewType: string, title: string): vscode.WebviewPanel {
    return vscode.window.createWebviewPanel(viewType, title, vscode.ViewColumn.Active, {
      enableScripts: true,
      retainContextWhenHidden: true,
      localResourceRoots: mediaRoots(this.ctx.extensionUri),
    });
  }

  private mount(webview: vscode.Webview, title: string, entry = "overview"): void {
    webview.options = { enableScripts: true, localResourceRoots: mediaRoots(this.ctx.extensionUri) };
    webview.html = webviewHtml(webview, this.ctx.extensionUri, entry, title);
  }
}

// Decode fault device/error/node for the table (same as the command path).
export function enrichFaults(records: PacketRecord[]): void {
  for (const r of records) {
    const s = r.sample as Record<string, unknown>;
    if (s.kind !== "fault") continue;
    s.device = faultName(Number(s.fault_code));
    s.error = errorName(Number(s.error_code));
    if (s.node == null || s.node === "unknown") s.node = `${faultNode(Number(s.fault_code))} (inferred)`;
  }
}

function emptyManifest(session: SessionManager): Manifest {
  return {
    mission: session.state.source ? `Live · ${session.state.source}` : "Live",
    packet_counts: {},
    total_frames: 0,
    crc_fail_total: 0,
    lo_rtc_s: 0,
  } as Manifest;
}
