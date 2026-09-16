# Tasks: Calibration, Guided Setup & Settings

**Input**: Design documents from `/specs/001-calibration-guided-setup/`

**Prerequisites**: [plan.md](./plan.md) (required), [spec.md](./spec.md) (required for user stories),
[research.md](./research.md), [data-model.md](./data-model.md), [contracts/](./contracts/),
[quickstart.md](./quickstart.md)

**Tests**: Included. The constitution's Hardware-in-the-Loop Verification principle (non-negotiable)
and this feature's own plan.md "Testing and verification plan" require native tests, on-target
tests, and serial/HTTP HIL scripts for every layer — this is an explicit requirement of this
project, not an optional default.

**Organization**: Tasks are grouped by user story (from spec.md, priorities in parentheses) to
enable independent implementation and testing of each story. Two P1 stories with almost no
user-visible surface of their own (US2, US6) are sequenced right after US1 rather than deferred,
because they close the loop on this project's non-negotiable Navigation Data Integrity guarantee
— see **Implementation Strategy** for the reasoning.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1, US2, US3, US4, US5, US6) — omitted for
  Setup, Foundational, and Polish tasks, which serve every story
- File paths are exact, per plan.md's Project Structure

## Path Conventions

Single embedded-firmware PlatformIO project (plan.md "Structure Decision"): `include/`, `src/`,
`data/`, `test/native/`, `test/esp32s3/`, `tools/hil/`, `checklists/` at the repository root.

---

## Phase 1: Setup

**Purpose**: Project initialization — pinned toolchain, environments, partition table.

- [X] T001 Create the project skeleton per plan.md's Project Structure: `include/`, `src/{heading,calibration,n2k_codec,settings,guide,drivers/{imu_driver,can_bus,kv_store,clock},services,tasks}/`, `data/{www,guide}/`, `test/{native,esp32s3}/`, `tools/hil/`, `checklists/`.
- [X] T002 [P] Pin every dependency in `platformio.ini` per research.md: `platform = pioarduino/platform-espressif32 @ release 55.03.311` (Arduino core 3.3.11 / ESP-IDF 5.5.5), `adafruit/Adafruit BNO08x @ 1.2.7`, `ttlappalainen/NMEA2000 @` commit `5b7b9fc3ccc18e30ebfba92da6486cffc625159` (verified via `git ls-remote` first), `ESP32Async/ESPAsyncWebServer @ 3.12.1`, `ESP32Async/AsyncTCP @ 3.5.0`, `bblanchon/ArduinoJson @ 7.4.3`; define `esp32s3`, `esp32s3_test`, `native` environments.
- [X] T003 [P] Create `partitions_16mb_ota.csv` per research.md §8 (`nvs` data/nvs at `0x9000`/`0x5000`; `otadata` data/ota at `0xe000`/`0x2000`; `app0` app/ota_0 at `0x10000`/`0x300000`; `app1` app/ota_1 at `0x310000`/`0x300000`; `littlefs` data/spiffs at `0x610000`/`0x9F0000`) and set `board_build.partitions = partitions_16mb_ota.csv` for the `esp32s3`/`esp32s3_test` environments in `platformio.ini`.
- [X] T004 [P] Configure `esp32s3`/`esp32s3_test` board settings in `platformio.ini`: `board_upload.flash_size = 16MB`, `board_build.psram_type = opi` (qio_opi), USB CDC on boot enabled, `board_build.filesystem = littlefs`.
- [X] T005 [P] Add zero-compiler-warnings build flags (`-Wall -Wextra -Werror` or the PlatformIO/ESP-IDF equivalent) to `esp32s3`, `esp32s3_test`, and `native` environments in `platformio.ini` per the constitution's Technical Constraints.
- [X] T006 [P] Configure the `native` environment in `platformio.ini` (`test_framework = unity`, a `build_src_filter` that includes only `src/heading/`, `src/calibration/`, `src/n2k_codec/`, `src/settings/`, `src/guide/`, and `test/native/fakes/`) so `pio test -e native` compiles with zero hardware dependencies.
- [X] T007 [P] Configure the `esp32s3_test` environment in `platformio.ini` for on-target Unity tests under `test/esp32s3/`.
- [X] T008 [P] Create `tools/hil/requirements.txt` pinning `pyserial`, `requests`, and `python-can`.
- [X] T009 [P] Create `secrets.ini.example` documenting the bench Wi-Fi credentials fields the `tools/hil/*` HTTP-check scripts expect (quickstart.md §7), and add `secrets.ini` to `.gitignore`.

**Checkpoint**: `pio run -e esp32s3` and `pio test -e native` both execute (even with empty/stub sources) using the pinned toolchain.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Shared infrastructure every user story depends on — hardware adapters, the heading
pipeline, persistence envelope, NMEA 2000 transport, task/core split, and the status API skeleton.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [X] T010 [P] Create `include/pin_config.h` with named constants for I2C SDA=8, SCL=9, BNO08x INT=15, BNO08x RST=16, TWAI TX=4, TWAI RX=5 (quickstart.md Wiring table), each with a comment citing the constitution reserved-pin range it avoids (flash/PSRAM 26–37, native USB 19/20, strapping 0/3/45/46).
- [X] T011 [P] Implement `src/services/diag_log.h`/`.cpp`: a structured serial log line writer implementing the `[TAG] key=value ...` grammar in contracts/serial-log.md (tags `SYS`, `IMU`, `N2K`, `CAL`, `WIFI`, `DATA`), including the one-time `[SYS] boot fw=... reset_reason=... heap=...` line and a periodic `[SYS] heap=... uptime=...` heartbeat.
- [X] T012 Implement `src/drivers/kv_store/kv_store.h`/`.cpp`: the `KeyValueStore` interface over NVS `Preferences`, plus a generic record-envelope helper implementing data-model.md §1's write-then-verify-then-commit sequence (`schema_version` + `crc32` over the rest of the record) and §5's on-load validation (schema or CRC mismatch → reset only that record to default, leave every other record untouched, emit `[DATA] reset record=<name> reason=crc_mismatch|schema_mismatch`).
- [X] T013 [P] Implement a fake `KeyValueStore` (in-memory) in `test/native/fakes/fake_kv_store.h` for native tests.
- [X] T014 [P] Unit tests for the record envelope (valid record round-trips; CRC-mismatch and schema-mismatch each reset only the affected record; a corrupted record never touches another) in `test/native/test_settings/test_record_envelope.cpp`.
- [X] T015 [P] Implement `src/drivers/clock/clock.h`/`.cpp`: the `Clock` interface providing monotonic time plus a wall-clock date sourced from the last-received NMEA 2000 System Time (PGN 126992), returning "date unavailable" if none has been received since boot (data-model.md §4).
- [X] T016 [P] Implement a fake `Clock` in `test/native/fakes/fake_clock.h` for native tests.
- [X] T017 Define `src/drivers/imu_driver/imu_driver.h`: the `ImuDriver` interface (init; read report → rotation vector + per-sensor accuracy 0-3; `setCalibrationConfig`; `saveDcd`; `tare`), independent of Adafruit_BNO08x so it is fake-able in native tests.
- [X] T018 Implement `src/drivers/imu_driver/imu_driver_bno08x.cpp`: the `ImuDriver` implementation over Adafruit_BNO08x 1.2.7 (research.md §2) via I2C on `pin_config.h`'s SDA/SCL, interrupt-driven reads on INT, hard reset on RST after a comms fault, calling `sh2_setCalConfig`, `sh2_saveDcdNow`, `sh2_setTareNow`/`sh2_persistTare`.
- [X] T019 [P] Implement a fake `ImuDriver` fed from recorded/synthetic report sequences in `test/native/fakes/fake_imu_driver.h`.
- [X] T020 [P] Implement `src/heading/quaternion.h`/`.cpp`: quaternion multiply/conjugate/→Euler math shared by every downstream heading step.
- [X] T021 [P] Implement `src/heading/normalize.h`/`.cpp`: angle normalization to `[0, 2π)` and circular-difference helpers, covering the 0°/360° wraparound.
- [X] T022 [P] Unit tests for `normalize.cpp` covering the 0°/360° wraparound with known vectors in `test/native/test_heading/test_normalize.cpp` (constitution Principle I).
- [X] T023 Implement `src/heading/pipeline.h`/`.cpp`: the fixed-order heading pipeline from data-model.md §6 (raw rotation vector → `level_reference_quat` → `mounting_offset_rad` → `DeviationCorrection` curve → normalize → quality gate → `HeadingReading` snapshot), using identity/zero corrections when Stage B/C are `NOT_DONE`, and passing `CalibrationSession.active_stage` plus the persisted `SensorCalibrationProfile` (if any) through to the quality gate so it can apply the Stage A in-progress freeze (data-model.md §6 "Stage A in-progress freeze", FR-041).
- [X] T024 Implement `src/heading/quality_gate.h`/`.cpp`: the `HEADING_MIN_ACCURACY` gate — "magnetometer, accelerometer, and gyroscope have all reached 'High' accuracy" (FR-014/FR-041, quoted) — producing `HeadingReading.valid` and `reason_if_invalid` ∈ `SENSOR_NOT_CALIBRATED`, `SENSOR_ACCURACY_LOW`, `SENSOR_DISCONNECTED`; as pure-function inputs, take `CalibrationSession.active_stage` and the persisted `SensorCalibrationProfile` (if any) so that while `active_stage == A` and a profile exists, accuracy is read from that profile instead of the live `ImuDriver` report, keeping heading valid throughout an in-progress Stage A recalibration attempt (data-model.md §6, FR-041).
- [X] T025 [P] Unit tests for `pipeline.cpp` asserting the fixed correction order — "sensor calibration → level reference and mounting offset → deviation correction → normalization" (FR-044, quoted) — with known vectors in `test/native/test_heading/test_pipeline.cpp`.
- [X] T026 [P] Unit tests for `quality_gate.cpp` covering all-High passes, any-sensor-below-High fails, and the exact `reason_if_invalid` mapping in `test/native/test_heading/test_quality_gate.cpp`.
- [X] T027 [P] Unit tests for `quality_gate.cpp`'s Stage A in-progress freeze in `test/native/test_heading/test_quality_gate_stage_a_inflight.cpp`: live accuracy dropping mid-attempt does NOT flip `valid` to false when a persisted `SensorCalibrationProfile` exists — covering spec.md's "Given a previously saved, valid Stage A calibration exists, When the user starts a new Stage A recalibration attempt, Then the device keeps transmitting heading computed from the previously saved calibration" (quoted); with no persisted profile, gating still falls back to live accuracy (data-model.md §6, FR-041).
- [X] T028 [P] Implement `src/heading/smoothing.h`/`.cpp`: a circular-value-aware smoothing filter that reduces stationary noise without adding noticeable lag during turns (FR-043).
- [X] T029 [P] Unit tests for `smoothing.cpp` asserting stationary output stays within a bounded peak-to-peak window over a synthetic 30 s trace, and tracks a synthetic turn without added lag beyond a defined bound, in `test/native/test_heading/test_smoothing.cpp` (supports SC-003).
- [X] T030 Implement `src/n2k_codec/pgn_codec.h`/`.cpp`: pure encode/decode for PGN 127250 (heading + deviation + variation fields), 127251 (rate of turn), 127257 (attitude), 129026 (COG/SOG), 127258 (variation), with unit conversion to/from radians and rad/s (constitution Principle I).
- [X] T031 [P] Unit tests for `pgn_codec.cpp` round-tripping known field values, including range-boundary values, in `test/native/test_n2k_codec/test_pgn_codec.cpp`.
- [X] T032 Define `src/drivers/can_bus/can_bus.h`: the `CanBus` interface (send frame, receive frame, bus state, self-test/no-ACK toggle), independent of TWAI specifics so it is fake-able.
- [X] T033 Implement `src/drivers/can_bus/twai_node_bus.cpp`: a project-owned `tNMEA2000` subclass over ESP-IDF 5.5's node-based TWAI API (`twai_new_node_onchip`, `twai_node_enable`, `twai_node_transmit`, `twai_node_register_event_callbacks`) per research.md §3, wired to `pin_config.h`'s TWAI TX/RX, with bus-off recovery via `twai_node_recover()` and error counters via `twai_node_get_info()`, and an `enable_self_test` bench/no-ACK mode surfaced as `n2k.bus_state == "BENCH_MODE"`.
- [X] T034 Configure `tNMEA2000` base node identity and behavior in `src/services/n2k_service.h`/`.cpp`'s init path: `SetProductInformation` (model ID, software version, model version, manufacturer serial code), `SetDeviceInformation` (unique number, device/function/class codes for an NMEA 2000 compass), a default source address, and the library's ISO-request (59904) / PGN-list (126464) / heartbeat (126993) handling (defaults, or `SetHeartbeatInterval` if a non-default rate is justified); call `NMEA2000.Open()` and `ParseMessages()` on schedule from `N2kTask` so address claim (60928) and product/config info (126996/126998) responses are live node behavior, not assumed (constitution Principle II).
- [X] T035 Implement/extend `tools/hil/can_capture.py` to assert base node behavior: address claim completes (`[N2K] address_claimed addr=..`-style log line), product info (126996)/config info (126998)/PGN list (126464) responses appear on request, and heartbeat (126993) appears on schedule in bench/self-test mode (constitution Principle IV); real-bus multi-node verification is called out as an explicit item in `checklists/manual-verification.md` (T131), never assumed.
- [X] T036 [P] Implement a fake `CanBus` in `test/native/fakes/fake_can_bus.h`.
- [X] T037 On-target Unity test in `test/esp32s3/test_twai/test_twai_start_stop.cpp`: TWAI start, stop, and bus-off recovery in self-test mode.
- [X] T038 Implement `src/services/n2k_service.h`/`.cpp`: wraps `ttlappalainen/NMEA2000` + `CanBus`, transmits 127250/127251/127257 from the current `HeadingReading` at NMEA 2000 default rates, skips 127250 (logging `[N2K] tx pgn=127250 skipped reason=quality_gate`) whenever `HeadingReading.valid` is false, and parses incoming 129026/127258/129029 into a bus-status snapshot (`n2k.cog_sog_source_present`, `n2k.variation_source_present`).
- [X] T039 [P] Unit tests in `test/native/test_n2k_codec/test_transmit_gate.cpp` asserting 127250 is only encoded when `HeadingReading.valid` is true, using the fake `CanBus`.
- [X] T040 Implement `src/tasks/imu_task.h`/`.cpp`: `ImuTask` on Core 1 (high priority), reads `ImuDriver` on its INT interrupt, runs the heading pipeline, publishes `HeadingReading` via a mutex-protected snapshot; no `delay()`-based busy logic (constitution Principle III).
- [X] T041 Implement `src/tasks/n2k_task.h`/`.cpp`: `N2kTask` on Core 1 (high priority), drives `N2kService` on schedule and monitors bus health; no `delay()`-based busy logic.
- [X] T042 Implement `src/tasks/app_task.h`/`.cpp`: `AppTask` on Core 0, owns `CalibrationService`/`SettingsService` state, persistence, and WebSocket broadcast; reads UI commands from a command queue.
- [X] T043 Wire a FreeRTOS task watchdog for `ImuTask`, `N2kTask`, and `AppTask` in `src/main.cpp` (constitution Principle III).
- [X] T044 Implement `src/tasks/shared_state.h`/`.cpp`: the mutex-protected `HeadingReading` snapshot and `AppTask`'s command queue as the only cross-task data paths (no unprotected globals).
- [X] T045 Implement `src/services/web_api.h`/`.cpp`: ESPAsyncWebServer bootstrap serving LittleFS static assets from `data/www/`, JSON via ArduinoJson v7, and a `/ws` WebSocket endpoint per contracts/websocket.md.
- [X] T046 Implement `GET /api/status` per contracts/rest-api.md, assembling `readiness`, `heading`, `stages`, `session`, `n2k`, `settings`, `system` from `AppTask` state.
- [X] T047 Implement the periodic `status` WebSocket broadcast at `WS_STATUS_RATE_HZ_IDLE`, boosted to `WS_STATUS_RATE_HZ_ACTIVE_CAL` while a calibration session is active (data-model.md §1.5, contracts/websocket.md), bounded so it never starves `ImuTask`/`N2kTask`.
- [X] T048 [P] Implement the shared UI shell in `data/www/index.html`, `data/www/style.css`, `data/www/app.js`: Dashboard/Calibration/Settings tab navigation, large-numeral high-contrast mobile-first layout, and a WebSocket client rendering `status` messages into the Dashboard's live heading/pitch/roll/rate-of-turn display.
- [X] T049 Implement `src/services/calibration_service.h`/`.cpp`: the `CalibrationSession` in-memory model (data-model.md §2.1) enforcing a single active procedure across all clients, with `CALIBRATION_SESSION_INACTIVITY_TIMEOUT_S` auto-cancel on client inactivity.
- [X] T050 Unit tests in `test/native/test_calibration/test_session_singleton.cpp` asserting a second start attempt while a session is active is refused and produces a `[CAL] busy stage=X rejected=Y`-style result.
- [X] T051 Implement `POST /api/calibration/{stage}/start` and `/cancel` per contracts/rest-api.md, returning `409` with `error.code = "CALIBRATION_BUSY"` and `active_stage` when another procedure is already running.

**Checkpoint**: `pio test -e native` and `pio test -e esp32s3_test` pass; `esp32s3` firmware boots, emits `[SYS] boot ...`, claims its NMEA 2000 address (`[N2K] address_claimed addr=..`) and responds to product/config info and PGN-list requests, and (once the BNO08x reports sufficient accuracy) transmits `[N2K] tx pgn=127250 ok` in bench mode; `GET /api/status` and the Dashboard render live data. No calibration stage exists yet.

---

## Phase 3: User Story 1 - Guided bench sensor calibration (Stage A) (Priority: P1) 🎯 MVP

**Goal**: A new owner completes bench sensor calibration end-to-end from the built-in guide alone.

**Independent Test**: Run Stage A from a fresh/uncalibrated device on a bench (no boat, no NMEA
2000 bus) and confirm a calibration saves at the required quality, and a poor attempt never
overwrites a prior good result.

- [X] T052 [P] [US1] Implement `src/guide/guide_content.h`/`.cpp`: loads the guide payload from `data/guide/guide_content.json` (FR-008 shape: what/why, before-you-start checklist, numbered illustrated steps, duration, success indicator, failure help) and serves it via `GET /api/guide` (contracts/rest-api.md), independent of calibration logic (FR-011).
- [X] T053 [P] [US1] Author Stage A's guide content in `data/guide/guide_content.json`: before-you-start checklist (final enclosure with cables attached; ≥1 m from interference sources; powered and connected); 3 numbered steps (hold still, six positions, slow rotation) each referencing an illustration id; duration estimate; success indicator; failure causes/remedies.
- [X] T054 [P] [US1] Add Stage A's 3 inline-SVG illustrations to `data/www/illustrations/stage_a.svg`, referenced by `guide_content.json`'s `illustration_svg_id`.
- [X] T055 [US1] Implement `src/calibration/stage_a.h`/`.cpp`: the Stage A state machine (data-model.md §3.1) — `Idle → AwaitingStillness → AwaitingPositions → AwaitingRotation → Evaluating → Done/TimedOut/Cancelled` — using named constants `STAGE_A_STILLNESS_GYRO_VAR_MAX`, `STAGE_A_STILLNESS_HOLD_S`, `STAGE_A_POSITION_HOLD_S`, `STAGE_A_POSITION_GRAVITY_TOL_DEG`, `STAGE_A_ROTATION_COVERAGE_BINS`, `STAGE_A_ROTATION_COVERAGE_MIN_FRACTION`, `STAGE_A_TIMEOUT_S`.
- [X] T056 [P] [US1] Unit tests in `test/native/test_calibration/test_stage_a_stillness.cpp`: stillness detection against synthetic/recorded gyro-variance traces.
- [X] T057 [P] [US1] Unit tests in `test/native/test_calibration/test_stage_a_positions.cpp`: each of the six rest positions (top/bottom/left/right/front/back) detected independently, order-agnostic, each requiring `STAGE_A_POSITION_HOLD_S` held.
- [X] T058 [P] [US1] Unit tests in `test/native/test_calibration/test_stage_a_rotation.cpp`: rotation-coverage bin tracking reaching `STAGE_A_ROTATION_COVERAGE_MIN_FRACTION` from a synthetic figure-8/axis-turn trace.
- [X] T059 [US1] Unit tests in `test/native/test_calibration/test_stage_a_accept.cpp` for: "The system MUST save a new Stage A result only once the magnetometer, accelerometer, and gyroscope have all reached 'High' accuracy, and MUST NOT overwrite an existing saved calibration with a result of lower quality" (FR-014, quoted verbatim) — assert a below-High result never overwrites a prior saved High-quality profile.
- [X] T060 [US1] Unit tests in `test/native/test_calibration/test_stage_a_timeout_cancel.cpp`: timeout leaves the previous calibration in effect and offers retry (FR-015); Cancel at any time leaves the previous calibration in effect (FR-016).
- [X] T061 [US1] Implement `SensorCalibrationProfile` persistence in `stage_a.cpp` via the T012 record envelope: `saved_at` (Clock), `mag_accuracy`/`accel_accuracy`/`gyro_accuracy` (each "MUST be 3 (High) per FR-014", quoted from data-model.md §1.1), `firmware_version`.
- [X] T062 [US1] Call `ImuDriver::saveDcd()` only when Stage A transitions `Evaluating → Done`; never call it on `Cancel`/`TimedOut`, relying on the BNO08x's own last-saved DCD remaining active.
- [X] T063 [US1] On-target Unity test in `test/esp32s3/test_imu/test_stage_a_cancel_restore.cpp`: start Stage A, cancel mid-procedure, confirm reported sensor accuracy/behavior afterward is unchanged from before Stage A started.
- [X] T064 [US1] Implement `POST /api/calibration/a/start`, `/cancel`, `/reset` (contracts/rest-api.md) wired to `stage_a.cpp`, `/reset` requiring `{"confirm": true}` (FR-005).
- [X] T065 [US1] Implement `calibration_progress`/`calibration_result` WebSocket messages for Stage A (contracts/websocket.md), including `session.progress.a` (`mag_acc`, `accel_acc`, `gyro_acc`, `positions_done`, `rotation_coverage_pct`).
- [X] T066 [US1] Emit `[CAL] stage=A state=...` transition lines, `[CAL] stage=A result=saved mag=.. accel=.. gyro=..`, and `[CAL] stage=A result=timeout|cancelled` (contracts/serial-log.md).
- [X] T067 [P] [US1] Implement the Calibration tab shell and Stage A card in `data/www/calibration.js`: guide view rendered from `GET /api/guide` with the current step auto-highlighted from `calibration_progress`, live 4-level quality indicators for mag/accel/gyro, six-position coverage display, Start/Cancel controls.
- [X] T068 [US1] Implement `tools/hil/stage_a_walkthrough.py`: boots the device, prompts the operator through the Stage A procedure, asserts on `[CAL] stage=A ...` lines with a hard timeout, confirms `result=saved` with mag/accel/gyro all High.

**Checkpoint**: Stage A is fully usable end-to-end on the bench, independent of Stage B/C/Settings.

---

## Phase 4: User Story 2 - Calibration readiness overview and journey guidance (Priority: P1)

**Goal**: The owner can always see, at a glance, whether the compass is trustworthy right now and
what to do next, and can redo/reset any stage independently.

**Independent Test**: Drive the device through each combination of stage states (none done, only A
done, A+B done, all done, a stage reset) and confirm the readiness summary and per-stage statuses
reflect that state correctly — independent of actually running a calibration procedure.

- [ ] T069 [P] [US2] Implement `src/calibration/stage_status.h`/`.cpp`: `StageStatus` derivation for A, B, and C (data-model.md §2.3) — `NOT_DONE`/`DONE` derived from record presence, quality/`saved_at` when `DONE`. Quote verbatim from FR-002: "Needs redo MUST result only from an explicit user reset (FR-005); the system MUST NOT automatically change a stage's persisted status because of a transient drop in live sensor accuracy during normal operation."
- [ ] T070 [US2] Unit tests in `test/native/test_calibration/test_readiness_summary.cpp`: readiness derivation for every stage-state combination (none / A only / A+B / all three) matches the spec's three readiness states, and status never auto-flips on a live accuracy drop.
- [ ] T071 [US2] Implement `src/calibration/readiness.h`/`.cpp` feeding `GET /api/status.readiness`, including the FR-001 real-time note, quoted verbatim: "This summary MUST also reflect, in real time and independently of any stage's persisted status, whenever heading is currently withheld per FR-041 ..., even while the underlying stage still reads 'Done'."
- [ ] T072 [US2] Implement `POST /api/calibration/{stage}/reset` confirmation enforcement (`{"confirm": true}` required, FR-005) for all three stages, and `Recalibrate` (re-invoking `start` on a `DONE` stage) leaving the other stages' saved results untouched.
- [ ] T073 [US2] Unit tests in `test/native/test_calibration/test_reset_confirmation.cpp`: reset without `confirm: true` is rejected and the record is unchanged; reset with `confirm: true` clears only the targeted stage.
- [ ] T074 [P] [US2] Implement the readiness-summary banner and first-boot welcome banner (FR-003) in `data/www/calibration.js`, driven by `GET /api/status.readiness`/the `status` WebSocket message, explaining what's lost by skipping a stage (FR-004).
- [ ] T075 [P] [US2] Implement per-stage status cards (Not done / In progress / Done-with-quality-and-date / Needs redo) for A, B, C in `data/www/calibration.js`, with Recalibrate and Reset-behind-confirmation controls.
- [ ] T076 [US2] Extend `tools/hil/http_status_check.py` to assert `GET /api/status.stages` and `.readiness` reflect the expected values after Stage A completes (via T068's fixture), with B and C still `NOT_DONE`.

**Checkpoint**: Readiness summary and per-stage status/recalibrate/reset work for Stage A now, and are structurally ready for B/C once those stages exist.

---

## Phase 5: User Story 6 - Trustworthy heading at all times (Priority: P1)

**Goal**: Independently demonstrate, via serial logs and status data alone, that heading is never
stale/frozen/invalid and is withheld with a clear reason whenever it can't be trusted.

**Independent Test**: Force low sensor accuracy, disconnect the sensor, and swing heading through
0°/360°, observing serial logs, `GET /api/status`, and the UI — no boat, no specific calibration
stage's own UI required.

- [ ] T077 [US6] Unit tests in `test/native/test_heading/test_wraparound_e2e.cpp`: the full pipeline (T023) stays continuous and correctly normalized to `[0°, 360°)` with no jump or sign error crossing 0°/360°, using known input vectors.
- [ ] T078 [US6] Wire `HeadingReading.reason_if_invalid` into `GET /api/status.heading.reason_if_invalid` and the `status`/`calibration_progress` WebSocket payloads, and surface it as plain-language text on the Dashboard and readiness summary (FR-041).
- [ ] T079 [US6] On-target Unity test in `test/esp32s3/test_imu/test_disconnect_no_repeat.cpp`: disconnect the BNO08x mid-run, assert `[IMU] disconnected` then `valid=0 reason=SENSOR_DISCONNECTED` appear and no further `[N2K] tx pgn=127250 ok` lines are emitted.
- [ ] T080 [US6] Implement `tools/hil/quality_gate_check.py`: force low sensor accuracy (debug hook or operator-guided), assert zero `[N2K] tx pgn=127250 ok` and at least one `[N2K] tx pgn=127250 skipped reason=quality_gate` line while the condition holds (SC-007).
- [ ] T081 [US6] Implement `tools/hil/boot_restore_check.py`: power-cycle a device with a saved Stage A profile, assert `[IMU] valid=1 ...` appears within 10 s of `[SYS] boot ...` (SC-006).
- [ ] T082 [US6] Unit tests in `test/native/test_heading/test_smoothing_bounds.cpp` asserting stationary peak-to-peak noise stays within the SC-003 bound over a 30 s synthetic stationary trace.
- [ ] T083 [US6] Unit test in `test/native/test_heading/test_single_source.cpp` asserting `GET /api/status.heading` and the value `N2kService` transmits both read the same published `HeadingReading` snapshot for a given pipeline tick (FR-042).

**Checkpoint**: The Navigation Data Integrity guarantee is independently demonstrated end-to-end via serial logs and status data.

---

## Phase 6: User Story 3 - Guided installation alignment on the boat (Stage B) (Priority: P2)

**Goal**: Zero the level reference and compute a mounting heading offset, via a known bearing or
GPS course.

**Independent Test**: At the dock with the boat at rest for the level step; known-bearing method
needs no bus dependency; GPS-course method exercised via the bench GPS-injection tooling.

- [ ] T084 [P] [US3] Author Stage B's guide content in `data/guide/guide_content.json` (before-you-start: permanently mounted; at rest/calm water for level; can determine true bow bearing; steps: Set level, choose Known bearing or GPS course; duration; success; failure help).
- [ ] T085 [P] [US3] Add Stage B's illustrations to `data/www/illustrations/stage_b.svg`.
- [ ] T086 [US3] Implement `src/n2k_codec/variation.h`/`.cpp`: magnetic-variation source resolution, quoted from FR-024: "corrected for magnetic variation read automatically from the NMEA 2000 bus when available, with manual user entry offered as a fallback when no bus-provided variation is present" — shared by Stage B and Stage C.
- [ ] T087 [P] [US3] Unit tests in `test/native/test_n2k_codec/test_variation_source.cpp`: bus-provided variation preferred when fresh (within `STAGE_C_REFERENCE_MAX_AGE_S`), manual value used otherwise.
- [ ] T088 [US3] Implement `src/calibration/stage_b.h`/`.cpp`: the Stage B state machine (data-model.md §3.2) — `Idle → LevelCapturing → LevelSet → (AwaitingBearingEntry | AwaitingGpsAlignment) → OffsetComputed → Saved/Discarded`.
- [ ] T089 [US3] Implement level capture: average attitude over `STAGE_B_LEVEL_STILL_WINDOW_S` with `STAGE_B_LEVEL_GYRO_VAR_MAX`, store as `level_reference_quat` (4×float32 quaternion — "so heading stays correct when the device is tilted", per data-model.md §1.2, not separate pitch/roll offsets).
- [ ] T090 [US3] Implement known-bearing offset, quoted from FR-018: "A true bearing entered this way MUST be converted to magnetic, using the same magnetic-variation source as FR-024 ..., before it is compared against the device's own magnetic heading reading" — producing `mounting_offset_rad` in range `(−π, π]`.
- [ ] T091 [US3] Implement GPS-course offset: accept samples "only while boat speed is above a minimum and course is steady" (FR-019, quoted) using `STAGE_B_GPS_MIN_SOG`, `STAGE_B_GPS_COG_STEADY_WINDOW_S`, `STAGE_B_GPS_COG_STEADY_MAX_STDDEV_DEG`; compute offset as the circular mean of `(COG − variation − heading)`; expose `waiting_reason` ∈ `SPEED_TOO_LOW`, `COURSE_NOT_STEADY`.
- [ ] T092 [P] [US3] Unit tests in `test/native/test_calibration/test_stage_b_bearing.cpp`: known-bearing offset math including the true→magnetic conversion, normalized to `(−π, π]`.
- [ ] T093 [P] [US3] Unit tests in `test/native/test_calibration/test_stage_b_gps.cpp`: GPS-course sample acceptance/rejection against `STAGE_B_GPS_MIN_SOG`/steadiness thresholds and the circular-mean offset computation, using synthetic COG/SOG traces.
- [ ] T094 [US3] Implement the FR-020 offset preview, quoted: "Before the user accepts a computed mounting offset, the UI MUST preview how the currently displayed heading would change if that offset is accepted" — in `session.progress.b.preview_offset_deg` and the Stage B UI.
- [ ] T095 [US3] Persist `InstallationAlignment` (T012 envelope) on Accept; apply `level_reference_quat`/`mounting_offset_rad` in the heading pipeline (T023) in place of the identity/zero defaults.
- [ ] T096 [US3] Unit tests in `test/native/test_heading/test_pipeline_with_stage_b.cpp`: pipeline output changes correctly once an `InstallationAlignment` record is present, and falls back to identity/zero when absent.
- [ ] T097 [US3] Implement `POST /api/calibration/b/level`, `/bearing`, and reuse `/apply`, `/discard`, `/cancel`, `/reset` (contracts/rest-api.md) wired to `stage_b.cpp`.
- [ ] T098 [US3] Emit `[CAL] stage=B ...` transition/result lines and throttled `[N2K] rx pgn=129026 sog=.. cog=..` lines (contracts/serial-log.md).
- [ ] T099 [P] [US3] Implement the Stage B card/guide/procedure UI in `data/www/calibration.js`: Set level control, Known-bearing entry form, GPS-course Align control with live waiting-reason display, offset preview, Accept/Discard.
- [ ] T100 [US3] Implement `tools/hil/stage_b_check.py`: drive Stage B via `POST /api/debug/gps-inject` (quickstart.md §6) end-to-end on the bench and assert the resulting `InstallationAlignment` via `GET /api/status`.

**Checkpoint**: Stage B is independently usable/testable (bench, with simulated GPS) without Stage C.

---

## Phase 7: User Story 5 - Change the Wi-Fi network name from the Settings tab (Priority: P2)

**Goal**: Change the SSID safely, with validation, a drop-connection warning, and a documented
recovery path — with zero interruption to NMEA 2000 output.

**Independent Test**: Change the SSID and confirm it persists across a reboot and NMEA 2000 output
is never interrupted, independent of any calibration state.

- [ ] T101 [P] [US5] Implement `src/settings/network_settings.h`/`.cpp`: SSID validation, quoted from FR-035: "1-32 characters, no leading or trailing whitespace" — and the `NetworkSettings` record (T012 envelope).
- [ ] T102 [P] [US5] Unit tests in `test/native/test_settings/test_ssid_validation.cpp` covering the FR-035 boundary cases verbatim: empty string, exactly 1 char, exactly 32 chars, 33 chars, leading space, trailing space, a valid mixed-content SSID.
- [ ] T103 [US5] Implement `GET`/`POST /api/settings` (contracts/rest-api.md) with the `applies_in_s` warning field (FR-036) and last-write-wins concurrency, quoted from FR-047: "the most recently received valid save completes and persists, and a client whose save was superseded MUST see the current, now-saved value the next time it loads Settings."
- [ ] T104 [US5] Unit tests in `test/native/test_settings/test_concurrent_save.cpp`: two near-simultaneous valid saves resolve last-write-wins per FR-047.
- [ ] T105 [US5] Implement the AP-restart sequence in `src/services/settings_service.h`/`.cpp`: save → respond → restart AP after `applies_in_s`, while `N2kService`/`ImuTask` continue uninterrupted (FR-037); emit `[WIFI] ssid_changed to=.. applies_in_s=..` then `[WIFI] ap_restart ssid=..`.
- [ ] T106 [US5] On-target/HIL test confirming no gap in `[N2K] tx pgn=127250 ok` spanning an SSID change and AP restart (SC-008), in `test/esp32s3/test_wifi/test_ssid_change_no_gap.cpp` or via `tools/hil/ssid_change_check.py` (T109) if AP restart cannot be exercised as a pure on-target Unity test.
- [ ] T107 [US5] Implement the network-recovery path (FR-038): a BOOT-button long-press handler in `src/tasks/app_task.cpp` reading `pin_config.h`'s BOOT pin as input only (never driving it) for `NETWORK_RESET_BOOT_HOLD_S`, plus `POST /api/network/reset` (contracts/rest-api.md) as the bench-friendly secondary path; emit `[WIFI] network_reset trigger=boot_hold|http_api`.
- [ ] T108 [P] [US5] Implement the Settings tab in `data/www/settings.js`: current-SSID display, validated input with inline error messages, pre-apply drop-connection warning showing the new network name, laid out to accommodate future settings without redesign (FR-039).
- [ ] T109 [US5] Implement `tools/hil/ssid_change_check.py`: change the SSID over HTTP, assert it persists across a reboot and that there is no gap in `[N2K] tx pgn=127250 ok` spanning the change (SC-008).

**Checkpoint**: Settings/SSID change is fully independent of any calibration stage.

---

## Phase 8: User Story 4 - Guided compass swing on the water (Stage C) (Priority: P3)

**Goal**: Measure and correct the boat's own magnetic deviation via a GPS-referenced swing, with a
manual 8-point fallback.

**Independent Test**: End-to-end on synthetic/injected heading and GPS-course data representing a
controlled turn, independent of an actual boat (on-water behavior is the manual checklist, SC-010).

- [ ] T110 [P] [US4] Author Stage C's guide content (GPS swing + manual 8-point fallback) in `data/guide/guide_content.json` per FR-008's shape, plus illustrations in `data/www/illustrations/stage_c.svg`.
- [ ] T111 [US4] Implement `src/calibration/deviation_fit.h`/`.cpp`: least-squares fit of `deviation(θ) = A + B·sinθ + C·cosθ + D·sin2θ + E·cos2θ`, solving the 5×5 normal equations with no dynamic allocation, sector-balanced weights across `STAGE_C_SECTOR_COUNT` (36) sectors.
- [ ] T112 [US4] Unit tests in `test/native/test_calibration/test_deviation_fit.cpp`, quoted from SC-009: "verified against synthetic swing data with known deviation, including noisy, missing-sector, and bad-sample scenarios, without requiring an actual boat" — assert recovered coefficients match known-input coefficients within tolerance for the clean case, and correct rejection for missing-sector/bad-sample cases.
- [ ] T113 [US4] Implement `src/calibration/sample_filter.h`/`.cpp`: per-sample acceptance (`STAGE_C_MIN_SOG`; `STAGE_C_MAX_TURN_RATE_DEG_S`/`STAGE_C_TURN_RATE_STEADY_TOL_DEG_S`; `STAGE_C_REFERENCE_MAX_AGE_S` freshness; sensor-accuracy check) plus per-sector outlier rejection via `STAGE_C_OUTLIER_MAD_MULTIPLIER` (median absolute deviation).
- [ ] T114 [P] [US4] Unit tests in `test/native/test_calibration/test_sample_filter.cpp`: each rejection condition individually (too slow; turn too fast/uneven; stale reference; low accuracy; MAD outlier).
- [ ] T115 [US4] Implement `src/calibration/stage_c.h`/`.cpp`: the Stage C GPS-swing state machine (data-model.md §3.3) — `Idle → Swinging → Evaluating → ResultReady/Rejected → Saved/Discarded` — tracking sector coverage (`STAGE_C_MIN_SAMPLES_PER_SECTOR` per sector) across `STAGE_C_MIN_FULL_TURNS`; acceptance quoted from the plan: "RMS residual ≤ threshold, max |deviation| ≤ sanity limit, and full coverage. Otherwise, reject with a specific reason" (`STAGE_C_MAX_RMS_RESIDUAL_DEG`, `STAGE_C_MAX_ABS_DEVIATION_DEG`).
- [ ] T116 [US4] Unit tests in `test/native/test_calibration/test_stage_c_state_machine.cpp`: full coverage + good fit → `ResultReady`; incomplete coverage or high residual → `Rejected` with the specific reason surfaced.
- [ ] T117 [US4] Implement the manual 8-point swing sub-flow (data-model.md §3.4) in `stage_c.cpp`: `AwaitingPoint(1..8)` collecting `STAGE_C_MANUAL_POINT_COUNT` (8, fixed) reference values at N/NE/E/SE/S/SW/W/NW, feeding the same `deviation_fit`.
- [ ] T118 [P] [US4] Unit tests in `test/native/test_calibration/test_stage_c_manual.cpp`: 8-point manual entry fits the same model correctly against known synthetic reference values.
- [ ] T119 [US4] Persist `DeviationCorrection` (T012 envelope) on Apply only, quoted from FR-028: "the new correction MUST take effect only after Apply, and Discard MUST leave the previous correction (if any) unchanged" — wire into the heading pipeline (T023) replacing the zero default.
- [ ] T120 [US4] Unit tests in `test/native/test_heading/test_pipeline_with_stage_c.cpp`: pipeline applies the saved `DeviationCorrection` curve correctly, and heading transmitted during an in-progress-but-not-yet-applied swing still uses the previous correction (FR-029).
- [ ] T121 [US4] Implement `POST /api/calibration/c/start` (`mode: "gps"|"manual"`), `/manual-point`, and reuse `/apply`, `/discard`, `/cancel`, `/reset` (contracts/rest-api.md).
- [ ] T122 [US4] Emit `[CAL] stage=C sector_coverage=.. turns=..` progress lines and `[CAL] stage=C result=rejected reason=.. rms_deg=..` (contracts/serial-log.md).
- [ ] T123 [US4] Implement the FR-032 cross-stage note, quoted: "When the user starts a new Stage A calibration attempt or resets Stage A, the system MUST indicate to the user that the Stage C result may also need to be redone" — Stage A's start/reset handlers (T055/T072) emit a hint consumed by the Calibration tab when a `DeviationCorrection` already exists.
- [ ] T124 [P] [US4] Implement the Stage C card/guide/procedure UI in `data/www/calibration.js`: Start swing, circular sector-coverage progress ring, too-fast warning, deviation curve/table + max deviation + residual display, Apply/Discard, and the manual 8-point fallback UI when no COG source is present.
- [ ] T125 [US4] Implement `tools/hil/stage_c_swing_check.py`: drive a full synthetic swing (known deviation, injected via `POST /api/debug/gps-inject` plus simulated turning) end-to-end and assert the resulting `DeviationCorrection` against the known input.

**Checkpoint**: All six user stories are independently functional; every calibration stage plus Settings works standalone and together.

---

## Phase 9: Polish & Cross-Cutting Concerns

**Purpose**: Improvements and verification that span every user story.

- [ ] T126 [P] Implement generic corrupted/incompatible-record recovery on boot across all four record types (data-model.md §5, FR-045/046) in `src/drivers/kv_store/kv_store.cpp`'s boot-time load path, wired into `src/main.cpp`.
- [ ] T127 On-target Unity test in `test/esp32s3/test_nvs/test_record_recovery.cpp`: deliberately corrupt one record's CRC on-target, reboot, confirm only that record resets to default and the others (each pre-populated with valid data) are untouched.
- [ ] T128 [P] Implement `tools/hil/capture.py`'s generic timeout-bounded capture/assert helper shared by the other HIL scripts, ensuring the serial port is always released before any upload.
- [ ] T129 Verify and fix zero-compiler-warnings across the `esp32s3` and `native` builds as a final build gate (constitution Technical Constraints).
- [ ] T130 [P] Document the NMEA 2000 electrical installation rules (backbone power via a proper regulator, declared Load Equivalency Number, no extra 120 Ω termination on the VP230 drop cable if present) in `README.md` or `docs/installation.md` (constitution Principle VII).
- [ ] T131 [P] Create `checklists/manual-verification.md` covering SC-001 (≤2° static error at 8 headings), SC-002 (≤3° error at ±15° tilt), SC-005 (≤2° error after Stage C on the water), and SC-010 (chartplotter display, on-water Stage B/C procedures, sunlight readability) as explicit manual checklist items, never assumed passing.
- [ ] T132 [P] Exercise an OTA update + rollback-on-failed-boot cycle against the two-app-slot partition table (T003) and document the result (constitution Principle VII).
- [ ] T133 Run the full quickstart.md validation sequence end-to-end (native → build/flash → on-target → HIL → HTTP checks → optional CAN capture) and record the actual captured output (constitution Principle IV).

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately.
- **Foundational (Phase 2)**: Depends on Setup — BLOCKS every user story.
- **US1 (Phase 3, P1)**: Depends only on Foundational.
- **US2 (Phase 4, P1)**: Depends on Foundational; uses US1's `SensorCalibrationProfile`/Stage A
  status as its first real data point, but its own readiness/status/reset machinery is generic
  across all three stages from the start.
- **US6 (Phase 5, P1)**: Depends on Foundational (the heading pipeline/quality gate it validates)
  and benefits from US2's readiness-summary real-time note (FR-001) already existing to attach
  `reason_if_invalid` messaging to.
- **US3 (Phase 6, P2)**: Depends on Foundational. Independent of US1/US2/US6 except that it shares
  the heading pipeline (already built in Foundational) and introduces `variation.h` (T086), which
  US4 later reuses.
- **US5 (Phase 7, P2)**: Depends only on Foundational — fully independent of every calibration
  story.
- **US4 (Phase 8, P3)**: Depends on Foundational and reuses US3's `variation.h` (T086) — the one
  cross-story code dependency in this plan, justified per the task-organization rule ("put a
  shared entity in the earliest story that needs it") rather than duplicating variation-resolution
  logic.
- **Polish (Phase 9)**: Depends on every phase above being complete enough to exercise (in
  practice, all of them).

### Parallel Opportunities

- All Setup tasks marked `[P]` (T002-T009) run in parallel once T001 creates the directory
  skeleton.
- Within Foundational, the independent driver/module tracks — `kv_store`/`clock` (T012-T016),
  `imu_driver` (T017-T019), `heading/*` (T020-T029), `n2k_codec`/`can_bus` (T030-T039) — can
  proceed in parallel; `tasks/*` (T040-T044) and `web_api`/`calibration_service` (T045-T051)
  depend on those modules existing first.
- Once Foundational is complete, **US1, US2's generic machinery, US6, US5, and (mostly) US3** can
  be staffed in parallel by different developers; only US4's `variation.h` reuse creates a soft
  ordering preference for US3 before US4.
- Within each story, `[P]`-marked guide-content/illustration/UI tasks and independent test files
  run in parallel; sequential tasks in the same file (e.g., a stage's state machine file gaining
  persistence, then API wiring, then logging) do not.

---

## Parallel Example: User Story 1

```bash
# Guide content, illustrations, and native tests for Stage A can all proceed together
# once T055 (the state machine) exists:
Task: "Author Stage A guide content in data/guide/guide_content.json"                 # T053
Task: "Add Stage A illustrations to data/www/illustrations/stage_a.svg"               # T054
Task: "Unit tests: stillness detection in test/native/test_calibration/test_stage_a_stillness.cpp"   # T056
Task: "Unit tests: six-position coverage in test/native/test_calibration/test_stage_a_positions.cpp" # T057
Task: "Unit tests: rotation coverage in test/native/test_calibration/test_stage_a_rotation.cpp"      # T058
```

---

## Implementation Strategy

### MVP First

The smallest genuinely safe, demonstrable increment is **Setup + Foundational + US1 + US2 + US6**
(all three P1 stories), not US1 alone: US1 without US6 could save a bench calibration but has no
independently-verified guarantee it withholds heading when untrustworthy, and US2 is what makes
the multi-stage journey legible at all. This trio delivers a bench-usable, trustworthy compass
(heading transmits correctly once Stage A passes, is withheld with a clear reason otherwise, and
the UI always shows accurate readiness) — genuinely shippable as a bench-calibration-only release
even before a boat is involved.

1. Complete Phase 1 (Setup) and Phase 2 (Foundational) — CRITICAL, blocks everything.
2. Complete Phase 3 (US1) → **validate independently** (bench, no boat).
3. Complete Phase 4 (US2) → validate independently (status/readiness across stage-state
   combinations).
4. Complete Phase 5 (US6) → validate independently (forced-fault HIL scripts).
5. **STOP and demo**: a trustworthy bench-only compass is already real at this point.

### Incremental Delivery (P2/P3 remainder)

6. Add Phase 6 (US3, Stage B) → validate independently (bench + simulated GPS) → demo.
7. Add Phase 7 (US5, Settings) → validate independently → demo (can be done in parallel with 6 by
   a second developer; no shared files).
8. Add Phase 8 (US4, Stage C) → validate independently (synthetic swing) → demo.
9. Complete Phase 9 (Polish), including the manual on-water/chartplotter checklist that only a
   human can execute.

### Parallel Team Strategy

With multiple developers, after Foundational:
- Developer A: US1 → US2 → US6 (the P1 spine, sequenced as above).
- Developer B: US5 (Settings) — fully independent, can start immediately after Foundational.
- Developer C: US3 → US4 (Stage B then C, since C reuses B's `variation.h`).

---

## Notes

- `[P]` tasks touch different files with no incomplete-task dependency.
- `[Story]` labels map every user-story-phase task back to spec.md for traceability; Setup,
  Foundational, and Polish tasks intentionally carry no label.
- Every data-model.md §1.5 named threshold constant is referenced by at least one task above —
  none are left to be invented ad hoc during implementation.
- Verify native tests fail before implementing the corresponding logic where a test task precedes
  its implementation task in the same phase.
- Commit after each task or logical group; every commit must build (constitution Development
  Workflow).
- Stop at any checkpoint to validate a story independently before moving on.
