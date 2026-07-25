# `interfaces/` — THHOR-BOLT wire ABI

The single source of truth for the on-wire packet contract shared between the
**flight software** (C++, `flight-software/`) and the **ground codec**
(Rust, `telemetry-tools/crates/bolt-codec`). Header-only; no build of its own.

## Layout

```text
interfaces/
├── include/
│   └── bolt/
│       ├── wire.hpp              umbrella — pulls in the whole contract
│       └── wire/
│           ├── annotations.hpp   WIRE(...) / PACKET(...) reflection annotations
│           ├── header.hpp        PacketHeader + wire constants (sync, sizes, version)
│           ├── types.hpp         PayloadType, GapReason, NodeId, BootReason
│           ├── uplink.hpp        CommandOpcode, CommandAckStatus (RXSM → BTC)
│           └── payloads.hpp      per-type payload structs (WIRE/PACKET annotated)
└── tools/
    ├── schemagen/                host-only C++26 reflection generator (clang-p2996)
    └── generated/
        └── schema.json           generated — consumed by bolt-codec's build.rs
```

## Consuming the headers

```cpp
#include <bolt/wire/payloads.hpp>   // one type
#include <bolt/wire.hpp>            // everything
```

Add `interfaces/include` to the include path:

- **Flight build** — `add-path` in `flight-software/shared/shared.clayer.yml`.
- **clangd / IDE** — `-I../interfaces/include` in `flight-software/.clangd`; this
  folder's own `.clangd` covers editing the headers standalone.

## Regenerating the schema + ICD

`tools/schemagen` reflects over the annotated structs (real C++26 P2996 +
P3394) and regenerates three artefacts from the annotations — the single source,
so they never drift:

- `tools/generated/schema.json` — the ground codec's decode tables (`bolt-codec`)
- `docs/interfaces/ICD-007-packet-payloads.md` — the human interface control doc
- `tools/generated/icd-007.tex` — the same tables as LaTeX for the SED appendix
  (`\input` it under the appendix subsection; needs `float` + `siunitx`)

```sh
tools/schemagen/run-schemagen.sh          # VS Code: Run Build Task
```

Needs the clang-p2996 toolchain (baked into the devcontainer;
`../scripts/install-clang-p2996.sh` otherwise). The compile also enforces
the contract's invariants (every field annotated, packed layout tight, every
struct has a `PACKET`), so a broken annotation fails regeneration.

## Editing without compile errors

The annotation macros (`WIRE`, `PACKET`) are real C++26 annotations only under
`-DBOLT_SCHEMAGEN`; otherwise they expand to nothing. clangd is configured
(`.clangd`) **without** that define, so the headers parse as plain C++ and edit
cleanly. Only `tools/schemagen/schemagen.cpp` itself needs the reflection
compiler; its diagnostics are suppressed in the IDE.
