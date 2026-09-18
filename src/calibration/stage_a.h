#pragma once

#include <cstdint>

#include "drivers/kv_store/kv_store.h"
#include "drivers/kv_store/record_envelope.h"
#include "heading/quaternion.h"
#include "thresholds.h"

// Stage A state machine (data-model.md §3.1):
//
//   Idle -Start-> AwaitingStillness -gyro settles-> AwaitingPositions
//   AwaitingPositions -all 6 positions held-> AwaitingRotation
//   AwaitingRotation -coverage reached-> Evaluating
//   Evaluating -all High-> Done       Evaluating -timeout-> TimedOut
//   (any state) -Cancel-> Cancelled
//
// Pure logic, native-testable, driven entirely by fed-in IMU samples and a
// monotonic clock value -- no direct ImuDriver/Clock dependency.
namespace calibration
{

enum class StageAState
{
    kIdle,
    kAwaitingStillness,
    kAwaitingPositions,
    kAwaitingRotation,
    kEvaluating,
    kDone,
    kTimedOut,
    kCancelled,
};

// Which of the six body-frame half-axes is currently closest to the gravity
// direction (data-model.md's six rest positions), in a fixed, arbitrary but
// consistent order.
enum class RestPosition : uint8_t
{
    kPosX = 0,
    kNegX,
    kPosY,
    kNegY,
    kPosZ,
    kNegZ,
};
constexpr int kRestPositionCount = 6;

struct StageAProgress
{
    uint8_t mag_accuracy = 0;
    uint8_t accel_accuracy = 0;
    uint8_t gyro_accuracy = 0;
    bool positions_done[kRestPositionCount] = {false, false, false, false, false, false};
    float rotation_coverage_fraction = 0.0f;  // [0, 1]
};

// One fed-in IMU sample: the raw rotation vector (for gravity direction and
// rotation coverage), raw gyro (for the stillness check), per-sensor
// accuracy, and the elapsed time since the previous sample.
struct StageASample
{
    heading::Quaternion raw_quat;
    float gyro_x_rad_s = 0.0f;
    float gyro_y_rad_s = 0.0f;
    float gyro_z_rad_s = 0.0f;
    uint8_t mag_accuracy = 0;
    uint8_t accel_accuracy = 0;
    uint8_t gyro_accuracy = 0;
    float dt_s = 0.0f;
};

// The ESP32-side metadata that drives Stage A's status display
// (data-model.md §1.1) -- the BNO08x's own Dynamic Calibration Data persists
// separately, inside the sensor's own on-chip flash via the SH-2 "save DCD"
// command (ImuDriver::saveDcd()), not here.
struct SensorCalibrationProfile
{
    char saved_at_iso8601[32] = {0};
    uint8_t mag_accuracy = 0;
    uint8_t accel_accuracy = 0;
    uint8_t gyro_accuracy = 0;
    char firmware_version[16] = {0};
};

constexpr uint8_t kSensorCalibrationProfileSchemaVersion = 1;

// Saves `profile` via the T012 record envelope (write-then-verify-then-
// commit). Refuses (returns false, leaving any previous record untouched)
// unless mag/accel/gyro are all High (3) -- FR-014's "MUST NOT overwrite an
// existing saved calibration with a result of lower quality" held as an
// invariant here too, not just by StageA's own Done-reachability.
bool saveSensorCalibrationProfile(KeyValueStore &store, const SensorCalibrationProfile &profile);

// Loads the persisted profile. Returns true (with `profile_out` filled) only
// on Status::kOk; a schema/CRC mismatch or absent record both mean "treat as
// not done" per data-model.md §5 and return false, leaving `profile_out`
// untouched.
// `status_out`, when non-null, receives the underlying record_envelope::Status
// (kOk/kAbsent/kCrcMismatch/kSchemaMismatch) so a hardware-only caller can
// distinguish "never saved" from "corrupted" and reset+log the latter
// (FR-045/046) -- this function itself never mutates storage.
bool loadSensorCalibrationProfile(KeyValueStore &store, SensorCalibrationProfile &profile_out,
                                   record_envelope::Status *status_out = nullptr);

class StageA
{
public:
    void start(float now_s);
    void cancel();

    // Advances the state machine with one new sample. `now_s` is monotonic
    // seconds (matches `start()`'s clock).
    void update(const StageASample &sample, float now_s);

    StageAState state() const { return state_; }
    const StageAProgress &progress() const { return progress_; }

private:
    void updateStillness(const StageASample &sample);
    void updatePositions(const StageASample &sample);
    void updateRotationCoverage(const StageASample &sample);
    void evaluate(float now_s);

    StageAState state_ = StageAState::kIdle;
    StageAProgress progress_;

    float started_at_s_ = 0.0f;
    float stillness_held_s_ = 0.0f;

    int position_current_index_ = -1;
    float position_current_held_s_ = 0.0f;

    bool rotation_bins_visited_[thresholds::kStageARotationCoverageBins] = {false};
};

}  // namespace calibration
