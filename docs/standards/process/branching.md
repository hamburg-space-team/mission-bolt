# Branching Convention

Short version: branch off `main`, prefix the name by intent, link
to the issue, push early.

## Branch Names

Pattern: `<type>/<issue-number>-<short-slug>`.

| Prefix | When |
|---|---|
| `feature/` | New functionality |
| `fix/` | Bug fix |
| `docs/` | Documentation only |
| `refactor/` | Restructure without behaviour change |
| `chore/` | Tooling, CI, dependency bumps |
| `test/` | Adding or fixing tests |

The slug is 2-4 words separated by hyphens. Goal is "skim the branch
list and know what each one is."

Good:

```
feature/142-spectrometer-recovery
fix/188-can-fragment-reassembly
docs/204-icd-005-i2c
chore/210-bump-cmsis-toolbox
```

Bad:

```
my-branch
fix-stuff
maxs-test
```

## Linking to Issues

Every branch should reference a real GitHub issue. If no issue
exists for the work, open one first. This keeps the issue tracker
as the canonical list of what's in flight.

A one-line GitHub comment ("I'll take this") on the issue avoids
two people picking up the same thing.

## Off `main`, Into `main`

We branch off `main`, never off another feature branch. Merging
back uses **Squash and Merge** so `main` stays linear.

Long-lived branches drift. Aim to finish and merge within a couple
of days. Anything longer should be split, or live behind a feature
flag.

## Force-Push

OK on your own feature branch before review starts.

Not OK once review has started -- push fixup commits instead so the
reviewer can see what changed. We squash on merge anyway, so the
intermediate commits never reach `main`.

Never on `main`.

## Deleting

Delete the branch after merge. GitHub offers a one-click delete in
the PR view.
