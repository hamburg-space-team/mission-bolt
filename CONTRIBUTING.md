# Contributing to BOLT THHOR

First off, thanks for taking the time to contribute!

All types of contributions are encouraged and valued. See the [Table of Contents](#table-of-contents) for different ways to help and details about how this project handles them. Please make sure to read the relevant section before making your contribution. It will make it a lot easier for us maintainers and smooth out the experience for all involved. The community looks forward to your contributions.

For information about how we work as a team, see [docs/handbook/](docs/handbook/).

> And if you like the project, but just don't have time to contribute, that's fine. There are other easy ways to support the project and show your appreciation, which we would also be very happy about:
>
> - Star the project
> - Tweet about it
> - Refer this project in your project's readme
> - Mention the project at local meetups and tell your friends/colleagues

<!-- omit in toc -->
## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [I Have a Question](#i-have-a-question)
- [I Want To Contribute](#i-want-to-contribute)
  - [Reporting Bugs](#reporting-bugs)
  - [Suggesting Enhancements](#suggesting-enhancements)
  - [Your First Code Contribution](#your-first-code-contribution)
  - [Improving The Documentation](#improving-the-documentation)
  - [Architectural Changes](#architectural-changes)
  - [Interface Changes](#interface-changes)
- [Styleguides](#styleguides)
  - [Coding Standards](#coding-standards)
  - [Commit Messages](#commit-messages)
  - [Documentation Style](#documentation-style)
- [Pull Request Workflow](#pull-request-workflow)
- [Join The Project Team](#join-the-project-team)

## Code of Conduct

This project and everyone participating in it is governed by the
[BOLT THHOR Code of Conduct](CODE_OF_CONDUCT.md).
By participating, you are expected to uphold this code. Please report unacceptable behavior
to <conduct@thhor.space>.

## I Have a Question

> If you want to ask a question, we assume that you have read the available [Documentation](docs/) and the [Onboarding Guide](docs/handbook/onboarding/).

Before you ask a question, it is best to search for existing [Issues](https://github.com/hamburg-space-team/mission-bolt/issues) and [Discussions](https://github.com/hamburg-space-team/mission-bolt/discussions) that might help you. In case you have found a suitable issue and still need clarification, you can write your question there.

If you then still feel the need to ask a question and need clarification, we recommend the following:

- Open a [Discussion](https://github.com/hamburg-space-team/mission-bolt/discussions) for technical questions or general topics.
- Open an [Issue](https://github.com/hamburg-space-team/mission-bolt/issues/new) if you suspect a specific bug or have a concrete feature request.
- Provide as much context as you can about what you're running into.
- Provide project and platform versions (toolchain, MCU target, build configuration), depending on what seems relevant.

Team members can also discuss in our Slack workspace. Ask for an invite during onboarding.

We will then take care of it as soon as possible.

## I Want To Contribute

> ### Legal Notice <!-- omit in toc -->
>
> When contributing to this project, you must agree that you have authored 100% of the content and that you have the necessary rights to the content.

### Reporting Bugs

<!-- omit in toc -->
#### Before Submitting a Bug Report

A good bug report shouldn't leave others needing to chase you up for more information. Therefore, we ask you to investigate carefully, collect information and describe the issue in detail in your report. Please complete the following steps in advance to help us fix any potential bug as fast as possible.

- Make sure that you are using the latest version on the `main` branch.
- Determine if your bug is really a bug and not an error on your side, e.g. using incompatible toolchain versions or wrong MCU configuration. Make sure that you have read the [documentation](docs/). If you are looking for support, you might want to check [this section](#i-have-a-question).
- To see if others have experienced (and potentially already solved) the same issue, check if there is already a bug report in the [bug tracker](https://github.com/hamburg-space-team/mission-bolt/issues?q=label%3Abug).
- Collect information about the bug:
  - Stack trace or fault information (if available)
  - Hardware target (BTC, EXP1, EXP2, EXP3, dev board, simulator)
  - Build configuration (debug, release)
  - Toolchain version (ARM GNU, CMSIS-Toolbox)
  - Reproduction steps
  - Possibly your input data and the observed output
  - Whether the issue is reproducible on the HIL platform or only on hardware

<!-- omit in toc -->
#### How Do I Submit a Good Bug Report?

> You must never report security related issues, vulnerabilities or bugs including sensitive information to the issue tracker, or elsewhere in public. Instead, sensitive bugs must be sent by email to <security@hamburgspace.de>. See [SECURITY.md](SECURITY.md) for details.

We use GitHub issues to track bugs and errors. If you run into an issue with the project:

- Open an [Issue](https://github.com/hamburg-space-team/mission-bolt/issues/new?template=bug_report.md) using the bug report template.
- Explain the behavior you would expect and the actual behavior.
- Please provide as much context as possible and describe the *reproduction steps* that someone else can follow to recreate the issue. This usually includes your code. For good bug reports you should isolate the problem and create a reduced test case.
- Provide the information you collected in the previous section.

Once it's filed:

- The issue will be labeled accordingly.
- Someone from the team will try to reproduce the issue with your provided steps. If there are no reproduction steps or no obvious way to reproduce the issue, we will ask you for those steps and mark the issue as `needs-repro`. Bugs with the `needs-repro` tag will not be addressed until they are reproduced.
- If the issue can be reproduced, it will be marked `needs-fix`, possibly along with other tags (such as `critical`), and the issue will be left to be [implemented by someone](#your-first-code-contribution).

### Suggesting Enhancements

This section guides you through submitting an enhancement suggestion for BOLT THHOR, **including completely new features and minor improvements to existing functionality**. Following these guidelines will help the community to understand your suggestion and find related suggestions.

<!-- omit in toc -->
#### Before Submitting an Enhancement

- Make sure that you are using the latest version on `main`.
- Read the [documentation](docs/) carefully and find out if the functionality is already covered, maybe by an individual configuration.
- Perform a [search](https://github.com/hamburg-space-team/mission-bolt/issues) to see if the enhancement has already been suggested. If it has, add a comment to the existing issue instead of opening a new one.
- Find out whether your idea fits with the scope and aims of the project. BOLT THHOR is aerospace flight software with specific reliability and verifiability constraints. Features that compromise these are unlikely to be accepted.
- For non-trivial enhancements, open a [GitHub Discussion](https://github.com/hamburg-space-team/mission-bolt/discussions) before creating an issue. This helps align on scope and approach.

<!-- omit in toc -->
#### How Do I Submit a Good Enhancement Suggestion?

Enhancement suggestions are tracked as [GitHub issues](https://github.com/hamburg-space-team/mission-bolt/issues).

- Use a **clear and descriptive title** for the issue to identify the suggestion.
- Provide a **step-by-step description of the suggested enhancement** in as many details as possible.
- **Describe the current behavior** and **explain which behavior you expected to see instead** and why. At this point you can also tell which alternatives do not work for you.
- You may want to **include diagrams or sketches** which help you demonstrate the proposal. PlantUML and Mermaid sources are preferred over images so they can be versioned.
- **Explain why this enhancement would be useful** to BOLT THHOR and our missions. Mention any related ADRs or ICDs if applicable.

### Your First Code Contribution

#### Setting Up Your Environment

Follow the setup guide in [docs/handbook/onboarding/02-development-setup.md](docs/handbook/onboarding/02-development-setup.md). It covers:

- Required toolchain (ARM GNU, CMSIS-Toolbox, Python)
- IDE setup (VS Code recommended, but any editor works)
- Cloning the repository and building locally
- Running tests
- Connecting to hardware or HIL platform

#### Finding Something to Work On

- **New contributors:** Check issues labeled [`good-first-issue`](https://github.com/hamburg-space-team/mission-bolt/labels/good-first-issue). These are scoped to be a reasonable first contribution.
- **Returning contributors:** Check issues labeled [`help-wanted`](https://github.com/hamburg-space-team/mission-bolt/labels/help-wanted) or pick from your area of expertise.
- **Specific area of interest:** Browse the codebase or ask in Slack which areas need contributors right now.

#### Making Your First Pull Request

See the [Pull Request Workflow](#pull-request-workflow) section below.

For your first PR, expect more detailed review feedback than usual. This is part of learning our standards, not criticism of your work. Other team members will help you understand the why behind the feedback.

### Improving The Documentation

Documentation contributions are highly valued. Documentation lives in several places:

- **[architecture.md](architecture.md)** - System architecture overview
- **[docs/interfaces/](docs/interfaces/)** - Interface Control Documents (ICDs)
- **[docs/decisions/](docs/decisions/)** - Architecture Decision Records (ADRs)
- **[docs/standards/](docs/standards/)** - Coding, documentation, and process standards
- **[docs/handbook/](docs/handbook/)** - Team handbook and onboarding
- **[docs/guides/](docs/guides/)** - How-to guides for common tasks

To improve documentation:

- Fix typos and unclear wording via direct pull requests
- Propose larger restructuring via a GitHub Discussion first
- Follow [docs/standards/documentation/](docs/standards/documentation/) for format and style

If documentation is missing, that itself is a contribution opportunity. Open an issue describing what is missing and where it should live.

### Architectural Changes

Changes affecting the system architecture require an Architecture Decision Record (ADR) before implementation.

The workflow:

1. Open a PR adding the new ADR in `docs/decisions/`
2. Use the template at [docs/standards/documentation/adr-template.md](docs/standards/documentation/adr-template.md)
3. Discuss in the PR until consensus is reached
4. Merge the ADR with status `Accepted`
5. Implement the change in a subsequent PR, referencing the ADR

See existing ADRs in [docs/decisions/](docs/decisions/) for examples.

### Interface Changes

Changes affecting interfaces between components require updates to the corresponding Interface Control Document (ICD).

The workflow:

1. Identify the affected ICD in [docs/interfaces/](docs/interfaces/)
2. Update the ICD using the template at [docs/standards/documentation/icd-template.md](docs/standards/documentation/icd-template.md)
3. Coordinate with everyone using the interface (typically via GitHub Discussion or Slack)
4. Bump the ICD version number
5. Update all implementations atomically (preferably in the same PR)

Interfaces include CAN protocol, packet formats, UART protocols to LiFi transceivers, and the RS-422 link to RXSM. See the ICD index in [docs/interfaces/README.md](docs/interfaces/README.md) for the full list.

## Styleguides

### Coding Standards

We follow language-specific standards documented in [docs/standards/coding/](docs/standards/coding/):

- **C++** (Flight Software): [docs/standards/coding/cpp.md](docs/standards/coding/cpp.md) -- HIC++ based with aerospace-specific constraints
- **Python** (Ground Software, Tools): [docs/standards/coding/python.md](docs/standards/coding/python.md)
- **JavaScript/TypeScript** (Frontend): [docs/standards/coding/javascript.md](docs/standards/coding/javascript.md)

Key principles for all code:

- No dynamic allocation after initialization (flight software)
- All return values checked
- Hard timeouts on all I/O
- Meaningful test coverage for new code
- Style enforced automatically by linters and formatters

### Commit Messages

We use [Conventional Commits](https://www.conventionalcommits.org/)
(`<type>(<scope>): <subject>`). See
[docs/standards/process/commit-messages.md](docs/standards/process/commit-messages.md)
for the full format, type list, and examples.

### Documentation Style

- Use Markdown for most documents
- LaTeX is acceptable for formal mission reviews (CDR, PDR, etc.)
- Diagrams in PlantUML or Mermaid (text-based, versionable)
- Follow templates in [docs/standards/documentation/](docs/standards/documentation/)
- Write in English throughout the repository

## Pull Request Workflow

For our complete branching, issue, and PR conventions, see [docs/standards/process/](docs/standards/process/).

In summary:

1. **Discuss before you build.** For non-trivial changes, open a Discussion in GitHub or Slack first.

2. **Make sure an issue exists** for your work. Every branch should be linked to an issue. If no issue exists for what you want to do, create one first.

3. **Create a branch** from `main` following our [branching convention](docs/standards/process/branching.md).

4. **Make your changes** following our standards.

5. **Write or update tests.** All code changes require corresponding test changes.

6. **Update documentation** if your change affects behavior, interfaces, or workflows.

7. **Open a pull request** against `main`. Fill out the PR template completely, including the linked issue.

8. **Address review feedback.** Other contributors will review your PR. Respond to all comments and re-request review when ready.

9. **Wait for approval.** Code is merged only after approval from a Code Owner (see [CODEOWNERS](CODEOWNERS)).

Every PR must:

- Be linked to a tracking issue
- Have a clear, descriptive title following commit message format
- Pass all CI checks (formatting, linting, static analysis, tests)
- Have at least one approval from a Code Owner
- Update documentation if applicable
- Include tests for new functionality

## Join The Project Team

BOLT THHOR is built primarily by students of TU Hamburg. If you are a TUHH student interested in joining the team:

- Visit our information sessions (announced via Hamburg Space Team channels)
- Contact us at <bolt@thhor.space>
- Read our [onboarding documentation](docs/handbook/onboarding/) to understand what we do

New members typically start with an onboarding period, paired with an experienced team member as a mentor. After demonstrating familiarity with our standards and codebase, members take on increasing responsibility, eventually becoming Code Owners of specific modules.

External contributors (non-TUHH) are also welcome to contribute via pull requests, following the standard contribution workflow described above.

<!-- omit in toc -->
## Attribution

This guide is based on the [contributing.md](https://contributing.md/generator) template,
adapted for the BOLT THHOR project at TUHH Orbital Research.
