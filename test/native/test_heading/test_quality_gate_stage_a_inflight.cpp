#include <unity.h>

#include "heading/quality_gate.h"

// spec.md: "Given a previously saved, valid Stage A calibration exists, When
// the user starts a new Stage A recalibration attempt, Then the device keeps
// transmitting heading computed from the previously saved calibration."
void test_live_drop_during_stage_a_does_not_invalidate_when_profile_exists(void)
{
    heading::SensorAccuracySnapshot live_now_low{0, 1, 0};  // mid-recalibration, accuracy has dropped
    heading::SavedCalibrationAccuracy saved{true, 3, 3, 3};

    auto result = heading::evaluate(true, live_now_low, heading::ActiveCalibrationStage::kA, saved);

    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_TRUE(result.reason == heading::InvalidReason::kNone);
}

void test_no_freeze_without_a_saved_profile(void)
{
    heading::SensorAccuracySnapshot live_low{0, 0, 0};
    heading::SavedCalibrationAccuracy saved{};  // exists=false: first-ever attempt

    auto result = heading::evaluate(true, live_low, heading::ActiveCalibrationStage::kA, saved);

    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_TRUE(result.reason == heading::InvalidReason::kSensorNotCalibrated);
}

void test_freeze_does_not_apply_to_stage_b(void)
{
    heading::SensorAccuracySnapshot live_low{0, 0, 0};
    heading::SavedCalibrationAccuracy saved{true, 3, 3, 3};

    auto result = heading::evaluate(true, live_low, heading::ActiveCalibrationStage::kB, saved);

    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_TRUE(result.reason == heading::InvalidReason::kSensorAccuracyLow);
}

void test_freeze_does_not_apply_to_stage_c(void)
{
    heading::SensorAccuracySnapshot live_low{0, 0, 0};
    heading::SavedCalibrationAccuracy saved{true, 3, 3, 3};

    auto result = heading::evaluate(true, live_low, heading::ActiveCalibrationStage::kC, saved);

    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_TRUE(result.reason == heading::InvalidReason::kSensorAccuracyLow);
}

void test_freeze_does_not_apply_when_no_stage_active(void)
{
    heading::SensorAccuracySnapshot live_low{0, 0, 0};
    heading::SavedCalibrationAccuracy saved{true, 3, 3, 3};

    auto result = heading::evaluate(true, live_low, heading::ActiveCalibrationStage::kNone, saved);

    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_TRUE(result.reason == heading::InvalidReason::kSensorAccuracyLow);
}
