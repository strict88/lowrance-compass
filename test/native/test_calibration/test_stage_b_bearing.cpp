#include <unity.h>

#include "calibration/stage_b.h"
#include "thresholds.h"

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;
constexpr float kEpsDeg = 0.05f;

// Drives a fresh StageB to kAwaitingBearingEntry via a stationary level
// capture.
calibration::StageB stageAwaitingBearing()
{
    calibration::StageB stage;
    stage.start(0.0f);
    stage.beginLevelCapture(0.0f);

    heading::Quaternion identity{1.0f, 0.0f, 0.0f, 0.0f};
    float dt = 0.1f;
    int steps = static_cast<int>(thresholds::kStageBLevelStillWindowS / dt) + 2;
    for (int i = 0; i < steps; ++i)
    {
        stage.updateLevelCapture(identity, 0.0f, 0.0f, 0.0f, dt);
    }

    stage.chooseKnownBearing();
    return stage;
}
}  // namespace

void test_bearing_offset_zero_when_magnetic_bearing_matches_heading(void)
{
    calibration::StageB stage = stageAwaitingBearing();
    TEST_ASSERT_TRUE(stage.state() == calibration::StageBState::kAwaitingBearingEntry);

    // True 90 deg, +10 deg (East) variation -> magnetic 80 deg; heading
    // already reads 80 deg -> zero offset.
    stage.enterBearing(90.0f * kDegToRad, 10.0f * kDegToRad, 80.0f * kDegToRad);

    TEST_ASSERT_TRUE(stage.state() == calibration::StageBState::kOffsetComputed);
    TEST_ASSERT_TRUE(stage.offsetMethod() == calibration::StageBOffsetMethod::kKnownBearing);
    TEST_ASSERT_FLOAT_WITHIN(kEpsDeg, 0.0f, stage.previewOffsetRad() * kRadToDeg);
}

void test_bearing_offset_matches_hand_computed_value(void)
{
    calibration::StageB stage = stageAwaitingBearing();

    // True 90 deg, -5 deg (West) variation -> magnetic 95 deg; heading reads
    // 100 deg -> offset = 95 - 100 = -5 deg.
    stage.enterBearing(90.0f * kDegToRad, -5.0f * kDegToRad, 100.0f * kDegToRad);

    TEST_ASSERT_FLOAT_WITHIN(kEpsDeg, -5.0f, stage.previewOffsetRad() * kRadToDeg);
}

void test_bearing_offset_wraps_correctly_across_zero(void)
{
    calibration::StageB stage = stageAwaitingBearing();

    // True 5 deg, +10 deg variation -> magnetic -5 deg (355 deg); heading
    // reads 350 deg -> offset = 355 - 350 = 5 deg (short way, not -355).
    stage.enterBearing(5.0f * kDegToRad, 10.0f * kDegToRad, 350.0f * kDegToRad);

    TEST_ASSERT_FLOAT_WITHIN(kEpsDeg, 5.0f, stage.previewOffsetRad() * kRadToDeg);
}

void test_bearing_offset_stays_within_plus_minus_180(void)
{
    calibration::StageB stage = stageAwaitingBearing();

    // True 0 deg, 0 variation -> magnetic 0; heading reads 179 deg -> offset
    // should be -179 deg (short way), not +181.
    stage.enterBearing(0.0f, 0.0f, 179.0f * kDegToRad);

    float offset_deg = stage.previewOffsetRad() * kRadToDeg;
    TEST_ASSERT_TRUE(offset_deg > -180.0f && offset_deg <= 180.0f);
    TEST_ASSERT_FLOAT_WITHIN(kEpsDeg, -179.0f, offset_deg);
}

void test_level_reference_quat_is_captured_before_bearing_entry(void)
{
    calibration::StageB stage = stageAwaitingBearing();
    heading::Quaternion level = stage.levelReferenceQuat();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, level.w);
}
