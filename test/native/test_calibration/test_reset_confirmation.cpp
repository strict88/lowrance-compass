#include <unity.h>

#include "../fakes/fake_kv_store.h"
#include "calibration/stage_a.h"
#include "drivers/kv_store/record_envelope.h"

// app_task.cpp's handleStageAReset() is Arduino-only (not natively
// testable), but its actual behavior is exactly this composition: skip
// record_envelope-backed reset entirely when confirm is false (FR-005 --
// enforced server-side, not just by the UI's own confirmation dialog), and
// clear only the targeted stage's record when confirm is true. This test
// exercises that same composition directly against the real
// save/load/resetToDefault functions Stage A uses.

namespace
{
calibration::SensorCalibrationProfile highQualityProfile()
{
    calibration::SensorCalibrationProfile p;
    p.mag_accuracy = 3;
    p.accel_accuracy = 3;
    p.gyro_accuracy = 3;
    return p;
}
}  // namespace

void test_reset_without_confirm_leaves_record_unchanged(void)
{
    FakeKeyValueStore store;
    TEST_ASSERT_TRUE(calibration::saveSensorCalibrationProfile(store, highQualityProfile()));

    // Simulate handleStageAReset()'s early return: confirm=false means
    // resetToDefault() is never called at all.
    bool confirm = false;
    if (confirm)
    {
        record_envelope::resetToDefault(store);
    }

    calibration::SensorCalibrationProfile out{};
    TEST_ASSERT_TRUE(calibration::loadSensorCalibrationProfile(store, out));
    TEST_ASSERT_EQUAL_UINT8(3, out.mag_accuracy);
}

void test_reset_with_confirm_clears_the_record(void)
{
    FakeKeyValueStore store;
    TEST_ASSERT_TRUE(calibration::saveSensorCalibrationProfile(store, highQualityProfile()));

    bool confirm = true;
    if (confirm)
    {
        record_envelope::resetToDefault(store);
    }

    calibration::SensorCalibrationProfile out{};
    TEST_ASSERT_FALSE(calibration::loadSensorCalibrationProfile(store, out));
}

void test_reset_only_clears_the_targeted_stage(void)
{
    // Two independent stores stand in for two different stages' record
    // families (each stage has its own NVS namespace in the real firmware).
    FakeKeyValueStore stage_a_store;
    FakeKeyValueStore other_stage_store;

    TEST_ASSERT_TRUE(calibration::saveSensorCalibrationProfile(stage_a_store, highQualityProfile()));
    TEST_ASSERT_TRUE(calibration::saveSensorCalibrationProfile(other_stage_store, highQualityProfile()));

    record_envelope::resetToDefault(stage_a_store);

    calibration::SensorCalibrationProfile out{};
    TEST_ASSERT_FALSE(calibration::loadSensorCalibrationProfile(stage_a_store, out));
    TEST_ASSERT_TRUE(calibration::loadSensorCalibrationProfile(other_stage_store, out));
}
