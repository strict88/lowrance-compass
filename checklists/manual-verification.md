# Manual Verification Checklist

Covers what the agent cannot observe directly (physical accuracy, chartplotter display, on-water
procedures) per constitution Principle IV — these MUST be confirmed by a human with real hardware
on a real boat and MUST NOT be assumed passing. Check each item off only after actually observing
it; note the date, device firmware version, and observer.

## Physical heading accuracy

- [ ] **SC-001** — After Stage A completes (bench, away from metal, level surface), static heading
  error is ≤ 2° at each of 8 evenly spaced true/reference headings (0°, 45°, 90°, ..., 315°).
  Compare the device's displayed/transmitted heading against a known-good reference compass or
  bearing at each heading.
- [ ] **SC-002** — With the device tilted to pitch or roll up to ±15° (in any combination), heading
  error stays ≤ 3° at a representative set of headings.
- [ ] **SC-005** — After Stage C (compass swing) completes on the boat, the remaining heading error
  is ≤ 2° across all headings, checked against a known-good reference while underway or at each of
  several headings at the dock.

## Chartplotter / bus display

- [ ] The Lowrance chartplotter (or another NMEA 2000 display) shows a heading source from this
  device and it matches the device's own UI/serial-reported heading at the same instant (FR-042).
- [ ] Rate of turn (PGN 127251) and attitude/pitch-roll (PGN 127257), if displayed by the
  chartplotter, look physically sensible during a turn and while tilted.
- [ ] When heading is withheld (e.g., forced low accuracy via `tools/hil/quality_gate_check.py`),
  the chartplotter shows "no data"/dashes for this source rather than a frozen or fabricated value.

## On-water procedures

- [ ] **Stage B, GPS-course method**: underway at a safe, steady cruising speed and heading, the
  guide's waiting-reason messaging (`SPEED_TOO_LOW` / `COURSE_NOT_STEADY`) shows and clears
  correctly, and the resulting mounting-offset preview looks physically correct before Accept.
  Also complete once with the **known-bearing** method for comparison.
- [ ] **Stage C, GPS swing**: on open water with room for ≥ 2 full slow circles, the guided swing
  completes with full sector coverage, the too-fast warning appears if the turn is rushed and
  clears once slowed, and the resulting deviation curve/max-deviation/residual look plausible
  before Accept.
- [ ] **Stage C, manual 8-point fallback**: with no GPS course source on the bus, steady the boat
  on each of the 8 compass points and confirm each entered reference bearing is accepted and the
  procedure advances correctly through all 8 points to a fit result.
- [ ] Readiness summary and per-stage status on the Calibration tab match reality throughout an
  actual on-water session (not just on the bench).

## Sunlight / UI readability (constitution Principle VI)

- [ ] The Dashboard's heading/pitch/roll/rate-of-turn numerals are readable in direct sunlight on
  deck, at the layout's default size and contrast.

---

Record results here (date, firmware version, observer, pass/fail/notes) as this checklist is run:

| Date | Firmware | Observer | Item(s) | Result | Notes |
|------|----------|----------|---------|--------|-------|
|      |          |          |         |        |       |
