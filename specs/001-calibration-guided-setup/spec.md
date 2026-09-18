# Feature Specification: Calibration, Guided Setup & Settings

**Feature Branch**: `001-calibration-guided-setup`

**Created**: 2026-09-16

**Status**: Draft

**Input**: User description: "Specify the calibration, guided setup, and settings feature set for
Lowrance Compass, the ESP32-S3 + BNO08x compass that sends heading to a Lowrance Elite FS 9 over
NMEA 2000 and serves a Wi-Fi web UI. Follow the project constitution, especially Navigation Data
Integrity and Hardware-in-the-Loop Verification.

Why: The compass is only useful if its heading can be trusted. Two separate sources of error must
be handled — sensor error (magnetometer/accelerometer/gyroscope calibration and enclosure
distortion) and installation error (mounting angle and the boat's own magnetic deviation). A bench
calibration fixes the first; only a calibration done on the boat fixes the second. A new owner must
be able to do both correctly from the web UI alone.

Users and context: A boat owner with no special technical knowledge, using a phone browser
connected to the device's Wi-Fi, first on a desk/workbench and then on the boat outdoors in
sunlight, with no internet access. All guide content must work offline.

Feature 1 — Calibration journey with built-in guides (P1): A Calibration tab presents three
stages in recommended order — A. Sensor calibration (bench), B. Installation alignment (boat, at
rest), C. Compass swing (boat, underway) — each with its own status (Not done / In progress / Done
with quality and date / Needs redo), an overall readiness summary, a first-boot welcome banner,
free ordering with guidance toward A→B→C, and independent Recalibrate/Reset-with-confirmation per
stage. Every stage shows a built-in guide before Start (what/why, before-you-start checklist,
numbered illustrated steps, duration, what success looks like, what to do if it fails), kept
available during the procedure with the current step auto-highlighted, phone-first and
sunlight-readable.

Stage A (bench): before-you-start checks (final enclosure, ≥1 m from interference sources,
powered and connected); procedure (hold still for gyro, six-position rest for accelerometer with
per-position progress, slow rotation/figure-8 for magnetometer, watch indicators until saved);
live 4-level quality indicators per sensor; automatic step advancement from detected
stillness/position coverage/rotation coverage; save only at required quality, never overwriting a
good calibration with a worse one; timeout with explained likely cause and retry, previous
calibration retained; Cancel at any time retains previous calibration.

Stage B (boat, at rest): before-you-start checks (permanently mounted, boat at rest/calm water for
level, user can determine true bow bearing); procedure (Set level captures pitch/roll zero;
heading alignment via Known bearing entry or GPS course via Align to GPS; computed mounting offset
shown, e.g. "Offset: +4.5°", accept or discard); offset/level saved and applied to heading/pitch/
roll; GPS method offered only when a course-over-ground source is detected on the bus, samples used
only while speed is above a minimum and course is steady, UI explains why it's waiting; UI previews
how the new offset changes the currently displayed heading before acceptance.

Stage C (boat, underway): before-you-start checks (Stage B recommended complete, calm water, open
water away from structures/vessels, engine and normal underway electronics on, room to turn safely
with a lookout); procedure (bring boat to slow steady speed, Start swing, turn at least two full
circles at roughly 1–3 minutes per circle with too-fast warning, circular progress ring fills per
heading sector covered, device computes and shows a deviation curve/table with max deviation and
residual error, user presses Apply or Discard); algorithm records own magnetic heading paired with
a GPS-course-over-ground reference corrected for magnetic variation while turning, rejects samples
for low speed/uneven or excessive turn rate/heading-course disagreement suggesting current or
drift, fits a smooth deviation curve over 0–360° (constant + once-per-turn + twice-per-turn
components) from accepted samples and applies it as a per-heading correction, rejects the swing
with an explanation if sector coverage is incomplete or fit error is too high; manual 8-point swing
(N/NE/E/SE/S/SW/W/NW with entered reference values) is offered as a fallback when GPS course is
unavailable; deviation correction persists across reboots/firmware updates and can be viewed,
redone, or reset; heading keeps transmitting with the existing correction during the swing, the new
correction applies only after Apply; the UI notes that a Stage A quality degradation may also mean
Stage C should be redone.

Common calibration behavior: all calibration state/progress/detected step/results are exposed
through machine-readable status data and structured serial log lines; only one calibration runs at
a time even under a double press or two open browsers; closing the browser never breaks the
device — the procedure continues and can be reopened, or times out safely.

Feature 2 — Settings tab with Wi-Fi SSID change (P1): shows and lets the user change the current
SSID; validates it before saving (1–32 characters, no leading/trailing whitespace), with a clear
error and nothing saved on invalid input; warns before applying that the connection will drop and
shows the network name to reconnect to; the new SSID persists across reboots; changing it never
interrupts NMEA 2000 compass output; a documented recovery path restores default network settings;
the tab layout allows more settings to be added later.

Feature 3 — Robust, high-accuracy heading (P1): heading stays accurate across the full 0–360°
range including across north; when calibration quality is too low to trust, the device sends "data
not available" or stops sending heading per the constitution, and the UI shows why; the UI heading
matches what is sent to the chartplotter; heading output is smooth when stationary without
noticeable lag during turns; corrections apply in this order — sensor calibration → level
reference and mounting offset → deviation correction → normalization.

Success criteria (SC-001 through SC-010) cover static and dynamic heading accuracy after Stage A
and Stage C, stationary noise, time-to-complete Stage A, boot-time calibration restore, the
no-heading-while-untrusted guarantee, SSID-change continuity, native unit-test coverage of the
Stage C fit algorithm on synthetic data, and an explicit manual verification checklist for
physical accuracy, chartplotter display, and on-water procedures that the agent cannot observe
directly.

Edge cases: no GPS course-over-ground source on the bus; speed too low, turning too fast, or
incomplete circles during a swing; strong current/wind mismatching course and heading; skipping
Stage A or B before running C; magnetic disturbance during calibration or normal use; sensor
disconnect/reset during calibration; power loss while saving calibration; corrupted or
incompatible stored calibration after an update; two clients with the UI open at once.

Out of scope: network settings other than SSID (unless decided during clarify); user accounts/
authentication beyond constitution requirements; automatic continuous deviation learning while
underway.

Clarification candidates flagged by the requester: whether the Wi-Fi password should also be
changeable; whether heading is sent while Stage A is running; the minimum calibration quality
required before heading is sent; the source of magnetic variation for the GPS reference; UI
language(s); whether Stage C should be a separate feature spec."

## Clarifications

### Session 2026-09-16

- Q: What makes the device decide, on its own, that Stage A's saved calibration has "degraded" and
  needs redoing, rather than only being reset by the user? → A: Manual only — the device never
  automatically flips a stage's persisted status due to a transient drop in live sensor accuracy;
  "Needs redo" results only from an explicit user reset. A live accuracy drop during normal
  operation still suppresses heading transmission per FR-041, but leaves the persisted Stage A
  status unchanged.
- Q: When stored calibration data is found corrupted, or was written by an incompatible earlier
  firmware version, what should the device do with it? → A: Reset only the affected stage(s) to
  "Not done"/safe defaults, on boot, leaving any other stage whose stored data still validates
  correctly untouched.
- Q: When the user enters a "known bearing" for Stage B alignment, is that value magnetic or true,
  and does the device convert it? → A: True, converted using variation — the user enters a true
  bearing, and the device converts it to magnetic (using the same bus-provided-with-manual-fallback
  variation source as Stage C) before comparing it to its own magnetic heading reading.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Guided bench sensor calibration (Stage A) (Priority: P1)

A new owner unboxes the device, mounts it in its final enclosure, powers it up, and connects to
its Wi-Fi from a phone. Without any manual, they open the Calibration tab, read the built-in guide
for Stage A, and follow the on-screen steps (hold still, six positions, slow rotation) while
watching live quality indicators until the device confirms the sensors are calibrated and saves
the result.

**Why this priority**: Sensor calibration is the foundation every other stage and all heading
accuracy depend on. It is also the only stage that can be completed and demonstrated off the boat,
making it the smallest possible slice that delivers real value (a sensor that reports trustworthy
relative orientation).

**Independent Test**: Can be fully tested on a bench, without a boat or NMEA 2000 bus, by running
Stage A from a fresh/uncalibrated device and confirming a calibration is saved with the required
quality and that a poor attempt does not overwrite a prior good result.

**Acceptance Scenarios**:

1. **Given** a device with no saved sensor calibration, **When** the user opens the Calibration
   tab, **Then** the tab shows a welcome banner explaining the three stages and points the user to
   Stage A.
2. **Given** the Stage A guide is open, **When** the user reads it before pressing Start, **Then**
   it shows what the stage does and why, a before-you-start checklist, numbered illustrated steps,
   an expected duration, what success looks like, and what to do if it fails.
3. **Given** Stage A is running, **When** the user holds the device still, rests it on each of the
   six sides, and slowly rotates it through a figure-8 and axis turns, **Then** the UI shows live
   4-level quality for magnetometer, accelerometer, and gyroscope, marks each position/step as
   covered automatically, and advances the guide's highlighted step without further user input.
4. **Given** all three sensors reach the required quality, **When** the device finishes evaluating
   the result, **Then** it saves the new calibration, marks Stage A "Done" with its quality and
   date, and updates the overall readiness summary.
5. **Given** a previously saved good calibration exists, **When** a new Stage A attempt finishes
   below the required quality, **Then** the previous calibration is kept unchanged and the stage
   status reflects the attempt did not succeed.
6. **Given** Stage A is in progress, **When** the user presses Cancel, **Then** the procedure stops
   immediately and the previously saved calibration remains in effect.
7. **Given** Stage A is in progress, **When** the configured timeout elapses without reaching
   required quality, **Then** the UI explains a likely cause (e.g., nearby metal, movement too
   fast) and offers a retry, and the previous calibration remains in effect.

---

### User Story 2 - Calibration readiness overview and journey guidance (Priority: P1)

At any time, the owner (or installer) can open the Calibration tab and immediately understand,
without prior knowledge, whether the compass is safe to rely on right now, which stages are done,
and what to do next — including redoing or resetting any single stage independently.

**Why this priority**: Trust in the heading depends on the owner always being able to see current
calibration status at a glance; this is what makes the three-stage journey usable end-to-end and
prevents someone from unknowingly relying on an unfinished setup.

**Independent Test**: Can be tested by putting the device into each combination of stage
states (none done, only A done, A+B done, all done, a stage marked "Needs redo") and confirming
the readiness summary and per-stage statuses reflect that state correctly, independent of actually
running a calibration.

**Acceptance Scenarios**:

1. **Given** no stage has ever been completed, **When** the user opens the Calibration tab,
   **Then** the readiness summary reads as not calibrated / heading not sent, and a welcome banner
   points to Stage A.
2. **Given** only Stage A is done, **When** the user opens the tab, **Then** the readiness summary
   indicates a usable-but-incomplete state and recommends the next stage, and the UI explains what
   is lost by not completing Stages B and C.
3. **Given** all three stages are done, **When** the user opens the tab, **Then** the readiness
   summary reads as ready to navigate, and each stage shows "Done" with its saved quality and date.
4. **Given** a completed stage, **When** the user chooses Recalibrate, **Then** only that stage's
   guide and procedure start, leaving the other stages' saved results untouched until that stage
   also completes.
5. **Given** a completed stage, **When** the user chooses Reset, **Then** the UI requires an
   explicit confirmation before clearing that stage's saved result and returning its status to
   "Not done".
6. **Given** Stage A is persisted as "Done" but its live sensor accuracy has since dropped below
   the FR-014 threshold during normal operation, **When** the user opens the Calibration tab,
   **Then** Stage A's stage status still reads "Done" with its original saved quality and date,
   while the overall readiness summary separately and immediately shows that heading is currently
   unavailable.
7. **Given** any stage is currently in progress, **When** the user tries to start a different
   stage (including via a second browser tab or a double press of Start), **Then** the second
   attempt is refused or queued behind the first, and only one calibration procedure ever runs at a
   time.
8. **Given** a stage's stored calibration data is found corrupted or written by an incompatible
   firmware version, **When** the device boots, **Then** only that stage resets to "Not done" with
   safe defaults, any other stage whose stored data still validates is left untouched, and the
   reset is recorded in the serial log and reflected in that stage's status.

---

### User Story 3 - Guided installation alignment on the boat (Stage B) (Priority: P2)

After permanently mounting the device on the boat, the owner uses the Calibration tab at the dock
or on calm water to zero the level reference and correct for the device not being perfectly
aligned with the bow, using either a known landmark bearing or the boat's own GPS course.

**Why this priority**: Stage B corrects installation-specific error that a bench calibration
cannot fix, and is the next logical step after Stage A, but it requires being on the boat and is
not needed to validate Stage A in isolation.

**Independent Test**: Can be tested at the dock with the boat at rest for the level step, and
either with a known bearing (no bus dependency) or, when a course-over-ground source is present on
the NMEA 2000 bus, by running the boat in a straight line and confirming the offset is computed and
previewed correctly before being accepted.

**Acceptance Scenarios**:

1. **Given** the boat is at rest and sitting normally in the water, **When** the user presses Set
   level, **Then** the device stores the current pitch and roll as the new zero reference.
2. **Given** the user knows the true bearing of a landmark or dock line the bow is pointed at,
   **When** they choose Known bearing and enter that value, **Then** the device converts it to
   magnetic using the available variation source and computes a mounting offset from the
   difference between that converted value and the device's current magnetic heading.
3. **Given** a course-over-ground source is present on the NMEA 2000 bus, **When** the user chooses
   GPS course and runs the boat in a straight line at steady speed, **Then** Align to GPS becomes
   usable, samples are accepted only while speed is above the minimum and course is steady, and the
   UI explains any waiting condition (e.g., "Speed too low", "Course not steady").
4. **Given** no course-over-ground source is detected on the bus, **When** the user opens Stage B,
   **Then** the GPS course method is not offered, and only Known bearing is available.
5. **Given** a mounting offset has been computed, **When** it is shown to the user, **Then** the UI
   also previews how the currently displayed heading would change if the offset is accepted, before
   the user accepts or discards it.
6. **Given** the user accepts the offset, **When** the change is saved, **Then** it persists across
   reboots and is applied to all subsequent heading, pitch, and roll outputs.

---

### User Story 4 - Guided compass swing on the water (Stage C) (Priority: P3)

Once installed and aligned, the owner takes the boat to open, calm water with the engine and
normal underway electronics on, and follows the Calibration tab's guide to turn through at least
two slow circles while the device measures and corrects the boat's own magnetic deviation at every
heading.

**Why this priority**: Stage C delivers the highest-value accuracy correction (the boat's own
deviation) but is the most demanding stage — it requires open water, a working GPS course
reference, and a completed Stage B — so it is the last stage in the natural sequence and the
most complex to implement and verify.

**Independent Test**: Can be tested end-to-end on synthetic/injected heading and GPS course data
representing a controlled turn (see SC-009), independent of an actual boat, by confirming the
device correctly accepts good samples, rejects bad ones, fits a known deviation curve, and applies
it only after Apply; on-water behavior is confirmed separately via the manual verification
checklist (SC-010).

**Acceptance Scenarios**:

1. **Given** the boat is at a slow, steady speed in open calm water, **When** the user presses
   Start swing, **Then** the device begins recording paired heading/course samples and the UI shows
   a circular progress ring.
2. **Given** the swing is in progress, **When** the boat completes turning through a heading
   sector, **Then** that sector of the progress ring fills in, and the UI warns the user if the
   turn rate is too fast.
3. **Given** speed is too low, the turn rate is too high or uneven, or heading and course disagree
   in a way suggesting current or drift, **When** samples are collected under those conditions,
   **Then** those samples are rejected and not used in the fit.
4. **Given** at least two full circles have been completed with sufficient sector coverage,
   **When** the device fits the deviation curve, **Then** it shows the resulting curve or table,
   the maximum deviation found, and the remaining error after correction.
5. **Given** sector coverage is incomplete or the fit error is too high, **When** the device
   evaluates the collected samples, **Then** the swing is rejected with an explanation and no
   correction is applied.
6. **Given** a computed deviation result is shown, **When** the user presses Apply, **Then** the
   new correction is saved, persists across reboots and firmware updates, and takes effect for all
   subsequent heading output; **When** the user presses Discard instead, **Then** the previous
   correction remains in effect and nothing is saved.
7. **Given** the swing is still in progress and not yet applied, **When** the device transmits
   heading to the chartplotter during that time, **Then** it uses the existing (previous)
   correction, not the in-progress result.
8. **Given** no course-over-ground source is available on the bus, **When** the user opens Stage C,
   **Then** a manual 8-point swing is offered as a fallback, letting the user stop on each of the 8
   compass points and enter a trusted reference value.
9. **Given** Stage A's saved quality later degrades, **When** the user views calibration status,
   **Then** the UI notes that the Stage C result may also need to be redone.

---

### User Story 5 - Change the Wi-Fi network name from the Settings tab (Priority: P2)

The owner opens the Settings tab to rename the device's Wi-Fi network to something they'll
recognize, understanding beforehand that their browser will disconnect and what network to
reconnect to.

**Why this priority**: This is a small, self-contained convenience feature independent of the
calibration journey, valuable on its own, but lower stakes than getting the heading itself right.

**Independent Test**: Can be fully tested by changing the SSID and confirming it persists across a
reboot and that NMEA 2000 heading output is never interrupted, independent of any calibration
state.

**Acceptance Scenarios**:

1. **Given** the Settings tab is open, **When** the user views it, **Then** the current SSID is
   shown.
2. **Given** the user enters a new SSID between 1 and 32 characters with no leading or trailing
   whitespace, **When** they save it, **Then** the UI warns that the connection will drop and shows
   the new network name to reconnect to, before the change is applied.
3. **Given** the user enters an empty SSID, one longer than 32 characters, or one with leading or
   trailing whitespace, **When** they try to save it, **Then** the UI shows a clear error and
   nothing is saved.
4. **Given** the SSID change is applied, **When** the device restarts its Wi-Fi with the new name,
   **Then** NMEA 2000 heading output continues without interruption.
5. **Given** the device has been rebooted after an SSID change, **When** it starts up, **Then** it
   broadcasts the new SSID, not the previous one.
6. **Given** the user cannot reconnect after a change (e.g., forgotten SSID), **When** they follow
   the documented recovery path, **Then** the device's network settings are restored to their
   defaults.

---

### User Story 6 - Trustworthy heading at all times (Priority: P1)

A user watching the chartplotter or the compass's own UI can always trust that any heading shown
or transmitted is current and accurate, and is told clearly on the UI whenever heading is being
withheld because it cannot be trusted yet.

**Why this priority**: This is the direct expression of the project's non-negotiable Navigation
Data Integrity principle and underlies every other story — a beautifully guided calibration
journey is worthless if the device can still silently output stale or untrustworthy heading.

**Independent Test**: Can be tested independently of any specific calibration stage by forcing
low sensor accuracy, disconnecting the sensor, and swinging the heading through 0°/360° while
observing serial logs, the status data endpoint, and the UI, without needing a boat.

**Acceptance Scenarios**:

1. **Given** calibration quality is below the required threshold, **When** the device would
   otherwise transmit heading, **Then** it instead sends the NMEA 2000 "data not available" value
   or stops sending the heading PGN entirely, and the UI explains why heading is unavailable.
2. **Given** the sensor disconnects or fails while previously reporting valid heading, **When**
   the failure is detected, **Then** the device stops sending heading (or sends "data not
   available") rather than repeating the last known value.
3. **Given** a previously saved, valid Stage A calibration exists, **When** the user starts a new
   Stage A recalibration attempt, **Then** the device keeps transmitting heading computed from the
   previously saved calibration, not from the in-progress attempt, until the new result is saved,
   cancelled, or the attempt times out.
4. **Given** the true heading crosses 0°/360° (north), **When** heading is read from the UI or the
   bus, **Then** the values remain continuous and correctly normalized to [0°, 360°) with no jump
   or sign error.
5. **Given** the device is stationary, **When** heading is observed over time, **Then** it stays
   smooth without perceptible jitter; **Given** the device is turning, **When** heading is
   observed, **Then** it tracks the turn without noticeable lag.
6. **Given** any heading value is shown in the UI, **When** the same instant's value is checked on
   the NMEA 2000 bus, **Then** the two match.

---

### Edge Cases

- What happens when Stage B or Stage C is opened but no GPS course-over-ground source is ever
  detected on the NMEA 2000 bus?
- How does the system handle a compass swing where boat speed drops too low, the turn rate becomes
  too fast or uneven, or the two circles are never completed?
- How does the system handle strong current or wind causing GPS course over ground to disagree
  with the boat's actual heading during Stage B or Stage C?
- What happens when a user runs Stage C without having completed Stage A or Stage B first?
- How does the system respond to a magnetic disturbance appearing mid-procedure during any stage,
  or during ordinary (non-calibration) operation?
- How does the system handle the sensor disconnecting or resetting while a calibration procedure
  is in progress?
- What happens if power is lost at the exact moment a calibration result is being saved?
- Stored calibration data found corrupted or written by an incompatible earlier firmware version:
  the affected stage(s) reset to "Not done" on boot; any other stage whose data still validates
  correctly is left untouched (see FR-045/FR-046).
- What happens when two clients (e.g., two phones) have the Calibration or Settings tab open at
  the same time, including both attempting to start a calibration or save a setting?

## Requirements *(mandatory)*

### Functional Requirements

**Calibration journey & readiness**

- **FR-001**: The Calibration tab MUST show an overall readiness summary derived from the status
  of all three stages (e.g., not calibrated/heading not sent, usable/one or more stages
  recommended, ready to navigate). This summary MUST also reflect, in real time and independently
  of any stage's persisted status, whenever heading currently has a quality warning per FR-041
  (e.g., due to a transient live accuracy drop or the sensor being disconnected), even while the
  underlying stage still reads "Done".
- **FR-002**: Each stage MUST show its own persisted status as one of: Not done, In progress, Done
  (with saved quality and date), or Needs redo. Needs redo MUST result only from an explicit user
  reset (FR-005); the system MUST NOT automatically change a stage's persisted status because of a
  transient drop in live sensor accuracy during normal operation.
- **FR-003**: On first boot, or whenever no stage has ever been completed, the UI MUST show a
  welcome banner explaining the three stages and directing the user to Stage A.
- **FR-004**: Stages MUST be startable in any order, and the UI MUST recommend the A → B → C order
  and explain what is lost by skipping a stage.
- **FR-005**: Each stage MUST be independently re-runnable (Recalibrate) without affecting the
  saved results of the other stages, and independently resettable to its default (not-done) state
  behind an explicit confirmation step.
- **FR-006**: The system MUST allow only one calibration procedure to run at a time across all
  clients and entry points, including when triggered twice in quick succession or from two
  separate browser sessions.
- **FR-007**: Closing the browser during an in-progress calibration MUST NOT corrupt device state;
  the procedure MUST either remain resumable/viewable from another browser session or safely time
  out while retaining the previous saved result.

**Built-in guides**

- **FR-008**: Every stage MUST present a guide, before the procedure can be started, containing:
  what the stage does and why; a before-you-start checklist of conditions to confirm; numbered,
  illustrated step-by-step instructions; an expected duration; a description of what success looks
  like; and common failure causes with suggested remedies.
- **FR-009**: The guide MUST remain accessible during the procedure, with the current step
  highlighted, and MUST advance automatically as the device detects progress, without requiring the
  user to manually mark steps complete.
- **FR-010**: Guide illustrations MUST be available without any internet connection (no
  externally-hosted content).
- **FR-011**: Guide content MUST be maintainable (correctable/rewritable) independently of the
  underlying calibration detection and procedure logic.

**Stage A — sensor calibration**

- **FR-012**: The system MUST display live quality for the magnetometer, accelerometer, and
  gyroscope during Stage A, each expressed using the sensor's four accuracy levels (unreliable,
  low, medium, high).
- **FR-013**: The system MUST automatically detect and advance through Stage A's sub-steps
  (stillness for gyroscope, per-position coverage of the six rest positions for accelerometer,
  rotation coverage for magnetometer) without requiring the user to confirm each step manually.
- **FR-014**: The system MUST save a new Stage A result only once the magnetometer, accelerometer,
  and gyroscope have all reached "High" accuracy, and MUST NOT overwrite an existing saved
  calibration with a result of lower quality.
- **FR-015**: The system MUST apply a timeout to Stage A; on expiry it MUST explain a likely cause
  (e.g., nearby interference, motion too fast) and offer a retry, while leaving the previously
  saved calibration in effect.
- **FR-016**: The user MUST be able to cancel Stage A at any time, leaving the previously saved
  calibration in effect.

**Stage B — installation alignment**

- **FR-017**: The user MUST be able to capture the current pitch and roll as the zero level
  reference while the boat is at rest.
- **FR-018**: The user MUST be able to compute a mounting heading offset either by entering a known
  true bearing (e.g., read from a chart), or, when available, by using the boat's GPS course over
  ground. A true bearing entered this way MUST be converted to magnetic, using the same
  magnetic-variation source as FR-024 (NMEA 2000 bus when available, with manual entry as
  fallback), before it is compared against the device's own magnetic heading reading.
- **FR-019**: The GPS course method MUST be offered only when a course-over-ground source is
  detected on the NMEA 2000 bus, MUST use samples only while boat speed is above a minimum and
  course is steady, and MUST tell the user why it is waiting when those conditions are not met.
- **FR-020**: Before the user accepts a computed mounting offset, the UI MUST preview how the
  currently displayed heading would change if that offset is accepted.
- **FR-021**: The user MUST be able to accept or discard a computed level reference or mounting
  offset; accepted values MUST persist across reboots and MUST apply to all subsequent heading,
  pitch, and roll output.

**Stage C — compass swing**

- **FR-022**: The user MUST be able to start a compass swing procedure and see live progress as a
  representation of which heading sectors have been sufficiently covered across at least two full
  turns.
- **FR-023**: The system MUST warn the user when the boat is turning faster than the procedure can
  reliably use.
- **FR-024**: The system MUST record paired samples of its own magnetic heading and a
  GPS-course-over-ground-derived reference heading, corrected for magnetic variation read
  automatically from the NMEA 2000 bus when available, with manual user entry offered as a
  fallback when no bus-provided variation is present, while the boat is turning.
- **FR-025**: The system MUST reject samples taken while speed is too low, turn rate is too high or
  uneven, or heading and course disagree in a way suggesting current or drift, and MUST exclude
  rejected samples from the fitted result.
- **FR-026**: From the accepted samples, the system MUST fit a deviation correction covering the
  full 0–360° heading range (a constant term plus once-per-turn and twice-per-turn components) and
  MUST reject the swing, with an explanation, if heading-sector coverage is incomplete or the fit
  error is too high.
- **FR-027**: Before applying a computed deviation result, the system MUST show the resulting
  deviation curve or table, the maximum deviation found, and the remaining error expected after
  correction.
- **FR-028**: The user MUST be able to Apply or Discard a computed deviation result; the new
  correction MUST take effect only after Apply, and Discard MUST leave the previous correction (if
  any) unchanged.
- **FR-029**: While a swing is in progress and not yet applied, the system MUST continue
  transmitting heading using the existing (previous) correction.
- **FR-030**: The system MUST offer a manual 8-point swing (stopping on N, NE, E, SE, S, SW, W, NW
  and accepting a user-entered reference value at each) as a fallback when no GPS course-over-ground
  source is available.
- **FR-031**: An applied deviation correction MUST persist across reboots and firmware updates, and
  MUST remain viewable, re-runnable, and resettable at any time.
- **FR-032**: When the user starts a new Stage A calibration attempt or resets Stage A, the system
  MUST indicate to the user that the Stage C result may also need to be redone, since Stage A's
  outcome affects Stage C's correction.

**Observability**

- **FR-033**: All calibration state — current stage statuses, in-progress step, and saved results —
  MUST be exposed through machine-readable status data and structured serial log lines sufficient
  to verify calibration behavior without using the web UI.

**Settings**

- **FR-034**: The Settings tab MUST display the device's current Wi-Fi network name (SSID) and
  allow the user to change it.
- **FR-035**: The system MUST validate a new SSID (1–32 characters, no leading or trailing
  whitespace) before saving; on invalid input it MUST show a clear error and MUST NOT save
  anything.
- **FR-036**: Before applying a validated SSID change, the system MUST warn the user that the
  connection will drop and MUST show the network name to reconnect to.
- **FR-037**: A changed SSID MUST persist across reboots, and applying the change MUST NOT
  interrupt NMEA 2000 compass output.
- **FR-038**: A documented recovery path MUST exist to restore the device's default network
  settings.
- **FR-039**: The Settings tab's layout MUST accommodate additional settings being added later
  without a redesign of the existing ones.

**Heading integrity**

- **FR-040**: The system MUST produce heading that remains accurate and correctly normalized to
  [0°, 360°) across the full range, including continuity across the north crossing.
- **FR-041**: Whenever the currently saved calibration's accuracy falls below the FR-014 threshold
  (magnetometer, accelerometer, and gyroscope all "High"), the system MUST still compute, transmit,
  and display its best-available heading, but MUST flag it as low-confidence everywhere it is
  surfaced: the UI MUST show why accuracy is currently low alongside the numeric heading value, and
  the NMEA 2000/serial diagnostics MUST record the accuracy reason. The system MUST send the NMEA
  2000 "data not available" value or stop sending the heading PGN — and the UI MUST show heading as
  unavailable — only when the sensor is disconnected or has stopped reporting entirely (no data
  exists to give). While a new Stage A attempt is in progress, the device MUST continue transmitting
  heading using the previously saved calibration (not the in-progress attempt) until the new result
  is saved, cancelled, or the attempt times out.
- **FR-042**: The heading value shown in the UI MUST match the heading value transmitted on the
  NMEA 2000 bus at the same instant.
- **FR-043**: Heading output MUST remain smooth while stationary and MUST track turns without
  noticeable lag.
- **FR-044**: The system MUST apply heading corrections in this order: sensor calibration → level
  reference and mounting offset → deviation correction → normalization.

**Data integrity & recovery**

- **FR-045**: On boot, the system MUST validate each stage's stored calibration data; any stage
  whose data fails validation (corrupted, or written in a format incompatible with the running
  firmware) MUST be reset to "Not done" with safe defaults, while any other stage whose stored data
  validates successfully MUST be left untouched.
- **FR-046**: A stage reset due to failed data validation MUST be recorded in the structured serial
  log and reflected in the stage's status/readiness summary so the user understands why that stage
  needs to be redone.

**Concurrency**

- **FR-047**: When two clients attempt to save a Settings change (e.g., SSID) at nearly the same
  time, the system MUST resolve this with last-write-wins: the most recently received valid save
  completes and persists, and a client whose save was superseded MUST see the current, now-saved
  value the next time it loads Settings.

### Key Entities

- **Stage Status**: Per-stage (A, B, C) record of persisted state (Not done / In progress / Done /
  Needs redo), saved quality, and the date it was last completed; drives the overall readiness
  summary. Persisted state changes only through an explicit user action (completing, recalibrating,
  or resetting a stage) — it is distinct from the live, real-time trust indicator described in
  FR-001, which can show heading as currently unavailable without altering this persisted state.
- **Sensor Calibration Profile**: The Stage A result — the saved magnetometer/accelerometer/
  gyroscope calibration and its quality, applied to all subsequent orientation readings.
- **Installation Alignment**: The Stage B result — the zero level reference (pitch/roll) and the
  mounting heading offset, and the method (known bearing or GPS course) used to derive it.
- **Deviation Correction**: The Stage C result — the fitted deviation curve/coefficients across
  0–360°, the maximum deviation found, the residual error after correction, and whether it came
  from a GPS-referenced or manual swing.
- **Calibration Session**: The single active in-progress procedure (which stage, current step,
  collected samples, elapsed/remaining time), enforced to be unique across all clients.
- **Network Settings**: The device's current and pending Wi-Fi SSID and the state of an in-progress
  change.
- **Heading Reading**: The live, corrected heading (and pitch/roll/rate of turn) value shared
  identically between the UI and the NMEA 2000 output at any instant.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: After Stage A, static heading error is ≤ 2° at 8 evenly spaced headings on a level
  bench away from metal.
- **SC-002**: With pitch or roll up to ±15°, heading error stays ≤ 3°.
- **SC-003**: When stationary, heading noise is ≤ 0.5° peak-to-peak over 30 seconds.
- **SC-004**: A first-time user following only the on-screen guide completes Stage A within 3
  minutes.
- **SC-005**: After Stage C, the remaining heading error on the boat is ≤ 2° across all headings.
- **SC-006**: The saved calibration is restored and reports usable accuracy within 10 seconds of
  boot.
- **SC-007**: No heading is sent while the sensor is disconnected or not reporting; whenever
  calibration quality is below the configured threshold but the sensor is still reporting, the
  heading sent/shown is flagged as low-confidence rather than withheld. Both are verifiable from
  serial logs and status data alone.
- **SC-008**: An SSID change takes effect within 15 seconds, with no gap in heading output.
- **SC-009**: The Stage C deviation-fitting algorithm is verified against synthetic swing data with
  known deviation, including noisy, missing-sector, and bad-sample scenarios, without requiring an
  actual boat.
- **SC-010**: Physical heading accuracy (SC-001, SC-002, SC-005), what the chartplotter itself
  displays, and the on-water portions of Stages B and C are confirmed through an explicit manual
  verification checklist, since these cannot be observed directly through automated means.

## Assumptions

- The Wi-Fi password is not changeable as part of this feature; only the SSID is in scope, per
  Feature 2 as described. Password management may be addressed in a future feature.
- The web UI is English-only for this feature; additional languages are out of scope unless
  requested later.
- Stage C (compass swing) is specified within this same feature set rather than as a separate spec,
  since it is one stage of a single three-stage calibration journey described as one feature by the
  requester.
- Exact numeric thresholds not given by the requester (minimum speed and course-steadiness bounds
  for Stage B's GPS alignment, minimum/maximum turn rate bounds for Stage C, calibration timeout
  durations) are treated as tunable parameters to be defined during planning and validated against
  the Success Criteria above, rather than fixed in this specification.
- "Course-over-ground source detected on the bus" means the device has recently received a valid
  NMEA 2000 message containing course over ground and speed; the exact PGN(s) consulted are a
  planning-level detail.
- The three-level readiness summary wording ("Ready to navigate", "Usable – compass swing
  recommended", "Not calibrated – heading not sent") given by the requester is illustrative; final
  wording may be refined during planning as long as the same three readiness states are conveyed.
- Concurrent Settings edits (FR-047) are resolved with simple last-write-wins, since SSID is a
  single low-stakes value; no locking or merge behavior is required, unlike the single-active-
  session rule that calibration requires (FR-006).
