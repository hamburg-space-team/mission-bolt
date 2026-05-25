# ICD-XXX: [Interface Name]

<!--
Replace XXX with the next available ICD number (zero-padded, e.g. 007).
Title should clearly identify what two components are connected.
Good: "ICD-002: CAN Protocol BTC-to-Experiments"
Bad:  "ICD-002: Communication"
-->

## Document Information

| Field | Value |
|-------|-------|
| Document ID | ICD-XXX |
| Version | 0.1 |
| Status | Draft |
| Date | YYYY-MM-DD |
| Owner | [Name or working group] |
| Reviewers | [Names] |

## Change History

<!--
Track every version that goes through Review or Approved status.
Bump version for breaking changes (e.g., 1.0 → 2.0).
Bump minor version for additions (e.g., 1.0 → 1.1).
Patch version for clarifications (e.g., 1.0 → 1.0.1).
-->

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | YYYY-MM-DD | [Name] | Initial draft |

## Purpose

<!--
Brief description of what this interface does and why it exists.
1-3 paragraphs.
-->

[Describe the interface's purpose in the system. What does it enable? 
What components rely on it?]

## Scope

<!--
What is covered by this ICD and what is explicitly not.
Helps prevent scope creep.
-->

This document specifies:

- [Aspect 1]
- [Aspect 2]
- [Aspect 3]

This document does not cover:

- [Out of scope item 1]
- [Out of scope item 2]

## Parties

<!--
The components on each side of this interface.
Include enough identification (board, MCU, role) to be unambiguous.
-->

| Component | Hardware | Role |
|-----------|----------|------|
| [Component A] | [MCU/board] | [What it does in this interface] |
| [Component B] | [MCU/board] | [What it does in this interface] |

## Physical Layer

<!--
For hardware interfaces only. Skip this section for pure software interfaces.
-->

### Electrical Specification

| Parameter | Value | Notes |
|-----------|-------|-------|
| Bus type | [e.g., CAN 2.0B, RS-422] | |
| Bit rate | [e.g., 1 Mbit/s] | |
| Signaling | [e.g., differential] | |
| Voltage levels | [e.g., 0-3.3V] | |

### Connector and Pinout

<!-- If applicable, include connector type and pin assignments -->

| Pin | Signal | Direction | Notes |
|-----|--------|-----------|-------|
| 1 | [Signal name] | [In/Out/Bidir] | |
| 2 | [Signal name] | [In/Out/Bidir] | |

### Cable Specification

<!-- If applicable -->

[Wire gauge, shielding, maximum length, termination requirements]

## Protocol Specification

### General Format

<!--
Describe the message framing, byte ordering, encoding.
-->

[How are messages structured? What is the byte order? Are there sync 
words, length fields, checksums?]

### Timing Requirements

<!--
How fast must messages be sent? What are response time requirements?
-->

| Parameter | Value | Notes |
|-----------|-------|-------|
| [Parameter name] | [Value] | [Context] |

### Identifier Assignment

<!--
For bus protocols, who uses which IDs.
-->

| ID Range | Owner | Purpose |
|----------|-------|---------|
| [Range] | [Owner] | [Use case] |

## Message Definitions

<!--
For each message in the protocol, provide a detailed specification.
Repeat this subsection for every message.
-->

### Message: [Message Name]

| Field | Value |
|-------|-------|
| ID | [Identifier] |
| Direction | [A → B or B → A or bidirectional] |
| Length | [Bytes] |
| Frequency | [How often this is sent] |
| Purpose | [What this message conveys] |

**Payload Structure:**

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 4 B | uint32 LE | [field_name] | [description] |
| 4 | 1 B | uint8 | [field_name] | [description] |
| 5 | 3 B | uint24 | reserved | Set to 0 |

**Example:**

```
Hex bytes: 0x37 0x05 0x00 0x00 0x01 0x00 0x00 0x00
Decoded: field_name=1335, status=ENABLED
```

**Notes:**

[Any clarifications about field semantics, edge cases, valid ranges]

---

### Message: [Next Message Name]

[Repeat structure above for each message]

## Error Handling

<!--
What happens when things go wrong?
- Corrupted messages
- Missing messages
- Out-of-sequence messages
- Unexpected messages
-->

### Corruption Detection

[How is corruption detected? CRC? Length check? Magic word?]

### Recovery from Corruption

[What does each party do when a corrupted message is detected?]

### Missing Messages

[What if expected messages don't arrive? Timeout values, fallback behaviour]

### Out-of-Sequence Messages

[What if messages arrive in unexpected order?]

## Constants and Limits

<!--
Numeric constants that both parties must agree on.
-->

| Constant | Value | Description |
|----------|-------|-------------|
| [CONSTANT_NAME] | [Value] | [Meaning] |

## Test Vectors

<!--
Concrete examples that can be used to verify implementations.
Include both valid and invalid examples.
-->

### Valid Message Example 1

```
[Hex bytes of a valid message]
Expected interpretation: [What this message means]
```

### Valid Message Example 2

```
[Another valid example]
```

### Invalid Message Example (Corrupted)

```
[Hex bytes of an invalid message]
Expected behaviour: [How implementations should respond]
```

## Implementation Notes

<!--
Optional: guidance for implementers.
Common pitfalls, suggested data structures, performance considerations.
-->

[Anything implementers should know that isn't strictly part of the 
specification]

## References

<!--
Related documents:
- ADRs that justify this interface
- Other ICDs this depends on
- Standards documents
- Implementation modules
-->

- Related ADRs: [ADR-XXX](../decisions/ADR-XXX-name.md)
- Related ICDs: [ICD-YYY](ICD-YYY-name.md)
- Standards: [Relevant standard reference]
- Implementation: [`path/to/code/`](../../shared/path/to/code/)
- Datasheet: [Link to component datasheet if applicable]

## Glossary

<!--
Optional: define terms specific to this interface.
-->

| Term | Definition |
|------|------------|
| [Term] | [Definition] |
