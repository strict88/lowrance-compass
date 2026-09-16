#include <unity.h>

#include "calibration/stage_a.h"
#include "thresholds.h"

namespace
{
calibration::StageASample stillSample(float dt_s)
{
    calibration::StageASample s;
    s.raw_quat = heading::Quaternion{1.0f, 0.0f, 0.0f, 0.0f};
    s.gyro_x_rad_s = 0.0f;
    s.gyro_y_rad_s = 0.0f;
    s.gyro_z_rad_s = 0.0f;
    s.dt_s = dt_s;
    return s;
}

calibration::StageASample movingSample(float dt_s)
{
    calibration::StageASample s = stillSample(dt_s);
    s.gyro_z_rad_s = 1.0f;  // well above kStageAStillnessGyroVarMax
    return s;
}
}  // namespace

void test_stillness_holds_through_to_awaiting_positions(void)
{
    calibration::StageA stage;
    stage.start(0.0f);
    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kAwaitingStillness);

    float dt = 0.1f;
    float t = 0.0f;
    int steps = static_cast<int>(thresholds::kStageAStillnessHoldS / dt) + 2;
    for (int i = 0; i < steps; ++i)
    {
        t += dt;
        stage.update(stillSample(dt), t);
    }

    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kAwaitingPositions);
}

void test_movement_resets_the_stillness_timer(void)
{
    calibration::StageA stage;
    stage.start(0.0f);

    float dt = 0.1f;
    float t = 0.0f;

    // Hold still for most of the window...
    int almost_steps = static_cast<int>(thresholds::kStageAStillnessHoldS / dt) - 2;
    for (int i = 0; i < almost_steps; ++i)
    {
        t += dt;
        stage.update(stillSample(dt), t);
    }
    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kAwaitingStillness);

    // ...then move, which must reset the accumulated hold time.
    t += dt;
    stage.update(movingSample(dt), t);
    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kAwaitingStillness);

    // Only 2 more still steps (less than the full hold) should NOT be enough
    // after the reset.
    t += dt;
    stage.update(stillSample(dt), t);
    t += dt;
    stage.update(stillSample(dt), t);
    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kAwaitingStillness);
}
