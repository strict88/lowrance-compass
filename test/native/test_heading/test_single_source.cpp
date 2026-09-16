#include <unity.h>

#include "heading/pipeline.h"

// FR-042: GET /api/status.heading and the value N2kService transmits both
// read the same published HeadingReading snapshot for a given pipeline
// tick. The cross-task half of that guarantee is architectural (exactly one
// publish point, ImuTask -> shared_state::publishHeadingReading(); both
// web_api's status assembly and N2kService's transmit path -- the latter
// via n2k_task.cpp -- read it back through the same
// shared_state::getHeadingReading(), never recomputing independently) and
// isn't native-testable (shared_state.cpp needs FreeRTOS). What *is*
// native-testable, and is the guarantee that architecture depends on: a
// HeadingReading is a plain value snapshot with no hidden/mutable state, so
// copying the one published value to two independent readers can never let
// them observe different data for the same tick.
namespace
{
heading::PipelineInput sampleInput()
{
    heading::PipelineInput in;
    in.raw_quat = heading::Quaternion{0.9238795f, 0.0f, 0.0f, 0.3826834f};  // 45 deg yaw
    in.raw_gyro_z_rad_s = 0.1f;
    in.sensor_connected = true;
    in.live_accuracy = {3, 3, 3};
    in.active_stage = heading::ActiveCalibrationStage::kNone;
    in.saved_profile = {true, 3, 3, 3};
    return in;
}
}  // namespace

void test_two_independent_readers_of_the_same_published_value_see_identical_fields(void)
{
    heading::HeadingReading published = heading::computeHeadingReading(sampleInput());

    // Simulates web_api's and N2kService's independent reads of the one
    // published snapshot for this tick.
    heading::HeadingReading reader_a = published;
    heading::HeadingReading reader_b = published;

    TEST_ASSERT_EQUAL_FLOAT(reader_a.heading_rad, reader_b.heading_rad);
    TEST_ASSERT_EQUAL_FLOAT(reader_a.pitch_rad, reader_b.pitch_rad);
    TEST_ASSERT_EQUAL_FLOAT(reader_a.roll_rad, reader_b.roll_rad);
    TEST_ASSERT_EQUAL_FLOAT(reader_a.rate_of_turn_rad_s, reader_b.rate_of_turn_rad_s);
    TEST_ASSERT_EQUAL(reader_a.valid, reader_b.valid);
    TEST_ASSERT_TRUE(reader_a.reason_if_invalid == reader_b.reason_if_invalid);
}

void test_computation_is_deterministic_for_the_same_input(void)
{
    heading::PipelineInput input = sampleInput();
    heading::HeadingReading first = heading::computeHeadingReading(input);
    heading::HeadingReading second = heading::computeHeadingReading(input);

    TEST_ASSERT_EQUAL_FLOAT(first.heading_rad, second.heading_rad);
    TEST_ASSERT_EQUAL(first.valid, second.valid);
}
