# ADR-013: Assisted Development Under Verified Acceptance

## Status

Proposed

## Date

2026-07-26

## Context

Coding assistants are already used in this project, including in the
flight tree. Prohibiting them is not enforceable with the tooling and the
team structure we have, and it would not be the right call even if it
were: they are useful, and the practice is becoming ordinary in the
industry our members will work in.

That leaves the question of on what basis assisted output may be merged
into software that flies once, with no opportunity to patch after
lift-off.

Two properties of assistants shape the answer. They produce output that
is correct in appearance regardless of whether it is correct in fact, and
they produce it faster than review can absorb on trust. Neither is a
statement about quality. Both are statements about what a review process
has to be able to withstand.

We are in an unusually good position to answer, because the repository
already contains the machinery an answer needs. The system invariants
state what must hold. The interface control documents state what the
contracts are. The wire format is generated from the flight structs, so
the decoder and the documentation cannot diverge from the code by
construction. Static analysis and host unit tests already run on every
change; an allocator-symbol check over the flight images and a
layer-boundary check land together with this decision, each demonstrated
to fail on a known-bad input before being trusted. The question is
therefore not whether to build a verification chain but whether to state
explicitly that assisted output is accepted on the strength of it.

REXUS does not require DO-178C or full ECSS software compliance, so no
external standard obliges us to classify our tools. We do it anyway
because the classification question is the useful one.

## Decision

We permit assisted development everywhere in this repository, including
the flight tree, and we accept its output on two conditions that must
both hold.

First, every machine-checkable property is checked by a machine, through
the same gates that apply to code written by hand. Second, code that its
author cannot explain with the assistant closed is removed rather than
kept, annotated or deferred.

We do not restrict which files an assistant may touch. A rule of that
shape is not enforceable and would suggest a degree of control we do not
have. We restrict what may be accepted without an independent check, and
there the answer is nothing.

Four categories are written without assistance, because they are the
reference the rest is measured against: the system invariants, the
architecture decisions, the expected value ranges used to judge built-in
test results, and safety assessments.

Each assisted workflow is classified by what an undetected error in it
would cause and by what would detect that error independently, following
the reasoning of DO-330 without claiming to meet its objectives. The
classification is recorded in
[`docs/standards/ai/tool-classification.md`](../standards/ai/tool-classification.md)
and it is the artefact we maintain, rather than the policy statement.

Assisted contributions carry an `Assisted-by:` trailer and a human
`Signed-off-by:`. An assistant never signs off. The rule is enforced by a
commit hook and re-checked in CI.

## Alternatives Considered

### Alternative A: Prohibit assistants in the flight tree

Restrict assistance to ground software, tooling and documentation, and
require the flight tree to be written by hand.

**Rejected because:** it is not enforceable, and an unenforceable rule
that we state publicly is worse than an accurate one. We would be
claiming a boundary we cannot demonstrate, and a reviewer who found a
counter-example would have reason to doubt everything else we assert. The
boundary that is actually enforceable is the acceptance criterion, not the
file path.

### Alternative B: Permit assistance with no policy

Treat assistants as ordinary editor tooling and rely on existing review.

**Rejected because:** the existing review process was designed for the
rate and the failure modes of human authorship. It assumes an author who
hesitates when unsure. Assistant output carries no hesitation, so the
signal review depends on is absent, and the process degrades silently
rather than visibly. Writing nothing down also leaves us unable to answer
the disclosure question the SED needs answered.

### Alternative C: Require full tool qualification

Treat the assistant as a development tool under DO-330 and qualify it to
the level its classification implies.

**Rejected because:** it is not achievable. Qualification requires
deterministic behaviour and access to development evidence, and we have
neither. The same prompt does not reliably produce the same output, and
the tool's internals are not available to us. Attempting it would produce
a document that claims more than it can support. The defensible posture is
the opposite: treat the workflow as an unqualified tool and require
independent detection of its errors.

### Alternative D: Percentage limits on assisted code

Cap the share of a change that may be assisted, measured by tooling.

**Rejected because:** the measure has no relationship to the risk. A
single wrong scale factor is more damaging than an entire correctly
generated driver, and a cap creates an incentive to disguise rather than
disclose. Comprehension is the property we care about, and it does not
correlate with line counts.

## Consequences

### Positive

- The acceptance criterion is stated and, for the machine-checkable half,
  auditable rather than asserted.
- The classification exercise surfaced a result we had not seen: an
  assistant writing verification code is more dangerous than one writing
  flight code, because a defect in the verification makes everything
  downstream look correct. Every new check now lands with a demonstration
  that it fails on a known-bad input. The rule caught two defects in our
  own checks before they were trusted (recorded in the classification).
- It named the two places with no mechanical net: interface annotation
  values and safety assessments. Both now have explicit human owners.
- Provenance is recorded in the repository, so the SED can describe the
  practice with evidence behind it.
- The skills library documents our procedures in a form people read too,
  which helps onboarding independently of any assistant.

### Negative

- The comprehension rule costs throughput, sometimes considerably, and it
  will be under most pressure close to a deadline, which is exactly when
  it matters most.
- Verification code now needs more review effort than flight code, which
  is counter-intuitive and will need explaining to every new member.
- Provenance trailers add friction to every commit and depend on a hook
  that a fresh clone does not install automatically (mitigated in the
  devcontainer, where VS Code signs off automatically, and by the CI
  stage that re-checks the branch).
- We carry a policy no external authority has reviewed. If a programme
  requirement appears later, we may have to redo it.

### Neutral

- The classification borrows DO-330's reasoning without claiming its
  objectives. That has to be stated wherever the classification is cited,
  or it will be read as a compliance claim.
- The policy applies to the whole repository, so ground software carries
  the same disclosure requirement as flight code even though its risk
  profile is different.

## Implementation Notes

All five items exist; the order mattered because the later ones depend on
the earlier ones being real rather than intended.

1. `scripts/hooks/commit-msg` and `scripts/check-provenance.sh`, one
   shared implementation with a `--selftest` that fails on both bad cases.
2. The `provenance` stage in `verify.sh` and CI (which fetches full
   history so the range check has something to check).
3. The pull request template with the two questions: can you explain
   every line, and who checked any changed unit or scale against the
   datasheet.
4. `AGENTS.md` at the repository root as the entry point an assistant
   reads, pointing at the standards rather than restating them.
5. The skills library under `.claude/skills/`, procedures first for the
   workflows where getting the sequence wrong is expensive: wire format
   changes, firmware review against the invariants, adding a driver.

The fail-first requirement for new checks is the item most likely to be
skipped under time pressure. It is also the one whose absence is
invisible, which is the argument for holding it.

## References

- Related ADRs: [ADR-012](ADR-012-error-step-trace.md) (wire-stable
  diagnostics, the same argument for machine-enforced contracts)
- [Assisted development policy](../standards/ai/policy.md)
- [Classification of assisted workflows](../standards/ai/tool-classification.md)
- [Provenance of assisted contributions](../standards/ai/provenance.md)
- [System invariants](../standards/system-invariants.md)
- External: RTCA DO-330 / EUROCAE ED-215, software tool qualification
- External: Linux kernel AI coding assistants policy, for the
  `Assisted-by:` trailer form and the rule that an agent never signs off

## Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-07-26 | Max Atslega | Initial draft |
