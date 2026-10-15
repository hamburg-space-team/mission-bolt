# ICD-008: UART command protocol of the STM32U031 LiFi transceiver

## Document Information

| Field | Value |
|-------|-------|
| Document ID | ICD-008 |
| Version | 1.3 |
| Status | Draft, derived from firmware branch `InitialPrototype_CDR` @ `ae81b9a` |
| Date | 2026-10-06 |
| Owner | LiFi transceiver firmware |

## Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-09-17 | Max | First version written from the firmware sources (not from the CDR text) |
| 1.1 | 2026-09-17 | Max | Non-blocking per-tick operation scheme for EXP2, driver split into sender/receiver roles |
| 1.2 | 2026-09-17 | Max | Driver section describes the flight-software component under `shared/lifi` |
| 1.3 | 2026-10-06 | Max | EXP2 runs polling-mode rounds (bench-script replica) instead of feed-through; threshold findings; limitation #8 |

## Purpose

Exact wire behaviour of the LiFi transceiver's **host UART** (the
"console" UART in the firmware) as implemented, so a host controller
(EXP2, EXP3, the stacks) can drive it without reading the firmware.

This document is the source of truth for the transceiver side.
[ICD-003](ICD-003-uart-to-lifi-exp2.md) and
[ICD-004](ICD-004-uart-to-lifi-exp3.md) describe how each experiment
uses the link; where they disagree with this document, this document
is right (see [Differences to ICD-003](#differences-to-icd-003)).

## Scope

In:

- Host UART parameters, boot behaviour, initial state
- Command framing, every command byte for byte, responses, side effects
- Operating modes and buffer model
- Byte-exact sequences for transmit and receive
- Timing, error handling, known firmware limitations and workarounds
- The flight-software host driver

Out of scope:

- Optical layer and analog front end
- The LiFi-side UART framing beyond what the host must know

## Parties

| Component | Hardware | Role |
|---|---|---|
| Host | any MCU with one UART | Sends commands and data, reads responses |
| LiFi transceiver | STM32U031F8 | Bridges host UART <-> optical link |

The transceiver hardware is symmetric. Transmit and receive roles are
purely a matter of which commands the host sends.

## Physical Layer

### Host UART (transceiver "console")

| Parameter | Value |
|---|---|
| Baud rate | **115 200, fixed in firmware** (not changeable at runtime) |
| Format | 8 data bits, no parity, 1 stop bit |
| Flow control | none |
| Levels | 3.3 V CMOS, idle high |
| Transceiver pins | TX = PA5, RX = PA7 (USART3) |

The `\f` command changes **only the optical link** baud rate. The host
UART always stays at 115 200.

### Optical link (transceiver <-> transceiver)

| Parameter | Value |
|---|---|
| Framing | asynchronous 8N1, one UART frame per byte |
| Baud rate | firmware accepts 240 .. 1 000 000, **default 115 200** |
| Optical front-end limit | **500 kbit/s**. Highest standard rate that works: **460 800**. Rates above are accepted by the firmware but the optics cannot follow. |
| Signalling | inverted: idle = LED off, start bit = LED on |
| Payload | raw bytes, **no packetisation, no CRC, no addressing** |

Both ends must be set to the same optical baud rate. The transceiver
runs from its internal RC oscillator (HSI16), see
[Clock tolerance](#clock-tolerance).

## Boot Behaviour and Initial State

About 10 ms after reset the transceiver sends 13 bytes on the host
UART:

```text
48 65 6C 6C 6F 20 57 6F 72 6C 64 0A 0D    "Hello World\n\r"
```

The host must discard this (flush RX) before sending commands.

Initial state after reset:

| Item | Value |
|---|---|
| Mode | polling (buffer) mode |
| Reception | disabled, receiver front end off |
| Optical baud rate | 115 200 |
| Comparator threshold | 2500 (of 4095) |
| TX buffer size | 2000 bytes |
| RX buffer size | 2000 bytes |

## Command Framing

A command is the escape byte `0x5C` (`\`) followed by one command byte
and a fixed number of argument bytes. No terminator, no checksum.

```text
5C  <cmd>  [arg0 arg1 ...]
```

Rules:

- `0x5C 0x5C` is a literal backslash. In feed-through mode it is sent
  over the optical link as a single `0x5C`. In polling mode it produces
  a NAK like any other data byte.
- Arguments are binary, not ASCII, and are **big-endian** (most
  significant byte first) in every command that takes a number.
- Argument bytes are never escaped. A `0x5C` inside an argument is an
  argument byte. The same applies to the payload of `\s`.
- An unknown command byte replies NAK.
- In polling mode, every byte that is not part of a command replies
  one NAK. Do not send line endings or other stray bytes.
- Commands may be sent back to back in one write. The transceiver
  processes them in order.

Responses:

| Byte | Meaning |
|---|---|
| `0x06` ACK | command executed |
| `0x15` NAK | command rejected or data byte outside a command in polling mode |

Read commands reply with data instead of ACK/NAK.

## Command Reference

| Cmd | Byte | Arguments | Reply | Description |
|---|---|---|---|---|
| Set buffer size | `N` 0x4E | uint16 BE | ACK / NAK | TX buffer = value, RX buffer = 4000 - value. Clears **both** buffers and resets all read/write positions. NAK if value > 4000. |
| Load buffer | `s` 0x73 | exactly *N* raw bytes | **none** | Streams *N* bytes into the TX buffer, where *N* is the value last set with `N`. No escaping inside the payload. |
| Send buffer | `S` 0x53 | none | ACK / NAK | Starts the optical transmission of the loaded bytes. ACK is sent when the transmission **starts**, not when it ends. NAK if the buffer is empty. |
| Set optical baud | `f` 0x66 | uint32 BE | ACK / NAK | Firmware range 240 .. 1 000 000; the optical front end is limited to 500 kbit/s, so use at most 460 800. Reconfigures both optical directions. NAK outside the firmware range. |
| Enable reception | `R` 0x52 | none | ACK | Switches the receiver front end on and starts capturing. ACK arrives >= 1 ms later (front-end settling). |
| Disable reception | `X` 0x58 | none | ACK | Stops capturing and switches the front end off. Already received, unread bytes stay readable. |
| Feed-through mode | `F` 0x46 | none | ACK | See [Modes](#operating-modes). |
| Polling mode | `P` 0x50 | none | ACK | See [Modes](#operating-modes). |
| Clear RX buffer | `C` 0x43 | none | ACK | Discards unread received bytes. Prefer `N` (see limitations). |
| Received count | `?` 0x3F | none | uint16 **LE** (2 bytes) | Number of received, unread bytes. Little-endian, LSB first. |
| Read received | `r` 0x72 | none | 0 .. RX-size raw bytes | Sends all unread received bytes. No length prefix, no terminator. Sends nothing if nothing arrived since the last `r`. |
| Set threshold | `T` 0x54 | uint16 BE | ACK / NAK | Comparator threshold, 0 .. 4095 (12-bit DAC, full scale = VDDA). NAK above 4096. |
| Read photodiode | `d` 0x64 | none | uint16 **BE** (2 bytes) | One 12-bit ADC sample of the photodiode, 0 .. 4095, MSB first. Takes about 1.5 ms. |

Note the two response encodings: `?` is little-endian, `d` is
big-endian. This is how the firmware is built; both are kept here
verbatim.

## Operating Modes

Two independent flags define the behaviour: **mode** (`P` / `F`) and
**reception** (`R` / `X`).

### Polling mode (`P`, default)

- Host -> optical: only through `N`, `s`, `S`. Any other non-command
  byte replies NAK.
- Optical -> host: received bytes are stored in the RX buffer. The host
  reads them with `?` and `r`. Nothing is sent to the host unasked.
- Responses on the host UART are unambiguous: only ACK, NAK and the
  replies of `?`, `r`, `d`.

### Feed-through mode (`F`)

- Host -> optical: every non-command byte is transmitted over the
  optical link immediately. Literal `0x5C` must be sent as `0x5C 0x5C`.
  `s` payload bytes are also sent immediately instead of being stored.
- Optical -> host: if reception is enabled, received bytes are written
  to the host UART as they arrive (after an idle gap of one byte time,
  or every half RX buffer during a continuous stream).
- The host UART therefore carries forwarded data **and** ACK/NAK bytes
  on the same stream. A received data byte `0x06` or `0x15` is
  indistinguishable from an ACK/NAK. Configure the transceiver fully
  before enabling reception in this mode and avoid sending commands
  while data may arrive.
- Sustained streaming needs optical baud >= 115 200. If the optical
  rate is lower than the host rate, the 512-byte host RX ring in the
  transceiver overruns and bytes are lost silently.

## Buffer Model

One 4000-byte region is shared:

```text
|<------ TX buffer (N) ------>|<------ RX buffer (4000 - N) ------>|
```

- `N` sets the split and clears everything.
- The TX buffer holds exactly what `s` streamed. `S` sends all of it.
  `s` can be repeated; each `s` overwrites from the start.
- The RX buffer is a ring. If more than its size arrives between two
  reads, the oldest bytes are overwritten and the count from `?` is
  wrong. No overrun indication exists. Size the RX buffer for the
  longest burst between reads.

## Sequences

All examples are hex bytes on the host UART. `->` host to transceiver,
`<-` transceiver to host.

### Transmit a block (polling mode)

Send "Hi" (2 bytes):

```text
-> 5C 50                  \P   polling mode
<- 06
-> 5C 4E 00 02            \N   TX buffer = 2 bytes
<- 06
-> 5C 73 48 69            \s   payload "Hi"    (no reply)
-> 5C 53                  \S   send
<- 06                                           (transmission starts)
```

Transmission time = bytes x 10 / optical baud. At 115 200 baud a
2000-byte buffer takes 174 ms; at 460 800 it takes 43 ms. Do not send
`S` again before that time has passed, and do not `s` new data into
the buffer while it is being sent (the DMA reads the buffer live).

### Transmit a stream (feed-through mode)

```text
-> 5C 46                  \F   feed-through
<- 06
-> 48 69 5C 5C 21         "Hi\!"   -> optical link carries 48 69 5C 21
```

### Receive (polling mode)

```text
-> 5C 50                  \P   polling mode
<- 06
-> 5C 54 09 C4            \T   threshold 2500
<- 06
-> 5C 4E 07 D0            \N   TX 2000 / RX 2000, resets positions
<- 06
-> 5C 52                  \R   enable reception
<- 06                          (>= 1 ms later)

   ... optical data arrives ...

-> 5C 58                  \X   freeze: stop capturing
<- 06
-> 5C 3F                  \?   count
<- 02 00                       2 bytes unread (little-endian)
-> 5C 72                  \r   read
<- 48 69                       "Hi"
```

Freezing with `X` before `?`/`r` guarantees that `r` returns exactly
the number of bytes reported by `?`. Without `X`, more bytes may land
between the two commands and `r` returns more than `?` said.

To receive again, issue `N` (with the same value) and then `R`. See
[Known limitations](#known-firmware-limitations-and-workarounds) for
why `N` is required.

### Receive (feed-through mode)

```text
-> 5C 46 5C 52            \F \R
<- 06 06
   ... every received byte appears here as it arrives ...
```

### Change the optical baud rate

Both transceivers:

```text
-> 5C 66 00 07 08 00      \f   460 800
<- 06
```

If the receiver was enabled, send `R` again afterwards (see
limitations).

## Timing

| Parameter | Value | Notes |
|---|---|---|
| Boot message | ~10 ms after reset | 13 bytes, flush it |
| Command reply latency | < 1 ms | except `R` and `d` |
| `R` reply | ~1 ms | front-end enable delay |
| `d` reply | ~1.5 ms | enable delay + conversion |
| `S` reply | immediate | transmission runs in background |
| Optical transmission time | bytes x 10 / baud | host must wait this long |
| Recommended host timeout | 20 ms | for any ACK/NAK or fixed-size reply |
| Recommended `r` read | until 20 ms of silence | reply has no length prefix |
| Receive-to-host latency (feed-through) | ~1 byte time + < 1 ms | idle detection |

The transceiver services the host UART from a main loop that sleeps
between events. Commands that arrive while it is busy writing a long
`r` reply are queued and processed afterwards.

## Error Handling

| Situation | Transceiver behaviour | Host action |
|---|---|---|
| Unknown command byte | NAK | fix host |
| Data byte in polling mode | NAK per byte | fix host, do not send stray bytes |
| `N` > 4000 | NAK | |
| `f` outside 240 .. 1 000 000 | NAK | |
| `f` above 460 800 | ACK, but the optics cannot follow | host must never send it |
| `T` > 4096 | NAK | |
| `S` with empty buffer | NAK | load with `s` first |
| No reply within 20 ms | transceiver hung, reset or link broken | retry once, then power-cycle the transceiver |
| Optical frame/noise error | byte dropped, reception continues | detect via host-level CRC |
| Optical overrun | reception stops silently | send `X`, `N`, `R` to restart |

Recovery sequence that returns the transceiver to a known state
without a reset: `\P \X \N <size>`.

## Known Firmware Limitations and Workarounds

These are properties of the firmware at the revision named above. The
host must respect them until the firmware is fixed.

| # | Limitation | Workaround |
|---|---|---|
| 1 | `s` accepts at most 255 bytes. For `N` >= 256 the payload length is truncated modulo 256; for multiples of 256 it is 0 and `s` replies NAK. | Keep `N` <= 255 per block. |
| 2 | `R` after a previous receive session does not reset the read position. The first bytes of the new session are misreported or lost. | Always send `N` (same value) directly before `R`. Use `N` instead of `C` to clear. |
| 3 | `f` switches the receiver front end off even when reception is enabled. | Send `R` again after `f`. |
| 4 | `d` in polling mode with reception enabled switches the front end off. | Only use `d` when reception is disabled, or send `R` again after `d`. |
| 5 | `S` and `?`/`d` replies are started from short-lived memory; a reply byte can be corrupted under unusual timing. | Validate replies where possible, retry on garbage. |
| 6 | No CRC or framing on the optical link. | The host adds its own frame sync, length and CRC. |
| 7 | ACK/NAK bytes and forwarded data share the host UART in feed-through mode. | Configure before `R`, or use polling mode. |
| 8 | `f` while a `S` transmission is still running replies NAK. | Wait `bytes x 10 / baud` after the `S` ACK before reconfiguring the sender. |

## Clock Tolerance

The transceiver has no crystal. Its 16 MHz RC oscillator is specified
at -0.75 % .. +0.5 % at 30 C and drifts up to -2 % .. +1.5 % over
-40 .. 125 C. UART tolerates roughly 3.75 % total mismatch between
sender and receiver. Two transceivers at different temperatures can
exceed that. Consequences for the host:

- The **host UART** at 115 200 is safe as long as the host has a
  crystal.
- The **optical link** carries the error of both transceivers. Expect
  bit errors at high temperature differences. A host-level CRC and a
  fallback to a lower optical rate are required for flight.

## Differences to ICD-003

Items in ICD-003 v0.1 that do not match the firmware:

| ICD-003 says | Firmware does |
|---|---|
| Host UART 921 600 baud | Host UART is fixed at 115 200. `f` only changes the optical link. |
| Optical rate 921 600 in the rate table | Optical front end is limited to 500 kbit/s. Highest usable standard rate is 460 800. |
| `N` uint16 LE | uint16 **BE** |
| `f` uint32 LE | uint32 **BE** |
| Buffer <= 10 KB | 4000 bytes total, shared between TX and RX; `s` limited to 255 |
| `?` uint16 LE | correct |
| no `T`, `d` | present, see reference |
| ACK within 20 ms after `S` means sent | ACK means transmission started; add bytes x 10 / baud |

## Non-Blocking Operation: BER Rounds (EXP2)

The host must never block on the transceiver. EXP2 runs one bit-error
round at a time across several 40 ms ticks; all UART traffic goes
through interrupt-driven rings, and every phase has a tick budget, so a
silent transceiver costs one round and never stalls the tick.

The round replicates the transceiver team's bench script
(`tools/TransceiverPyControl/speedtest_simple.py` in the firmware
repository). It passed on the bench on 2026-10-06 (self-test step 6:
64 of 64 bytes, 0 bit errors). An earlier feed-through scheme (receiver
forwarding optical bytes unsolicited, one round per tick) never
delivered a block on the hardware and was dropped.

### Round sequence

| Phase | Sender | Receiver | Ends when | Tick budget |
|---|---|---|---|---|
| SETUP | `\P \N <64> \s <block> \f <baud>` (3 ACKs) | `\P \f <baud> \T <thr> \N <64> \R` (5 ACKs) | all 8 ACKs counted | 10 |
| SENDING | `\S` (1 ACK) | - | ACK counted | 10 |
| SETTLE | - | - | 3 ticks passed (64 B take 67 ms at 9 600) | 3 |
| READING | - | `\r`, reply collected | line quiet for 2 ticks, or nothing after 4 ticks | 20 |

`\N` sits directly before `\R` so every round starts its read position
afresh (limitation #2). Both setup scripts are written in one go and the
ACKs come back in order, so the host only counts them. A NAK, or a phase
over its budget, ends the round as failed.

In FLIGHT every round sends a fresh block: PRBS-15 (ICD-003 pattern
code 0x01) from a new 15-bit seed. EXP2_BER carries `round_seq`,
`pattern`, `seed`, `block_offset` and `rx_count`; the received bytes
themselves (the first 512) follow as EXP2_RX chunks of 40 bytes with the
same `round_seq`, two per tick, before the next round starts. From these
the ground rebuilds the sent block and every bit error position, and
sees the bytes around the block (`tools/lifi-errors.py`). Self-test
step 6 keeps the fixed PRBS-7 block.

EXP2_BER's latency (`timestamp_recv_us - timestamp_send_us`) is the time
from handing `\S` to the sender's host UART until the sender's ACK
arrives, stamped by the UART interrupt. It covers the host UART both ways
and the sender's command handling, not the optical path: in polling mode
the host cannot see when the receiver got the light. The difference is
computed on EXP2 wrap-safe (the us clock wraps every 2^32 cycles, 53.7 s
at 80 MHz), so the ground subtraction is valid across a wrap.

The `\r` reply carries no framing (limitation #6). The host keeps up to
512 bytes and scores every 64-byte window against the pattern; the
window with the fewest bit errors counts, a byte that never arrived
counts as 8 errors.

### Comparator threshold

The receiver output depends on the ambient light. On the bench
(2026-10-06, room light dimmed, 115 200) the thresholds 2000, 2500,
3000 and 3500 each delivered the block intact, 4000 produced a
continuous `0x00` stream and 1500 and below delivered nothing. In bright
room light the photodiode amplifier sat at its 3.3 V rail and no
threshold decoded the block. Whether this window holds inside the closed
module is not known.

EXP2 flight cycles the four working thresholds at each of the ICD-003
rates 9 600, 38 400, 115 200 and 460 800 (rate table entries 0..3), one
round per combination; each EXP2_BER packet carries the rate index and
the threshold it used (ICD-007). Only 115 200 has been tested on the
bench; whether the threshold window holds at the other rates is not
known. 921 600 (entry 4) is left out, it is above what the front end
follows (Optical link, above).

### Failure handling

| Event | Host action |
|---|---|
| NAK or missing ACK | round ends as failed, EXP2_BER `measurement_valid = 0` |
| Reply empty | round valid as a protocol exchange, EXP2_BER `measurement_valid = 0` |
| Reply present | EXP2_BER valid, bit errors of the best window |
| 3 failed rounds in a row | the transceiver that owed the reply is latched, trace `LIFI_ROUND` (`PROTOCOL_ERROR` for a NAK, `TIMEOUT` for silence) |

An empty reply is a dark round, not a link fault, and does not count
towards the latch. A latched transceiver is retried after the
DeviceBase cooldown (ADR-005).

## Flight-Software Driver

`flight-software/shared/lifi/`, C++23, no allocation, no blocking:

| File | Content |
|---|---|
| `lifi_protocol.hpp` | command bytes, limits, `frame_time_ms`, the `Script<N>` encoder |
| `lifi_pattern.hpp` | PRBS-7 / PRBS-15 / fixed test patterns (ICD-003 codes), compile-time |
| `lifi_transceiver.hpp/.cpp` | `LifiTransceiver` (ACK counting, strikes), `LifiSender`, `LifiReceiver` |
| `lifi_transceiver.test.cpp` | Catch2 host tests against a fake link |

The UART underneath is a `UartLink` (`shared/comms/uart_link.hpp`):
`write()` enqueues a whole script or nothing, `read()` drains the
interrupt ring, both without blocking. The flight implementation is
`CmsisUartLink` over the CMSIS-Driver USART; host tests supply a fake.

EXP2 uses `LifiSender` / `LifiReceiver` for the ping self-test steps
and as the health latch of each transceiver. The BER round itself drives
both host UARTs directly (`exp2-bouncy-castle/app/exp2_computer.cpp`,
round engine shared by FLIGHT and self-test step 6), because the
feed-through receiver path never worked on the hardware.

Errors are `Result<T>` with the `Step` codes `0x9x`
(`docs/handbook/fault-trace-codes.md`); the fault LED codes are 11
(sender) and 12 (receiver) (`docs/handbook/led-blink-codes.md`).
