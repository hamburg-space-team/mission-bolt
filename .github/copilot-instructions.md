# Copilot instructions

Read [AGENTS.md](../AGENTS.md) at the repository root first; it is the
entry point for every assistant and covers the rules that are not
inferable from the tree (generated files, wire-format regeneration, the
binding invariants, commit provenance).

Task procedures live in `docs/guides/` and are tool-neutral:

- [Changing the wire format](../docs/guides/changing-the-wire-format.md)
- [Reviewing firmware changes](../docs/guides/reviewing-firmware-changes.md)
- [Adding a driver](../docs/guides/adding-a-driver.md)

Verify with `./scripts/verify.sh` before claiming a change done. A
substantially assisted commit carries `Assisted-by: <agent>:<model>` and
a human `Signed-off-by:`; never add the `Signed-off-by:` yourself
(docs/standards/ai/provenance.md).
