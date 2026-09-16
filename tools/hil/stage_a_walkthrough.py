#!/usr/bin/env python3
"""Walk an operator through Stage A (bench sensor calibration) and assert the
structured [CAL] serial log lines (contracts/serial-log.md) confirm a
successful save at High accuracy.

This script only observes the serial log and prompts the operator -- it does
not press Start itself, since Stage A is meant to be started from the phone
UI exactly as a real owner would. Run it, then open the Calibration tab on
your phone and tap Start when prompted.

Usage:
    python tools/hil/stage_a_walkthrough.py --port <PORT> [--timeout 360]
"""

import argparse
import re
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
    for token in rest.split():
        if "=" in token:
            k, v = token.split("=", 1)
            kv[k] = v
    return tag, kv


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", required=True, help="serial port, e.g. COM5 or /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=360.0, help="hard overall timeout in seconds (default: 360)")
    args = parser.parse_args()

    print("Opening", args.port, "...")
    ser = serial.Serial(args.port, args.baud, timeout=1)

    seen_awaiting_stillness = False
    seen_awaiting_positions = False
    seen_awaiting_rotation = False
    result_line = None

    prompted_stillness = False
    prompted_positions = False
    prompted_rotation = False

    print("Open the Calibration tab on your phone and tap Start on Stage A when ready.")
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

            tag, kv = parse_line(line)
            if tag != "CAL" or kv.get("stage") != "A":
                continue

            print("  ", line.strip())

            state = kv.get("state")
            if state == "AwaitingStillness" and not prompted_stillness:
                seen_awaiting_stillness = True
                prompted_stillness = True
                print(">>> Set the compass down on a stable surface and leave it untouched.")
            if "positions" in kv and not prompted_positions:
                seen_awaiting_positions = True
                prompted_positions = True
                print(">>> Hold the compass steady in each of its six positions in turn, a few seconds each.")
            if state == "AwaitingRotation" and not prompted_rotation:
                seen_awaiting_rotation = True
                prompted_rotation = True
                print(">>> Slowly rotate the compass through as many orientations as you can (figure-8 motion).")

            if "result" in kv:
                result_line = kv
                break
    finally:
        ser.close()

    if result_line is None:
        print("FAIL: no [CAL] stage=A result=... line observed within", args.timeout, "s", file=sys.stderr)
        sys.exit(1)

    if result_line.get("result") != "saved":
        print("FAIL: Stage A did not save:", result_line, file=sys.stderr)
        sys.exit(1)

    mag = int(result_line.get("mag", -1))
    accel = int(result_line.get("accel", -1))
    gyro = int(result_line.get("gyro", -1))
    ok = mag == 3 and accel == 3 and gyro == 3

    print()
    print("[stage_a_walkthrough] result=saved mag=%d accel=%d gyro=%d" % (mag, accel, gyro))
    if not ok:
        print("FAIL: saved result was not all-High (3)", file=sys.stderr)
        sys.exit(1)

    if not (seen_awaiting_stillness and seen_awaiting_positions and seen_awaiting_rotation):
        print("WARNING: not all expected state transitions were observed on serial "
              "(may just be a fast run); result=saved all-High is the authoritative pass criterion.")

    print("PASS: Stage A saved with mag=3 accel=3 gyro=3")
    sys.exit(0)


if __name__ == "__main__":
    main()
