#include <unity.h>

#include <cstring>

#include "../fakes/fake_kv_store.h"
#include "calibration/stage_a.h"
#include "calibration/stage_b.h"
#include "calibration/stage_c.h"
#include "settings/network_settings.h"

// FR-045/046: every record type's own load*() wrapper (not just
// record_envelope::load() itself) must reset a corrupted/incompatible record
// to default and report the status so a hardware-only caller can log it
// (app_task.cpp's logIfRecordCorrupted / diag_log::logRecordReset) -- this is
// the "generic recovery on boot across all four record types" gap.

void test_stage_a_wrapper_resets_corrupted_profile(void)
{
    FakeKeyValueStore store;
    calibration::SensorCalibrationProfile in;
    in.mag_accuracy = 3;
    in.accel_accuracy = 3;
    in.gyro_accuracy = 3;
    TEST_ASSERT_TRUE(calibration::saveSensorCalibrationProfile(store, in));

    store.corrupt("s0", 5, 0xFF);

    calibration::SensorCalibrationProfile out{};
    record_envelope::Status status = record_envelope::Status::kOk;
    bool ok = calibration::loadSensorCalibrationProfile(store, out, &status);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_TRUE(status == record_envelope::Status::kCrcMismatch);

    // The corrupted record must have been reset -- a fresh load reports absent.
    record_envelope::Status status_after = record_envelope::Status::kOk;
    bool ok_after = calibration::loadSensorCalibrationProfile(store, out, &status_after);
    TEST_ASSERT_FALSE(ok_after);
    TEST_ASSERT_TRUE(status_after == record_envelope::Status::kAbsent);
}

void test_stage_b_wrapper_resets_corrupted_alignment(void)
{
    FakeKeyValueStore store;
    calibration::InstallationAlignment in;
    in.mounting_offset_rad = 0.1f;
    TEST_ASSERT_TRUE(calibration::saveInstallationAlignment(store, in));

    store.corrupt("s0", 5, 0xFF);

    calibration::InstallationAlignment out{};
    record_envelope::Status status = record_envelope::Status::kOk;
    bool ok = calibration::loadInstallationAlignment(store, out, &status);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_TRUE(status == record_envelope::Status::kCrcMismatch);

    record_envelope::Status status_after = record_envelope::Status::kOk;
    bool ok_after = calibration::loadInstallationAlignment(store, out, &status_after);
    TEST_ASSERT_FALSE(ok_after);
    TEST_ASSERT_TRUE(status_after == record_envelope::Status::kAbsent);
}

void test_stage_c_wrapper_resets_corrupted_deviation(void)
{
    FakeKeyValueStore store;
    calibration::DeviationCorrection in;
    in.coefficients[0] = 0.5f;
    TEST_ASSERT_TRUE(calibration::saveDeviationCorrection(store, in));

    store.corrupt("s0", 5, 0xFF);

    calibration::DeviationCorrection out{};
    record_envelope::Status status = record_envelope::Status::kOk;
    bool ok = calibration::loadDeviationCorrection(store, out, &status);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_TRUE(status == record_envelope::Status::kCrcMismatch);

    record_envelope::Status status_after = record_envelope::Status::kOk;
    bool ok_after = calibration::loadDeviationCorrection(store, out, &status_after);
    TEST_ASSERT_FALSE(ok_after);
    TEST_ASSERT_TRUE(status_after == record_envelope::Status::kAbsent);
}

void test_network_settings_wrapper_resets_corrupted_ssid(void)
{
    FakeKeyValueStore store;
    settings::NetworkSettings in;
    std::strncpy(in.ssid, "MyBoat", sizeof(in.ssid) - 1);
    TEST_ASSERT_TRUE(settings::saveNetworkSettings(store, in));

    store.corrupt("s0", 5, 0xFF);

    settings::NetworkSettings out{};
    record_envelope::Status status = record_envelope::Status::kOk;
    bool ok = settings::loadNetworkSettings(store, out, &status);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_TRUE(status == record_envelope::Status::kCrcMismatch);

    record_envelope::Status status_after = record_envelope::Status::kOk;
    bool ok_after = settings::loadNetworkSettings(store, out, &status_after);
    TEST_ASSERT_FALSE(ok_after);
    TEST_ASSERT_TRUE(status_after == record_envelope::Status::kAbsent);
}
