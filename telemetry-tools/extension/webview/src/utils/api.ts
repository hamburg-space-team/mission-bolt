import { useEffect } from "react";
import type { ToWebview, FromWebview } from "../../../src/messages";

interface VsCodeApi {
  postMessage(msg: unknown): void;
  getState(): unknown;
  setState(state: unknown): void;
}
declare function acquireVsCodeApi(): VsCodeApi;

const vscode = acquireVsCodeApi();

export function post(msg: FromWebview): void {
  vscode.postMessage(msg);
}

// Persist a little identity (source / packet name) so the host's panel
// serializer can rebuild this view + reload its data after a window reload.
export function saveState(state: Record<string, unknown>): void {
  vscode.setState(state);
}

/** Subscribe to host messages; posts `ready` once on mount. */
export function useMessages(handler: (msg: ToWebview) => void): void {
  useEffect(() => {
    const onMessage = (e: MessageEvent) => handler(e.data as ToWebview);
    window.addEventListener("message", onMessage);
    post({ type: "ready" });
    return () => window.removeEventListener("message", onMessage);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);
}
