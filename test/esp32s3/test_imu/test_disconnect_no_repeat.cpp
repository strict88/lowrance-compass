// On-target Unity test: the driver-level half of "disconnect is detected
// and heading stops repeating stale data" (SC-007/FR-041). The full chain
// -- [IMU] disconnected, then valid=0 reason=SENSOR_DISCONNECTED, then no
// further [N2K] tx pgn=127250 ok lines -- spans ImuTask/N2kService running
// together in the full firmware and is asserted from outside over serial by
// tools/hil/quality_gate_check.py, not from inside a standalone Unity test
// (which runs its own setup()/loop(), not main.cpp's task graph).
//
// What *is* testable here, on-target and unattended: ImuDriverBno08x's
// isConnected() genuinely flips to false after reports stop arriving (not
// just once, permanently reporting connected because SOME earlier report
// arrived), and flips back once reports resume -- the exact mechanism
// ImuTask relies on to ever emit "[IMU] disconnected" or gate heading at
// all. A real wire disconnect is simulated by simply not polling the
// sensor, which exercises the same time-since-last-report logic a genuine
// physical disconnect would trigger.
#include <Arduino.h>
#include <unity.h>

#include "drivers/imu_driver/imu_driver_bno08x.h"

namespace
{
ImuDriverBno08x g_driver;
}  // namespace

void setUp(void) {}
void tearDown(void) {}

void test_connected_after_init_and_polling(void)
{
    TEST_ASSERT_TRUE(g_driver.init());

    uint32_t start = millis();
    while (millis() - start < 1000)
    {
        ImuReport report;
        g_driver.readReport(report);
        delay(5);
    }

    TEST_ASSERT_TRUE(g_driver.isConnected());
}

void test_disconnected_reported_after_reports_stop_and_no_repeat(void)
{
    // Deliberately stop polling for well past the disconnect timeout --
    // simulates a wire disconnect from the driver's point of view, since
    // isConnected() only knows "how long since the last report", not the
    // reason no report arrived.
    delay(1500);

    TEST_ASSERT_FALSE(g_driver.isConnected());

    // Confirm it doesn't waver back to "connected" on its own without a
    // fresh report -- i.e. it doesn't repeat/assume stale data is fine.
    delay(500);
    TEST_ASSERT_FALSE(g_driver.isConnected());
}

void test_reconnects_once_reports_resume(void)
{
    uint32_t start = millis();
    while (millis() - start < 1000)
    {
        ImuReport report;
        g_driver.readReport(report);
        delay(5);
    }

    TEST_ASSERT_TRUE(g_driver.isConnected());
}

void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_connected_after_init_and_polling);
    RUN_TEST(test_disconnected_reported_after_reports_stop_and_no_repeat);
    RUN_TEST(test_reconnects_once_reports_resume);
    UNITY_END();
}

void loop() {}
