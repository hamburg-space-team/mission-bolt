import * as fs from "fs";
import * as path from "path";

// One capture's cache slot: an indexed SQLite DB decoded once from the raw
export interface CacheEntry {
  rawPath: string;
  fingerprint: string; // size+mtime; changes invalidate the slot
  dir: string;
  dbPath: string; // packet DB (bolt-postflight --db)
  manifestPath: string;
}

// Disk cache: a .raw is decoded once into an indexed SQLite DB (keyed by
// size+mtime); later views query it instead of re-decoding. Under <captures>/.cache
export class CaptureCache {
  private static readonly MAX_BYTES = 8 * 1024 * 1024 * 1024; // 8 GB, LRU-evicted

  constructor(private readonly rootFn: () => string) {}

  private root(): string {
    return path.join(this.rootFn(), ".cache");
  }

  // size+mtime detects a changed/regrown raw without hashing gigabytes
  fingerprint(rawPath: string): string {
    const st = fs.statSync(rawPath);
    return `${st.size}-${Math.round(st.mtimeMs)}`;
  }

  entryFor(rawPath: string): CacheEntry {
    const fp = this.fingerprint(rawPath);
    const base = path.basename(rawPath).replace(/[^\w.-]/g, "_");
    const dir = path.join(this.root(), `${base}__${fp}`);
    return {
      rawPath,
      fingerprint: fp,
      dir,
      dbPath: path.join(dir, "packets.sqlite"),
      manifestPath: path.join(dir, "manifest.json"),
    };
  }

  // Usable only once the build finished (.ready written after DB + manifest)
  isReady(entry: CacheEntry): boolean {
    return (
      fs.existsSync(path.join(entry.dir, ".ready")) &&
      fs.existsSync(entry.dbPath) &&
      fs.existsSync(entry.manifestPath)
    );
  }

  prepareDir(entry: CacheEntry): void {
    fs.rmSync(entry.dir, { recursive: true, force: true });
    fs.mkdirSync(entry.dir, { recursive: true });
  }

  markReady(entry: CacheEntry): void {
    fs.writeFileSync(path.join(entry.dir, ".ready"), entry.fingerprint);
    this.touch(entry);
    this.evict();
  }

  // Bump mtime so LRU sees this slot as most-recently used
  touch(entry: CacheEntry): void {
    const now = new Date();
    try {
      fs.utimesSync(entry.dir, now, now);
    } catch {
      /* best effort */
    }
  }

  // Drop oldest slots until the cache fits under MAX_BYTES
  private evict(): void {
    let slots: Array<{ dir: string; mtime: number; size: number }> = [];
    try {
      slots = fs
        .readdirSync(this.root())
        .map((n) => path.join(this.root(), n))
        .filter((p) => fs.statSync(p).isDirectory())
        .map((p) => ({ dir: p, mtime: fs.statSync(p).mtimeMs, size: dirSize(p) }));
    } catch {
      return;
    }
    let total = slots.reduce((a, s) => a + s.size, 0);
    if (total <= CaptureCache.MAX_BYTES) return;
    slots.sort((a, b) => a.mtime - b.mtime); // oldest first
    for (const s of slots) {
      if (total <= CaptureCache.MAX_BYTES) break;
      fs.rmSync(s.dir, { recursive: true, force: true });
      total -= s.size;
    }
  }
}

function dirSize(dir: string): number {
  let total = 0;
  try {
    for (const name of fs.readdirSync(dir)) {
      const st = fs.statSync(path.join(dir, name));
      total += st.isDirectory() ? dirSize(path.join(dir, name)) : st.size;
    }
  } catch {
    /* ignore */
  }
  return total;
}
