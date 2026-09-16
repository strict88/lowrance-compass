#include <unity.h>

#include <cmath>

#include "calibration/stage_a.h"
#include "thresholds.h"

// FR-014 (quoted): "The system MUST save a new Stage A result only once the
// magnetometer, accelerometer, and gyroscope have all reached 'High'
// accuracy, and MUST NOT overwrite an existing saved calibration with a
// result of lower quality." At the state-machine level this is satisfied by
// construction: kDone is reachable only when all three accuracies are High,
// so a below-High result can never produce a save to begin with.

namespace
{
calibration::StageASample evaluatingSample(uint8_t mag, uint8_t accel, uint8_t gyro)
{
    calibration::StageASample s;
    s.raw_quat = heading::Quaternion{1.0f, 0.0f, 0.0f, 0.0f};
    s.mag_accuracy = mag;
    s.accel_accuracy = accel;
    s.gyro_accuracy = gyro;
    s.dt_s = 0.1f;
    return s;
}

// Drives a fresh StageA straight into kEvaluating by directly invoking the
// private transitions is not possible from a test, so this replays the same
// full path as the other Stage A tests.
calibration::StageA stageAtEvaluating()
{
    calibration::StageA stage;
    stage.start(0.0f);

    float t = 0.0f;
    calibration::StageASample still = evaluatingSample(0, 0, 0);
    still.dt_s = thresholds::kStageAStillnessHoldS + 1.0f;
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
            calibration::StageASample s = evaluatingSample(3, 3, 3);
            s.raw_quat = q;
            stage.update(s, t);
        }
    }

    // Sweep rotation coverage.
    for (int i = 0; i < 400 && stage.state() == calibration::StageAState::kAwaitingRotation; ++i)
    {
        float yaw = 0.31f * static_cast<float>(i);
        float pitch = 0.17f * static_cast<float>(i);
        heading::Quaternion q{std::cos(yaw) * std::cos(pitch), std::sin(pitch), std::sin(yaw) * std::cos(pitch), 0.0f};
        t += dt;
        calibration::StageASample s = evaluatingSample(3, 3, 3);
        s.raw_quat = q;
        stage.update(s, t);
    }

    return stage;
}
}  // namespace

void test_all_high_accuracy_reaches_done(void)
{
    calibration::StageA stage = stageAtEvaluating();
    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kEvaluating);

    stage.update(evaluatingSample(3, 3, 3), 100000.0f);

    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kDone);
}

void test_below_high_accuracy_never_reaches_done(void)
{
    calibration::StageA stage = stageAtEvaluating();

    // Not yet timed out (well under kStageATimeoutS from the started_at_s_=0
    // baseline): stays in Evaluating, never Done, regardless of how close
    // any single sensor is to High.
    stage.update(evaluatingSample(2, 3, 3), 1.0f);
    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kEvaluating);

    stage.update(evaluatingSample(3, 2, 3), 1.1f);
    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kEvaluating);

    stage.update(evaluatingSample(3, 3, 2), 1.2f);
    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kEvaluating);
}
