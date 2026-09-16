#pragma once

#include <cstdint>

// Per-sample acceptance (FR-023/FR-025, reference freshness, sensor
// accuracy) and per-sector outlier rejection via median absolute deviation
// (data-model.md's STAGE_C_OUTLIER_MAD_MULTIPLIER). Pure logic,
// native-testable.
namespace calibration
{

enum class SampleRejectReason
{
    kAccepted,
    kTooSlow,
    kTurnTooFastOrUneven,
    kReferenceStale,
    kLowSensorAccuracy,
};

struct SwingSampleInput
{
    float sog_m_s = 0.0f;
    float turn_rate_rad_s = 0.0f;       // current instantaneous turn rate
    float turn_rate_prev_rad_s = 0.0f;  // previous accepted sample's turn rate, for steadiness
    float reference_age_s = 0.0f;       // age of the COG/variation reference this sample used
    uint8_t mag_accuracy = 0;
    uint8_t accel_accuracy = 0;
    uint8_t gyro_accuracy = 0;
};

// Per-sample acceptance gate. Returns kAccepted if the sample should be
// used, else the specific rejection reason (checked in a fixed priority
// order: sensor accuracy, reference freshness, speed, then turn rate).
SampleRejectReason evaluateSample(const SwingSampleInput &input);

constexpr int kMaxOutlierCheckSamples = 64;

// Median-absolute-deviation outlier rejection within one sector's collected
// deviation values. `accepted_out` (same length as `deviations_rad`,
// caller-owned) is filled with which indices to keep. `count` above
// kMaxOutlierCheckSamples is clamped (only the first kMaxOutlierCheckSamples
// participate in the median/MAD calculation; any beyond that are rejected).
void rejectOutliers(const float *deviations_rad, int count, bool *accepted_out);

}  // namespace calibration
