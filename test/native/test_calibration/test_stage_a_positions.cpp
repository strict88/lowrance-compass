#include <unity.h>

#include "calibration/stage_a.h"
#include "thresholds.h"

namespace
{

// Six quaternions, each rotating the world "down" vector (0,0,-1) into a
// distinct body-frame half-axis via conjugate(q) -- i.e. each puts a
// different face of the device toward the ground. Derived from the standard
// axis-angle -> quaternion correspondence q(axis,theta) =
// (cos(theta/2), axis*sin(theta/2)):
//   identity                              -> body -Z down
//   180 deg about X                       -> body +Z down
//   90 deg about Y  (conjugated view)     -> body +X down
//  -90 deg about Y                        -> body -X down
//  -90 deg about X                        -> body +Y down
//   90 deg about X                        -> body -Y down
const heading::Quaternion kSixPositionQuats[6] = {
    {1.0f, 0.0f, 0.0f, 0.0f},                  // -Z
    {0.0f, 1.0f, 0.0f, 0.0f},                  // +Z
    {0.70710678f, 0.0f, 0.70710678f, 0.0f},    // +X
    {0.70710678f, 0.0f, -0.70710678f, 0.0f},   // -X
    {0.70710678f, -0.70710678f, 0.0f, 0.0f},   // +Y
    {0.70710678f, 0.70710678f, 0.0f, 0.0f},    // -Y
};

calibration::StageASample sampleAt(const heading::Quaternion &q)
{
    calibration::StageASample s;
    s.raw_quat = q;
    s.mag_accuracy = 3;
    s.accel_accuracy = 3;
    s.gyro_accuracy = 3;
    s.dt_s = 0.1f;
    return s;
}

int countDone(const calibration::StageAProgress &p)
{
    int n = 0;
    for (bool d : p.positions_done)
    {
        if (d) ++n;
    }
    return n;
}

calibration::StageA readyStage()
{
    calibration::StageA stage;
    stage.start(0.0f);
    // Fast-forward past AwaitingStillness with a single long still sample.
    calibration::StageASample still = sampleAt(kSixPositionQuats[0]);
    still.dt_s = thresholds::kStageAStillnessHoldS + 1.0f;
    stage.update(still, still.dt_s);
    return stage;
}

}  // namespace

void test_one_position_held_gets_marked_done(void)
{
    calibration::StageA stage = readyStage();
    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kAwaitingPositions);

    float t = thresholds::kStageAStillnessHoldS + 1.0f;
    float dt = 0.1f;
    int steps = static_cast<int>(thresholds::kStageAPositionHoldS / dt) + 2;
    for (int i = 0; i < steps; ++i)
    {
        t += dt;
        stage.update(sampleAt(kSixPositionQuats[0]), t);
    }

    TEST_ASSERT_EQUAL_INT(1, countDone(stage.progress()));
}

void test_two_different_positions_are_independently_marked(void)
{
    calibration::StageA stage = readyStage();

    float t = thresholds::kStageAStillnessHoldS + 1.0f;
    float dt = 0.1f;
    int steps = static_cast<int>(thresholds::kStageAPositionHoldS / dt) + 2;

    for (int i = 0; i < steps; ++i)
    {
        t += dt;
        stage.update(sampleAt(kSixPositionQuats[0]), t);
    }
    for (int i = 0; i < steps; ++i)
    {
        t += dt;
        stage.update(sampleAt(kSixPositionQuats[1]), t);
    }

    TEST_ASSERT_EQUAL_INT(2, countDone(stage.progress()));
}

void test_all_six_positions_in_any_order_transition_to_rotation(void)
{
    calibration::StageA stage = readyStage();

    float t = thresholds::kStageAStillnessHoldS + 1.0f;
    float dt = 0.1f;
    int steps = static_cast<int>(thresholds::kStageAPositionHoldS / dt) + 2;

    // Deliberately out of "natural" order to assert order-independence.
    int order[6] = {3, 0, 5, 1, 4, 2};
    for (int idx : order)
    {
        for (int i = 0; i < steps; ++i)
        {
            t += dt;
            stage.update(sampleAt(kSixPositionQuats[idx]), t);
        }
    }

    TEST_ASSERT_EQUAL_INT(6, countDone(stage.progress()));
    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kAwaitingRotation);
}

void test_brief_hold_below_threshold_does_not_mark_done(void)
{
    calibration::StageA stage = readyStage();

    float t = thresholds::kStageAStillnessHoldS + 1.0f;
    float dt = 0.1f;
    int steps = static_cast<int>(thresholds::kStageAPositionHoldS / dt) - 3;  // short of the hold
    for (int i = 0; i < steps; ++i)
    {
        t += dt;
        stage.update(sampleAt(kSixPositionQuats[0]), t);
    }

    TEST_ASSERT_EQUAL_INT(0, countDone(stage.progress()));
}
