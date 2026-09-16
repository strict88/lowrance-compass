#!/usr/bin/env python3
"""Drive Stage B's GPS-course method end-to-end on the bench, using synthetic
GPS input (quickstart.md section 6), and assert the resulting
InstallationAlignment via GET /api/status.

Requires the device built with -D DEBUG_GPS_INJECT=1 (the `esp32s3_bench`
PlatformIO environment) in bench Station mode. The device must already be
sitting still (for the level-capture step) when this script starts.

Usage:
    python tools/hil/stage_b_check.py --host <device-ip> [--sog-kn 5.0] [--cog-deg 90.0] [--variation-deg -8.0]
"""

import argparse
import sys
import time

try:
    import requests
except ImportError:
    print("ERROR: requests is required (pip install -r tools/hil/requirements.txt)", file=sys.stderr)
    sys.exit(2)


def post(host, path, body=None, timeout=5.0):
    url = "http://%s%s" % (host, path)
    resp = requests.post(url, json=body or {}, timeout=timeout)
    return resp


def get_status(host, timeout=5.0):
    resp = requests.get("http://%s/api/status" % host, timeout=timeout)
    resp.raise_for_status()
    return resp.json()


def wait_for(predicate, timeout_s, poll_s, description, host):
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        status = get_status(host)
        if predicate(status):
            return status
        time.sleep(poll_s)
    raise TimeoutError("timed out waiting for: %s" % description)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", required=True)
    parser.add_argument("--sog-kn", type=float, default=5.0)
    parser.add_argument("--cog-deg", type=float, default=90.0)
    parser.add_argument("--variation-deg", type=float, default=-8.0)
    parser.add_argument("--level-timeout", type=float, default=15.0)
    parser.add_argument("--gps-timeout", type=float, default=30.0)
    args = parser.parse_args()

    print("Starting Stage B...")
    r = post(args.host, "/api/calibration/b/start")
    if not r.ok:
        print("FAIL: could not start Stage B:", r.text, file=sys.stderr)
        sys.exit(1)

    print("Requesting level capture (device must be still)...")
    r = post(args.host, "/api/calibration/b/level")
    if not r.ok:
        print("FAIL: could not start level capture:", r.text, file=sys.stderr)
        sys.exit(1)

    try:
        wait_for(lambda s: s["session"]["progress"].get("level_set"), args.level_timeout, 0.5,
                 "level_set", args.host)
    except TimeoutError as e:
        print("FAIL:", e, file=sys.stderr)
        sys.exit(1)
    print("Level set.")

    print("Choosing GPS course method...")
    r = post(args.host, "/api/calibration/b/gps-course")
    if not r.ok:
        print("FAIL: could not choose GPS course:", r.text, file=sys.stderr)
        sys.exit(1)

    print("Injecting synthetic GPS (sog=%.1f kn, cog=%.1f deg, variation=%.1f deg)..." %
          (args.sog_kn, args.cog_deg, args.variation_deg))

    deadline = time.monotonic() + args.gps_timeout
    preview_deg = None
    while time.monotonic() < deadline:
        inj = post(args.host, "/api/debug/gps-inject",
                   {"sog_kn": args.sog_kn, "cog_deg": args.cog_deg, "variation_deg": args.variation_deg})
        if not inj.ok:
            print("FAIL: gps-inject endpoint unavailable -- was the device built with "
                  "-D DEBUG_GPS_INJECT=1 (the esp32s3_bench environment)?", file=sys.stderr)
            sys.exit(1)

        status = get_status(args.host)
        progress = status["session"]["progress"]
        if progress.get("preview_offset_deg") is not None:
            preview_deg = progress["preview_offset_deg"]
            break
        time.sleep(0.5)

    if preview_deg is None:
        print("FAIL: never reached a computed offset within %.0fs" % args.gps_timeout, file=sys.stderr)
        sys.exit(1)
    print("Computed preview offset: %.2f deg" % preview_deg)

    print("Applying...")
    r = post(args.host, "/api/calibration/b/apply")
    if not r.ok:
        print("FAIL: apply failed:", r.text, file=sys.stderr)
        sys.exit(1)

    status = get_status(args.host)
    stage_b = status["stages"]["b"]
    if stage_b.get("state") != "DONE":
        print("FAIL: stages.b.state != DONE after apply:", stage_b, file=sys.stderr)
        sys.exit(1)
    if stage_b.get("method") != "GPS_COURSE":
        print("FAIL: stages.b.method != GPS_COURSE:", stage_b, file=sys.stderr)
        sys.exit(1)

    print()
    print("PASS: Stage B saved via GPS course, offset_deg=%.2f, saved_at=%s" %
          (stage_b.get("offset_deg", float("nan")), stage_b.get("saved_at")))
    sys.exit(0)


if __name__ == "__main__":
    main()
