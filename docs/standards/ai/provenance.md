# Provenance of Assisted Contributions

Status: proposed. Companion to [policy.md](policy.md).

A contribution produced with substantial help from an assistant carries a
trailer saying so. The point is not to police tool use. It is that a year
from now, when a fault is being traced, "was this written by hand?" should
be answerable from the repository rather than from memory.

## The trailer

We follow the convention the open-source ecosystem has converged on. The
Linux kernel's AI coding assistants policy prescribes the form, and
Fedora, Rocky Linux, OpenTelemetry, the Apache Software Foundation, LLVM
and QEMU have published comparable guidance.

```
feat(flight): add TMP117 temperature driver

Drives the BTC and EXP1/2/3 temperature sensors. Raw int16 is
forwarded to ground; scaling happens in the decoder.

Closes #142
Assisted-by: claude-code:claude-fable-5
Signed-off-by: Max Atslega <max@atslega.dev>
```

Three rules, and the third is the one that matters.

**`Assisted-by:` rather than `Co-authored-by:`.** The kernel form is
`Assisted-by: AGENT_NAME:MODEL_VERSION [TOOL1] [TOOL2]`. It keeps
responsibility with the human contributor and gives the tool no
authorship. That the hosting UI does not count it as a contributor is the
feature, not a shortcoming. A `Co-authored-by:` naming a human co-author
is unaffected; one naming an agent is treated as an assistance trailer.

**Disclose when a significant part of the change came from the tool
without substantial rework.** A completed line, a renamed variable or a
formatting fix is not worth a trailer. A driver, a state machine, a
document section is.

**An assistant never adds `Signed-off-by:`.** That line certifies that a
person had the right to submit the contribution and takes responsibility
for it. Only a person can certify that. The kernel policy is explicit on
this point and so are we. In this container, VS Code appends the
sign-off automatically (`git.alwaysSignOff` in the devcontainer
defaults); on the command line use `git commit -s`.

## Checked, not trusted

The convention is enforced the same way every other rule here is
enforced, and the checks obey the fail-first rule of
[ADR-013](../../decisions/ADR-013-assisted-development.md): both carry a
`--selftest` that proves the rules reject known-bad input.

[`scripts/hooks/commit-msg`](../../../scripts/hooks/commit-msg) rejects
a commit that carries an assistance trailer without a human
`Signed-off-by:`, and rejects a `Signed-off-by:` that names an agent.
Install it once per clone:

```bash
git config core.hooksPath scripts/hooks
```

The same directory carries a `prepare-commit-msg` hook that adds a
commented `#Assisted-by:` template to interactive commits as a reminder;
uncomment it when the change was assisted, leave it alone otherwise. It
stays out of `-m`/merge/squash messages, where comment lines would
survive into the commit.

[`scripts/check-provenance.sh`](../../../scripts/check-provenance.sh)
runs the same rules over a commit range and is the `provenance` stage of
`verify.sh`, so a branch that bypassed the hook fails in CI rather than
merging quietly. The hook and the stage share one implementation
(`--message` mode), so they cannot drift apart.

## Pull request

The pull request template asks two questions that the trailer cannot
answer:

- Can you explain every line you are submitting, with the assistant
  closed? If not, the change is not ready
  ([policy.md](policy.md), criterion 2).
- If this change touches a unit, scale, offset or validity gate in
  `interfaces/`, who checked the value against the datasheet? Name them.
  The drift check proves the artefacts agree with each other, not that
  the number is right.

## Why bother

Three reasons, in the order they will actually matter to us.

Reviewers benefit from knowing. A change disclosed as assisted gets read
for the failure modes an assistant has, which are not the failure modes a
tired human has.

The project's formal deliverable (for BOLT: the SED) claims a
verification chain, and the claim should be auditable.
It is weaker to write that we verify assisted output than to be able to
show which commits were assisted and that each carries a human signature.

And disclosure is becoming an expectation rather than a courtesy. EU AI
Act transparency obligations take effect in August 2026; whether source
code falls in scope is not settled, and the cost of having disclosed
anyway is a trailer.

## Related

- [policy.md](policy.md) - acceptance criteria
- [tool-classification.md](tool-classification.md) - what each workflow can break
- [Commit messages](../process/commit-messages.md) - the surrounding format
- [ADR-013](../../decisions/ADR-013-assisted-development.md)
