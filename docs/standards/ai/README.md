# Assisted Development

How this project uses coding assistants, and on what basis their output
is merged. Binding for the whole repository, including the flight tree.

The decision is recorded in
[ADR-013](../../decisions/ADR-013-assisted-development.md).

## In one screen

Two conditions, both required:

1. **Every machine-checkable property is checked by a machine.** The same
   gates apply regardless of who or what produced the change.
2. **Code its author cannot explain with the assistant closed is
   removed**, not annotated and not deferred.

We do not restrict which files may be touched. We restrict what may be
accepted without an independent check, and there the answer is nothing.

Four things are written without assistance, because they are the
reference the rest is measured against: the system invariants, the
architecture decisions, the expected value ranges for built-in test, and
safety assessments.

Assisted commits carry `Assisted-by:` and a human `Signed-off-by:`. An
assistant never signs off.

## Pages

| Page | Contents |
|---|---|
| [policy.md](policy.md) | The acceptance criteria, the exclusions, and why interface annotations need extra care |
| [tool-classification.md](tool-classification.md) | What each assisted workflow can break and what catches it |
| [provenance.md](provenance.md) | Commit trailer convention, the hook, the CI check |
| [../documentation/writing-style.md](../documentation/writing-style.md) | House prose style. Applies to everyone, not only to assisted text |

## Machine-facing files

These are read by assistants, not by people, but they are part of the
same system and are reviewed like any other source.

| Path | Purpose |
|---|---|
| [`AGENTS.md`](../../../AGENTS.md) | Entry point. Repository map and the rules that are not inferable from the tree. Read natively by Codex-style agents; `GEMINI.md` and `.github/copilot-instructions.md` point here |
| [`docs/guides/`](../../guides/) | The task procedures themselves, tool-neutral: wire format changes, firmware review, adding a driver. Candidates queued: adding a build target, decoding a capture, adding a self-test step |
| `.claude/skills/*/SKILL.md`, `.github/instructions/*.instructions.md`, `.cursor/rules/*.mdc` | Per-tool adapters (Claude, Copilot, Cursor) that point at the guides so no tool carries its own copy |
| `.claude/settings.json` | Blocks assistant edits to the generated CubeMX/RTE trees and vendored littlefs; reading stays allowed for debugging |
| [`scripts/hooks/commit-msg`](../../../scripts/hooks/commit-msg) | Enforces the provenance rules locally |
| [`scripts/check-provenance.sh`](../../../scripts/check-provenance.sh) | Enforces them again in CI, and carries the selftest both share |

The skills are written to be read by people as well. A procedure only a
machine reads is a procedure nobody maintains.

## The result worth knowing

The classification exercise produced one finding that changed how we
review: **an assistant writing verification code is more dangerous than
one writing flight code.** A defect in flight code is caught by the
verification. A defect in the verification makes everything downstream
look correct.

Consequence: a new or changed check lands together with a demonstration
that it fails on a known-bad input. See
[tool-classification.md](tool-classification.md), which also records the
two defects this rule has already caught in our own checks.

## Using this beyond BOLT

Nothing in the two acceptance criteria, the provenance convention or the
fail-first rule is specific to this project; they transfer to any
repository as they stand. What a new project replaces is the
instantiation:

- the gate list (`scripts/verify.sh --list` is ours; yours will differ),
- the reference documents named in the exclusions (our
  [system invariants](../system-invariants.md), your equivalent),
- the concrete residual risks in the
  [classification table](tool-classification.md), which come from our
  codebase, and
- the allowlist in `scripts/check-flight-invariants.sh`, which encodes
  our layering debt, not a principle.

The scripts take no BOLT-specific input beyond those lists and can be
copied as starting points.
