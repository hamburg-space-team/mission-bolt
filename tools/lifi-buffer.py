#!/usr/bin/env python3
"""Show what the EXP2 LiFi receiver captured in the last BER round.

Reads the self-test state of Exp2Computer over the debug station's GDB
port and prints:
  - the threshold table of the last sweep (bytes received, bit errors,
    best offset per threshold),
  - the capture buffer of the last round as a hex dump with ASCII,
  - a byte histogram and the best match against the 64-byte PRBS-7 block.

The capture holds only the LAST round - the last threshold of the
self-test sweep, or the latest FLIGHT round, which overwrites it. The
per-threshold table is written by the self-test only.

    tools/lifi-buffer.py                # station from $BOLT_STATION
    tools/lifi-buffer.py --all          # dump all 512 bytes, not just the received ones
    tools/lifi-buffer.py --elf path.elf

The core is halted for the read and resumed afterwards.
"""

import argparse
import json
import os
import re
import subprocess
import sys
import urllib.request
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_ELF = os.path.join(HERE, "..", "flight-software", "out", "exp2", "exp2-bouncy-castle", "Debug", "exp2.elf")
INSTANCE = "'(anonymous namespace)::instance_g'"
CAPTURE = 512
BLOCK = 64


def prbs7(n):
    """Same generator as shared/lifi/lifi_pattern.hpp: x^7 + x^6 + 1, seed 0x7F, MSB first."""
    state, out = 0x7F, []
    for _ in range(n):
        byte = 0
        for _ in range(8):
            bit = ((state >> 6) ^ (state >> 5)) & 1
            state = ((state << 1) | bit) & 0x7F
            byte = (byte << 1) | bit
        out.append(byte)
    return out


def exp2_gdb_port(host):
    """GDB port of the probe sitting on EXP2, or None. Never guess a port."""
    try:
        with urllib.request.urlopen(f"http://{host}:8080/api/debugger", timeout=6) as r:
            probes = json.load(r).get("probe", {}).get("probes", [])
    except OSError as e:
        sys.exit(f"station API unreachable at {host}:8080 ({e}) - run scripts/set-station-ip.sh --discover")
    for p in probes:
        if p.get("board") == "exp2":
            return p.get("gdb_port")
    return None


def gdb_read(elf, host, port):
    cmds = [
        "set pagination off",
        "set print elements 0",
        "set print repeats 0",
        f"target extended-remote {host}:{port}",
        f"p/u {INSTANCE}->round_count",
        f"p/u {INSTANCE}->sweep_idx",
        f"p/u {INSTANCE}->round_best_offset",
        f"p/u {INSTANCE}->sweep_count_at",
        f"p/u {INSTANCE}->sweep_err_at",
        f"p/u {INSTANCE}->sweep_offset_at",
        "p/u Exp2Computer::LOOPBACK_THRESHOLDS",
        f"x/{CAPTURE}xb {INSTANCE}->round_buf._M_elems",
        "monitor resume",
        "detach",
    ]
    args = ["arm-none-eabi-gdb", "-q", "-batch", elf]
    for c in cmds:
        args += ["-ex", c]
    res = subprocess.run(args, capture_output=True, text=True, timeout=60)
    return res.stdout + res.stderr


def parse(out):
    values = {}
    for m in re.finditer(r"^\$(\d+) = (.*)$", out, re.M):
        values[int(m.group(1))] = m.group(2)

    def scalar(i):
        return int(values[i]) if i in values else None

    def array(i):
        if i not in values:
            return None
        inner = re.search(r"\{([0-9, ]+)\}", values[i])
        return [int(x) for x in inner.group(1).split(",")] if inner else None

    buf = []
    for line in re.findall(r"^0x[0-9a-f]+.*?:\s*(.*)$", out, re.M):
        buf += [int(x, 16) for x in re.findall(r"0x([0-9a-f]{2})\b", line)]
    return {
        "count": scalar(1),
        "sweep_idx": scalar(2),
        "best_offset": scalar(3),
        "count_at": array(4),
        "err_at": array(5),
        "offset_at": array(6),
        "thresholds": array(7),
        "buf": buf[:CAPTURE],
    }


def best_match(buf, pattern):
    """Offset with the fewest bit errors against the full pattern block."""
    best = (None, None)
    for off in range(0, max(0, len(buf) - len(pattern)) + 1):
        errs = sum(bin(a ^ b).count("1") for a, b in zip(buf[off:off + len(pattern)], pattern))
        if best[1] is None or errs < best[1]:
            best = (off, errs)
    return best


def hexdump(data, expected_at=None, pattern=None):
    for row in range(0, len(data), 16):
        chunk = data[row:row + 16]
        cells = []
        for i, b in enumerate(chunk):
            pos = row + i
            mark = " "
            if expected_at is not None and expected_at <= pos < expected_at + len(pattern):
                mark = "=" if b == pattern[pos - expected_at] else "!"
            cells.append(f"{b:02x}{mark}")
        text = "".join(chr(b) if 0x20 <= b < 0x7F else "." for b in chunk)
        print(f"  {row:04x}  {' '.join(cells):<64} |{text}|")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--elf", default=DEFAULT_ELF)
    ap.add_argument("--host", default=os.environ.get("BOLT_STATION", "bolt-station.local"))
    ap.add_argument("--all", action="store_true", help="dump all 512 capture bytes")
    a = ap.parse_args()

    if not os.path.exists(a.elf):
        sys.exit(f"ELF not found: {a.elf} - build exp2.Debug+exp2-bouncy-castle first")
    port = exp2_gdb_port(a.host)
    if port is None:
        sys.exit("no probe on exp2 at the station - refusing to read another board")

    d = parse(gdb_read(a.elf, a.host, port))
    if d["count"] is None or not d["buf"]:
        sys.exit("GDB read failed (ELF does not match the flashed image?)")

    pattern = prbs7(BLOCK)
    thr = d["thresholds"] or [None] * 8
    if not any(d["count_at"] or []) and not any(d["err_at"] or []):
        print("No loopback since the last boot - start the self-test first (step 6).")
        return
    print("Threshold sweep (last self-test run):")
    print("  thr   bytes  bit_err/512  offset")
    for i in range(len(thr)):
        err = d["err_at"][i] if d["err_at"] else None
        err_s = "no reply" if err == 0xFFFF else str(err)
        print(f"  {thr[i]!s:>4}  {d['count_at'][i]:>5}  {err_s:>11}  {d['offset_at'][i]:>6}")

    n = min(d["count"], CAPTURE)
    received = d["buf"][:n]
    last_thr = thr[d["sweep_idx"]] if d["sweep_idx"] is not None and d["sweep_idx"] < len(thr) else "?"
    print(f"\nCapture of the last round (threshold {last_thr}): {d['count']} bytes received, "
          f"{min(n, CAPTURE)} kept")
    if received:
        hist = Counter(received).most_common(5)
        print("  most common bytes: " + ", ".join(f"0x{b:02x} x{c}" for b, c in hist))
        off, errs = best_match(received, pattern)
        if off is not None:
            print(f"  best match vs. 64-byte PRBS-7: offset {off}, {errs}/{BLOCK * 8} bit errors"
                  + ("  <-- block received intact" if errs == 0 else ""))
        print("  ('=' byte matches the pattern at the best offset, '!' differs)\n")
        hexdump(received, off, pattern)
    else:
        print("  (empty - the receiver returned nothing)")
    if a.all and n < CAPTURE:
        # a round only overwrites as many bytes as it receives; the rest is from earlier rounds
        print(f"\nBytes {n}..{CAPTURE - 1}: STALE, left over from earlier rounds or runs:")
        hexdump(d["buf"][n:])
    print("\nExpected block (PRBS-7, 64 bytes):")
    hexdump(pattern)


if __name__ == "__main__":
    main()
