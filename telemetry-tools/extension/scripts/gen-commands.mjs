// Generate src/protocol.gen.ts (UPLINK_COMMANDS) from the wire schema.
//
// Single source: interfaces/tools/generated/schema.json - the CommandOpcode enum
// and its `.danger` annotation. So the command list, labels and the dangerous
// flag live in one place (bolt/wire/uplink.hpp) and can never drift from the
// firmware / the ground codec. Do not hand-edit src/protocol.gen.ts.
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

const ts = `// @generated from interfaces/tools/generated/schema.json (CommandOpcode) - do not edit.
// Regenerate with: npm run gen:commands  (or tools/schemagen/run-schemagen.sh upstream).
export const UPLINK_COMMANDS = [
${rows}
] as const;
`;

writeFileSync(outPath, ts);
console.log(`wrote ${outPath} (${cmd.values.length} commands)`);
