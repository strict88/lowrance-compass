#!/usr/bin/env python3
"""Assert a device with a saved Stage A calibration restores a valid heading
within 10 s of boot (SC-006), without needing to redo Stage A.

Prompts the operator to power-cycle the device (this script has no way to
do that itself), then watches the serial log for [SYS] boot ... followed by
the first [IMU] valid=1 ... line, asserting the gap is within the bound.

Usage:
    python tools/hil/boot_restore_check.py --port <PORT> --timeout 10
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
        return None, []
    try:
        tag_end = line.index("]")
    except ValueError:
        return None, []
    tag = line[1:tag_end]
    tokens = line[tag_end + 1:].strip().split()
    return tag, tokens


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=10.0,
                         help="max seconds after boot before valid=1 must appear (SC-006 bound)")
    parser.add_argument("--overall-timeout", type=float, default=60.0,
                         help="hard overall timeout waiting for boot + restore (default: 60)")
    args = parser.parse_args()

    print("Power-cycle the device now (it must already have a saved Stage A calibration).")

    ser = serial.Serial(args.port, args.baud, timeout=1)

    boot_time = None
    overall_deadline = time.monotonic() + args.overall_timeout

    try:
        while time.monotonic() < overall_deadline:
            raw = ser.readline()
            if not raw:
                continue
            try:
                line = raw.decode("utf-8", errors="replace")
            except Exception:
                continue

            tag, tokens = parse_line(line)

            if tag == "SYS" and "boot" in tokens and boot_time is None:
                boot_time = time.monotonic()
                print("  ", line.strip())
                continue

            if boot_time is None:
                continue

            if tag == "IMU" and "valid=1" in tokens:
                elapsed = time.monotonic() - boot_time
                print("  ", line.strip())
                print()
                print("[boot_restore_check] valid=1 appeared %.2fs after boot" % elapsed)
                if elapsed <= args.timeout:
                    print("PASS: within the %.0fs bound" % args.timeout)
                    sys.exit(0)
                else:
                    print("FAIL: exceeded the %.0fs bound" % args.timeout, file=sys.stderr)
                    sys.exit(1)
    finally:
        ser.close()

    if boot_time is None:
        print("FAIL: no [SYS] boot line observed within %.0fs" % args.overall_timeout, file=sys.stderr)
    else:
        print("FAIL: no [IMU] valid=1 line observed within %.0fs of boot" % args.overall_timeout, file=sys.stderr)
    sys.exit(1)


if __name__ == "__main__":
    main()
