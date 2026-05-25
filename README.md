# BOLT (Beamed Optical Light Transmission)
BOLT explores the usage of LiFi for Intra-Satellite Communications in real world scenarios. We want to address the current weaknesses of LiFi, building on the research of Project SatelLight.

This repository contains the flight software, ground station, and supporting 
infrastructure for BOLT missions developed by TUHH Orbital Research at 
Hamburg University of Technology.

## Currently Flying

**THHOR-BOLT REXUS 37** - Three-experiment LiFi technology demonstrator on 
a REXUS sounding rocket.

## What's in This Repository

### Software

- [`flight-software/`](flight-software/) -- Embedded code for STM32-based flight computers
- [`ground-station/`](ground-station/) -- Telemetry reception, dashboards, and operations tools

### Documentation

- [`overview.md`](overview.md) - Mission overview
- [`architecture.md`](architecture.md) - System architecture
- [`docs/`](docs/) - All project documentation
  - [`interfaces/`](docs/interfaces/) - Interface Control Documents (ICDs)
  - [`decisions/`](docs/decisions/) - Architecture Decision Records (ADRs)
  - [`standards/`](docs/standards/) - Coding, documentation, and process standards
  - [`handbook/`](docs/handbook/) - Team handbook and onboarding
  - [`guides/`](docs/guides/) - How-to guides

## Related Repositories

- [bolt-thhor-hil](https://github.com/hamburg-space-team/bolt-thhor-hil) - Hardware-in-the-Loop test platform
- [bolt-flight-analytics](https://github.com/hamburg-space-team/bolt-flight-analytics) - Post-flight data analysis tools

## Getting Started

### For New Contributors

Start with our [onboarding guide](docs/handbook/onboarding/). It will walk 
you through environment setup, our standards, and how to make your first 
contribution.

### For Curious Visitors
To understand what we build and why:

1. Read the [current mission overview](overview.md)
2. Look at the [system architecture](architecture.md)
3. Browse our [Architecture Decision Records](docs/decisions/) for engineering rationale

### For Returning Contributors

- Latest [open issues](https://github.com/hamburg-space-team/mission-bolt/issues)
- Current [discussions](https://github.com/hamburg-space-team/mission-bolt/discussions)
- See [CONTRIBUTING.md](CONTRIBUTING.md) for the contribution workflow

## About BOLT

BOLT is a Hamburg Space Team project investigating LiFi for
intra-satellite communication. For the full mission rationale,
the three experiments, and the science objectives, see
[overview.md](overview.md).

## Contributing

We welcome contributions from team members and external contributors. See 
[CONTRIBUTING.md](CONTRIBUTING.md) for guidelines and 
[CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) for our community standards.

## Security

For security issues, please see [SECURITY.md](SECURITY.md). Do not report 
security vulnerabilities through public GitHub issues.

## Contact

- General: <bolt@thhor.space>
- Security: <security@hamburgspace.de>
- Code of Conduct: <conduct@thhor.space>