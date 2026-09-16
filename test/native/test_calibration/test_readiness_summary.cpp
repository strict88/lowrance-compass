#include <unity.h>

#include "calibration/readiness.h"
#include "calibration/stage_status.h"

void test_stage_status_derives_purely_from_record_presence(void)
{
    TEST_ASSERT_TRUE(calibration::deriveStageStatus(true) == calibration::PersistedState::kDone);
    TEST_ASSERT_TRUE(calibration::deriveStageStatus(false) == calibration::PersistedState::kNotDone);
}

void test_readiness_none_done(void)
{
    calibration::ReadinessInput input;
    TEST_ASSERT_TRUE(calibration::deriveReadiness(input) == calibration::Readiness::kNotCalibrated);
}

void test_readiness_only_a_done(void)
{
    calibration::ReadinessInput input;
    input.stage_a_done = true;
    TEST_ASSERT_TRUE(calibration::deriveReadiness(input) == calibration::Readiness::kUsableIncomplete);
}

void test_readiness_a_and_b_done(void)
{
    calibration::ReadinessInput input;
    input.stage_a_done = true;
    input.stage_b_done = true;
    TEST_ASSERT_TRUE(calibration::deriveReadiness(input) == calibration::Readiness::kUsableIncomplete);
}

void test_readiness_all_three_done(void)
{
    calibration::ReadinessInput input;
    input.stage_a_done = true;
    input.stage_b_done = true;
    input.stage_c_done = true;
    TEST_ASSERT_TRUE(calibration::deriveReadiness(input) == calibration::Readiness::kReady);
}

// FR-001: a currently-withheld heading overrides persisted status in real
// time, even when every stage reads Done.
void test_readiness_withheld_overrides_all_done(void)
{
    calibration::ReadinessInput input;
    input.stage_a_done = true;
    input.stage_b_done = true;
    input.stage_c_done = true;
    input.heading_currently_withheld = true;
    TEST_ASSERT_TRUE(calibration::deriveReadiness(input) == calibration::Readiness::kNotCalibrated);
}

void test_readiness_withheld_overrides_partial_done(void)
{
    calibration::ReadinessInput input;
    input.stage_a_done = true;
    input.heading_currently_withheld = true;
    TEST_ASSERT_TRUE(calibration::deriveReadiness(input) == calibration::Readiness::kNotCalibrated);
}

// FR-002: persisted stage status has no live-accuracy input at all -- this
// is the compile-time guarantee (deriveStageStatus takes only a bool
// "record exists"), demonstrated here by showing the same record-presence
// input always yields the same status, run repeatedly.
void test_stage_status_never_flips_without_a_record_change(void)
{
    for (int i = 0; i < 5; ++i)
    {
        TEST_ASSERT_TRUE(calibration::deriveStageStatus(true) == calibration::PersistedState::kDone);
    }
}
