#include <unity.h>

#include "../fakes/fake_clock.h"
#include "services/calibration_service.h"

void test_start_when_idle_succeeds(void)
{
    FakeClock clock;
    CalibrationService service(clock);

    CalibrationService::Stage busy;
    bool started = service.tryStart(CalibrationService::Stage::kA, busy);

    TEST_ASSERT_TRUE(started);
    TEST_ASSERT_TRUE(service.activeStage() == CalibrationService::Stage::kA);
}

void test_second_start_while_active_is_refused(void)
{
    FakeClock clock;
    CalibrationService service(clock);

    CalibrationService::Stage busy;
    TEST_ASSERT_TRUE(service.tryStart(CalibrationService::Stage::kA, busy));

    // A second start attempt for a different stage while A is active MUST be
    // refused (FR-006), reporting which stage is busy -- this is the
    // evidence a caller uses to emit "[CAL] busy stage=A rejected=B".
    bool started_b = service.tryStart(CalibrationService::Stage::kB, busy);

    TEST_ASSERT_FALSE(started_b);
    TEST_ASSERT_TRUE(busy == CalibrationService::Stage::kA);
    TEST_ASSERT_TRUE(service.activeStage() == CalibrationService::Stage::kA);
}

void test_cancel_frees_the_session_for_a_new_start(void)
{
    FakeClock clock;
    CalibrationService service(clock);

    CalibrationService::Stage busy;
    TEST_ASSERT_TRUE(service.tryStart(CalibrationService::Stage::kA, busy));
    service.cancel(CalibrationService::Stage::kA);

    TEST_ASSERT_TRUE(service.activeStage() == CalibrationService::Stage::kNone);
    TEST_ASSERT_TRUE(service.tryStart(CalibrationService::Stage::kB, busy));
}

void test_cancel_wrong_stage_is_a_no_op(void)
{
    FakeClock clock;
    CalibrationService service(clock);

    CalibrationService::Stage busy;
    TEST_ASSERT_TRUE(service.tryStart(CalibrationService::Stage::kA, busy));
    service.cancel(CalibrationService::Stage::kB);  // not the active stage

    TEST_ASSERT_TRUE(service.activeStage() == CalibrationService::Stage::kA);
}

void test_inactivity_timeout_auto_cancels(void)
{
    FakeClock clock;
    CalibrationService service(clock);

    CalibrationService::Stage busy;
    clock.setMillisForTest(1000);
    TEST_ASSERT_TRUE(service.tryStart(CalibrationService::Stage::kC, busy));

    clock.setMillisForTest(1000 + 60 * 1000);  // well under the timeout
    TEST_ASSERT_TRUE(service.checkInactivityTimeout() == CalibrationService::Stage::kNone);
    TEST_ASSERT_TRUE(service.activeStage() == CalibrationService::Stage::kC);

    clock.setMillisForTest(1000 + 1000 * 1000);  // well past the timeout
    TEST_ASSERT_TRUE(service.checkInactivityTimeout() == CalibrationService::Stage::kC);
    TEST_ASSERT_TRUE(service.activeStage() == CalibrationService::Stage::kNone);
}

void test_client_activity_resets_the_inactivity_window(void)
{
    FakeClock clock;
    CalibrationService service(clock);

    CalibrationService::Stage busy;
    clock.setMillisForTest(0);
    TEST_ASSERT_TRUE(service.tryStart(CalibrationService::Stage::kA, busy));

    clock.setMillisForTest(1000 * 1000);
    service.noteClientActivity();

    clock.setMillisForTest(1000 * 1000 + 60 * 1000);
    TEST_ASSERT_TRUE(service.checkInactivityTimeout() == CalibrationService::Stage::kNone);
    TEST_ASSERT_TRUE(service.activeStage() == CalibrationService::Stage::kA);
}
