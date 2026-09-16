#pragma once

#include <cstdint>

// The HEADING_MIN_ACCURACY gate (FR-014/FR-041): "magnetometer, accelerometer,
// and gyroscope have all reached 'High' accuracy". Pure logic, native-testable.
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

// `sensor_connected=false` always yields kSensorDisconnected, taking
// precedence over everything else. Otherwise: while `active_stage == kA` and
// `saved_profile.exists`, the gate is evaluated against `saved_profile`
// instead of `live_accuracy` (the "Stage A in-progress freeze", FR-041,
// data-model.md §6) so an in-progress recalibration attempt never trips
// `valid` to false just because live accuracy is transiently low mid-attempt.
QualityGateResult evaluate(bool sensor_connected, const SensorAccuracySnapshot &live_accuracy,
                            ActiveCalibrationStage active_stage, const SavedCalibrationAccuracy &saved_profile);

}  // namespace heading
