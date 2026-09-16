#pragma once

#include <cstdint>

#include "drivers/clock/clock.h"

// The CalibrationSession in-memory model (data-model.md §2.1): enforces a
// single active calibration procedure across all clients (FR-006), with
// CALIBRATION_SESSION_INACTIVITY_TIMEOUT_S auto-cancel on client inactivity
// (FR-007). Pure logic driven through the Clock interface -- native
// testable; logging (`[CAL] busy ...`) is the caller's responsibility (the
// command-processing integration point, not this class) so this stays
// dependency-free.
//
// Per-stage procedure logic (state machines, progress, candidate results)
// lives in src/calibration/stage_{a,b,c}.h once those stages exist; this
// class only enforces the singleton/timeout rule shared by all three.
class CalibrationService
{
public:
    enum class Stage
    {
        kNone,
        kA,
        kB,
        kC,
    };

    explicit CalibrationService(Clock &clock) : clock_(clock) {}

    // Starts `stage` if no other procedure is active. Returns true on
    // success. On failure (another stage already active), returns false and
    // sets `busy_stage_out` to that stage.
    bool tryStart(Stage stage, Stage &busy_stage_out);

    // No-op-safe: cancelling a stage that isn't the active one does nothing.
    void cancel(Stage stage);

    // Ends whichever stage is currently active (used when a stage reaches a
    // terminal state: Done/TimedOut/Saved/Rejected/Discarded).
    void endActive();

    void noteClientActivity();

    // Auto-cancels the active session if CALIBRATION_SESSION_INACTIVITY_TIMEOUT_S
    // has elapsed since the last client activity. Call periodically from
    // AppTask. Returns the stage that was auto-cancelled, or kNone if
    // nothing timed out.
    Stage checkInactivityTimeout();

    Stage activeStage() const { return active_stage_; }

private:
    Clock &clock_;
    Stage active_stage_ = Stage::kNone;
    uint32_t last_client_activity_ms_ = 0;
};
