# Onboarding -- First Contribution

You're shipping your first PR this week.

## Find Something

Look at [`good-first-issue`](https://github.com/hamburg-space-team/mission-bolt/labels/good-first-issue).
Each is roughly half a day's work for someone new.

If nothing grabs you, ask in `#bolt_software` for a suggestion.
Docs improvements count.

Comment "I'll take this" on the issue so we don't step on each
other.

## Branch

```bash
git checkout main
git pull
git checkout -b feature/<issue-number>-<short-slug>
```

Convention, not enforced by tooling: `feature/`, `fix/`, `docs/`,
`chore/` + number + slug.

## Build, Test, Repeat

Relevant guides:

- [Building the flight software](../../guides/building-flight-software.md)
- [Running flight unit tests](../../guides/running-flight-unit-tests.md)
- [Debugging with OpenOCD](../../guides/debugging-with-openocd.md)
- [Running the ground station](../../guides/running-ground-station.md)

For flight code, before committing:

1. VS Code task **"Format"**.
2. **"Lint"**. Fix the warnings or justify suppressing them.
3. **"Test: Build & Run"**.

For ground code, run linter and tests of whatever you touched.

## Commit

[Conventional Commits](https://www.conventionalcommits.org/):
`<type>(<scope>): <subject>`.

Types: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`, `style`,
`perf`, `build`, `ci`.

Example:

```
fix(flight): drop AS7265X gain on saturated channels

If a channel reads >= 60000 ADC counts at the current gain, step
down for the next measurement.

Refs #142
```

## Draft PR Early

Push, open the PR as Draft. Reviewers know not to start a full
pass yet.

A good draft description:

- Links the issue (`Closes #142`)
- 2-3 sentences on what changes
- Anything you're unsure about
- Test plan (what you ran, what you saw)

## Pass CI

Format check, clang-tidy, host tests, docs build. Fix anything red
before requesting review.

## Get Reviewed

Flip to Ready for review. Request review from the relevant
[CODEOWNERS](../../../CODEOWNERS) entry. Optionally ping in
`#bolt_software`.

For your first PR expect more comments than usual. It's how the
standards transfer.

Address comments by pushing new commits (not rebases). We squash on
merge. Re-request review when done.

Comments that cite an invariant ("violates I-2") or an ADR aren't
nitpicks. Take them seriously.

## Merge

One Code Owner approval plus green CI -> squash & merge. Delete the
branch.
