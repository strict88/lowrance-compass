#include "quality_gate.h"

namespace heading
{

QualityGateResult evaluate(bool sensor_connected, const SensorAccuracySnapshot &live_accuracy,
                            ActiveCalibrationStage active_stage, const SavedCalibrationAccuracy &saved_profile)
{
    if (!sensor_connected)
    {
        return {false, InvalidReason::kSensorDisconnected};
    }

    SensorAccuracySnapshot effective = live_accuracy;
    if (active_stage == ActiveCalibrationStage::kA && saved_profile.exists)
    {
        // Stage A in-progress freeze (FR-041, data-model.md §6): gate on the
        // previously saved profile's accuracy, not live accuracy, so an
        // in-progress recalibration attempt never trips heading invalid.
        effective.mag_accuracy = saved_profile.mag_accuracy;
        effective.accel_accuracy = saved_profile.accel_accuracy;
        effective.gyro_accuracy = saved_profile.gyro_accuracy;
    }

    bool all_high = effective.mag_accuracy >= kHeadingMinAccuracy && effective.accel_accuracy >= kHeadingMinAccuracy &&
                     effective.gyro_accuracy >= kHeadingMinAccuracy;
    if (all_high)
    {
        return {true, InvalidReason::kNone};
    }

    // Low accuracy is surfaced as a warning (`reason`), not a transmit/display
    // block: the pipeline still has a genuine heading number to give, just
    // one that hasn't reached the FR-014 accuracy bar yet. Only a
    // disconnected sensor (handled above) has no data at all to give.
    InvalidReason reason = saved_profile.exists ? InvalidReason::kSensorAccuracyLow : InvalidReason::kSensorNotCalibrated;
    return {true, reason};
}

}  // namespace heading
