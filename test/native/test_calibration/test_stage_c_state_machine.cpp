#include <unity.h>

#include "calibration/stage_c.h"
#include "thresholds.h"

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;

// Drives a fresh StageC through a synthetic GPS swing: a slow, steady turn
// (5 deg/s, well under STAGE_C_MAX_TURN_RATE_DEG_S) covering `turns` full
// rotations at 2 deg per sample, with a constant deviation offset
// (`deviation_deg`) between the reference and raw heading at every sample.
// Stops early (without calling stopSwing()) if the state machine leaves
// kSwinging on its own (full coverage reached).
calibration::StageC swingThroughSweep(float deviation_deg, int turns)
{
    calibration::StageC stage;
    stage.start(0.0f);
    stage.startGpsSwing();

    const float turn_rate_rad_s = 5.0f * kDegToRad;
    const float dt = 0.4f;  // 2 deg of heading advance per sample at 5 deg/s
    const float step_deg = 2.0f;
    const int total_steps = turns * 360 / static_cast<int>(step_deg) + 20;

    float heading_deg = 0.0f;
    float t = 0.0f;
    for (int i = 0; i < total_steps; ++i)
    {
        if (stage.state() != calibration::StageCState::kSwinging)
        {
            break;
        }
        float raw_heading_rad = heading_deg * kDegToRad;
        float reference_rad = (heading_deg + deviation_deg) * kDegToRad;
        t += dt;
        stage.updateSwing(raw_heading_rad, reference_rad, turn_rate_rad_s, 2.0f, 0.1f, 3, 3, 3, t);
        heading_deg += step_deg;
    }
    return stage;
}
}  // namespace

void test_full_gps_swing_with_zero_deviation_reaches_result_ready(void)
{
    calibration::StageC stage = swingThroughSweep(0.0f, thresholds::kStageCMinFullTurns + 1);

    TEST_ASSERT_TRUE(stage.state() == calibration::StageCState::kResultReady);
    TEST_ASSERT_TRUE(stage.rejectReason() == calibration::StageCRejectReason::kNone);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, stage.candidateResidualRmsRad() * kRadToDeg);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 0.0f, stage.candidateMaxDeviationRad() * kRadToDeg);
}

void test_swing_stopped_early_is_rejected_incomplete_coverage(void)
{
    calibration::StageC stage;
    stage.start(0.0f);
    stage.startGpsSwing();

    // Feed only a handful of samples, nowhere near full sector coverage or
    // STAGE_C_MIN_FULL_TURNS, then stop early.
    float t = 0.0f;
    for (int i = 0; i < 5; ++i)
    {
        t += 0.4f;
        stage.updateSwing(static_cast<float>(i) * 2.0f * kDegToRad, static_cast<float>(i) * 2.0f * kDegToRad,
                           5.0f * kDegToRad, 2.0f, 0.1f, 3, 3, 3, t);
    }
    stage.stopSwing();

    TEST_ASSERT_TRUE(stage.state() == calibration::StageCState::kRejected);
    TEST_ASSERT_TRUE(stage.rejectReason() == calibration::StageCRejectReason::kIncompleteCoverage);
}

void test_swing_with_large_constant_deviation_is_rejected_deviation_too_high(void)
{
    calibration::StageC stage = swingThroughSweep(20.0f, thresholds::kStageCMinFullTurns + 1);

    TEST_ASSERT_TRUE(stage.state() == calibration::StageCState::kRejected);
    TEST_ASSERT_TRUE(stage.rejectReason() == calibration::StageCRejectReason::kDeviationTooHigh);
}

void test_apply_and_discard_transitions(void)
{
    calibration::StageC applied = swingThroughSweep(0.0f, thresholds::kStageCMinFullTurns + 1);
    TEST_ASSERT_TRUE(applied.state() == calibration::StageCState::kResultReady);
    applied.apply(100.0f);
    TEST_ASSERT_TRUE(applied.state() == calibration::StageCState::kSaved);

    calibration::StageC discarded = swingThroughSweep(0.0f, thresholds::kStageCMinFullTurns + 1);
    TEST_ASSERT_TRUE(discarded.state() == calibration::StageCState::kResultReady);
    discarded.discard();
    TEST_ASSERT_TRUE(discarded.state() == calibration::StageCState::kIdle);
}

void test_cancel_from_swinging_moves_to_cancelled(void)
{
    calibration::StageC stage;
    stage.start(0.0f);
    stage.startGpsSwing();
    stage.updateSwing(0.0f, 0.0f, 5.0f * kDegToRad, 2.0f, 0.1f, 3, 3, 3, 0.4f);
    stage.cancel();
    TEST_ASSERT_TRUE(stage.state() == calibration::StageCState::kCancelled);
}
