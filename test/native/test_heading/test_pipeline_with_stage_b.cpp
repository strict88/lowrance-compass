#include <unity.h>

#include <cmath>

#include "calibration/stage_b.h"
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

void test_absent_installation_alignment_yields_identity_zero(void)
{
    calibration::InstallationAlignment alignment;  // never populated meaningfully
    heading::LevelReference level_ref;
    heading::MountingOffset offset;

    calibration::applyInstallationAlignment(alignment, /*exists=*/false, level_ref, offset);

    TEST_ASSERT_FALSE(level_ref.has_level);
    TEST_ASSERT_FALSE(offset.has_offset);
}

void test_present_installation_alignment_carries_through_to_the_pipeline(void)
{
    calibration::InstallationAlignment alignment;
    // level_reference_quat left at identity default (w=1); mounting offset 10 deg.
    alignment.mounting_offset_rad = 10.0f * kDegToRad;

    heading::LevelReference level_ref;
    heading::MountingOffset offset;
    calibration::applyInstallationAlignment(alignment, /*exists=*/true, level_ref, offset);

    TEST_ASSERT_TRUE(level_ref.has_level);
    TEST_ASSERT_TRUE(offset.has_offset);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 10.0f * kDegToRad, offset.mounting_offset_rad);

    heading::PipelineInput in;
    in.raw_quat = yawQuat(80.0f * kDegToRad);
    in.sensor_connected = true;
    in.live_accuracy = {3, 3, 3};
    in.saved_profile = {true, 3, 3, 3};
    in.level_reference = level_ref;
    in.mounting_offset = offset;

    heading::HeadingReading result = heading::computeHeadingReading(in);
    TEST_ASSERT_FLOAT_WITHIN(kEpsDeg, 90.0f, result.heading_rad * kRadToDeg);
}

void test_nonidentity_level_reference_is_applied(void)
{
    calibration::InstallationAlignment alignment;
    heading::Quaternion level30 = yawQuat(30.0f * kDegToRad);
    alignment.level_reference_quat[0] = level30.w;
    alignment.level_reference_quat[1] = level30.x;
    alignment.level_reference_quat[2] = level30.y;
    alignment.level_reference_quat[3] = level30.z;

    heading::LevelReference level_ref;
    heading::MountingOffset offset;
    calibration::applyInstallationAlignment(alignment, true, level_ref, offset);

    heading::PipelineInput in;
    in.raw_quat = yawQuat(90.0f * kDegToRad);
    in.sensor_connected = true;
    in.live_accuracy = {3, 3, 3};
    in.saved_profile = {true, 3, 3, 3};
    in.level_reference = level_ref;
    in.mounting_offset = offset;

    heading::HeadingReading result = heading::computeHeadingReading(in);
    // conjugate(level30)*raw(90) composes to yaw(60) for pure-Z rotations.
    TEST_ASSERT_FLOAT_WITHIN(kEpsDeg, 60.0f, result.heading_rad * kRadToDeg);
}
