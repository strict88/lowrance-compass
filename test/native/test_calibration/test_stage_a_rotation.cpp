#include <unity.h>

#include <cmath>

#include "calibration/stage_a.h"
#include "heading/quaternion.h"
#include "thresholds.h"

namespace
{

constexpr float kPi = 3.14159265358979323846f;

heading::Quaternion axisAngle(float ax, float ay, float az, float rad)
{
    float half = rad / 2.0f;
    float s = std::sin(half);
    return heading::Quaternion{std::cos(half), ax * s, ay * s, az * s};
}

calibration::StageASample sampleAt(const heading::Quaternion &q, float dt_s)
{
    calibration::StageASample s;
    s.raw_quat = q;
    s.mag_accuracy = 3;
    s.accel_accuracy = 3;
    s.gyro_accuracy = 3;
    s.dt_s = dt_s;
    return s;
}

// Drives a fresh StageA from Idle through AwaitingStillness and all six
// AwaitingPositions holds (using the same six known quaternions as
// test_stage_a_positions.cpp) so the rotation-coverage step can be tested in
// isolation.
calibration::StageA stageReadyForRotation()
{
    const heading::Quaternion kSixPositionQuats[6] = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.70710678f, 0.0f, 0.70710678f, 0.0f},
        {0.70710678f, 0.0f, -0.70710678f, 0.0f},
        {0.70710678f, -0.70710678f, 0.0f, 0.0f},
        {0.70710678f, 0.70710678f, 0.0f, 0.0f},
    };

    calibration::StageA stage;
    stage.start(0.0f);

    float t = 0.0f;
    calibration::StageASample still = sampleAt(kSixPositionQuats[0], thresholds::kStageAStillnessHoldS + 1.0f);
    t += still.dt_s;
    stage.update(still, t);

    float dt = 0.1f;
    int steps = static_cast<int>(thresholds::kStageAPositionHoldS / dt) + 2;
    for (const auto &q : kSixPositionQuats)
    {
        for (int i = 0; i < steps; ++i)
        {
            t += dt;
            stage.update(sampleAt(q, dt), t);
        }
    }

    return stage;
}

}  // namespace

void test_rotation_coverage_reaches_threshold_from_synthetic_sweep(void)
{
    calibration::StageA stage = stageReadyForRotation();
    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kAwaitingRotation);

    float t = 1000.0f;  // arbitrary continuation point
    // A figure-8-like sweep: vary yaw (about Z) and pitch (about Y) broadly
    // enough to visit most of the 8x8 sphere-bin grid.
    for (int yaw_step = 0; yaw_step < 12; ++yaw_step)
    {
        float yaw = (2.0f * kPi) * (static_cast<float>(yaw_step) / 12.0f);
        for (int pitch_step = 0; pitch_step < 8; ++pitch_step)
        {
            float pitch = -kPi / 2.0f + kPi * (static_cast<float>(pitch_step) / 7.0f);
            heading::Quaternion qz = axisAngle(0, 0, 1, yaw);
            heading::Quaternion qy = axisAngle(0, 1, 0, pitch);
            heading::Quaternion q = heading::multiply(qz, qy);

            t += 0.1f;
            stage.update(sampleAt(q, 0.1f), t);

            if (stage.state() == calibration::StageAState::kEvaluating)
            {
                break;
            }
        }
        if (stage.state() == calibration::StageAState::kEvaluating)
        {
            break;
        }
    }

    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kEvaluating);
    TEST_ASSERT_TRUE(stage.progress().rotation_coverage_fraction >= thresholds::kStageARotationCoverageMinFraction);
}

void test_rotation_coverage_starts_at_zero_on_entry(void)
{
    calibration::StageA stage = stageReadyForRotation();
    TEST_ASSERT_TRUE(stage.progress().rotation_coverage_fraction < thresholds::kStageARotationCoverageMinFraction);
}
