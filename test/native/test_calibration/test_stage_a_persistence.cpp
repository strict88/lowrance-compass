#include <unity.h>

#include <cstring>

#include "../fakes/fake_kv_store.h"
#include "calibration/stage_a.h"

void test_save_and_load_round_trip(void)
{
    FakeKeyValueStore store;
    calibration::SensorCalibrationProfile in;
    std::strncpy(in.saved_at_iso8601, "2026-09-16T12:00:00Z", sizeof(in.saved_at_iso8601) - 1);
    in.mag_accuracy = 3;
    in.accel_accuracy = 3;
    in.gyro_accuracy = 3;
    std::strncpy(in.firmware_version, "0.1.0", sizeof(in.firmware_version) - 1);

    TEST_ASSERT_TRUE(calibration::saveSensorCalibrationProfile(store, in));

    calibration::SensorCalibrationProfile out{};
    TEST_ASSERT_TRUE(calibration::loadSensorCalibrationProfile(store, out));
    TEST_ASSERT_EQUAL_STRING(in.saved_at_iso8601, out.saved_at_iso8601);
    TEST_ASSERT_EQUAL_UINT8(3, out.mag_accuracy);
    TEST_ASSERT_EQUAL_UINT8(3, out.accel_accuracy);
    TEST_ASSERT_EQUAL_UINT8(3, out.gyro_accuracy);
    TEST_ASSERT_EQUAL_STRING(in.firmware_version, out.firmware_version);
}

// FR-014: never persist (or let a caller accidentally persist) a
// below-High result.
void test_below_high_accuracy_refuses_to_save(void)
{
    FakeKeyValueStore store;
    calibration::SensorCalibrationProfile in;
    in.mag_accuracy = 2;
    in.accel_accuracy = 3;
    in.gyro_accuracy = 3;

    TEST_ASSERT_FALSE(calibration::saveSensorCalibrationProfile(store, in));

    calibration::SensorCalibrationProfile out{};
    TEST_ASSERT_FALSE(calibration::loadSensorCalibrationProfile(store, out));
}

void test_load_before_any_save_reports_absent(void)
{
    FakeKeyValueStore store;
    calibration::SensorCalibrationProfile out{};
    TEST_ASSERT_FALSE(calibration::loadSensorCalibrationProfile(store, out));
}

void test_high_quality_save_never_overwritten_by_a_failed_attempt(void)
{
    FakeKeyValueStore store;
    calibration::SensorCalibrationProfile good;
    good.mag_accuracy = 3;
    good.accel_accuracy = 3;
    good.gyro_accuracy = 3;
    std::strncpy(good.firmware_version, "0.1.0", sizeof(good.firmware_version) - 1);
    TEST_ASSERT_TRUE(calibration::saveSensorCalibrationProfile(store, good));

    calibration::SensorCalibrationProfile bad;
    bad.mag_accuracy = 1;
    bad.accel_accuracy = 3;
    bad.gyro_accuracy = 3;
    TEST_ASSERT_FALSE(calibration::saveSensorCalibrationProfile(store, bad));

    calibration::SensorCalibrationProfile out{};
    TEST_ASSERT_TRUE(calibration::loadSensorCalibrationProfile(store, out));
    TEST_ASSERT_EQUAL_UINT8(3, out.mag_accuracy);
}
