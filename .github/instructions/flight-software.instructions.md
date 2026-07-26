---
applyTo: "flight-software/**"
---

Flight code is bound by the invariants in
docs/standards/system-invariants.md (40 ms tick budget, bounded
timeouts, no dynamic allocation, monotonic tick, loose coupling, no
silent substitution). Review checklist and house patterns:
[docs/guides/reviewing-firmware-changes.md](../../docs/guides/reviewing-firmware-changes.md).
New hardware follows
[docs/guides/adding-a-driver.md](../../docs/guides/adding-a-driver.md).
Never edit `*/generated/`, `*/RTE/` or `lib/littlefs`. Verify with
`./scripts/verify.sh`.
