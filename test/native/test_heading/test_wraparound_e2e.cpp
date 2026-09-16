#include <unity.h>

#include <cmath>

#include "heading/pipeline.h"

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;
constexpr float kEpsDeg = 0.05f;

heading::Quaternion yawQuat(float rad)
{
    return heading::Quaternion{std::cos(rad / 2.0f), 0.0f, 0.0f, std::sin(rad / 2.0f)};
}

heading::PipelineInput baseInput(float raw_heading_deg)
{
    heading::PipelineInput in;
    in.raw_quat = yawQuat(raw_heading_deg * kDegToRad);
    in.sensor_connected = true;
    in.live_accuracy = {3, 3, 3};
    in.active_stage = heading::ActiveCalibrationStage::kNone;
    in.saved_profile = {true, 3, 3, 3};
    return in;
}

float expectedWrap(float deg)
{
    float wrapped = std::fmod(deg, 360.0f);
    if (wrapped < 0.0f) wrapped += 360.0f;
    return wrapped;
}
}  // namespace

// End-to-end (full computeHeadingReading() pipeline, not just normalize.cpp
// in isolation): known input vectors straddling the 0/360 boundary from both
// directions, with no jump or sign error.
void test_wraparound_from_below(void)
{
    float inputs[] = {350.0f, 355.0f, 359.0f, 359.9f, 0.0f, 0.1f, 1.0f, 5.0f, 10.0f};
    for (float deg : inputs)
    {
        heading::HeadingReading r = heading::computeHeadingReading(baseInput(deg));
        float out_deg = r.heading_rad * kRadToDeg;
        TEST_ASSERT_TRUE(out_deg >= 0.0f);
        TEST_ASSERT_TRUE(out_deg < 360.0f);
        TEST_ASSERT_FLOAT_WITHIN(kEpsDeg, expectedWrap(deg), out_deg);
    }
}

void test_wraparound_via_mounting_offset_push_past_360(void)
{
    heading::PipelineInput in = baseInput(355.0f);
    in.mounting_offset.has_offset = true;
    in.mounting_offset.mounting_offset_rad = 10.0f * kDegToRad;  // 355 + 10 = 365 -> 5

    heading::HeadingReading r = heading::computeHeadingReading(in);
    TEST_ASSERT_FLOAT_WITHIN(kEpsDeg, 5.0f, r.heading_rad * kRadToDeg);
}

void test_wraparound_via_mounting_offset_pull_below_zero(void)
{
    heading::PipelineInput in = baseInput(5.0f);
    in.mounting_offset.has_offset = true;
    in.mounting_offset.mounting_offset_rad = -10.0f * kDegToRad;  // 5 - 10 = -5 -> 355

    heading::HeadingReading r = heading::computeHeadingReading(in);
    TEST_ASSERT_FLOAT_WITHIN(kEpsDeg, 355.0f, r.heading_rad * kRadToDeg);
}

// Continuity: a small, steady change in raw input near the boundary produces
// a correspondingly small change in output, never a large discontinuous
// jump (the failure mode this guards against: naive modulo arithmetic that
// mishandles negative results, per constitution Principle I).
void test_continuity_stepping_through_the_boundary(void)
{
    float prev_deg = -1.0f;
    bool have_prev = false;
    for (float deg = -3.0f; deg <= 3.0f; deg += 0.5f)
    {
        heading::HeadingReading r = heading::computeHeadingReading(baseInput(deg));
        float out_deg = r.heading_rad * kRadToDeg;

        if (have_prev)
        {
            float step = out_deg - prev_deg;
            if (step > 180.0f) step -= 360.0f;
            if (step < -180.0f) step += 360.0f;
            TEST_ASSERT_FLOAT_WITHIN(0.6f, 0.5f, step);  // input stepped by 0.5 deg each iteration
        }
        prev_deg = out_deg;
        have_prev = true;
    }
}

void test_exactly_zero_and_exactly_360_both_normalize_to_zero(void)
{
    heading::HeadingReading r0 = heading::computeHeadingReading(baseInput(0.0f));
    heading::HeadingReading r360 = heading::computeHeadingReading(baseInput(360.0f));
    TEST_ASSERT_FLOAT_WITHIN(kEpsDeg, 0.0f, r0.heading_rad * kRadToDeg);
    TEST_ASSERT_FLOAT_WITHIN(kEpsDeg, 0.0f, r360.heading_rad * kRadToDeg);
}
