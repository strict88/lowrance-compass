#include <unity.h>

#include "calibration/stage_c.h"
#include "thresholds.h"

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;

// Feeds all 8 manual points (N, NE, E, SE, S, SW, W, NW true bearings) with
// zero variation and a constant deviation offset between the true bearing
// and the raw heading the operator reports at each point.
calibration::StageC manualSwingThroughAllPoints(float deviation_deg)
{
    calibration::StageC stage;
    stage.start(0.0f);
    stage.startManualSwing();

    for (int i = 0; i < thresholds::kStageCManualPointCount; ++i)
    {
        float true_bearing_deg = static_cast<float>(i) * (360.0f / thresholds::kStageCManualPointCount);
        float raw_heading_deg = true_bearing_deg - deviation_deg;
        stage.enterManualPoint(true_bearing_deg * kDegToRad, raw_heading_deg * kDegToRad, 0.0f);
    }
    return stage;
}
}  // namespace

void test_manual_swing_all_eight_points_with_zero_deviation_reaches_result_ready(void)
{
    calibration::StageC stage = manualSwingThroughAllPoints(0.0f);

    TEST_ASSERT_TRUE(stage.state() == calibration::StageCState::kResultReady);
    TEST_ASSERT_TRUE(stage.rejectReason() == calibration::StageCRejectReason::kNone);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, stage.candidateResidualRmsRad() * kRadToDeg);
}

void test_manual_point_index_advances_after_each_confirmation(void)
{
    calibration::StageC stage;
    stage.start(0.0f);
    stage.startManualSwing();

    TEST_ASSERT_EQUAL_INT(1, stage.manualPointIndex());
    stage.enterManualPoint(0.0f, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT(2, stage.manualPointIndex());
    TEST_ASSERT_TRUE(stage.state() == calibration::StageCState::kAwaitingManualPoint);
}

void test_manual_swing_with_large_deviation_is_rejected_deviation_too_high(void)
{
    calibration::StageC stage = manualSwingThroughAllPoints(20.0f);

    TEST_ASSERT_TRUE(stage.state() == calibration::StageCState::kRejected);
    TEST_ASSERT_TRUE(stage.rejectReason() == calibration::StageCRejectReason::kDeviationTooHigh);
}

void test_manual_swing_result_ready_can_be_applied(void)
{
    calibration::StageC stage = manualSwingThroughAllPoints(0.0f);
    TEST_ASSERT_TRUE(stage.state() == calibration::StageCState::kResultReady);
    stage.apply(50.0f);
    TEST_ASSERT_TRUE(stage.state() == calibration::StageCState::kSaved);
}

void test_entering_a_point_outside_awaiting_state_is_a_no_op(void)
{
    calibration::StageC stage;
    stage.start(0.0f);
    // Still kIdle -- never called startManualSwing().
    stage.enterManualPoint(0.0f, 0.0f, 0.0f);
    TEST_ASSERT_TRUE(stage.state() == calibration::StageCState::kIdle);
    TEST_ASSERT_EQUAL_INT(0, stage.manualPointIndex());
}
