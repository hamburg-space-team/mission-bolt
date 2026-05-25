# THHOR-BOLT REXUS 37 -- Mission Overview

## Mission Statement

THHOR-BOLT REXUS 37 is a technology demonstration of LiFi (Light Fidelity) 
communication for intra-satellite applications. The mission flies three 
sub-experiments on the REXUS sounding rocket to characterise optical 
communication under realistic flight conditions and to validate the 
technology for upcoming projects, including the THHOR-HANS satellite mission.

## Background

Satellites contain many internal components that need to communicate with 
each other. These components are typically connected via wiring harnesses, 
which can account for 7 to 10 percent of the total satellite weight. The 
complex harness also has an adverse effect on the flexibility of the 
Assembly, Integration, and Test (AIT) processes, causing significant time 
and cost overheads.

LiFi offers an alternative: optical wireless communication for intra-satellite 
data transfer. Compared to traditional radio-frequency communication, LiFi 
provides higher bandwidth potential, eliminates electromagnetic interference 
between subsystems, and reduces susceptibility to eavesdropping due to signal 
confinement within structural walls.

Existing LiFi research has primarily been conducted in laboratory conditions. 
THHOR-BOLT extends this work by testing LiFi under realistic flight conditions 
including launch vibration, atmospheric variation, vacuum exposure, rapidly 
changing thermal conditions, and microgravity.

## Mission Goals

The mission has two main goals:

**Technical Demonstration.** Provide a technical demonstration for LiFi 
technology in a space context. Measured precision, signal transfer, and 
power transfer of the tested modules will be compared to commonly used 
wired data transfer.

**Survivability Validation.** Test the survivability of LiFi modules under 
launch and space conditions. Learned lessons and design decisions will be 
incorporated into upcoming projects like the THHOR-HANS satellite mission.

## The Three Experiments

THHOR-BOLT consists of three independent sub-experiments, each housed in its 
own light-tight enclosure to prevent cross-experiment interference.

### EXP1 -- Space Disco

EXP1 characterises how atmospheric conditions and flight dynamics affect 
optical link quality across the spectrum.

An LED array containing RGB, white, infrared, and ultraviolet LEDs emits 
defined light patterns through an enclosed tube. On the opposite side, an 
AS7265X 18-channel spectrometer measures the received light. The system 
cycles through different LED states, brightness levels, and spectrometer 
integration times to characterise the optical channel under varying 
conditions.

Simultaneously, atmospheric conditions (pressure, temperature) are recorded 
to correlate environmental changes with spectral measurements.

**Software role:** Drive LED drivers via I2C, control spectrometer integration 
cycles, coordinate measurement timing across multiple LED states, and 
correlate spectral data with environmental sensors.

### EXP2 -- Bouncy Castle

EXP2 measures the bit error rate (BER) of an optical link disturbed by 
free-floating particles.

Two LiFi transceivers face each other inside a sealed box. The transmitter 
sends a known PRBS-7 bit pattern via modulated light, the receiver decodes 
the signal, and the bit error rate is computed by comparing received against 
expected bits.

During microgravity, silicone particles inside the box float into the 
optical path between transmitter and receiver. The particles simulate loose 
or damaged components that may float through optical communication paths in 
real satellites. Two cameras record particle movement for post-flight 
correlation with measured signal degradation.

**Software role:** Coordinate LiFi transmitter and receiver via two separate 
UARTs, manage PRBS-7 bit pattern generation, perform per-tick BER 
computation, sweep through different LiFi data rates, and log full bit 
streams for post-flight analysis.

### EXP3 -- Floaty Boi

EXP3 demonstrates wireless power transfer combined with LiFi communication 
for distributed sensors.

Two magnetometer stacks operate in parallel: a Wired Stack receives power 
and data via cable, while a Wireless Stack receives power optically through 
high-power LEDs onto a solar cell with a supercapacitor as energy storage, 
and communicates exclusively via LiFi. Both stacks measure the magnetic 
field simultaneously.

The science goal is to compare measurements from both stacks. Magnetometers 
are particularly susceptible to electromagnetic interference from power 
supply currents, so we expect the wireless stack to provide more accurate 
readings free from cable-induced electromagnetic interference.

**Software role:** Coordinate the duty-cycled LED charging timeline, manage 
two LiFi transceivers with different latencies, synchronise sample streams 
from wired and wireless stacks, drive the laser/LED driver via PWM, and 
perform cable-LiFi cross-validation.

## Experiment Objectives

| Number | Objective |
|--------|-----------|
| Obj. 1 | Develop three sub-experiments to test intra-satellite LiFi communications in an environment close to a satellite's life-cycle |
| Obj. 2 | Test the survivability of the module under launch and space conditions |
| Obj. 3 | Measure the perceived deviation of the RGB colour spectrum under changing atmospheric conditions and rocket launch conditions |
| Obj. 4 | Measure environmental conditions for later data analysis and data correlation |
| Obj. 5 | Measure error when a LiFi signal is interfered with by floating particles |
| Obj. 6 | Test feasibility of an electrically isolated sensor under space conditions |
| Obj. 7 | Gather data on and validate the usage of LiFi technology in space conditions |

## System Architecture

Four STM32 flight computers -- one BTC master plus three experiment
controllers -- coordinated over CAN and downlinked over RS-422
through the REXUS Service Module.

For the diagram, class hierarchy, communication interfaces, and the
rest of the software architecture, see [architecture.md](architecture.md).

## Software Significance

THHOR-BOLT is software-heavy by aerospace standards. The flight software:

- Coordinates four flight computers with deterministic timing requirements
- Manages six LiFi transceiver co-processors with experiment-specific protocols
- Maintains scientific data integrity across mid-flight reboots
- Distinguishes valid measurements from sensor failures explicitly per packet
- Provides survivability guarantees for the local microSD storage

These properties make the mission a strong demonstration of how 
student-developed aerospace software can meet professional verification 
standards.


## Related Documents

- [Architecture](architecture.md) -- Flight software architecture for this mission
- [Architecture Decision Records](../decisions/) -- Engineering decisions
- [Interface Control Documents](../interfaces/) -- Interface specifications

## Contact

bolt@thhor.space