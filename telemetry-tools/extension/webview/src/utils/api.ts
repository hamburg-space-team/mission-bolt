import { useEffect, useRef } from "react";
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

/**
 * Subscribe to host messages; posts `ready` once on mount.
 *
 * The listener is registered once (so `ready` is posted once), but it must not
 * capture the handler from the first render: callers close over state, and a
 * frozen handler would read stale values forever. Dispatch through a ref that
 * every render refreshes instead.
 */
export function useMessages(handler: (msg: ToWebview) => void): void {
  const latest = useRef(handler);
  latest.current = handler;

  useEffect(() => {
    const onMessage = (e: MessageEvent) => latest.current(e.data as ToWebview);
    window.addEventListener("message", onMessage);
    post({ type: "ready" });
    return () => window.removeEventListener("message", onMessage);
  }, []);
}
