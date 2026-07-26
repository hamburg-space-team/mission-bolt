# Classification of Assisted Workflows

Status: proposed. Companion to [policy.md](policy.md).

## Why classify at all

Safety-critical practice does not ask whether a tool is good. It asks
what happens when the tool is wrong and nobody notices. DO-330, the
stand-alone tool qualification document that RTCA separated out of
DO-178B, classifies a tool by whether its output could *insert* an error
into the product or could *fail to detect* one, then sets the required
rigour from there. It was deliberately written to be domain-independent
so that it can be used outside aviation.

We are not certifying to DO-178C, and our programme (REXUS) does not
require it. We borrow the question because it is the right question, and
because it produces a table we can act on rather than a policy we can
only assert.

Applied to an assistant, the answer to the classification question is
uncomfortable and worth stating plainly: an assistant's output becomes
part of the product, which puts it in the category demanding the highest
rigour, and it cannot be qualified. It is non-deterministic, its
development evidence is unavailable to us, and the same prompt does not
reliably produce the same output. So we do not claim qualification. We
treat every assisted workflow as an unqualified tool and require that
something independent detects its errors.

## The finding that changed how we work

**An assistant writing verification code is more dangerous than one
writing flight code.**

A defect in flight code is caught by the verification. A defect in the
verification makes everything downstream look correct. A bench harness
that reports pass when it should report fail does not produce a visible
failure; it produces a green report and a false sense of coverage, and it
keeps producing them until someone thinks to distrust the harness itself.

This is DO-330's criterion-2 reasoning: a tool whose output justifies
reducing another activity has to be trusted more, not less. The mitigation
is specific and it is now a requirement: **every check must be shown to
fail on a known-bad input before it is trusted.** A new gate is landed
together with the demonstration that it rejects something it should
reject. The checks added since carry it as an executable `--selftest`
that the verify stage runs first, so the demonstration cannot silently
rot after landing.

The requirement has already earned its keep twice. The first version of
the prose check grepped for a mojibake byte instead of the em dash it
meant to find, and the first version of the allocation check flagged
placement `new`, which allocates nothing. Both were caught by writing the
known-bad (and known-good) fixtures, before either check was trusted.

## Classification table

| Workflow | Assistant produces | An undetected error causes | Detected independently by | Residual risk |
|---|---|---|---|---|
| Flight logic and drivers | Code in the flight image | A defect that flies | `clang-tidy` and `clang-format`, the four Debug builds, host unit tests, the `invariants` stage (allocating `operator new` in the image, HAL includes across the layer boundary), review under the comprehension rule | Semantic errors no gate encodes; newlib `malloc` linked via `vsnprintf` with unreachability unproven; the eight allowlisted HAL includes in `shared/`. Owner: reviewer |
| **Verification code**: gates, bench harness, expected-range comparison | Code that judges other code | Silent loss of coverage; green reports over real faults | Only a deliberate known-bad input. Nothing else looks at it | **Highest.** Requires the fail-first demonstration above |
| Interface annotations: unit, scale, offset, gate | Values that convert raw counts to engineering units | Consistently wrong engineering data in flight binary, decoder and ICD at once | Datasheet review by a person. The drift check proves agreement, not correctness | High. Owner: named reviewer in the PR |
| Build and toolchain configuration | Container, csolution, CI definitions | A flight binary built differently than believed | Version pins in one place (the devcontainer Dockerfile ARGs), reproducible container build, CI running the same `verify.sh` | Moderate |
| Bench operation: submitting campaigns | Job submissions | Wasted bench time | The queue and the harness verdict, which is not the assistant's to give | Low |
| Telemetry analysis | Plots, summaries, conclusions | A wrong conclusion about the science | Raw capture retained beside every derived result; conclusions checkable against the bytes | Low, and non-critical by construction |
| Documentation and formal deliverables (for BOLT: the SED) | Prose describing the design | A document that describes software we do not have | Review against code, and the requirement that every claim names where it is implemented | Moderate. A plausible description of absent behaviour is the failure mode |
| Skills and procedures | Instructions an assistant follows | An assistant that follows a wrong process confidently | The procedure's own gates fail when executed | Low, self-correcting |

## What this means in practice

Three consequences follow from the table.

**The gates are the product, not the overhead.** Every rule we move out
of review and into `verify.sh` reduces the residual risk column. That is
the only lever that scales, because review capacity does not.

**Verification code gets reviewed harder than flight code.** Inverted
from intuition, and correct. A pull request that adds or changes a check
needs the demonstration that it fails when it should.

**Two places have no mechanical net, and two are ratchets.** Interface
annotation values and safety assessments cannot be closed by tooling:
the first names its datasheet reviewer in the pull request, the second
is excluded from assistance entirely. The two checks earlier drafts
listed as missing now exist in
[`scripts/check-flight-invariants.sh`](../../../scripts/check-flight-invariants.sh),
each with its fail-first selftest. What they leave open is stated in
their output rather than implied away: `malloc` reachability, and the
`shared/` HAL allowlist that may shrink but not grow.

## Limitations of this classification

It is our own, not an accredited assessment. No authority has reviewed
it, and it borrows the reasoning of DO-330 without claiming any of its
objectives are met. Its value is that it forces us to name, for each
workflow, what would catch a mistake, and it makes the workflows with
no net visible instead of implicit.

It also says nothing about how likely an error is, only what happens when
one occurs. That is deliberate. We have no basis for estimating the
former and do not intend to pretend otherwise.

## References

- RTCA DO-330 / EUROCAE ED-215, software tool qualification, domain-independent by design
- [`policy.md`](policy.md) - the acceptance criteria
- [System invariants](../system-invariants.md) - what the gates enforce
- [ADR-013](../../decisions/ADR-013-assisted-development.md)
