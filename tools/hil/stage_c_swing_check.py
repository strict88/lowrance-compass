#!/usr/bin/env python3
"""Drive Stage C's GPS-swing method end-to-end on the bench against a known
synthetic deviation curve, using the same synthetic GPS injection as
stage_b_check.py (quickstart.md section 6), and assert the resulting
DeviationCorrection is close to the injected known curve.

Unlike Stage B's level/bearing inputs, Stage C's swing needs the compass to
actually be rotated through >=2 full slow turns -- there is no way to inject
a fake heading, only a fake GPS course/speed. This script therefore injects
a COG that is continuously derived from the device's own *live, currently
uncorrected* heading (GET /api/status.heading.heading_deg, which equals the
pre-deviation "compass heading" as long as no DeviationCorrection is saved
yet) plus a known deviation curve and variation, so the injected reference
is exactly the "true" bearing a perfect fit should recover -- while an
operator (or a turntable) physically/manually rotates the device through the
guide's two slow circles covering every heading.

Requires the device built with -D DEBUG_GPS_INJECT=1 (the `esp32s3_bench`
PlatformIO environment) in bench Station mode, and Stage A already done (so
sensor accuracy is High and heading is valid) with no prior Stage C result
saved (a fresh device, or after POST /api/calibration/c/reset).

Usage:
    python tools/hil/stage_c_swing_check.py --host <device-ip> \\
        [--coeff-a 0.5 --coeff-b 1.0 --coeff-c -0.5 --coeff-d 0.3 --coeff-e -0.2] \\
        [--variation-deg -8.0] [--sog-kn 5.0] [--timeout 600] \\
        [--max-residual-deg 2.0]
"""

import argparse
import math
import sys
import time

try:
    import requests
except ImportError:
    print("ERROR: requests is required (pip install -r tools/hil/requirements.txt)", file=sys.stderr)
    sys.exit(2)


def post(host, path, body=None, timeout=5.0):
    return requests.post("http://%s%s" % (host, path), json=body or {}, timeout=timeout)


def get_status(host, timeout=5.0):
    resp = requests.get("http://%s/api/status" % host, timeout=timeout)
    resp.raise_for_status()
    return resp.json()


def known_deviation_deg(heading_deg, a, b, c, d, e):
    theta = math.radians(heading_deg)
    return a + b * math.sin(theta) + c * math.cos(theta) + d * math.sin(2 * theta) + e * math.cos(2 * theta)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", required=True)
    parser.add_argument("--coeff-a", type=float, default=0.5)
    parser.add_argument("--coeff-b", type=float, default=1.0)
    parser.add_argument("--coeff-c", type=float, default=-0.5)
    parser.add_argument("--coeff-d", type=float, default=0.3)
    parser.add_argument("--coeff-e", type=float, default=-0.2)
    parser.add_argument("--variation-deg", type=float, default=-8.0)
    parser.add_argument("--sog-kn", type=float, default=5.0)
    parser.add_argument("--inject-interval", type=float, default=0.3)
    parser.add_argument("--timeout", type=float, default=600.0,
                         help="Max seconds to wait for full coverage (operator must complete >=2 slow circles).")
    parser.add_argument("--max-residual-deg", type=float, default=2.0,
                         help="Fit residual RMS must be below this to pass (SC-009-style tolerance).")
    args = parser.parse_args()

    status = get_status(args.host)
    if status["stages"]["c"]["state"] == "DONE":
        print("FAIL: Stage C already has a saved result -- POST /api/calibration/c/reset first so this "
              "script's injected reference isn't measured against an already-corrected heading.",
              file=sys.stderr)
        sys.exit(1)

    print("Starting Stage C GPS swing...")
    r = post(args.host, "/api/calibration/c/start", {"mode": "gps"})
    if not r.ok:
        print("FAIL: could not start Stage C:", r.text, file=sys.stderr)
        sys.exit(1)

    print("Injecting synthetic GPS derived from the known deviation curve "
          "(A=%.2f B=%.2f C=%.2f D=%.2f E=%.2f, variation=%.1f deg)." %
          (args.coeff_a, args.coeff_b, args.coeff_c, args.coeff_d, args.coeff_e, args.variation_deg))
    print("Now physically rotate the device through >=2 full, slow, steady circles "
          "covering every heading, per the Stage C guide.")

    deadline = time.monotonic() + args.timeout
    candidate = None
    while time.monotonic() < deadline:
        status = get_status(args.host)
        session = status.get("session", {})
        if session.get("active_stage") != "C":
            print("FAIL: Stage C session ended unexpectedly (rejected/cancelled) before a result was ready:",
                  status.get("stages", {}).get("c"), file=sys.stderr)
            sys.exit(1)

        heading_deg = status["heading"]["heading_deg"]
        deviation = known_deviation_deg(heading_deg, args.coeff_a, args.coeff_b, args.coeff_c, args.coeff_d,
                                         args.coeff_e)
        magnetic_bearing_deg = (heading_deg + deviation) % 360.0
        cog_deg = (magnetic_bearing_deg + args.variation_deg) % 360.0

        inj = post(args.host, "/api/debug/gps-inject",
                   {"sog_kn": args.sog_kn, "cog_deg": cog_deg, "variation_deg": args.variation_deg})
        if not inj.ok:
            print("FAIL: gps-inject endpoint unavailable -- was the device built with "
                  "-D DEBUG_GPS_INJECT=1 (the esp32s3_bench environment)?", file=sys.stderr)
            sys.exit(1)

        progress = session.get("progress", {})
        if progress.get("candidate_max_deviation_deg") is not None:
            candidate = progress
            break

        time.sleep(args.inject_interval)

    if candidate is None:
        print("FAIL: never reached a computed result within %.0fs" % args.timeout, file=sys.stderr)
        sys.exit(1)

    residual_rms_deg = candidate["candidate_residual_rms_deg"]
    print("Fit ready: max_deviation_deg=%.2f residual_rms_deg=%.2f" %
          (candidate["candidate_max_deviation_deg"], residual_rms_deg))
    if residual_rms_deg > args.max_residual_deg:
        print("FAIL: residual_rms_deg %.2f exceeds tolerance %.2f -- fit did not match the injected curve well" %
              (residual_rms_deg, args.max_residual_deg), file=sys.stderr)
        sys.exit(1)

    print("Applying...")
    r = post(args.host, "/api/calibration/c/apply")
    if not r.ok:
        print("FAIL: apply failed:", r.text, file=sys.stderr)
        sys.exit(1)

    status = get_status(args.host)
    stage_c = status["stages"]["c"]
    if stage_c.get("state") != "DONE":
        print("FAIL: stages.c.state != DONE after apply:", stage_c, file=sys.stderr)
        sys.exit(1)

    print()
    print("PASS: Stage C saved via GPS swing, max_deviation_deg=%.2f, residual_rms_deg=%.2f, saved_at=%s" %
          (stage_c.get("max_deviation_deg", float("nan")), stage_c.get("residual_rms_deg", float("nan")),
           stage_c.get("saved_at")))
    sys.exit(0)


if __name__ == "__main__":
    main()
