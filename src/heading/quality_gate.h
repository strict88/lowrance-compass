#pragma once

#include <cstdint>

// The HEADING_MIN_ACCURACY gate (FR-014): "magnetometer, accelerometer,
// and gyroscope have all reached 'High' accuracy". Pure logic, native-testable.
//
// `valid` reflects only whether a heading number exists to give at all
// (false exclusively for a disconnected sensor). Accuracy below the FR-014
// bar does NOT clear `valid` -- it's surfaced instead via `reason`, which is
// populated whenever there's a quality issue to warn about (kSensorAccuracyLow
// / kSensorNotCalibrated) even while `valid` is true, so callers can still
// display/transmit the heading while flagging it as not yet fully trustworthy.
namespace heading
{

// SH-2 accuracy enum: 0=Unreliable, 1=Low, 2=Medium, 3=High.
constexpr uint8_t kHeadingMinAccuracy = 3;

enum class InvalidReason
{
    kNone,
    kSensorNotCalibrated,
    kSensorAccuracyLow,
    kSensorDisconnected,
};

struct SensorAccuracySnapshot
{
    uint8_t mag_accuracy = 0;
    uint8_t accel_accuracy = 0;
    uint8_t gyro_accuracy = 0;
};

// The persisted SensorCalibrationProfile's accuracy fields (data-model.md
// §1.1), if one exists. `exists=false` means Stage A has never successfully
// completed.
struct SavedCalibrationAccuracy
{
    bool exists = false;
    uint8_t mag_accuracy = 0;
    uint8_t accel_accuracy = 0;
    uint8_t gyro_accuracy = 0;
};

enum class ActiveCalibrationStage
{
    kNone,
    kA,
    kB,
    kC,
};

struct QualityGateResult
{
    bool valid = false;
    InvalidReason reason = InvalidReason::kSensorNotCalibrated;
};

// `sensor_connected=false` always yields kSensorDisconnected (`valid=false`),
// taking precedence over everything else. Otherwise `valid` is always true,
// and `reason` reports the accuracy state: while `active_stage == kA` and
// `saved_profile.exists`, that reason is evaluated against `saved_profile`
// instead of `live_accuracy` (the "Stage A in-progress freeze", data-model.md
// §6) so an in-progress recalibration attempt never surfaces a fresh
// low-accuracy warning just because live accuracy is transiently low
// mid-attempt.
QualityGateResult evaluate(bool sensor_connected, const SensorAccuracySnapshot &live_accuracy,
                            ActiveCalibrationStage active_stage, const SavedCalibrationAccuracy &saved_profile);

}  // namespace heading
