# Commit Messages

We use [Conventional Commits](https://www.conventionalcommits.org/).
This page is the working agreement.

## Format

```
<type>(<scope>): <subject>

<optional body>

<optional footer>
```

- `type` -- one of the values below.
- `scope` -- the area touched (`flight`, `ground`, `comms`, `docs`,
  `exp1`, `exp2`, `exp3`, `btc`, `ci`). One word, no slashes.
- `subject` -- imperative, present tense, no period, <= 72 chars.
  "Add" not "Added", "Fix" not "Fixed".
- `body` -- wrap at 72. Explain the *why*, not the *what*. The diff
  is right there.
- `footer` -- `Closes #142`, `Refs #142`, breaking-change notes.

## Types

| Type | When |
|---|---|
| `feat` | New functionality |
| `fix` | Bug fix |
| `docs` | Docs-only change |
| `refactor` | Restructure without behaviour change |
| `test` | Add or fix tests |
| `chore` | Tooling, deps, CI |
| `style` | Formatting only (rare; the formatter does most of it) |
| `perf` | Performance fix that is not a behaviour fix |
| `build` | Build system or external deps |
| `ci` | CI configuration only |

## Examples

```
feat(flight): add TMP117 temperature driver

Drives the BTC and EXP1/2/3 temperature sensors. Raw int16
register values forwarded to ground per ADR-009.

Closes #142
```

```
fix(comms): handle CAN fragment reassembly with reordered frames

bxCAN can deliver frames out of order when two experiments
publish simultaneously. PacketBuilder was assuming sequential
indices. Now reassembles by index, not arrival order.

Refs #188
```

```
docs(icd): ICD-005 I2C bus address map
```

```
chore: bump CMSIS-Toolbox to 2.12.0
```

## Why Conventional Commits

Three reasons, in order:

1. **Reading history**. `git log --oneline` becomes scannable.
2. **PR titles**. Same convention applies; CI checks it.
3. **Changelogs**. If we ever generate one, `feat`/`fix` are the
   things end users care about.

## What We Don't Do

- No issue ID in the subject. Use the footer for that.
- No emoji.
- No vague subjects (`update stuff`, `wip`, `fixes`).
- No commits squashing unrelated changes. One commit = one logical
  change.
- No commits to `main` directly. Always through a PR.

## Reviewing Commit Messages

In code review, if a commit subject doesn't tell you what changed,
ask for a rewrite. We squash on merge but the PR title becomes the
final commit subject -- so the same rules apply to PR titles.
