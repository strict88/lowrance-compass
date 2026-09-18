// On-target Unity test: start Stage A, cancel mid-procedure, confirm the
// sensor's reported accuracy/behavior afterward is unchanged from before --
// i.e. cancelling never calls ImuDriver::saveDcd() (T062), so the BNO08x's
// own last-saved Dynamic Calibration Data remains the active one. Full
// physical verification (an operator confirming heading accuracy on the
// water) is a manual checklist item (checklists/manual-verification.md);
// this test only proves the automatable part -- gross sensor behavior
// continuity across a cancelled attempt.
#include <Arduino.h>
#include <unity.h>

#include "calibration/stage_a.h"
#include "drivers/imu_driver/imu_driver_bno08x.h"

namespace
{

ImuDriverBno08x g_driver;

// Polls the driver for up to `timeout_ms`, returning the last report seen
// (or a default-constructed one if none arrived).
ImuReport pollReports(uint32_t timeout_ms)
{
    ImuReport last{};
    uint32_t start = millis();
    while (millis() - start < timeout_ms)
    {
        ImuReport report;
        if (g_driver.readReport(report))
        {
            last = report;
        }
        delay(5);
    }
    return last;
}

}  // namespace

void setUp(void) {}
void tearDown(void) {}

void test_cancel_mid_stage_a_does_not_alter_sensor_behavior(void)
{
    TEST_ASSERT_TRUE(g_driver.init());

    // Let the sensor settle and record a baseline (whatever the chip's
    // already-persisted DCD currently reports).
    ImuReport baseline = pollReports(2000);

    calibration::StageA stage;
    stage.start(millis() / 1000.0f);

    // Feed it real samples for a bit -- with the board sitting still on the
    // bench this alone won't complete all three sub-steps, which is fine:
    // the point is to be mid-procedure, not finished.
    uint32_t feed_start = millis();
    while (millis() - feed_start < 3000)
    {
        ImuReport report;
        if (g_driver.readReport(report))
        {
            calibration::StageASample sample;
            sample.raw_quat = heading::Quaternion{report.quat_w, report.quat_x, report.quat_y, report.quat_z};
            sample.gyro_x_rad_s = report.gyro_x_rad_s;
            sample.gyro_y_rad_s = report.gyro_y_rad_s;
            sample.gyro_z_rad_s = report.gyro_z_rad_s;
            sample.mag_accuracy = report.mag_accuracy;
            sample.accel_accuracy = report.accel_accuracy;
            sample.gyro_accuracy = report.gyro_accuracy;
            sample.dt_s = 0.02f;
            stage.update(sample, millis() / 1000.0f);
        }
        delay(20);
    }

    TEST_ASSERT_FALSE(stage.state() == calibration::StageAState::kDone);

    stage.cancel();
    TEST_ASSERT_TRUE(stage.state() == calibration::StageAState::kCancelled);

    // Post-cancel: the chip's own DCD was never touched (saveDcd() is only
    // ever called on Evaluating->Done, see app_task.cpp), so accuracy
    // behavior should continue from where it was, not reset to zero/
    // unreliable.
    ImuReport after = pollReports(2000);

    if (baseline.mag_accuracy >= 2)
    {
        TEST_ASSERT_TRUE_MESSAGE(after.mag_accuracy > 0, "magnetometer accuracy dropped to Unreliable after cancel");
    }
    if (baseline.accel_accuracy >= 2)
    {
        TEST_ASSERT_TRUE_MESSAGE(after.accel_accuracy > 0, "accelerometer accuracy dropped to Unreliable after cancel");
    }
    if (baseline.gyro_accuracy >= 2)
    {
        TEST_ASSERT_TRUE_MESSAGE(after.gyro_accuracy > 0, "gyroscope accuracy dropped to Unreliable after cancel");
    }
}

void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_cancel_mid_stage_a_does_not_alter_sensor_behavior);
    UNITY_END();
}

void loop() {}
