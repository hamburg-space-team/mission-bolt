# Changing the Wire Format

Payloads, enums, uplink commands. For any edit under
`interfaces/include/bolt/wire/`, or when a task adds or changes
telemetry fields or packet types. Written for people and assistants
alike; `.claude/skills/wire-change` points here.

The annotated structs in `interfaces/include/bolt/wire/` are the single
source of truth. Everything else is generated from them; nothing else is
hand-edited.

## Stability rules (wire contracts outlive firmware versions)

- Type bytes: high nibble names the origin node (BTC 0x1x, EXP1 0x2x,
  EXP2 0x3x, EXP3 0x4x, system 0xFx). Pick the next free value in the
  node's range.
- Never reuse a retired byte. 0xF3 (old shared TIMING) stays dead; old
  captures still carry it.
- Enums: append values, never renumber. `SENSOR_FAILED` in `GapReason`
  is wire-stable but no longer emitted; leave such entries in place.
- Payload max 50 B (64 B frame minus 12 B header minus 2 B CRC). Packed,
  no padding; schemagen's static_asserts fail the regeneration if a gap
  appears or a `WIRE(...)` line is missing.
- New per-node payloads: one struct per node (aliases carry no PACKET
  annotation; see the *_TIMING / *_TEST blocks in payloads.hpp).
- Uplink commands: mark irreversible ones `.danger = true`; the ground
  UI generates an arm+confirm step from that flag.

## Sequence

1. Edit the annotated struct/enum. Annotation reference:
   `WIRE(.unit, .scale, .offset, .gate, .desc)`, `PACKET(.type, .node,
   .rate_hz, .desc)`. Gate semantics: `field:N` = bit N of `field`,
   bare name = byte non-zero (I-6: gate anything that can be invalid).
2. `interfaces/tools/schemagen/run-schemagen.sh` regenerates
   `schema.json`, `docs/interfaces/ICD-007-packet-payloads.md` and
   `interfaces/tools/generated/icd-007.tex` (the SED appendix; copy to
   Overleaf when it changes).
3. `./scripts/verify.sh schema-drift iface-headers rust-tests ext-build`
   covers the chain: drift, both header views, the Rust codec built from
   the schema, and the extension's generated command/packet tables.
4. Commit the source change and the regenerated artefacts together.

## Human gates (the machinery cannot check these)

- A changed **unit, scale, offset or gate** needs a person to verify the
  value against the datasheet, named in the PR
  (docs/standards/ai/policy.md). The drift check only proves the
  artefacts agree with each other.
- Do NOT add bandwidth/rate budgets to ICD-001 or the generated ICD:
  the telemetry rates are being reworked and the numbers are wrong by
  decision (2026-07-17). The `Rate Hz` column is annotation data and
  stays.
