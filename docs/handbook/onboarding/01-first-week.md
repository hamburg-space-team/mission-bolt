# Onboarding -- First Week

Welcome. Your first week is for orientation: understanding what we
build, why, and how we work together. You will not write
production code yet. That comes next week (see
[03-first-contribution.md](03-first-contribution.md)).

## Goals for the Week

By the end of the week you should be able to:

- Explain what THHOR-BOLT does in two minutes to a non-engineer.
- Name the four flight controllers and the three experiments.
- Find the system invariants.
- Run the host-side test suite locally.
- Have your dev environment set up
  ([02-development-setup.md](02-development-setup.md)).
- Know who to ask in Slack about any subsystem.

## Reading List

In this order:

1. [`README.md`](../../../README.md) -- the project at a glance.
2. [`overview.md`](../../../overview.md) -- the mission and the three
   experiments.
3. [`architecture.md`](../../../architecture.md) -- the software
   architecture.
4. [`docs/standards/system-invariants.md`](../../standards/system-invariants.md)
   -- the rules every change is checked against.
5. [`docs/handbook/how-we-work.md`](../how-we-work.md) -- process
   and meetings.
6. [`docs/handbook/working-groups.md`](../working-groups.md) -- where
   different work happens.
7. Skim the [`docs/decisions/`](../../decisions/) ADR index. Read
   the titles and statuses; pick one or two that interest you and
   read them in full.
8. Skim the [`docs/interfaces/`](../../interfaces/) ICD index.

This is a lot. Pace yourself; do one or two a day rather than all
at once. Take notes; you will have questions, and questions are how
you find your way around.

## Set Up Your Environment

Follow [02-development-setup.md](02-development-setup.md). You can
do this in parallel with the reading. If something does not work,
ask on `#bolt-onboarding` or in your DM with your mentor.

## Meet the Team

Your mentor will introduce you to the people working in the area
you are joining (see [working groups](../working-groups.md)).
Specifically:

- The Software Lead -- owns the flight code as a whole
- The Ground Software Lead -- owns ground station and analysis tools
- The Communications Lead -- owns the wire protocols and ICDs
- The V&V Lead -- owns testing infrastructure

You do not need to know them all by name in week one. You will,
within a month.

## Drop Into the Office

The office is where most integration happens. Even if your work is
software-only for now, spending an hour in the office to see the
flight boards, the HIL setup, and the people debugging them is
worth it.

Ask in slack for a good time to drop by.

## Next: Week 2

[03-first-contribution.md](03-first-contribution.md). You will pick
a `good-first-issue` and ship it through review.
