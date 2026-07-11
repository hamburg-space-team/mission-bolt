/** Human-readable cell value (arrays, nested objects, numbers). */
export function fmtValue(v: unknown): string {
  if (v == null) return "";
  if (Array.isArray(v)) return v.join(" → ");
  if (typeof v === "object") {
    return Object.entries(v as Record<string, unknown>)
      .map(([k, x]) => `${k}=${x}`)
      .join(", ");
  }
  if (typeof v === "number") return Number.isInteger(v) ? String(v) : v.toFixed(3);
  return String(v);
}

export const magnitude = (a: unknown): number =>
  Array.isArray(a) ? Math.hypot(Number(a[0]), Number(a[1]), Number(a[2])) : 0;
