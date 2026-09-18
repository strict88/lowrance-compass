<!--
Sync Impact Report
Version change: 1.0.0 → 2.0.0
Rationale: MAJOR — redefinition of a NON-NEGOTIABLE principle's guarantee. Principle I
previously required the device to withhold the heading PGN (or send "data not available")
whenever sensor accuracy fell below HEADING_MIN_ACCURACY. Product direction changed: a
flagged, best-effort heading is now considered more useful to the person steering than
silence while calibration converges, so low accuracy no longer suppresses transmission or
display — it is instead required to be flagged everywhere it's surfaced. The disconnected-
sensor guarantee (no data at all → no heading sent/shown) is unchanged and remains absolute.
Modified principles:
  - I. Navigation Data Integrity (NON-NEGOTIABLE) — scope of "MUST withhold" narrowed from
    "disconnected OR low accuracy" to "disconnected only"; added a MUST-flag requirement
    for any heading sent/shown below HEADING_MIN_ACCURACY.
Added sections: none
Removed sections: none
Deferred / TODO placeholders: none.
Templates requiring follow-up: specs/001-calibration-guided-setup/spec.md FR-041 and
related contract/data-model docs describe the old suppress-on-low-accuracy behavior and
should be updated to match (tracked separately from this constitution amendment).
This report is scratch material for human review; remove it once the amendment is
reviewed and before relying on the file as the durable governance record.
-->

# Lowrance Compass Constitution

## Core Principles

### I. Navigation Data Integrity (NON-NEGOTIABLE)
The device MUST never send stale, frozen, or fabricated heading data, and MUST NOT repeat
the last known value. If the sensor is disconnected or stops reporting entirely, the device
MUST stop sending heading PGNs or MUST send the NMEA 2000 "data not available" values, and
the UI MUST show heading as unavailable — there is no data to give.

When the sensor is connected and reporting but accuracy is below HEADING_MIN_ACCURACY
(magnetometer, accelerometer, and gyroscope all "High"), the device MUST still compute and
transmit its best-available heading rather than withholding it — but that heading MUST be
flagged as low-confidence everywhere it is surfaced: the UI MUST show an explicit
low-accuracy/not-yet-calibrated warning alongside the numeric value, and diagnostic/serial
log lines MUST record the accuracy reason (e.g. `SENSOR_ACCURACY_LOW`,
`SENSOR_NOT_CALIBRATED`). A low-accuracy heading MUST NEVER be sent or shown unflagged.
Every value placed on the bus MUST use correct NMEA 2000 units (radians, rad/s), correct
ranges (heading normalized to [0, 2π)), and the correct reference (magnetic vs. true). The
heading pipeline (sensor quaternion → Euler → mounting offset → deviation/variation →
normalized heading) MUST be deterministic, documented, and covered by unit tests with known
vectors, including the 0°/360° wraparound.

Rationale: A marine compass feeding a chartplotter is a safety-relevant navigation
instrument. Silently repeating or fabricating heading data, or presenting a low-confidence
value as if it were fully trustworthy, is worse than reporting no data, because either can
mislead a navigator who trusts the display. Total silence during normal low-accuracy
operation (e.g. while calibration is still converging), however, is not obviously safer
than a clearly labeled best-effort estimate — so the device gives its best number and lets
the flag do the warning, rather than giving nothing.

### II. NMEA 2000 Compliance
The device MUST behave as a well-mannered NMEA 2000 node: address claim (PGN 60928),
product and configuration information (126996/126998), ISO request handling (59904), PGN
list (126464), and heartbeat (126993). Primary output is Vessel Heading (PGN 127250).
Secondary outputs are Rate of Turn (127251) and Attitude (127257). Transmit rates MUST
follow NMEA 2000 defaults unless a spec documents and justifies otherwise, and rates MUST
be configurable within safe limits. The device MUST never flood or disrupt the bus:
bus-off, error-passive, and no-ACK conditions MUST be detected, logged, and recovered from
automatically. The project MUST use an established NMEA 2000 library (e.g. the
ttlappalainen NMEA2000 stack) paired with an ESP32-S3 TWAI-compatible driver, and that
driver's S3 compatibility MUST be verified before adoption.

Rationale: The device shares a physical bus with other critical marine electronics.
Protocol misbehavior or a misbehaving node can degrade or disrupt instruments beyond the
compass itself.

### III. Real-Time Isolation
Sensor reading and NMEA 2000 transmission are time-critical and MUST never be blocked by
Wi-Fi, the web server, file-system access, or logging. The firmware MUST use FreeRTOS
tasks with explicit core affinity and priorities, and MUST hand off data between tasks
through thread-safe mechanisms (queues, mutexes, or atomic snapshots) — never through
unprotected globals. Features MUST degrade independently: if Wi-Fi fails, compass output
MUST continue; if CAN fails, the web UI MUST still work and MUST show the fault. A task
watchdog MUST be enabled. Time-critical paths MUST NOT use `delay()`-based busy logic.

Rationale: The compass's core job — reading the IMU and publishing heading to the bus —
must keep working even when a secondary subsystem (Wi-Fi, web UI, logging) misbehaves or
is under load.

### IV. Hardware-in-the-Loop Verification (NON-NEGOTIABLE)
A task is not done until it builds cleanly, passes tests, is flashed to the connected
device, and its behavior is confirmed from real serial output. Testing MUST proceed in
layers:
1. `pio test -e native` for pure logic (math, conversions, PGN field encoding, config
   parsing) with no hardware dependencies.
2. `pio test -e <device-env>` for on-target Unity tests (sensor communication, TWAI
   driver start/stop, file system).
3. Runtime validation: flash the firmware, capture serial output with a bounded timeout,
   and check for expected structured log lines.

Firmware MUST emit machine-parseable diagnostic lines over USB serial (e.g.
`[N2K] tx pgn=127250 ok`, `[IMU] hdg=123.4 acc=3`, `[SYS] heap=... uptime=...`) so the
agent can assert on them. The agent MUST NOT start a serial monitor that blocks forever —
every monitor or read MUST use a timeout, and the serial port MUST be released before any
upload. If CAN frames cannot be acknowledged on the bench (no other node present), the
agent MUST use TWAI no-ACK or self-test mode for bench validation and MUST state clearly
that real-bus verification is still pending. Anything the agent cannot observe (what the
chartplotter displays, physical heading accuracy) MUST become an explicit manual
verification checklist item for the human, and MUST NOT be assumed to pass.

Rationale: This is embedded firmware for a physical instrument with real hardware
attached to the development machine; claims of "done" that are not backed by an actual
build, flash, and observed serial output are unverifiable and unacceptable.

### V. Hardware Abstraction and Configuration
All pin assignments MUST live in one configuration header. Pins MUST avoid ESP32-S3 GPIOs
reserved on the N16R8 module (26–37 for flash/octal PSRAM), native USB (19/20), and
strapping pins (0, 3, 45, 46) unless explicitly justified in that configuration header.
Drivers for the IMU and CAN MUST sit behind thin interfaces so that core logic compiles
and is testable in the native environment. User settings (mounting offset,
deviation/variation source, PGN rates, NMEA 2000 device instance, Wi-Fi credentials,
sensor calibration data) MUST be persisted in NVS or LittleFS, MUST be validated on load,
and MUST fall back to safe defaults when corrupted.

Rationale: Centralized pin configuration and thin driver interfaces keep hardware-specific
detail out of core logic, which is what makes native unit testing (Principle IV) possible
in the first place.

### VI. Web UI: Simple, Local, Robust
The UI MUST work fully offline on a boat with no internet: no CDN dependencies, all
assets served from flash. Access Point mode is the default, with optional Station mode for
bench development and updates. Live data MUST be pushed via WebSocket or Server-Sent
Events at a rate that cannot starve real-time tasks. The UI MUST provide: live heading,
pitch, roll, and rate of turn; sensor calibration status and controls; NMEA 2000 bus
status (address, error counters, TX counts); settings; and firmware version and OTA
update. The UI MUST prioritize clarity and readability in sunlight over visual flourish:
large numerals, high contrast, mobile-first layout, and it MUST stay lightweight and
dependency-free. A JSON status endpoint (e.g. `/api/status`) MUST mirror the UI data so it
can be tested programmatically.

Rationale: The UI runs disconnected from the internet, is read on a bright cockpit or deck
in direct sunlight, and must be verifiable by the agent through a plain HTTP endpoint
rather than only through visual inspection.

### VII. Reliability and Field Safety
OTA updates MUST use a dual-partition scheme with rollback on failed boot. Brownouts,
resets, and reset reasons MUST be logged and exposed in the UI. The device MUST boot to a
working compass state without any user interaction. Documentation MUST note the electrical
rules for a safe NMEA 2000 installation: power from the backbone through a proper
regulator, a declared Load Equivalency Number, and no extra 120 Ω termination on a drop
cable (remove it from the VP230 module if present).

Rationale: This device is installed on a boat, often unattended, and a bad update or a
brownout must not leave the vessel without a working compass or degrade the shared NMEA
2000 backbone.

## Technical Constraints

- Build system: PlatformIO. The platform, framework, and library versions MUST be pinned
  in `platformio.ini` — no floating versions.
- Framework: Arduino on ESP-IDF, unless a plan documents a justified reason to use pure
  ESP-IDF.
- Board configuration MUST correctly declare 16 MB flash, octal PSRAM (`qio_opi`), and an
  OTA-capable partition table.
- Required environments: a device environment for the ESP32-S3 and a `native` test
  environment.
- Language: modern C++ (C++17 or newer). No dynamic allocation in time-critical loops.
  Use PSRAM for large buffers where it helps.
- Builds MUST have zero compiler warnings in project code.

## Development Workflow

- Spec-driven: spec → plan → tasks → implement. Specs describe observable behavior,
  including what appears on the bus, in serial output, and in the UI.
- Every task MUST define how it will be verified: native test, on-device test, serial
  assertion, or manual checklist item.
- Implementation loop per task: build → native tests → upload → on-device tests → serial
  validation → report results with the actual captured output.
- Commits MUST be small and focused. Each commit MUST build.
- A hardware behavior MUST NOT be reported as verified unless it was actually observed.

## Governance

This constitution overrides conflicting guidance in specs, plans, or tasks. Plans MUST
include a constitution check that lists any deviations along with their justification.

Amendments require a documented rationale and a semantic version bump:
- MAJOR: removing or redefining a principle.
- MINOR: adding a principle or materially expanding guidance.
- PATCH: clarifications and non-semantic refinements.

The ratification date and the last-amended date MUST both be recorded and kept current.

Principles I (Navigation Data Integrity) and IV (Hardware-in-the-Loop Verification) are
non-negotiable and MUST NOT be waived by a plan.

**Version**: 2.0.0 | **Ratified**: 2026-09-16 | **Last Amended**: 2026-09-18
