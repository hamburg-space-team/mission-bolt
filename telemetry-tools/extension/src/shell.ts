import * as vscode from "vscode";
import * as fs from "fs";
import * as path from "path";

interface ManifestEntry {
  file: string;
  css?: string[];
  imports?: string[];
}
type Manifest = Record<string, ManifestEntry>;

function collectCss(manifest: Manifest, key: string, seen = new Set<string>()): string[] {
  if (seen.has(key)) return [];
  seen.add(key);
  const entry = manifest[key];
  if (!entry) return [];
  const css = [...(entry.css ?? [])];
  for (const imp of entry.imports ?? []) css.push(...collectCss(manifest, imp, seen));
  return css;
}

// localResourceRoots for a webview loading Vite bundles from media/
export function mediaRoots(extensionUri: vscode.Uri): vscode.Uri[] {
  return [vscode.Uri.joinPath(extensionUri, "media")];
}

// Build the HTML shell for a Vite-bundled React webview; resolves JS + CSS
// from the Vite manifest (the shared vendor chunk loads via ES-module imports)
export function webviewHtml(
  webview: vscode.Webview,
  extensionUri: vscode.Uri,
  entry: string,
  title: string,
): string {
  const dir = path.join(extensionUri.fsPath, "media", "webview");
  const uri = (rel: string) =>
    webview.asWebviewUri(vscode.Uri.joinPath(extensionUri, "media", "webview", rel)).toString();

  let jsFile = `${entry}.js`;
  let cssFiles: string[] = [];
  try {
    const manifest: Manifest = JSON.parse(fs.readFileSync(path.join(dir, ".vite", "manifest.json"), "utf8"));
    const key = `webview/src/entries/${entry}.tsx`;
    if (manifest[key]) {
      jsFile = manifest[key].file;
      cssFiles = [...new Set(collectCss(manifest, key))];
    }
  } catch {
    /* manifest missing (unbuilt): fall back to <entry>.js */
  }

  const nonce = String(Math.random()).slice(2) + String(Math.random()).slice(2);

  const csp = [
    `default-src 'none'`,
    `style-src ${webview.cspSource} 'unsafe-inline'`,
    `script-src ${webview.cspSource} 'nonce-${nonce}'`,
    `connect-src ${webview.cspSource}`,
    `font-src ${webview.cspSource}`,
    `img-src ${webview.cspSource} data:`,
  ].join("; ");

  const links = cssFiles.map((c) => `<link rel="stylesheet" href="${uri(c)}" />`).join("");

  return `
  <!doctype html>
  <html>
    <head>
      <meta charset="utf-8" />
      <meta http-equiv="Content-Security-Policy" content="${csp}" />
      <title>${title}</title>${links}
    </head>
  <body>
    <div id="root"></div>
    <script type="module" nonce="${nonce}" src="${uri(jsFile)}"></script>
  </body>
  </html>
`;
}
