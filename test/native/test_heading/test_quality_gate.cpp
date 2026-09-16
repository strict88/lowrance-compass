#include <unity.h>

#include "heading/quality_gate.h"

void test_all_high_passes(void)
{
    heading::SensorAccuracySnapshot live{3, 3, 3};
    heading::SavedCalibrationAccuracy saved{};
    auto result = heading::evaluate(true, live, heading::ActiveCalibrationStage::kNone, saved);
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_TRUE(result.reason == heading::InvalidReason::kNone);
}

void test_mag_below_high_fails(void)
{
    heading::SensorAccuracySnapshot live{2, 3, 3};
    heading::SavedCalibrationAccuracy saved{true, 3, 3, 3};
    auto result = heading::evaluate(true, live, heading::ActiveCalibrationStage::kNone, saved);
    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_TRUE(result.reason == heading::InvalidReason::kSensorAccuracyLow);
}

void test_accel_below_high_fails(void)
{
    heading::SensorAccuracySnapshot live{3, 1, 3};
    heading::SavedCalibrationAccuracy saved{true, 3, 3, 3};
    auto result = heading::evaluate(true, live, heading::ActiveCalibrationStage::kNone, saved);
    TEST_ASSERT_FALSE(result.valid);
}

void test_gyro_below_high_fails(void)
{
    heading::SensorAccuracySnapshot live{3, 3, 0};
    heading::SavedCalibrationAccuracy saved{true, 3, 3, 3};
    auto result = heading::evaluate(true, live, heading::ActiveCalibrationStage::kNone, saved);
    TEST_ASSERT_FALSE(result.valid);
}

void test_never_calibrated_reason(void)
{
    heading::SensorAccuracySnapshot live{0, 0, 0};
    heading::SavedCalibrationAccuracy saved{};  // exists=false
    auto result = heading::evaluate(true, live, heading::ActiveCalibrationStage::kNone, saved);
    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_TRUE(result.reason == heading::InvalidReason::kSensorNotCalibrated);
}

void test_disconnected_overrides_everything(void)
{
    heading::SensorAccuracySnapshot live{3, 3, 3};
    heading::SavedCalibrationAccuracy saved{true, 3, 3, 3};
    auto result = heading::evaluate(false, live, heading::ActiveCalibrationStage::kA, saved);
    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_TRUE(result.reason == heading::InvalidReason::kSensorDisconnected);
}
