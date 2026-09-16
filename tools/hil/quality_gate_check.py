#!/usr/bin/env python3
"""Assert PGN 127250 is never transmitted while sensor accuracy is below
High, per FR-041/SC-007.

Works two ways:
  - Naturally: a freshly-booted device with no saved Stage A calibration
    already has low/unreliable accuracy, so simply capturing the boot
    window is enough.
  - Operator-guided: bring a magnet or other source of magnetic
    interference near an already-calibrated device to force accuracy back
    down, per the prompt this script prints.

Usage:
    python tools/hil/quality_gate_check.py --port <PORT> --timeout 30
"""

import argparse
import sys
import time

try:
    import serial
except ImportError:
    print("ERROR: pyserial is required (pip install -r tools/hil/requirements.txt)", file=sys.stderr)
    sys.exit(2)


def parse_line(line):
    line = line.strip()
    if not line.startswith("["):
        return None, {}
    try:
        tag_end = line.index("]")
    except ValueError:
        return None, {}
    tag = line[1:tag_end]
    rest = line[tag_end + 1:].strip()
    kv = {}
    tokens = rest.split()
    for token in tokens:
        if "=" in token:
            k, v = token.split("=", 1)
            kv[k] = v
    return tag, kv, tokens


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=30.0)
    args = parser.parse_args()

    print("If the device is already calibrated, bring a magnet near the sensor now")
    print("to force accuracy below High; a freshly-booted, never-calibrated device")
    print("needs no operator action.")

    ser = serial.Serial(args.port, args.baud, timeout=1)

    tx_ok_count = 0
    tx_skipped_count = 0
    deadline = time.monotonic() + args.timeout

    try:
        while time.monotonic() < deadline:
            raw = ser.readline()
            if not raw:
                continue
            try:
                line = raw.decode("utf-8", errors="replace")
            except Exception:
                continue

            tag, kv, tokens = parse_line(line)
            if tag != "N2K" or "tx" not in tokens or kv.get("pgn") != "127250":
                continue

            print("  ", line.strip())
            if "ok" in tokens:
                tx_ok_count += 1
            elif kv.get("reason") == "quality_gate":
                tx_skipped_count += 1
    finally:
        ser.close()

    print()
    print("tx ok=%d, tx skipped(quality_gate)=%d" % (tx_ok_count, tx_skipped_count))

    if tx_ok_count > 0:
        print("FAIL: heading was transmitted while accuracy was expected to be below High", file=sys.stderr)
        sys.exit(1)
    if tx_skipped_count == 0:
        print("FAIL: never observed a quality_gate skip line -- accuracy may already be High "
              "(re-run after bringing a magnet near the sensor, or on a fresh uncalibrated boot)", file=sys.stderr)
        sys.exit(1)

    print("PASS: zero heading transmits, at least one quality_gate skip observed")
    sys.exit(0)


if __name__ == "__main__":
    main()
