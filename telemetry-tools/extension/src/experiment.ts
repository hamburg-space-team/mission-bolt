import * as vscode from "vscode";

import { SessionManager } from "./session";
import { FrameMsg } from "./protocol";
import { webviewHtml, mediaRoots } from "./shell";

export class ExperimentView {
  constructor(
    private readonly ctx: vscode.ExtensionContext,
    private readonly session: SessionManager,
  ) {}

  open(source: string): void {
    const panel = vscode.window.createWebviewPanel("bolt.experiment", `Bolt · ${source.toUpperCase()}`, vscode.ViewColumn.Active, {
      enableScripts: true,
      retainContextWhenHidden: true,
      localResourceRoots: mediaRoots(this.ctx.extensionUri),
    });
    panel.webview.html = webviewHtml(panel.webview, this.ctx.extensionUri, "experiment", `Bolt · ${source.toUpperCase()}`);

    const post = (msg: unknown) => panel.webview.postMessage(msg);
    const subs = [
      this.session.onFrame((f: FrameMsg) => f.source === source && post({ type: "frame", frame: f })),
      this.session.onReset(() => post({ type: "reset" })),
    ];
    panel.webview.onDidReceiveMessage((m) => {
      if (m?.type !== "ready") return;
      post({ type: "experiment", source });
      for (const f of this.session.recentFrames) if (f.source === source) post({ type: "frame", frame: f });
    });
    panel.onDidDispose(() => subs.forEach((d) => d.dispose()));
  }
}
