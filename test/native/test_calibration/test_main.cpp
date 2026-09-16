// Shared Unity entry point for every test_*.cpp in this directory --
// PlatformIO compiles all files under one test_dir subdirectory into a
// single binary, so only one main()/setUp()/tearDown() may exist here.
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

// test_session_singleton.cpp
void test_start_when_idle_succeeds(void);
void test_second_start_while_active_is_refused(void);
void test_cancel_frees_the_session_for_a_new_start(void);
void test_cancel_wrong_stage_is_a_no_op(void);
void test_inactivity_timeout_auto_cancels(void);
void test_client_activity_resets_the_inactivity_window(void);

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_start_when_idle_succeeds);
    RUN_TEST(test_second_start_while_active_is_refused);
    RUN_TEST(test_cancel_frees_the_session_for_a_new_start);
    RUN_TEST(test_cancel_wrong_stage_is_a_no_op);
    RUN_TEST(test_inactivity_timeout_auto_cancels);
    RUN_TEST(test_client_activity_resets_the_inactivity_window);

    return UNITY_END();
}
