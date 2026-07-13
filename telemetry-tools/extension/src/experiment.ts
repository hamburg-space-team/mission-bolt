import * as vscode from "vscode";

import { SessionManager } from "./session";
import { FrameMsg } from "./protocol";
import { PacketRecord } from "./messages";
import { webviewHtml, mediaRoots } from "./shell";

export class ExperimentView {
  constructor(
    private readonly ctx: vscode.ExtensionContext,
    private readonly session: SessionManager,
  ) {}

  open(source: unknown): void {
    const src = String(source ?? "");
    const panel = vscode.window.createWebviewPanel("bolt.experiment", `Bolt · ${src.toUpperCase()}`, vscode.ViewColumn.Active, {
      enableScripts: true,
      retainContextWhenHidden: true,
      localResourceRoots: mediaRoots(this.ctx.extensionUri),
    });
    this.attach(panel, src);
  }

  // Wire a new or restored (serializer) panel to the session.
  attach(panel: vscode.WebviewPanel, source: unknown): void {
    const src = String(source ?? "");
    panel.title = `Bolt · ${src.toUpperCase()}`;
    panel.webview.options = { enableScripts: true, localResourceRoots: mediaRoots(this.ctx.extensionUri) };
    panel.webview.html = webviewHtml(panel.webview, this.ctx.extensionUri, "experiment", panel.title);

    const post = (msg: unknown) => panel.webview.postMessage(msg);
    const subs = [
      this.session.onFrame((f: FrameMsg) => f.source === src && post({ type: "frame", frame: f })),
      this.session.onReset(() => post({ type: "reset" })),
    ];
    panel.webview.onDidReceiveMessage((m) => {
      if (m?.type !== "ready") return;
      post({ type: "experiment", source: src });
      void this.feed(src, post);
    });
    panel.onDidDispose(() => subs.forEach((d) => d.dispose()));
  }

  // Post-flight = not actively streaming but a capture exists: load its full
  // series so the playback controls appear. Live = replay recent frames.
  private async feed(src: string, post: (m: unknown) => void): Promise<void> {
    const raw = this.session.lastRaw;
    if (!this.session.state.connected && raw) {
      try {
        const records = (await this.session.queryCapture(raw, { source: src, limit: 500000 })) as unknown as PacketRecord[];
        if (records.length) {
          post({ type: "load", records });
          return;
        }
      } catch {
        /* fall through to live replay */
      }
    }
    for (const f of this.session.recentFrames) if (f.source === src) post({ type: "frame", frame: f });
  }
}
