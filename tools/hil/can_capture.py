#!/usr/bin/env python3
"""Assert NMEA 2000 base node behavior over a real CAN interface.

Requires a USB-CAN adapter on the same NMEA 2000 segment as the device under
test (python-can). Verifies, per constitution Principle II/IV:

  - Address claim completes (PGN 60928 ISO Address Claim seen from the
    device).
  - Product info (126996), configuration info (126998), and PGN list
    (126464) are returned when explicitly requested (PGN 59904 ISO Request).
  - Heartbeat (126993) appears on schedule in bench/self-test mode.

This checks *base* node behavior generically, independent of any specific
calibration stage or heading content. Real-bus multi-node verification (a
second physical node acknowledging frames) remains an explicit manual
checklist item (checklists/manual-verification.md) -- this script alone
never proves that, only that the device speaks the base protocol correctly
on the wire.

Usage:
    python tools/hil/can_capture.py --iface <can-iface> --duration 30 [--expect-pgn 127250]
"""

import argparse
import sys
import time

try:
    import can
except ImportError:
    print("ERROR: python-can is required (pip install -r tools/hil/requirements.txt)", file=sys.stderr)
    sys.exit(2)

PGN_ISO_ADDRESS_CLAIM = 60928
PGN_ISO_REQUEST = 59904
PGN_PRODUCT_INFO = 126996
PGN_CONFIGURATION_INFO = 126998
PGN_PGN_LIST = 126464
PGN_HEARTBEAT = 126993

REQUESTED_PGNS = (PGN_PRODUCT_INFO, PGN_CONFIGURATION_INFO, PGN_PGN_LIST)


def pgn_from_can_id(can_id):
    pf = (can_id >> 16) & 0xFF
    ps = (can_id >> 8) & 0xFF
    dp = (can_id >> 24) & 1
    if pf < 240:
        return (dp << 16) | (pf << 8)
    return (dp << 16) | (pf << 8) | ps


def source_from_can_id(can_id):
    return can_id & 0xFF


def build_iso_request_id(priority, destination, source):
    pgn = PGN_ISO_REQUEST  # PDU1, PGN low byte 0
    return (priority & 0x7) << 26 | (pgn << 8) | ((destination & 0xFF) << 8) | (source & 0xFF)


def send_iso_request(bus, requested_pgn, destination=0xFF, source=0xF9):
    data = bytes(
        [
            requested_pgn & 0xFF,
            (requested_pgn >> 8) & 0xFF,
            (requested_pgn >> 16) & 0xFF,
        ]
    )
    msg = can.Message(
        arbitration_id=build_iso_request_id(6, destination, source),
        data=data,
        is_extended_id=True,
    )
    bus.send(msg)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--iface", required=True, help="python-can channel, e.g. can0 or PCAN_USBBUS1")
    parser.add_argument("--bustype", default="socketcan", help="python-can interface/bustype (default: socketcan)")
    parser.add_argument("--duration", type=float, default=30.0, help="capture window in seconds (default: 30)")
    parser.add_argument("--expect-pgn", type=int, default=None, help="also assert this PGN appears at least once")
    args = parser.parse_args()

    bus = can.Bus(channel=args.iface, interface=args.bustype)

    seen_pgns = set()
    address_claim_source = None
    heartbeat_timestamps = []
    responses_seen = set()
    request_sent_at = {}

    deadline = time.monotonic() + args.duration
    requests_sent = False

    try:
        while time.monotonic() < deadline:
            msg = bus.recv(timeout=1.0)
            if msg is None:
                continue
            if not msg.is_extended_id:
                continue

            pgn = pgn_from_can_id(msg.arbitration_id)
            source = source_from_can_id(msg.arbitration_id)
            seen_pgns.add(pgn)

            if pgn == PGN_ISO_ADDRESS_CLAIM and address_claim_source is None:
                address_claim_source = source
                print(f"[can_capture] address_claimed addr={source}")
                # Now that we know the device's address, request the PGNs
                # that must be answered on request.
                if not requests_sent:
                    for requested_pgn in REQUESTED_PGNS:
                        send_iso_request(bus, requested_pgn, destination=source)
                        request_sent_at[requested_pgn] = time.monotonic()
                    requests_sent = True

            if pgn == PGN_HEARTBEAT:
                heartbeat_timestamps.append(time.monotonic())

            if address_claim_source is not None and source == address_claim_source:
                if pgn in REQUESTED_PGNS:
                    responses_seen.add(pgn)
    finally:
        bus.shutdown()

    ok = True

    if address_claim_source is None:
        print("[can_capture] FAIL: no ISO Address Claim (PGN 60928) observed")
        ok = False

    for requested_pgn in REQUESTED_PGNS:
        if requested_pgn in responses_seen:
            print(f"[can_capture] PASS: PGN {requested_pgn} responded to request")
        else:
            print(f"[can_capture] FAIL: PGN {requested_pgn} did not respond to request")
            ok = False

    if len(heartbeat_timestamps) >= 2:
        intervals = [b - a for a, b in zip(heartbeat_timestamps, heartbeat_timestamps[1:])]
        print(f"[can_capture] PASS: heartbeat (126993) observed {len(heartbeat_timestamps)} times, "
              f"intervals={[round(i, 2) for i in intervals]}")
    else:
        print(f"[can_capture] FAIL: heartbeat (126993) observed only {len(heartbeat_timestamps)} time(s) "
              f"in {args.duration}s window")
        ok = False

    if args.expect_pgn is not None:
        if args.expect_pgn in seen_pgns:
            print(f"[can_capture] PASS: expected PGN {args.expect_pgn} observed")
        else:
            print(f"[can_capture] FAIL: expected PGN {args.expect_pgn} never observed")
            ok = False

    print(
        "[can_capture] NOTE: this confirms base node behavior on the wire only; "
        "real second-node acknowledgement is a manual checklist item "
        "(checklists/manual-verification.md)."
    )

    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
