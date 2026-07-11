import * as vscode from "vscode";

import { SessionManager } from "./session";
import { FrameMsg, Manifest, StatsMsg } from "./protocol";
import { PanelFeedProvider } from "./panel";
import { PacketRecord } from "./messages";
import { webviewHtml, mediaRoots } from "./shell";

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
    const panel = vscode.window.createWebviewPanel("bolt.overviewLive", "Bolt · Live", vscode.ViewColumn.Active, {
      enableScripts: true,
      retainContextWhenHidden: true,
      localResourceRoots: mediaRoots(this.ctx.extensionUri),
    });
    panel.webview.html = webviewHtml(panel.webview, this.ctx.extensionUri, "overview", "Bolt · Live");
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
    const panel = vscode.window.createWebviewPanel("bolt.packets", `${name} · ${records.length}`, vscode.ViewColumn.Active, {
      enableScripts: true,
      retainContextWhenHidden: true,
      localResourceRoots: mediaRoots(this.ctx.extensionUri),
    });
    panel.webview.html = webviewHtml(panel.webview, this.ctx.extensionUri, "packets", name);
    panel.webview.onDidReceiveMessage((m) => {
      if (m?.type === "ready") panel.webview.postMessage({ type: "packets", name, records });
    });
  }

  private mount(webview: vscode.Webview, title: string): void {
    webview.options = { enableScripts: true, localResourceRoots: mediaRoots(this.ctx.extensionUri) };
    webview.html = webviewHtml(webview, this.ctx.extensionUri, "overview", title);
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
