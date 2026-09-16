#include <unity.h>

#include <cmath>

#include "heading/pipeline.h"

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;
constexpr float kEpsDeg = 0.05f;

// Pure yaw-only (rotation about Z) quaternion: yawQuat(a) * yawQuat(b) ==
// yawQuat(a+b), used to build known-vector test inputs.
heading::Quaternion yawQuat(float rad)
{
    return heading::Quaternion{std::cos(rad / 2.0f), 0.0f, 0.0f, std::sin(rad / 2.0f)};
}

heading::PipelineInput baseInput()
{
    heading::PipelineInput in;
    in.sensor_connected = true;
    in.live_accuracy = {3, 3, 3};
    in.active_stage = heading::ActiveCalibrationStage::kNone;
    in.saved_profile = {true, 3, 3, 3};
    return in;
}

}  // namespace

void test_raw_quat_only_yields_matching_heading(void)
{
    heading::PipelineInput in = baseInput();
    in.raw_quat = yawQuat(90.0f * kDegToRad);

    heading::HeadingReading r = heading::computeHeadingReading(in);

    TEST_ASSERT_FLOAT_WITHIN(kEpsDeg, 90.0f, r.heading_rad * kRadToDeg);
    TEST_ASSERT_FLOAT_WITHIN(kEpsDeg, 0.0f, r.pitch_rad * kRadToDeg);
    TEST_ASSERT_FLOAT_WITHIN(kEpsDeg, 0.0f, r.roll_rad * kRadToDeg);
}

void test_level_reference_applied_before_offset(void)
{
    heading::PipelineInput in = baseInput();
    in.raw_quat = yawQuat(90.0f * kDegToRad);
    in.level_reference.has_level = true;
    in.level_reference.level_reference_quat = yawQuat(30.0f * kDegToRad);

    heading::HeadingReading r = heading::computeHeadingReading(in);

    // conjugate(level)*raw composes to yawQuat(90-30) for pure-Z rotations.
    TEST_ASSERT_FLOAT_WITHIN(kEpsDeg, 60.0f, r.heading_rad * kRadToDeg);
}

void test_mounting_offset_adds_after_level(void)
{
    heading::PipelineInput in = baseInput();
    in.raw_quat = yawQuat(90.0f * kDegToRad);
    in.level_reference.has_level = true;
    in.level_reference.level_reference_quat = yawQuat(30.0f * kDegToRad);
    in.mounting_offset.has_offset = true;
    in.mounting_offset.mounting_offset_rad = 10.0f * kDegToRad;

    heading::HeadingReading r = heading::computeHeadingReading(in);

    TEST_ASSERT_FLOAT_WITHIN(kEpsDeg, 70.0f, r.heading_rad * kRadToDeg);
}

// Fixed order (FR-044): "sensor calibration -> level reference and mounting
// offset -> deviation correction -> normalization". The deviation curve MUST
// be evaluated at the post-offset heading, not the raw heading -- this test
// fails under the wrong order.
void test_deviation_evaluated_after_offset_not_before(void)
{
    heading::PipelineInput in = baseInput();
    in.raw_quat = yawQuat(0.0f);  // raw heading 0 deg
    in.mounting_offset.has_offset = true;
    in.mounting_offset.mounting_offset_rad = 90.0f * kDegToRad;  // post-offset heading 90 deg
    in.deviation_correction.has_correction = true;
    in.deviation_correction.coefficients.b = 0.1f;  // deviation(theta) = 0.1*sin(theta)

    heading::HeadingReading r = heading::computeHeadingReading(in);

    // Correct order: deviation(90 deg) = 0.1*sin(90 deg) = 0.1 rad added on
    // top of the 90 deg post-offset heading.
    float expected_deg = 90.0f + 0.1f * kRadToDeg;
    TEST_ASSERT_FLOAT_WITHIN(kEpsDeg, expected_deg, r.heading_rad * kRadToDeg);
}

void test_result_normalized_into_0_360(void)
{
    heading::PipelineInput in = baseInput();
    in.raw_quat = yawQuat(350.0f * kDegToRad);
    in.mounting_offset.has_offset = true;
    in.mounting_offset.mounting_offset_rad = 20.0f * kDegToRad;  // 350 + 20 = 370 -> wraps to 10

    heading::HeadingReading r = heading::computeHeadingReading(in);

    TEST_ASSERT_FLOAT_WITHIN(kEpsDeg, 10.0f, r.heading_rad * kRadToDeg);
    TEST_ASSERT_TRUE(r.heading_rad >= 0.0f);
}

void test_identity_corrections_when_stages_not_done(void)
{
    heading::PipelineInput in = baseInput();
    in.raw_quat = yawQuat(45.0f * kDegToRad);
    // level_reference, mounting_offset, deviation_correction all left at
    // their has_*=false defaults (Stage B/C NOT_DONE).

    heading::HeadingReading r = heading::computeHeadingReading(in);

    TEST_ASSERT_FLOAT_WITHIN(kEpsDeg, 45.0f, r.heading_rad * kRadToDeg);
}
