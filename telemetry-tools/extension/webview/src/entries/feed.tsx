import { Component, type ReactNode } from "react";
import { createRoot } from "react-dom/client";
import "../assets/theme.css";
import { Feed } from "../components/Feed/Feed";

const errStyle = { whiteSpace: "pre-wrap", color: "var(--vscode-errorForeground)", fontSize: 11 } as const;

// Without this a throw during render unmounts the whole tree, leaving a blank
// panel and no clue why. Renders the failure in place instead
class ErrorBoundary extends Component<{ children: ReactNode }, { error: Error | null }> {
  state: { error: Error | null } = { error: null };

  static getDerivedStateFromError(error: Error) {
    return { error };
  }

  componentDidCatch(error: Error, info: { componentStack?: string | null }) {
    console.error("[bolt/feed] render failed:", error, info.componentStack);
  }

  render() {
    const { error } = this.state;
    if (!error) return this.props.children;
    return <pre style={errStyle}>{`${error.name}: ${error.message}\n\n${error.stack ?? ""}`}</pre>;
  }
}

// Boundaries only cover render/lifecycle - a throw in the host-message handler
// or a rejected promise would still vanish silently
const root = document.getElementById("root")!;
const showFatal = (label: string, e: unknown) => {
  console.error(`[bolt/feed] ${label}:`, e);
  const detail = e instanceof Error ? `${e.name}: ${e.message}\n\n${e.stack ?? ""}` : String(e);
  root.insertAdjacentHTML("afterbegin", `<pre style="white-space:pre-wrap;color:var(--vscode-errorForeground);font-size:11px"></pre>`);
  root.firstChild!.textContent = `${label}\n\n${detail}`;
};
window.addEventListener("error", (e) => showFatal("uncaught error", e.error ?? e.message));
window.addEventListener("unhandledrejection", (e) => showFatal("unhandled rejection", e.reason));

createRoot(root).render(
  <ErrorBoundary>
    <Feed />
  </ErrorBoundary>,
);
