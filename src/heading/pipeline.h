#pragma once

#include "deviation_curve.h"
#include "quality_gate.h"
#include "quaternion.h"

// The fixed-order heading pipeline (data-model.md §6, FR-044): raw rotation
// vector -> level_reference_quat -> mounting_offset_rad -> DeviationCorrection
// curve -> normalize -> quality gate -> HeadingReading snapshot. Identity/zero
// corrections are used when Stage B/C are NOT_DONE. Pure logic,
// native-testable.
namespace heading
{

struct LevelReference
{
    bool has_level = false;
    Quaternion level_reference_quat;  // identity when !has_level
};

struct MountingOffset
{
    bool has_offset = false;
    float mounting_offset_rad = 0.0f;  // 0 when !has_offset
};

struct DeviationCorrectionInput
{
    bool has_correction = false;
    DeviationCoefficients coefficients{};  // zero curve when !has_correction
};

struct HeadingReading
{
    float heading_rad = 0.0f;  // [0, 2*pi)
    float pitch_rad = 0.0f;
    float roll_rad = 0.0f;
    float rate_of_turn_rad_s = 0.0f;
    bool valid = false;
    InvalidReason reason_if_invalid = InvalidReason::kSensorNotCalibrated;

    // Heading after level reference + mounting offset but BEFORE deviation
    // correction (the "compass heading" a DeviationCorrection curve is
    // fitted against/evaluated at -- see pipeline.cpp step 3). Stage C's
    // swing/manual-point flow needs this exact value as its raw input so it
    // measures deviation relative to the correction it's replacing, not a
    // heading that's already been deviation-corrected.
    float compass_heading_rad = 0.0f;  // [0, 2*pi)
};

struct PipelineInput
{
    Quaternion raw_quat;            // from ImuDriver rotation vector
    float raw_gyro_z_rad_s = 0.0f;  // yaw rate, for rate_of_turn_rad_s

    bool sensor_connected = true;
    SensorAccuracySnapshot live_accuracy;
    ActiveCalibrationStage active_stage = ActiveCalibrationStage::kNone;
    SavedCalibrationAccuracy saved_profile;

    LevelReference level_reference;
    MountingOffset mounting_offset;
    DeviationCorrectionInput deviation_correction;
};

HeadingReading computeHeadingReading(const PipelineInput &input);

}  // namespace heading
