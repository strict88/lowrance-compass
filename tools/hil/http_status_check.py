#!/usr/bin/env python3
"""Assert GET /api/status has the expected shape and values over HTTP, in
bench Station mode (quickstart.md section 7).

By default this only checks the response's shape/schema. Pass
--expect-stage-a-done to additionally assert Stage A reads DONE with
mag/accel/gyro all High (run tools/hil/stage_a_walkthrough.py first to get
there), and that Stage B/C still read NOT_DONE and readiness is
USABLE_INCOMPLETE -- covering T076 (contracts/rest-api.md, US2).

Usage:
    python tools/hil/http_status_check.py --host <device-ip> [--expect-stage-a-done]
"""

import argparse
import sys

try:
    import requests
except ImportError:
    print("ERROR: requests is required (pip install -r tools/hil/requirements.txt)", file=sys.stderr)
    sys.exit(2)

REQUIRED_TOP_LEVEL_KEYS = ("schema", "readiness", "heading", "stages", "session", "n2k", "settings", "system")
REQUIRED_STAGE_KEYS = ("a", "b", "c")


def check_shape(status):
    errors = []
    for key in REQUIRED_TOP_LEVEL_KEYS:
        if key not in status:
            errors.append("missing top-level key: %s" % key)

    if status.get("schema") != 1:
        errors.append("schema != 1: %r" % status.get("schema"))

    stages = status.get("stages", {})
    for key in REQUIRED_STAGE_KEYS:
        if key not in stages:
            errors.append("missing stages.%s" % key)
        elif "state" not in stages[key]:
            errors.append("missing stages.%s.state" % key)

    if status.get("readiness") not in ("NOT_CALIBRATED", "USABLE_INCOMPLETE", "READY"):
        errors.append("unexpected readiness value: %r" % status.get("readiness"))

    return errors


def check_stage_a_done(status):
    errors = []
    stage_a = status.get("stages", {}).get("a", {})
    if stage_a.get("state") != "DONE":
        errors.append("stages.a.state != DONE: %r" % stage_a.get("state"))
    quality = stage_a.get("quality") or {}
    for sensor in ("mag", "accel", "gyro"):
        if quality.get(sensor) != 3:
            errors.append("stages.a.quality.%s != 3: %r" % (sensor, quality.get(sensor)))
    if not stage_a.get("saved_at"):
        errors.append("stages.a.saved_at is empty/null")

    stage_b = status.get("stages", {}).get("b", {})
    stage_c = status.get("stages", {}).get("c", {})
    if stage_b.get("state") != "NOT_DONE":
        errors.append("stages.b.state != NOT_DONE: %r" % stage_b.get("state"))
    if stage_c.get("state") != "NOT_DONE":
        errors.append("stages.c.state != NOT_DONE: %r" % stage_c.get("state"))

    if status.get("readiness") != "USABLE_INCOMPLETE":
        errors.append("readiness != USABLE_INCOMPLETE with only Stage A done: %r" % status.get("readiness"))

    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", required=True, help="device IP or hostname (bench Station mode)")
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--expect-stage-a-done", action="store_true",
                         help="also assert Stage A is DONE at all-High and B/C are still NOT_DONE")
    args = parser.parse_args()

    url = "http://%s/api/status" % args.host
    resp = requests.get(url, timeout=args.timeout)
    resp.raise_for_status()
    status = resp.json()

    errors = check_shape(status)
    if args.expect_stage_a_done:
        errors += check_stage_a_done(status)

    if errors:
        print("FAIL:")
        for e in errors:
            print("  -", e)
        sys.exit(1)

    print("PASS: GET /api/status shape OK" + (", Stage A done / B+C not done" if args.expect_stage_a_done else ""))
    sys.exit(0)


if __name__ == "__main__":
    main()
