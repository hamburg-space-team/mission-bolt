# Assisted Development Policy

Status: proposed. Decision recorded in
[ADR-013](../../decisions/ADR-013-assisted-development.md).

This page is binding for everything in this repository, including the
flight tree. It says when output produced with an assistant may be
merged. It does not say which files an assistant may touch, because a
rule of that shape is not enforceable and would suggest a degree of
control we do not have.

## Two acceptance criteria

Both must hold. Neither substitutes for the other.

### 1. Machine-checkable properties are checked by a machine

Every change passes the gates in
[`scripts/verify.sh`](../../../scripts/verify.sh) regardless of how it
was produced (`--list` names the stages). Today that means: the four
Debug flight builds, the Catch2 host test suite, the wire-schema drift
check over `schema.json`, `ICD-007.md` and `icd-007.tex`, the standalone
compile of every `interfaces/` header in both the flight and the
reflection view, the flight-invariant checks (no allocating `operator
new` in any image, no HAL include across the layer boundary), the Rust
workspace build and tests with `fmt` and `clippy`, the extension build,
the provenance check, and `clang-format` plus `clang-tidy` on the flight
tree. Nothing merges because it looked plausible.

The invariant checks exist because the classification in
[tool-classification.md](tool-classification.md) named them as the gaps
most likely to be hit by idiomatic-looking assistant code. Both live in
[`scripts/check-flight-invariants.sh`](../../../scripts/check-flight-invariants.sh)
and both carry a `--selftest` that rejects a known-bad input, which the
stage runs first. The flight images are allocator-free by construction
(littlefs on static buffers with `LFS_NO_MALLOC`, assert and log paths
trap instead of printing, `operator delete` replaced; see
`shared/utils/assert_trap.cpp`), and the check rejects any allocator
symbol that reappears. One residual remains and is stated rather than
implied away: eight `shared/` files legitimately include HAL and are
allowlisted; the allowlist is a ratchet, so a ninth fails.

This is the same principle as the generated wire format. Where a
property can be checked mechanically, we do not rely on the care of
whoever produced it. The principle matters more, not less, when part of
the text is machine-produced, because an assistant emits
confident-looking output faster than review can absorb on trust.

### 2. Code that the author cannot explain is removed

Not annotated. Not deferred. Not left in place with a note saying it
seemed to work. **Removed**, and written again in a form the author
understands, or replaced with a simpler approach.

The characteristic failure of an assistant is not bad code. It is code
that is correct in appearance, plausible in structure, and understood by
nobody. That code passes review by consensus, because each reviewer
assumes another followed it. A fault inside it is undiagnosable at the
moment it matters, which for a payload that flies once is the same as
unrecoverable. Assistant output also carries no signal of its own
uncertainty, so the author has to supply that signal, and the only
reliable form it takes is being able to explain the code with the
assistant closed.

"The author" means whoever opens the pull request. A reviewer who cannot
follow a change asks for it to be rewritten rather than approving it
with a comment.

The rule costs throughput. We accept the cost for the reason given in
P-3: we do not need the software written quickly, we need to be able to
convince ourselves and our reviewers that it is correct.

## Excluded from assistance

Four categories are written by people, because they are the reference
against which everything else is judged. Using an assistant on the
standard *and* on the thing being measured against it removes the
independence that makes the measurement mean anything.

| Category | Why |
|---|---|
| [System invariants](../system-invariants.md) | They are the standard our review applies |
| Architecture decisions in [`docs/decisions/`](../../decisions/) | The decision and its reasoning are ours to make and defend. Assistance with the wording of an accepted decision is allowed; assistance in reaching it is not |
| Expected value ranges for built-in test | A generated expectation compared against a generated measurement proves nothing |
| Safety assessments | Including the optical hazard evaluation for the EXP3 charging emitter. An assessment is a claim we sign, not a draft we accept |

## Interface definitions need extra care

The schema drift check proves that the generated artefacts agree with
the annotated struct. It cannot tell whether the annotation is right. A
scale factor that is wrong in the annotation is wrong consistently in
the flight binary, the decoder and the ICD, and the generation step
propagates the mistake instead of catching it.

Any change to a unit, scale, offset or validity gate is therefore
reviewed against the datasheet by a person, and the review is noted in
the pull request. This is the one place where the machinery gives a
false sense of coverage.

## One correction the layer check surfaced

Earlier drafts of this policy cited the layer boundary as invariant I-5.
The real I-5 in [system-invariants.md](../system-invariants.md) is loose
inter-controller coupling. The no-HAL-in-mission-logic rule is a
structural rule from the `btc/app` split (`board/` owns the HAL,
`btc/`+`protocol/` stay clean); it is enforced by the `layers` check and
cited by name, not by an invariant number it does not have.

## Writing

Documentation and prose follow
[writing-style.md](../documentation/writing-style.md). It applies to
everyone, not only to assisted text, because padding is padding whoever
produced it. The reason it sits next to this policy is that a document
which sounds complete about unfinished work is the written form of a
silently substituted measurement, and P-4 forbids both.

## Provenance

Assisted contributions carry a trailer. See
[provenance.md](provenance.md). The trailer is checked locally by a hook
and again in CI, so the disclosure is a property of the repository
rather than a habit.

## Reading order for a new contributor

1. This page.
2. [`tool-classification.md`](tool-classification.md) for what each
   assisted workflow can break and what catches it.
3. [`provenance.md`](provenance.md) for the commit convention.
4. [`AGENTS.md`](../../../AGENTS.md) at the repository root, which is
   the entry point an assistant reads.

## Related

- [ADR-013](../../decisions/ADR-013-assisted-development.md) - the decision, alternatives and consequences
- [System invariants](../system-invariants.md)
- [Commit messages](../process/commit-messages.md)
- [C++ coding standard](../coding/cpp.md)
