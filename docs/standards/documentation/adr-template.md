# ADR-XXX: [Short Decision Title]

<!-- 
Replace XXX with the next available ADR number (zero-padded, e.g. 007).
Use a descriptive title that captures the decision, not the problem.
Good: "Producer-Consumer Ringbuffer for SD Logging"
Bad:  "How to handle SD card stalls"
-->

## Status

<!-- One of: Proposed | Accepted | Deprecated | Superseded by ADR-YYY -->

Proposed

## Date

<!-- ISO format: YYYY-MM-DD -->

YYYY-MM-DD

## Context

<!-- 
What is the situation that requires a decision?
What constraints apply (technical, organisational, time-based)?
What is the larger context?

Keep this section factual. Describe the problem, not the solution.
Aim for 2-4 paragraphs.
-->

[Describe the situation, constraints, and forces at play. Explain why a 
decision is needed now.]

## Decision

<!--
What did we decide? Be specific and unambiguous.
This should be a clear statement that someone could implement from.

Aim for 1-3 paragraphs.
-->

[State the decision clearly. Use active voice: "We use X because Y."]

## Alternatives Considered

<!--
What other options were on the table?
For each: brief description and why it was rejected.

This section is critical. ADRs without alternatives look like they
were made without consideration. Always include at least 2 alternatives.
-->

### Alternative A: [Name]

[Brief description]

**Rejected because:** [Specific reason]

### Alternative B: [Name]

[Brief description]

**Rejected because:** [Specific reason]

### Alternative C: [Name]

[Brief description]

**Rejected because:** [Specific reason]

## Consequences

<!--
What follows from this decision?
Be honest about both positive and negative consequences.
-->

### Positive

- [What gets better as a result of this decision]
- [Another positive consequence]
- [Another positive consequence]

### Negative

- [What gets worse or harder as a result]
- [Another negative consequence]
- [Another negative consequence]

### Neutral

- [Things that change but are neither clearly good nor bad]
- [Another neutral consequence]

## Implementation Notes

<!--
Optional: specific guidance for implementing this decision.
Include this section if there are non-obvious implementation details
that future readers should know.
-->

[Optional: implementation guidance, edge cases to watch for, 
related code modules, migration path from previous approach]

## References

<!--
Links to relevant material:
- Related ADRs (especially predecessors)
- Related ICDs
- External documentation, papers, standards
- Code modules implementing this decision
- Discussion threads or meeting notes
-->

- Related ADRs: [ADR-YYY](ADR-YYY-name.md)
- Related ICDs: [ICD-ZZZ](../interfaces/ICD-ZZZ-name.md)
- Implementation: [`path/to/code/`](../../flight-software/path/to/code/)
- External: [Link to relevant external resource]

## Revision History

<!--
Track changes to this ADR itself.
The ADR content can be updated for clarity, but the decision itself
should not change. If the decision changes, supersede this ADR with
a new one.
-->

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | YYYY-MM-DD | [Name] | Initial draft |
| 1.0 | YYYY-MM-DD | [Name] | Approved |