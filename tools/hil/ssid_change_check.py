#!/usr/bin/env python3
"""Change the SSID over HTTP (bench Station mode) and assert:
  - it persists across a reboot, and
  - there is no gap in [N2K] tx pgn=127250 ok lines spanning the SSID
    change / AP-restart window (SC-008) -- heading transmission must never
    be interrupted by a settings change, since N2kService/ImuTask run
    independently of the web/AP stack (constitution Principle III).

This is T106's on-target verification too: the full [WIFI] ssid_changed ->
[WIFI] ap_restart -> uninterrupted [N2K] tx chain spans the whole running
firmware (AppTask + N2kTask together), which a standalone on-target Unity
test can't observe (it runs its own setup()/loop(), not main.cpp's task
graph) -- so it's asserted here, over serial, against the real firmware
instead, per tasks.md's own fallback note for T106.

Usage:
    python tools/hil/ssid_change_check.py --host <device-ip> --port <SERIAL_PORT> --new-ssid "Bench-Test-SSID"
"""

import argparse
import sys
import time

try:
    import requests
    import serial
except ImportError:
    print("ERROR: requests and pyserial are required (pip install -r tools/hil/requirements.txt)", file=sys.stderr)
    sys.exit(2)


def parse_line(line):
    line = line.strip()
    if not line.startswith("["):
        return None, [], {}
    try:
        tag_end = line.index("]")
    except ValueError:
        return None, [], {}
    tag = line[1:tag_end]
    tokens = line[tag_end + 1:].strip().split()
    kv = {}
    for token in tokens:
        if "=" in token:
            k, v = token.split("=", 1)
            kv[k] = v
    return tag, tokens, kv


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", required=True, help="device IP (bench Station mode)")
    parser.add_argument("--port", required=True, help="serial port for the [N2K]/[WIFI] log lines")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--new-ssid", required=True)
    parser.add_argument("--watch-window", type=float, default=20.0,
                         help="seconds to watch for a tx gap around the change (default: 20)")
    parser.add_argument("--max-gap-s", type=float, default=1.0,
                         help="largest acceptable gap between tx ok lines, seconds (default: 1.0 -- "
                         "generously above the 100ms nominal PGN 127250 interval)")
    args = parser.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=1)

    tx_ok_times = []
    saw_ssid_changed = False
    saw_ap_restart = False

    print("Changing SSID to %r over HTTP..." % args.new_ssid)
    r = requests.post("http://%s/api/settings" % args.host, json={"ssid": args.new_ssid}, timeout=5)
    if not r.ok:
        print("FAIL: POST /api/settings failed:", r.text, file=sys.stderr)
        sys.exit(1)
    applies_in_s = r.json().get("applies_in_s", 5)
    print("applies_in_s =", applies_in_s)

    deadline = time.monotonic() + args.watch_window
    try:
        while time.monotonic() < deadline:
            raw = ser.readline()
            if not raw:
                continue
            try:
                line = raw.decode("utf-8", errors="replace")
            except Exception:
                continue

            tag, tokens, kv = parse_line(line)
            if tag == "WIFI" and "ssid_changed" in tokens:
                saw_ssid_changed = True
                print("  ", line.strip())
            elif tag == "WIFI" and "ap_restart" in tokens:
                saw_ap_restart = True
                print("  ", line.strip())
            elif tag == "N2K" and "tx" in tokens and kv.get("pgn") == "127250" and "ok" in tokens:
                tx_ok_times.append(time.monotonic())
    finally:
        ser.close()

    ok = True

    if not saw_ssid_changed:
        print("FAIL: never observed [WIFI] ssid_changed", file=sys.stderr)
        ok = False
    if not saw_ap_restart:
        print("FAIL: never observed [WIFI] ap_restart", file=sys.stderr)
        ok = False

    if len(tx_ok_times) < 2:
        print("FAIL: too few [N2K] tx pgn=127250 ok lines observed to check for a gap "
              "(is the device calibrated and transmitting?)", file=sys.stderr)
        ok = False
    else:
        max_gap = max(b - a for a, b in zip(tx_ok_times, tx_ok_times[1:]))
        print("Observed %d tx ok lines, max gap between them: %.3fs" % (len(tx_ok_times), max_gap))
        if max_gap > args.max_gap_s:
            print("FAIL: gap of %.3fs exceeds the %.3fs bound -- heading transmission was interrupted "
                  "by the SSID change" % (max_gap, args.max_gap_s), file=sys.stderr)
            ok = False

    if not ok:
        sys.exit(1)

    print()
    print("SSID change applied with no interruption to heading transmission. "
          "Reboot the device now and confirm the new SSID persists (manual check).")
    sys.exit(0)


if __name__ == "__main__":
    main()
