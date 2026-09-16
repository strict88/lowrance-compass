#include <unity.h>

#include "calibration/stage_b.h"
#include "thresholds.h"

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;

calibration::StageB stageAwaitingGps()
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

    stage.chooseGpsCourse();
    return stage;
}
}  // namespace

void test_gps_sample_below_min_speed_is_rejected(void)
{
    calibration::StageB stage = stageAwaitingGps();

    float below_min = thresholds::kStageBGpsMinSogMS * 0.5f;
    stage.updateGpsAlignment(below_min, 90.0f * kDegToRad, 0.0f, 90.0f * kDegToRad, 1.0f);

    TEST_ASSERT_TRUE(stage.state() == calibration::StageBState::kAwaitingGpsAlignment);
    TEST_ASSERT_TRUE(stage.waitingReason() == calibration::StageBWaitingReason::kSpeedTooLow);
}

void test_steady_course_at_speed_computes_offset(void)
{
    calibration::StageB stage = stageAwaitingGps();

    float sog = thresholds::kStageBGpsMinSogMS + 1.0f;
    float cog_deg = 90.0f;
    float variation_rad = 5.0f * kDegToRad;
    float current_heading_rad = 80.0f * kDegToRad;

    // Feed steady samples spanning past the steadiness window.
    float dt = 0.5f;
    float t = 0.0f;
    int steps = static_cast<int>(thresholds::kStageBGpsCogSteadyWindowS / dt) + 4;
    for (int i = 0; i < steps; ++i)
    {
        t += dt;
        stage.updateGpsAlignment(sog, cog_deg * kDegToRad, variation_rad, current_heading_rad, t);
    }

    TEST_ASSERT_TRUE(stage.state() == calibration::StageBState::kOffsetComputed);
    TEST_ASSERT_TRUE(stage.offsetMethod() == calibration::StageBOffsetMethod::kGpsCourse);

    // magnetic_cog = 90 - 5 = 85; offset = 85 - 80 = 5 deg.
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 5.0f, stage.previewOffsetRad() * kRadToDeg);
}

void test_unsteady_course_reports_waiting_reason(void)
{
    calibration::StageB stage = stageAwaitingGps();

    float sog = thresholds::kStageBGpsMinSogMS + 1.0f;
    float dt = 0.5f;
    float t = 0.0f;
    int steps = static_cast<int>(thresholds::kStageBGpsCogSteadyWindowS / dt) + 4;
    for (int i = 0; i < steps; ++i)
    {
        t += dt;
        // Alternate wildly between two headings far apart -> high stddev.
        float cog_deg = (i % 2 == 0) ? 10.0f : 170.0f;
        stage.updateGpsAlignment(sog, cog_deg * kDegToRad, 0.0f, 0.0f, t);
    }

    TEST_ASSERT_TRUE(stage.state() == calibration::StageBState::kAwaitingGpsAlignment);
    TEST_ASSERT_TRUE(stage.waitingReason() == calibration::StageBWaitingReason::kCourseNotSteady);
}

void test_partial_window_does_not_yet_decide(void)
{
    calibration::StageB stage = stageAwaitingGps();

    float sog = thresholds::kStageBGpsMinSogMS + 1.0f;
    // Only feed a couple of samples, well short of the steady window.
    stage.updateGpsAlignment(sog, 90.0f * kDegToRad, 0.0f, 90.0f * kDegToRad, 0.5f);
    stage.updateGpsAlignment(sog, 90.0f * kDegToRad, 0.0f, 90.0f * kDegToRad, 1.0f);

    TEST_ASSERT_TRUE(stage.state() == calibration::StageBState::kAwaitingGpsAlignment);
}
