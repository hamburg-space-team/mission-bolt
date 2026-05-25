# Working Groups

BOLT is organised in four working groups. Each owns a domain. Membership 
is fluid: people contribute where they can help, and most people are in 
more than one group over time.

There are no group leads. Each group coordinates itself through GitHub 
and Slack. Code review responsibilities are tracked in 
[CODEOWNERS](../../CODEOWNERS).

## Flight

Everything that runs on the flight MCUs. Tick architecture, sensor 
drivers, storage, fault management, the class hierarchy, the four 
flight binaries.

## Ground

Everything that runs on the ground. Packet receiver, backend, frontend, 
operations tools, post-flight analysis pipeline.

## Communication

Everything that moves data between things.

- **External:** flight to RXSM (RS-422), RXSM to ground (PCM)
- **Internal:** between flight controllers (CAN, SYNC protocol)
- **Intra:** within a controller's domain (UART to LiFi transceivers, 
  I2C to sensors, cable to wired stack)

Packet formats and CRC framing live here because they cross all three 
levels.

## Verification & Validation

Everything that proves the rest works. Unit tests, integration tests, 
the HIL platform, CI/CD, static analysis configuration, test coverage 
tracking.

## Joining a Group

Show up. Comment on issues, open pull requests, ask questions in the 
relevant Slack channel. After a few contributions you are in the group.

If you want to focus on something specific, mention it in your 
introduction. Someone already working in that area will pick it up 
and pair with you on your first contribution.