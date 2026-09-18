#include <unity.h>

#include <cmath>

#include "calibration/stage_c.h"
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
}  // namespace

void test_absent_deviation_correction_yields_zero_curve(void)
{
    calibration::DeviationCorrection correction;  // never populated meaningfully
    heading::DeviationCorrectionInput out;

    calibration::applyDeviationCorrection(correction, /*exists=*/false, out);

    TEST_ASSERT_FALSE(out.has_correction);
}

void test_present_deviation_correction_is_applied_at_compass_heading(void)
{
    calibration::DeviationCorrection correction;
    correction.coefficients[0] = 2.0f * kDegToRad;  // A: constant 2 deg offset
    // B, C, D, E left at zero for a simple, direction-independent case.

    heading::DeviationCorrectionInput out;
    calibration::applyDeviationCorrection(correction, /*exists=*/true, out);
    TEST_ASSERT_TRUE(out.has_correction);

    heading::PipelineInput in;
    in.raw_quat = yawQuat(90.0f * kDegToRad);
    in.sensor_connected = true;
    in.live_accuracy = {3, 3, 3};
    in.saved_profile = {true, 3, 3, 3};
    in.deviation_correction = out;

    heading::HeadingReading result = heading::computeHeadingReading(in);
    // Pre-deviation compass heading is unaffected by the correction...
    TEST_ASSERT_FLOAT_WITHIN(kEpsDeg, 90.0f, result.compass_heading_rad * kRadToDeg);
    // ...while the final heading has the constant 2 deg deviation added.
    TEST_ASSERT_FLOAT_WITHIN(kEpsDeg, 92.0f, result.heading_rad * kRadToDeg);
}

void test_deviation_correction_matches_evaluateDeviationRad_directly(void)
{
    calibration::DeviationCorrection correction;
    correction.coefficients[0] = 0.5f * kDegToRad;
    correction.coefficients[1] = 1.0f * kDegToRad;
    correction.coefficients[2] = -0.5f * kDegToRad;
    correction.coefficients[3] = 0.3f * kDegToRad;
    correction.coefficients[4] = -0.2f * kDegToRad;

    heading::DeviationCorrectionInput out;
    calibration::applyDeviationCorrection(correction, /*exists=*/true, out);

    float compass_heading_rad = 40.0f * kDegToRad;
    float expected_deviation_rad = heading::evaluateDeviationRad(out.coefficients, compass_heading_rad);

    heading::PipelineInput in;
    in.raw_quat = yawQuat(compass_heading_rad);
    in.sensor_connected = true;
    in.live_accuracy = {3, 3, 3};
    in.saved_profile = {true, 3, 3, 3};
    in.deviation_correction = out;

    heading::HeadingReading result = heading::computeHeadingReading(in);
    float expected_heading_deg = (40.0f + expected_deviation_rad * kRadToDeg);
    TEST_ASSERT_FLOAT_WITHIN(kEpsDeg, expected_heading_deg, result.heading_rad * kRadToDeg);
}
