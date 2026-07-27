// Generate src/protocol.gen.ts from the wire schema.
//
// Single source: interfaces/tools/generated/schema.json. Emits:
//   - UPLINK_COMMANDS   from the CommandOpcode enum (+ its .label/.danger)
//   - PACKET_WIRE_BYTES on-wire size per packet type (payload + header + CRC)
// So the command list, labels, danger flag and packet sizes live in one place
// (bolt/wire/*) and can never drift. Do not hand-edit src/protocol.gen.ts.
import { readFileSync, writeFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";

const here = dirname(fileURLToPath(import.meta.url));
const schemaPath = join(here, "../../../interfaces/tools/generated/schema.json");
const outPath = join(here, "../src/protocol.gen.ts");

const schema = JSON.parse(readFileSync(schemaPath, "utf8"));
const cmd = (schema.enums ?? []).find((e) => e.name === "CommandOpcode");
if (!cmd) throw new Error(`CommandOpcode enum missing from ${schemaPath}`);

// label comes from the .label annotation on the command; if empty, fall back to
// the canonical enum name (e.g. RESET_TICK). No invented formatting - the label
// is whatever bolt/wire/uplink.hpp says.
const rows = cmd.values
  .map((v) => {
    const id = v.name.toLowerCase();
    const label = v.label && v.label.length > 0 ? v.label : v.name;
    return `  { id: ${JSON.stringify(id)}, label: ${JSON.stringify(label)}, `
      + `dangerous: ${Boolean(v.danger)}, desc: ${JSON.stringify(v.desc)} },`;
  })
  .join("\n");

// On-wire bytes per packet type = payload size + 12 B header + 2 B CRC, keyed by
// the snake_case name used in the live/postflight packet counts (matches
// bolt-codec's PayloadType::name()).
const OVERHEAD = 12 + 2;
const snake = (p) => {
  let out = "";
  for (let i = 0; i < p.length; i++) {
    const c = p[i];
    if (c >= "A" && c <= "Z" && i !== 0) out += "_";
    out += c.toLowerCase();
  }
  return out;
};
const sizeByLayout = Object.fromEntries((schema.payloads ?? []).map((p) => [p.name, p.size]));
const sizeRows = (schema.types ?? [])
  .map((t) => `  ${JSON.stringify(snake(t.name))}: ${(sizeByLayout[t.layout] ?? 0) + OVERHEAD},`)
  .join("\n");

// Per-node self-test tables: test_id is the enum value, the name its
// .label annotation.
const selfTestRows = [];
for (const [node, enumName] of [
  ["btc", "BtcSelfTest"],
  ["exp1", "Exp1SelfTest"],
  ["exp2", "Exp2SelfTest"],
  ["exp3", "Exp3SelfTest"],
]) {
  const e = (schema.enums ?? []).find((x) => x.name === enumName);
  if (!e) throw new Error(`${enumName} enum missing from ${schemaPath}`);
  const labels = [...e.values]
    .sort((a, b) => a.value - b.value)
    .map((v) => (v.label && v.label.length > 0 ? v.label : v.name));
  selfTestRows.push(`  ${node}: ${JSON.stringify(labels)},`);
}

const ts = `// @generated from interfaces/tools/generated/schema.json - do not edit.
// Regenerate with: npm run gen:commands  (or tools/schemagen/run-schemagen.sh upstream).
export const UPLINK_COMMANDS = [
${rows}
] as const;

// On-wire bytes per packet type (payload + 12 B header + 2 B CRC), by count name.
export const PACKET_WIRE_BYTES: Record<string, number> = {
${sizeRows}
};

// Per-node self-test step names, indexed by test_id (bolt/wire/selftest.hpp).
export const SELF_TEST_STEPS: Record<string, string[]> = {
${selfTestRows.join("\n")}
};
`;

writeFileSync(outPath, ts);
console.log(
  `wrote ${outPath} (${cmd.values.length} commands, ${(schema.types ?? []).length} packet sizes, 4 self-test tables)`,
);
