import * as vscode from "vscode";
import * as path from "path";

import { SessionManager } from "./session";
import { FrameMsg, StatsMsg } from "./protocol";
import { PacketRecord, ToWebview } from "./messages";
import { webviewHtml, mediaRoots } from "./shell";

export class PanelFeedProvider implements vscode.WebviewViewProvider {
  private view?: vscode.WebviewView;
  private loaded?: PacketRecord[];

  constructor(
    private readonly ctx: vscode.ExtensionContext,
    private readonly session: SessionManager,
  ) {
    session.onReset(() => {
      this.loaded = undefined;
      this.postMessage({ type: "reset" });
    });
  }

  private postMessage(msg: ToWebview): void {
    this.view?.webview.postMessage(msg);
  }

  resolveWebviewView(view: vscode.WebviewView): void {
    this.view = view;
    view.webview.options = { enableScripts: true, localResourceRoots: mediaRoots(this.ctx.extensionUri) };
    view.webview.html = webviewHtml(view.webview, this.ctx.extensionUri, "feed", "Bolt Feed");

    const subs = [
      this.session.onFrame((f: FrameMsg) => {
        this.loaded = undefined;
        this.postMessage({ type: "frame", frame: f });
      }),
      this.session.onStats((s: StatsMsg) => this.postMessage({ type: "counts", counts: s.counts })),
    ];
    view.webview.onDidReceiveMessage((m) => {
      if (m?.type === "showType" && m.name) {
        void vscode.commands.executeCommand("bolt.showPackets", m.name);
      } else if (m?.type === "ready" || m?.type === "reload") {
        this.replay();
      }
    });
    view.onDidDispose(() => subs.forEach((d) => d.dispose()));
  }

  // Restore the current source after a (re)build or explicit reload
  private replay(): void {
    if (this.loaded) {
      this.postMessage({ type: "load", records: this.loaded });
      return;
    }
    this.postMessage({ type: "reset" });
    for (const f of this.session.recentFrames) this.postMessage({ type: "frame", frame: f });
  }

  // Load a capture's packets (newest-first from the cache DB) into the feed
  async loadCapture(rawPath: string): Promise<void> {
    const records = (await this.session.queryCapture(rawPath, { limit: 20000 })) as unknown as PacketRecord[];
    this.loaded = records;
    await vscode.commands.executeCommand("bolt.feed.focus");
    this.postMessage({ type: "load", records, name: path.basename(rawPath) });
  }
}
