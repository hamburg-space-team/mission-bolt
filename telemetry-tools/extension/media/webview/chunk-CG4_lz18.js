function fmtValue(v) {
  if (v == null) return "";
  if (Array.isArray(v)) return v.join(" → ");
  if (typeof v === "object") {
    return Object.entries(v).map(([k, x]) => `${k}=${x}`).join(", ");
  }
  if (typeof v === "number") return Number.isInteger(v) ? String(v) : v.toFixed(3);
  return String(v);
}
export {
  fmtValue as f
};
//# sourceMappingURL=chunk-CG4_lz18.js.map
