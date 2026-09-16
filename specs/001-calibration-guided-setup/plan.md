# Implementation Plan: Calibration, Guided Setup & Settings

**Branch**: `001-calibration-guided-setup` | **Date**: 2026-09-16 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/001-calibration-guided-setup/spec.md`

## Summary

A three-stage guided calibration journey (bench sensor calibration → installation alignment →
compass swing) exposed through a phone-first Calibration tab with built-in offline guides, plus a
Settings tab for the Wi-Fi SSID. The device only ever transmits heading once the underlying
sensor calibration is trustworthy, and every calibration stage, threshold, and record is designed
to be independently verifiable from native unit tests, on-target tests, and structured serial
output — never asserted "verified" from UI inspection alone. Technical approach: pure-logic
calibration/heading math lives in host-testable modules (`heading/`, `calibration/`, `n2k_codec/`,
`settings/`, `guide/`) behind thin hardware adapters (`ImuDriver`, `CanBus`, `KeyValueStore`,
`Clock`), driven by FreeRTOS tasks split across cores per the constitution's Real-Time Isolation
principle, with calibration/settings state served to the UI over a small REST+WebSocket API.

## Technical Context

**Language/Version**: C++17, Arduino framework on ESP-IDF (exact core/IDF version pinned per
`research.md` §1).

**Primary Dependencies**: `pioarduino/platform-espressif32` (Arduino core 3.3.11 / ESP-IDF 5.5.5);
`adafruit/Adafruit BNO08x @ 1.2.7`; `ttlappalainen/NMEA2000 @ 5b7b9fc` + a project-owned `tNMEA2000`
subclass over ESP-IDF 5.5's node-based TWAI API (`esp_twai.h`) — no maintained S3-correct bridge
exists, see `research.md` §3; `ESP32Async/ESPAsyncWebServer @ 3.12.1` + `ESP32Async/AsyncTCP @
3.5.0`; `bblanchon/ArduinoJson @ 7.4.3`; PlatformIO's bundled Unity (no separate pin). Every version
pinned exactly in `platformio.ini` per the constitution — no floating versions; full rationale and
rejected alternatives in `research.md`.

**Storage**: On-device NVS (`Preferences`) for `SensorCalibrationProfile` metadata,
`InstallationAlignment`, `DeviationCorrection`, and `NetworkSettings` records (`data-model.md` §1);
BNO08x on-chip flash for the sensor's own Dynamic Calibration Data via the SH-2 save-DCD command;
LittleFS for the web UI's static assets and the guide content file.

**Testing**: `pio test -e native` (Unity, host-side, pure logic), `pio test -e esp32s3_test`
(Unity, on-target), `tools/hil/*.py` serial HIL scripts (pyserial), `tools/hil/*_check.py` HTTP
checks (requests) in bench Station mode, optional `tools/hil/can_capture.py` (python-can) — see
`quickstart.md`.

**Target Platform**: ESP32-S3-N16R8 (16 MB flash, 8 MB octal PSRAM, native USB), BNO08x IMU over
I2C, VP230 CAN transceiver on the ESP32-S3 TWAI controller, NMEA 2000 backbone at 250 kbit/s, phone
browser (Wi-Fi AP) as the UI client.

**Project Type**: Single embedded-firmware project (PlatformIO), with a `data/` LittleFS image for
the web UI — no separate frontend/backend split (the "frontend" is static assets served by the
same firmware binary).

**Performance Goals**: WebSocket/status broadcast bounded to `WS_STATUS_RATE_HZ_IDLE` /
`WS_STATUS_RATE_HZ_ACTIVE_CAL` (`data-model.md` §1.5) so it never starves `ImuTask`/`N2kTask`;
heading pipeline runs once per IMU report with no dynamic allocation (constitution Technical
Constraints); SSID change completes with no gap in `127250` transmission (SC-008).

**Constraints**: Offline-only UI (no CDN, all assets from LittleFS); zero compiler warnings; no
`delay()`-based busy logic in `ImuTask`/`N2kTask`; heading MUST NOT transmit below
`HEADING_MIN_ACCURACY` (FR-041); single active calibration session across all clients (FR-006).

**Scale/Scope**: Single device, single boat installation; UI serves at most a handful of
concurrent phone/browser clients (owner + installer), not a multi-tenant service.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | How this plan satisfies it |
|---|---|---|
| I. Navigation Data Integrity (NON-NEGOTIABLE) | PASS | `data-model.md` §6 fixes the pipeline order (sensor cal → level/offset → deviation → normalize) exactly as the constitution requires; §2.2's `HeadingReading.valid` + `HEADING_MIN_ACCURACY` gate (FR-041) means the device never repeats stale data — it withholds or sends "not available" instead; native tests cover wraparound and pipeline ordering with known vectors (SC-009 covers the hardest case, Stage C's fit). |
| II. NMEA 2000 Compliance | PASS | This feature implements both the base N2K node behavior (address claim 60928, product/config info 126996/126998, ISO request 59904, PGN list 126464, heartbeat 126993 — configured via `SetProductInformation`/`SetDeviceInformation` and `NMEA2000.Open()`/`ParseMessages()` as part of the `tNMEA2000` node setup, `tasks.md` Foundational phase) and the PGNs this feature adds/consumes on top of it (127250 tx with deviation/variation fields, 127251, 127257 tx; 129026, 127258, optionally 129029 rx) — there is no pre-existing N2K node to build on (`src/` is currently a stub). Base node behavior is verified via `tools/hil/can_capture.py` and on-target serial lines, not assumed; real-bus multi-node verification is an explicit manual checklist item. TWAI/NMEA2000 library S3 compatibility is an explicit research item (`research.md` §3) before adoption, and bench mode (no-ACK/self-test) is a first-class, clearly-flagged status field (`n2k.bus_state`), not a hidden mode. |
| III. Real-Time Isolation | PASS | `ImuTask`/`N2kTask` on Core 1 (high priority) never block on Wi-Fi/web/logging; `AppTask` (calibration state machines, settings, WebSocket) on Core 0; cross-task data only via mutex-protected snapshots/queues (`HeadingReading`, `CalibrationSession`); WebSocket broadcast rate is explicitly bounded (`WS_STATUS_RATE_HZ_*`); task watchdog enabled on every task; no `delay()`-based busy loops in Stage A/B/C detection logic (event/threshold-driven state machines, `data-model.md` §3). |
| IV. Hardware-in-the-Loop Verification (NON-NEGOTIABLE) | PASS | Three-layer testing plan (`quickstart.md` §1-4) plus HTTP/CAN checks; every calibration/heading/N2K event has a structured serial line (`contracts/serial-log.md`); HIL scripts always use a bounded timeout and release the port before upload; bench mode is explicit when no second bus node exists; physical accuracy, chartplotter display, and on-water Stage B/C are explicit manual checklist items (SC-010), never assumed. |
| V. Hardware Abstraction and Configuration | PASS | All pin assignments captured in one place (`quickstart.md` Wiring table → the actual pin configuration header in Phase 0 of `tasks.md`); `ImuDriver`/`CanBus`/`KeyValueStore`/`Clock` are the only hardware-touching interfaces, with fakes for `native` tests; every persisted record is versioned + CRC32'd with defined fallback-to-default behavior on corruption (`data-model.md` §5, FR-045/046). See **Complexity Tracking** below for the one pin-reservation deviation (BOOT button reuse). |
| VI. Web UI: Simple, Local, Robust | PASS | Vanilla HTML/CSS/JS, inline SVG illustrations, no CDN, assets from LittleFS (`research.md` §9); AP mode default; WebSocket push at a bounded rate; `GET /api/status` mirrors every UI-visible field 1:1 (`contracts/rest-api.md`); large-numeral/high-contrast/mobile-first layout requirement carried into `tasks.md` UI tasks (not further specified here, as visual design is an implementation/task-level concern). |
| VII. Reliability and Field Safety | PASS (partially inherited) | Dual-partition OTA with rollback is an existing platform-level requirement this feature's partition table (`research.md` §8) must not regress, not something this feature newly implements; reset-reason logging already appears in `contracts/serial-log.md` (`[SYS] boot ... reset_reason=...`) and `GET /api/status.system.reset_reason`; device boots to a working (possibly "not calibrated") compass state with no user interaction, per `data-model.md` §5's boot-time validation; NMEA 2000 installation electrical notes are documentation, tracked as a `tasks.md` doc task, not a design decision here. |

No principle is waived. One deviation from the strict reading of a constraint (reusing the BOOT
button, a strapping pin, for the network-recovery long-press) is justified in **Complexity
Tracking** below, per Governance's requirement that any deviation be listed with justification.

**Post-design re-check** (after Phase 1 — `data-model.md`, `contracts/`, `quickstart.md`): no new
violations introduced. The one notable design turn from Phase 0's initial reading — needing a
project-owned `tNMEA2000`/TWAI subclass rather than an adopted library (`research.md` §3) — is a
*research* finding, not a constitution deviation: Principle II already anticipates exactly this
outcome ("If no maintained driver fits, write a thin `tNMEA2000` subclass..." per this feature's
own plan input, mirrored in the constitution's "driver's S3 compatibility MUST be verified before
adoption"). All seven principles remain PASS as designed.

## Project Structure

### Documentation (this feature)

```text
specs/001-calibration-guided-setup/
├── plan.md              # This file (/speckit-plan command output)
├── research.md          # Phase 0 output (/speckit-plan command)
├── data-model.md        # Phase 1 output (/speckit-plan command)
├── quickstart.md        # Phase 1 output (/speckit-plan command)
├── contracts/           # Phase 1 output (/speckit-plan command)
└── tasks.md             # Phase 2 output (/speckit-tasks command - NOT created by /speckit-plan)
```

### Source Code (repository root)

```text
include/
└── pin_config.h              # single source of pin assignments (constitution Principle V);
                               # populated in Phase 0 of tasks.md from quickstart.md's Wiring table

src/
├── heading/                  # pure logic, native-testable: quaternion->Euler, level rotation,
│   │                         # mounting offset, deviation correction, normalize/wraparound,
│   │                         # circular smoothing filter, quality gate (data-model.md §6)
├── calibration/               # pure logic, native-testable: Stage A/B/C state machines
│   │                         # (data-model.md §3), stillness/position/rotation-coverage/sector
│   │                         # detection, sample filtering, deviation curve fit
├── n2k_codec/                 # pure logic, native-testable: PGN field build/parse, unit conversion
├── settings/                  # pure logic, native-testable: SSID validation, schema versioning,
│   │                         # defaults, migration
├── guide/                     # pure logic, native-testable: guide content model (contracts/rest-api.md
│   │                         # GET /api/guide), loaded from one content source file
├── drivers/                   # hardware adapters behind interfaces (native tests use fakes):
│   ├── imu_driver/            #   ImuDriver over Adafruit_BNO08x (I2C, INT/RST per pin_config.h)
│   ├── can_bus/                #   CanBus: project-owned tNMEA2000 subclass over esp_twai.h (research.md §3)
│   ├── kv_store/                #   KeyValueStore over NVS Preferences (data-model.md §1, §5)
│   └── clock/                   #   Clock: monotonic + NMEA 2000 System Time (PGN 126992) wall-clock (data-model.md §4)
├── services/                  # CalibrationService, SettingsService, N2kService, WebApi, DiagLog
├── tasks/                     # ImuTask, N2kTask (Core 1); AppTask (Core 0); task-watchdog wiring
└── main.cpp

data/                          # LittleFS image source: UI (HTML/CSS/JS, inline SVG), guide content
├── www/
└── guide/

test/
├── native/                    # pio test -e native — one dir per pure-logic module above
│   ├── test_heading/
│   ├── test_calibration/
│   ├── test_n2k_codec/
│   └── test_settings/
└── esp32s3/                   # pio test -e esp32s3_test — on-target: IMU comms, TWAI start/stop/
                                # bus-off, NVS record read/corrupt/recover, LittleFS asset presence

tools/
└── hil/                       # Python: capture.py, boot_restore_check.py, quality_gate_check.py,
                                # stage_a_walkthrough.py, http_status_check.py, ssid_change_check.py,
                                # can_capture.py (quickstart.md)

checklists/
└── manual-verification.md     # SC-001, SC-002, SC-005, SC-010 (Phase 1 of tasks.md)
```

**Structure Decision**: Single embedded-firmware PlatformIO project (constitution's default; no
frontend/backend split — `data/www/` is static assets served by the same firmware binary via
LittleFS). Pure logic is isolated into `src/heading/`, `src/calibration/`, `src/n2k_codec/`,
`src/settings/`, `src/guide/` specifically so `test/native/` can compile and test them on the host
with zero hardware, per constitution Principle V ("core logic compiles and is testable in the
native environment") and Principle IV's three-layer testing model.

## Phases

Each phase ends buildable, native-tested, flashed, and serial-validated per constitution Principle
IV — not merely "code written." `/speckit-tasks` breaks these into individually verifiable tasks.

0. **Skeleton**: pinned `esp32s3`/`esp32s3_test`/`native` environments, custom OTA partition table
   (`research.md` §8), `include/pin_config.h` (from `quickstart.md`'s Wiring table), `[SYS]`
   diagnostic logging, native test harness with a first passing trivial test.
1. **Heading pipeline + IMU + quality gate + N2K output (bench mode)**: `ImuDriver`, `heading/`
   module, `HeadingReading` publish/subscribe, project-owned `tNMEA2000` subclass over `esp_twai.h`
   (`research.md` §3) configured with base node identity/behavior (address claim, product/config
   info, ISO request, PGN list, heartbeat — constitution Principle II) and transmitting
   127250/127251/127257 in bench (self-test/no-ACK) mode, gated by `HEADING_MIN_ACCURACY`.
2. **Persistence + Stage A + Calibration tab (guide rendering)**: `KeyValueStore`/NVS envelope
   (`data-model.md` §1, §5), `SensorCalibrationProfile`, Stage A state machine (§3.1), `GET
   /api/guide`, Calibration tab shell with the readiness summary and Stage A card.
3. **Web API/WebSocket + Settings tab + SSID change + recovery path**: full `contracts/rest-api.md`
   and `contracts/websocket.md` surface for status/settings, SSID validation and change flow
   (FR-034-039), BOOT-button network-recovery path (see Complexity Tracking).
4. **Incoming PGN parsing + GPS simulation + Stage B**: 129026/127258 (optionally 129029) receive
   path, `DEBUG_GPS_INJECT` bench endpoint (`quickstart.md` §6), Stage B state machine (§3.2)
   including the true→magnetic bearing conversion.
5. **Stage C**: swing state machine (§3.3), sector coverage, sample filtering, least-squares
   deviation fit, manual 8-point fallback (§3.4), curve/table UI.
6. **Hardening**: fault injection (sensor disconnect, power loss mid-save, corrupted/incompatible
   record recovery per FR-045/046), OTA update + rollback exercise, full manual verification
   checklist (`checklists/manual-verification.md`).

## Risks

| Risk | Mitigation in this plan |
|---|---|
| ESP32-S3 TWAI driver compatibility with the NMEA2000 stack | Resolved by research (`research.md` §3): no maintained S3-correct bridge exists, so a project-owned `tNMEA2000` subclass over ESP-IDF 5.5's new `esp_twai.h` node API is planned from the start, not discovered mid-implementation. This is now sized as its own body of work in Phase 1, including bus-off recovery and error counters via `twai_node_recover()`/`twai_node_get_info()`. |
| BNO08x library's DCD control and restoring calibration after Cancel | `research.md` §2 confirms `sh2_saveDcdNow()`/`sh2_setCalConfig()`/tare calls are available via Adafruit_BNO08x's exposed `sh2.h`. Cancel/timeout (data-model.md §3.1) relies on simply *not* calling save-DCD — the BNO08x's own last-saved DCD remains active on the chip, so "restore previous calibration" requires no explicit restore step, only discipline about when save-DCD is invoked. Verify this assumption with an on-target test in Phase 2 (cancel mid-Stage-A, confirm prior accuracy behavior is unchanged). |
| Async web server stability / memory under several WebSocket clients | `ESP32Async` fork chosen for its documented throughput work (`research.md` §4); `WS_STATUS_RATE_HZ_*` bounds broadcast cost regardless of client count; `Scale/Scope` above caps expected concurrent clients at a handful. Phase 3 includes a multi-tab manual/HIL check as a specific task, not just an assumption. |
| Whether the Elite FS 9 sends COG and variation on the bus | Spec already requires graceful degradation: Stage B/C's GPS methods are only offered when a COG source is detected (FR-019), and known-bearing / manual-8-point fallbacks exist unconditionally (FR-018, FR-030). `n2k.cog_sog_source_present` / `n2k.variation_source_present` in `GET /api/status` make this observable in HIL tests without the actual chartplotter. Confirmed/denied for real only via the manual checklist (SC-010) on the actual boat. |
| Magnetic interference from the device's own wiring and the VP230 module | Addressed at the process level, not purely code: Stage A's guide explicitly calls out the "final enclosure, cables attached" precondition (spec FR-008/Stage A before-you-start) so the bench calibration already accounts for the device's own fields; Stage C's deviation model and quality gate exist specifically to characterize and correct whatever residual interference remains once installed. No code mitigation beyond what's already in `data-model.md` §3/§6. |

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| Network-recovery long-press reads the BOOT button (GPIO0), a constitution-reserved strapping pin | FR-038 requires a documented recovery path when a user can't reconnect after an SSID change; a physical, always-available control that works even if Wi-Fi/web is entirely unreachable is the only option that can't itself be locked out by the failure it's recovering from. GPIO0 already carries the BOOT/strapping role on every ESP32-S3 devkit; this plan only *reads* it as a plain input after boot completes (never drives it), which is the same well-established pattern every "hold BOOT to reset" ESP32 product uses — it adds no new conflict with the boot-strapping function, since that function is already fixed by the module regardless of this feature. | A software-only recovery (serial command, `quickstart.md` §-style) was considered as the sole path, but rejected as the *only* path: it requires a USB-connected PC, which isn't available to an owner on the boat who only has the mis-configured Wi-Fi. The plan keeps the serial/HTTP `POST /api/network/reset` route too (`contracts/rest-api.md`) as a bench-friendly secondary path, but the physical button remains primary specifically for the on-the-water, no-PC case. |
