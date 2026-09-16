#include <unity.h>

#include <cmath>

#include "calibration/stage_a.h"
#include "thresholds.h"

namespace
{

calibration::StageASample sampleAt(const heading::Quaternion &q, uint8_t mag, uint8_t accel, uint8_t gyro, float dt_s)
{
    calibration::StageASample s;
    s.raw_quat = q;
    s.mag_accuracy = mag;
    s.accel_accuracy = accel;
    s.gyro_accuracy = gyro;
    s.dt_s = dt_s;
    return s;
}

// Drives a fresh StageA to kEvaluating with low accuracy throughout, so the
// timeout path (never reaching all-High) can be exercised.
calibration::StageA stageAtEvaluatingLowAccuracy(float *out_t)
{
    calibration::StageA stage;
    stage.start(0.0f);

    float t = 0.0f;
    calibration::StageASample still = sampleAt({1.0f, 0.0f, 0.0f, 0.0f}, 1, 1, 1, thresholds::kStageAStillnessHoldS + 1.0f);
    t += still.dt_s;
    stage.update(still, t);

    const heading::Quaternion kSixPositionQuats[6] = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.70710678f, 0.0f, 0.70710678f, 0.0f},
        {0.70710678f, 0.0f, -0.70710678f, 0.0f},
        {0.70710678f, -0.70710678f, 0.0f, 0.0f},
        {0.70710678f, 0.70710678f, 0.0f, 0.0f},
    };
    float dt = 0.1f;
    int steps = static_cast<int>(thresholds::kStageAPositionHoldS / dt) + 2;
    for (const auto &q : kSixPositionQuats)
    {
        for (int i = 0; i < steps; ++i)
        {
            t += dt;
            stage.update(sampleAt(q, 1, 1, 1, dt), t);
        }
    }

    for (int i = 0; i < 400 && stage.state() == calibration::StageAState::kAwaitingRotation; ++i)
    {
        float yaw = 0.31f * static_cast<float>(i);
        float pitch = 0.17f * static_cast<float>(i);
        heading::Quaternion q{std::cos(yaw) * std::cos(pitch), std::sin(pitch), std::sin(yaw) * std::cos(pitch), 0.0f};
        t += dt;
        stage.update(sampleAt(q, 1, 1, 1, dt), t);
    }

    *out_t = t;
    return stage;
}

}  // namespace

// FR-015: timeout leaves the previous calibration in effect (nothing is
// persisted, since kDone -- the only state that triggers a save, T061 --
// requires all-High accuracy, never reached here) and offers retry.
void test_timeout_fires_after_the_configured_duration(void)
{
    float t = 0.0f;
    calibration::StageA stage = stageAtEvaluatingLowAccuracy(&t);
    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kEvaluating);

    // Not yet timed out (relative to started_at_s_=0, set by start(0.0f) in
    // the helper above -- NOT relative to `t`, which is how far the setup
    // simulation itself advanced the clock).
    stage.update(sampleAt({1, 0, 0, 0}, 1, 1, 1, 0.1f), thresholds::kStageATimeoutS - 1.0f);
    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kEvaluating);

    // Past the timeout.
    stage.update(sampleAt({1, 0, 0, 0}, 1, 1, 1, 0.1f), thresholds::kStageATimeoutS + 1.0f);
    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kTimedOut);
}

void test_retry_after_timeout_starts_clean(void)
{
    float t = 0.0f;
    calibration::StageA stage = stageAtEvaluatingLowAccuracy(&t);
    stage.update(sampleAt({1, 0, 0, 0}, 1, 1, 1, 0.1f), t + thresholds::kStageATimeoutS + 1.0f);
    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kTimedOut);

    stage.start(t + thresholds::kStageATimeoutS + 100.0f);

    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kAwaitingStillness);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, stage.progress().rotation_coverage_fraction);
}

void test_cancel_from_awaiting_stillness(void)
{
    calibration::StageA stage;
    stage.start(0.0f);
    stage.update(sampleAt({1, 0, 0, 0}, 1, 1, 1, 0.1f), 1.0f);

    stage.cancel();

    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kCancelled);
}

void test_cancel_from_awaiting_positions(void)
{
    calibration::StageA stage;
    stage.start(0.0f);

    calibration::StageASample still = sampleAt({1, 0, 0, 0}, 1, 1, 1, thresholds::kStageAStillnessHoldS + 1.0f);
    stage.update(still, still.dt_s);
    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kAwaitingPositions);

    stage.cancel();

    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kCancelled);
}

void test_cancel_never_reports_done(void)
{
    calibration::StageA stage;
    stage.start(0.0f);
    stage.cancel();
    TEST_ASSERT_FALSE(stage.state() == calibration::StageAState::kDone);
}

void test_restart_after_cancel_starts_clean(void)
{
    calibration::StageA stage;
    stage.start(0.0f);
    calibration::StageASample still = sampleAt({1, 0, 0, 0}, 1, 1, 1, thresholds::kStageAStillnessHoldS + 1.0f);
    stage.update(still, still.dt_s);
    stage.cancel();

    stage.start(1000.0f);

    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kAwaitingStillness);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, stage.progress().rotation_coverage_fraction);
}
