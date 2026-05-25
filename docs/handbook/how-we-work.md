# How We Work

This document describes the day-to-day reality of working in Orbital. 
The mechanics: where we talk, how we decide, what a typical week looks 
like. For the formal rules, see [the standards](../standards/).

## Where We Talk

**Slack** is for everything time-sensitive. Quick questions, sync 
coordination, "is anyone in the office tonight", informal discussion. 
Channels are organised by working group plus a general channel.

**GitHub** is for everything that should be findable later. Issues, 
pull requests, discussions, design questions that need a written record. 
If a Slack conversation produces a decision, someone captures it in 
GitHub afterwards.

**Email** is for external communication only.

**In person** happens in the office and in our weekly meeting. The office 
sees the most informal colofficeoration. Hardware integration, debugging 
sessions, whiteboard discussions.

The general rule: if it took longer than 10 minutes of conversation, 
write something down. Future-you will thank present-you.

## How We Decide

Most decisions happen organically. Someone proposes a change in a PR, 
someone reviews it, it gets merged. No formal process needed.

For architectural decisions, we write an [ADR](../decisions/). The ADR 
captures the context, the decision, and the alternatives considered. 
This is not bureaucracy: it is how we remember why we did something 
six months later when the original reasoning has faded.

For interface decisions between subsystems, we write an [ICD](../interfaces/). 
The ICD becomes the contract that all parties can refer to.

For disagreements, we discuss in the PR or issue. If we cannot agree, 
we bring it to the relevant working group on Slack or in the next 
meeting. The Software Lead has the final call on flight software 
questions, the Ground Software Lead on ground questions. We have not 
needed to invoke this yet.

## How We Write Code

A typical contribution flow:

1. Pick or open an issue describing what needs to happen
2. Create a feature branch with the issue number in the name 
   (`feature/42-spectrometer-recovery`)
3. Make the change with reasonable commits along the way
4. Open a pull request, even if work is still in progress (mark as draft)
5. Get at least one review
6. Address feedback, iterate
7. Merge when CI is green and the reviewer has approved

Every branch links back to an issue. If you start work without one, 
open the issue first. This keeps GitHub's issue view as the canonical 
list of what is happening in the project, and makes it easy to find 
the context behind any branch later.

We use Conventional Commits for commit messages 
(`feat:`, `fix:`, `docs:`, `refactor:`, `test:`, `chore:`). The PR title 
follows the same convention.

For flight code, our HIC++ compliance and the system invariants 
constrain what we write. The compiler and `clang-tidy` enforce most of 
it. Code review catches the rest.

## How We Review

Reviews focus on three things:

- **Correctness:** Does it do what it claims?
- **Invariants:** Does it respect [the system invariants](../standards/system-invariants.md)?
- **Clarity:** Will we understand this code in three months?

We cite the invariants by number when relevant. "This violates I-2, 
add a deadline" is more useful than "this seems risky."

Reviews should not be a gate kept by perfectionism. If the code is 
better than what we have, it can probably ship. Open follow-up issues 
for improvements rather than blocking the PR.

We try to review within 48 hours. If it takes longer, ping in Slack.

## How We Test

Three test layers:

**Host-side unit tests** in Catch2 run on every push. They cover the 
host-portable parts of the codebase: CRC, packet building, state 
machines, protocol logic. These tests live next to the code they cover.

**Target integration tests** run on real STM32 hardware in our office. 
They cover timing-critical paths, peripheral interactions, and 
end-to-end behaviour. These happen during development sessions, not 
on every push.

**Hardware-in-the-Loop tests** run against the bolt-hil platform 
before releases. They cover full system behaviour with simulated 
sensors and external inputs.

If you add functionality that can be unit-tested host-side, write the 
test. If you fix a bug, write a test that would have caught it.

## How We Plan

We work in roughly two-week cycles, aligned to our weekly meeting. Each 
week we look at what is done, what is blocked, and what is next. We do 
not use a rigid sprint methodology.

Major milestones are the REXUS reviews: SDR, PDR, CDR, IPR, EAR. Each 
review has its own preparation period (typically two to three months 
of focused work) and produces a document that lives in the team's 
shared workspace.

For day-to-day priority, we use GitHub Issues with milestones.

## How We Meet

**Weekly all-hands** Roughly 60 minutes. 
Status updates from each working group, blockers, decisions that need 
team input. Notes go in the team's shared workspace.

**Working group syncs** happen as needed. Some groups meet weekly, 
others ad-hoc. These are smaller and more technical.

**office sessions** are not formal meetings but they are where most of 
the real integration work happens. Coordinate on Slack to find when 
others will be there.

## How We Handle Hardware

The flight hardware lives in the office. Treat it with care: it is 
expensive, lead times are long, and a damaged board can set us back 
weeks.

Always check with the Hardware Lead before powering on new boards. 
Always verify polarity and voltage before connecting power.

For software-only changes, you can develop on your own machine and use 
the host-side test suite. For changes that touch hardware behaviour, 
coordinate with whoever has hardware time scheduled.

## How We Handle Conflict

Disagreements are normal in engineering. We address them directly and 
respectfully:

- Discuss in the PR or issue first
- If unresolved, escalate to the relevant working group
- If still unresolved, bring it to the weekly meeting
- The relevant Lead makes the final call

We avoid letting disagreements simmer in Slack DMs. Public discussion 
in PRs and issues is healthier and creates a record.

If something feels off interpersonally, talk to a Working Group Lead, 
the Team Lead, or directly to the person involved. The 
[Code of Conduct](../../CODE_OF_CONDUCT.md) describes our standards.

## How We Document

We document as we go, not after. The right time to write an ADR is 
when the decision is fresh. The right time to update an ICD is when 
the interface changes. Documentation that lags reality is worse than 
no documentation.

Three layers:

- **Code comments** for non-obvious local logic
- **ADRs and ICDs** for decisions and interfaces
- **This handbook** for how we work as a team

If you are about to write a long Slack message explaining how something 
works, write it as a doc instead and link to it. Slack messages 
disappear, docs do not.

## How We Onboard

New contributors go through the [onboarding sequence](onboarding/). 
The first week focuses on understanding the project and getting your 
development environment working. The second week focuses on making 
your first contribution.

We try to pair new contributors with someone already working in the 
area they want to focus on. If you are new and want a pairing buddy, 
mention it in your intro.

## Related Documents

- [Working Groups](working-groups.md) - how we organise contributions
- [Standards](../standards/) - formal rules
- [System Invariants](../system-invariants.md) - what we will not compromise on
- [Code of Conduct](../../CODE_OF_CONDUCT.md) - how we treat each other
- [Onboarding](onboarding/) - for new contributors