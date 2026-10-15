#!/usr/bin/env python3
"""Bit error positions of every EXP2 LiFi round in a telemetry recording.

Joins each EXP2_BER packet with its EXP2_RX chunks (by round_seq),
rebuilds the block EXP2 sent from pattern + seed, and compares it with
the received bytes at block_offset. Decoding is done by bolt-postflight,
so this script only knows the PRBS generator and the join.

    tools/lifi-errors.py capture.raw                  # one line per round
    tools/lifi-errors.py capture.raw --bits           # plus every error bit position
    tools/lifi-errors.py capture.raw --csv errors.csv # one row per error bit

Bit positions count from the first bit sent: byte i, bit 7 is position
8*i, bit 0 is 8*i+7 (the transceiver sends MSB-first blocks as UART bytes,
so this is block order, not the order the bits were on the light).
"""

import argparse
import csv
import os
import subprocess
import sys
import tempfile
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
TT = os.path.join(HERE, "..", "telemetry-tools")
BLOCK = 64
BAUD = {0: 9600, 1: 38400, 2: 115200, 3: 460800, 4: 921600}  # ICD-003 rate table
PRBS7, PRBS15 = 0x00, 0x01  # ICD-003 pattern codes


def lfsr_bytes(state, order, n):
    """Same generator as shared/lifi/lifi_pattern.hpp: taps at the two top stages, MSB first."""
    mask = (1 << order) - 1
    out = []
    for _ in range(n):
        byte = 0
        for _ in range(8):
            bit = ((state >> (order - 1)) ^ (state >> (order - 2))) & 1
            state = ((state << 1) | bit) & mask
            byte = (byte << 1) | bit
        out.append(byte)
    return out


def expected_block(pattern, seed):
    if pattern == PRBS15:
        s = seed & 0x7FFF
        return lfsr_bytes(s if s else 0x7FFF, 15, BLOCK)
    if pattern == PRBS7:
        return lfsr_bytes(0x7F, 7, BLOCK)
    return None  # fixed patterns are not used in flight


def decode(raw, workdir):
    def export(ty):
        out = os.path.join(workdir, f"{ty}.csv")
        subprocess.run(
            ["cargo", "run", "-q", "-p", "bolt-postflight", "--", raw,
             "--manifest", os.path.join(workdir, "m.json"), "--csv", ty, "--csv-out", out],
            cwd=TT, check=True, capture_output=True)
        return list(csv.DictReader(open(out))) if os.path.exists(out) else []
    return export("exp2_ber"), export("exp2_rx")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("raw", help="recording from the station's port 5000 (.raw)")
    ap.add_argument("--bits", action="store_true", help="print every error bit position")
    ap.add_argument("--csv", help="write one row per error bit to this file")
    a = ap.parse_args()

    with tempfile.TemporaryDirectory() as tmp:
        bers, chunks = decode(os.path.abspath(a.raw), tmp)
    if not bers:
        sys.exit("no EXP2_BER packets in the recording")

    rx = defaultdict(dict)  # round_seq -> chunk_index -> bytes
    counts = {}
    for c in chunks:
        seq, idx = int(c["round_seq"]), int(c["chunk_index"])
        n = int(float(c["length"]))
        rx[seq][idx] = [int(float(c[f"b{i}"])) for i in range(n)]
        counts[seq] = int(c["chunk_count"])

    rows = []
    print(f"{'round':>6} {'baud':>7} {'thr':>5} {'seed':>6} {'rx':>5} {'off':>4} {'err':>4}  positions")
    for b in bers:
        seq = int(b["round_seq"])
        valid = b["measurement_valid"] in ("1", "1.0")
        exp = expected_block(int(b["pattern"]), int(b["seed"]))
        got_chunks = rx.get(seq, {})
        need = counts.get(seq, 0)
        complete = need > 0 and all(i in got_chunks for i in range(need))
        received = [x for i in range(need) for x in got_chunks.get(i, [])] if complete else None
        line = (f"{seq:>6} {BAUD.get(int(b['rate_index']), '?'):>7} {b['threshold']:>5} "
                f"0x{int(b['seed']):04x} {b['rx_count']:>5}")
        if not valid:
            print(f"{line}    -    -  no measurement")
            continue
        off = int(b["block_offset"])
        if received is None:
            print(f"{line} {off:>4} {b['bit_errors']:>4}  EXP2_RX chunks missing ({len(got_chunks)}/{need})")
            continue
        positions = []
        for i in range(BLOCK):
            got = received[off + i] if off + i < len(received) else None
            diff = 0xFF if got is None else got ^ exp[i]
            for bit in range(8):
                if diff & (0x80 >> bit):
                    positions.append(8 * i + bit)
        mismatch = "" if len(positions) == int(b["bit_errors"]) else f"  (EXP2 said {b['bit_errors']})"
        shown = f"{positions[:12]}{' ...' if len(positions) > 12 else ''}" if positions else "none"
        print(f"{line} {off:>4} {len(positions):>4}  {shown}{mismatch}")
        if a.bits and positions:
            print("        " + " ".join(map(str, positions)))
        for p in positions:
            rows.append({"round_seq": seq, "tick": b["tick"], "baud": BAUD.get(int(b["rate_index"])),
                         "threshold": b["threshold"], "seed": b["seed"], "block_offset": off,
                         "bit_position": p, "byte": p // 8, "bit_in_byte": 7 - p % 8})

    if a.csv:
        with open(a.csv, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=["round_seq", "tick", "baud", "threshold", "seed",
                                              "block_offset", "bit_position", "byte", "bit_in_byte"])
            w.writeheader()
            w.writerows(rows)
        print(f"\n{len(rows)} error bits written to {a.csv}")


if __name__ == "__main__":
    main()
