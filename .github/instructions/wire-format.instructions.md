---
applyTo: "interfaces/**"
---

Any change here alters the wire ABI between flight and ground. Follow
[docs/guides/changing-the-wire-format.md](../../docs/guides/changing-the-wire-format.md):
the annotated structs are the single source, everything else is
regenerated (`interfaces/tools/schemagen/run-schemagen.sh`), type bytes
and enum values are wire-stable (append, never renumber or reuse), and a
changed unit/scale/offset/gate needs a named human datasheet review in
the PR.
