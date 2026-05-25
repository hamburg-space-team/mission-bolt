# Security Policy - BOLT THHOR

BOLT THHOR is aerospace software developed by students at Hamburg University 
of Technology. While our flight software runs in isolated environments during 
missions, our ground station, build infrastructure, and supporting tools 
benefit from responsible security practices.

This document describes how to report security issues and what to expect 
from us in response.

## Scope

This security policy covers:

- **Flight Software** in this repository (BTC, EXP1, EXP2, EXP3 firmware)
- **Ground Station Software** including backend, frontend, and operational tools
- **Shared Components** including packet parsers and protocol implementations
- **Build and CI Infrastructure** including GitHub Actions workflows and 
  toolchain configurations
- **Repository Configuration** including secrets management and access controls

This policy does not cover:

- Third-party dependencies — report directly to the upstream maintainers
- Issues in hosting providers (GitHub, etc.) — report to the providers directly
- Theoretical attacks requiring physical access to the rocket during flight
- Hardware-level attacks on the MCUs themselves

## Reporting a Vulnerability

If you discover a security vulnerability in BOLT THHOR, please report it 
responsibly through the channels below.

### How to Report

**Email:**  <security@hamburgspace.de>

Please include:

- A clear description of the vulnerability
- The component affected (flight software, ground station, build infrastructure, etc.)
- Steps to reproduce
- Potential impact assessment
- Any suggested mitigations
- Your contact information for follow-up

**Encrypted communication:** If your report contains sensitive details, you 
may request our PGP key by sending an initial unencrypted message asking 
for it.

### What Not to Do

- **Do not open public GitHub issues** for security vulnerabilities
- **Do not exploit the vulnerability** beyond what is necessary to 
  demonstrate it
- **Do not access or modify data** that does not belong to you
- **Do not publicly disclose** the vulnerability before we have had a 
  reasonable opportunity to respond

## What to Expect

### Response Timeline

- **Acknowledgement:** Within 5 working days of your report
- **Initial Assessment:** Within 14 days, we will provide our assessment 
  of the issue
- **Fix Development:** Timeline depends on severity and complexity, 
  communicated after assessment
- **Coordinated Disclosure:** We will work with you to determine an 
  appropriate disclosure timeline

Please note that BOLT THHOR is a student project. Team members are also 
students with study obligations. Response times reflect best effort within 
this context.

### Severity Classification

We classify reported issues using the following framework:

**Critical:** Affects flight readiness of an active mission, exposes 
sensitive credentials, or allows remote code execution on team infrastructure.

**High:** Could lead to data loss, compromise of build pipeline integrity, 
or significant impact on team operations.

**Medium:** Affects ground station availability, could be combined with 
other issues for impact, or affects development environments.

**Low:** Minor issues that do not have direct security impact but should 
be addressed.

### Communication

Throughout the process, we will:

- Keep you informed of our progress
- Treat your report confidentially until disclosure is coordinated
- Credit you in any security advisory (unless you prefer to remain anonymous)
- Be honest about our limitations as a student team

## Coordinated Disclosure

We follow responsible disclosure principles. The general flow:

1. You report the vulnerability privately to us
2. We acknowledge and investigate
3. We develop a fix
4. We agree on a disclosure timeline with you
5. We release the fix and publish a security advisory
6. You may publish your own write-up after the fix is released

Typical disclosure timelines range from 30 to 90 days from initial report, 
depending on severity and complexity. We aim to be transparent about delays 
if they occur.

## Recognition

Researchers and contributors who responsibly disclose security issues are 
recognised in:

- Our security advisories
- Release notes for fixes
- An acknowledgements section in our documentation

If you prefer to remain anonymous, we respect that.

We do not currently offer monetary rewards. As a student aerospace project 
without commercial funding, we cannot fund a bug bounty programme. We hope 
that contributing to the security of aerospace software development at a 
European university is itself meaningful.

## Sensitive Information in the Repository

We work to keep sensitive information out of the public repository:

- API keys and credentials are stored in GitHub Secrets, never in code
- Mission-sensitive operational details are kept in internal documentation
- Hardware schematics and detailed configurations follow our documentation 
  segregation policy (see [docs/handbook/](docs/handbook/))

If you discover sensitive information accidentally committed to the 
repository, please report it via the security email rather than commenting 
publicly. Examples include:

- Credentials, tokens, or API keys
- Email addresses or personal contact details of team members beyond what 
  is publicly published
- Configuration files with operational details
- Sensitive documents from REXUS programme or industry partners

## Supply Chain Security

We use various third-party dependencies. To minimise supply chain risk:

- Dependencies are pinned to specific versions
- Major dependency updates require review
- CI runs security audits on dependency trees
- We monitor known vulnerability databases for our dependencies

If you discover that one of our dependencies has a vulnerability that 
affects us, please report it as a security issue using the channels above.

## Aerospace-Specific Considerations

Our flight software operates in environments different from typical 
software:

**During Flight:** The flight software runs autonomously without network 
connectivity. Traditional remote attack vectors do not apply during 
missions.

**During Development:** The same code is developed using standard 
development tools and may be more exposed to typical software security 
concerns.

**During Operations:** Our ground station receives telemetry over various 
links (PCM, RS-422, eventually wireless). The integrity of this telemetry 
is important for scientific results but cryptographic protection is not 
typically employed at our level.

Issues affecting flight-time behaviour are treated as quality issues 
rather than security issues, though we welcome reports about either.

## Contact

- Primary: <security@hamburgspace.de>
- Backup: Software Lead via <bolt@thhor.space>

For non-security questions, please use the standard contribution channels 
described in [CONTRIBUTING.md](CONTRIBUTING.md).

## Acknowledgements

Past security contributors will be listed here as the project matures.

---

This security policy is adapted from common open-source practices and 
tailored to the context of a student aerospace project. It is reviewed 
and updated periodically.