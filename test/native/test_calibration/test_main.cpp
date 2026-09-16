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

// test_stage_a_stillness.cpp
void test_stillness_holds_through_to_awaiting_positions(void);
void test_movement_resets_the_stillness_timer(void);

// test_stage_a_positions.cpp
void test_one_position_held_gets_marked_done(void);
void test_two_different_positions_are_independently_marked(void);
void test_all_six_positions_in_any_order_transition_to_rotation(void);
void test_brief_hold_below_threshold_does_not_mark_done(void);

// test_stage_a_rotation.cpp
void test_rotation_coverage_reaches_threshold_from_synthetic_sweep(void);
void test_rotation_coverage_starts_at_zero_on_entry(void);

// test_stage_a_accept.cpp
void test_all_high_accuracy_reaches_done(void);
void test_below_high_accuracy_never_reaches_done(void);

// test_stage_a_timeout_cancel.cpp
void test_timeout_fires_after_the_configured_duration(void);
void test_retry_after_timeout_starts_clean(void);
void test_cancel_from_awaiting_stillness(void);
void test_cancel_from_awaiting_positions(void);
void test_cancel_never_reports_done(void);
void test_restart_after_cancel_starts_clean(void);

// test_stage_a_persistence.cpp
void test_save_and_load_round_trip(void);
void test_below_high_accuracy_refuses_to_save(void);
void test_load_before_any_save_reports_absent(void);
void test_high_quality_save_never_overwritten_by_a_failed_attempt(void);

// test_readiness_summary.cpp
void test_stage_status_derives_purely_from_record_presence(void);
void test_readiness_none_done(void);
void test_readiness_only_a_done(void);
void test_readiness_a_and_b_done(void);
void test_readiness_all_three_done(void);
void test_readiness_withheld_overrides_all_done(void);
void test_readiness_withheld_overrides_partial_done(void);
void test_stage_status_never_flips_without_a_record_change(void);

// test_reset_confirmation.cpp
void test_reset_without_confirm_leaves_record_unchanged(void);
void test_reset_with_confirm_clears_the_record(void);
void test_reset_only_clears_the_targeted_stage(void);

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

    RUN_TEST(test_stillness_holds_through_to_awaiting_positions);
    RUN_TEST(test_movement_resets_the_stillness_timer);

    RUN_TEST(test_one_position_held_gets_marked_done);
    RUN_TEST(test_two_different_positions_are_independently_marked);
    RUN_TEST(test_all_six_positions_in_any_order_transition_to_rotation);
    RUN_TEST(test_brief_hold_below_threshold_does_not_mark_done);

    RUN_TEST(test_rotation_coverage_reaches_threshold_from_synthetic_sweep);
    RUN_TEST(test_rotation_coverage_starts_at_zero_on_entry);

    RUN_TEST(test_all_high_accuracy_reaches_done);
    RUN_TEST(test_below_high_accuracy_never_reaches_done);

    RUN_TEST(test_timeout_fires_after_the_configured_duration);
    RUN_TEST(test_retry_after_timeout_starts_clean);
    RUN_TEST(test_cancel_from_awaiting_stillness);
    RUN_TEST(test_cancel_from_awaiting_positions);
    RUN_TEST(test_cancel_never_reports_done);
    RUN_TEST(test_restart_after_cancel_starts_clean);

    RUN_TEST(test_save_and_load_round_trip);
    RUN_TEST(test_below_high_accuracy_refuses_to_save);
    RUN_TEST(test_load_before_any_save_reports_absent);
    RUN_TEST(test_high_quality_save_never_overwritten_by_a_failed_attempt);

    RUN_TEST(test_stage_status_derives_purely_from_record_presence);
    RUN_TEST(test_readiness_none_done);
    RUN_TEST(test_readiness_only_a_done);
    RUN_TEST(test_readiness_a_and_b_done);
    RUN_TEST(test_readiness_all_three_done);
    RUN_TEST(test_readiness_withheld_overrides_all_done);
    RUN_TEST(test_readiness_withheld_overrides_partial_done);
    RUN_TEST(test_stage_status_never_flips_without_a_record_change);

    RUN_TEST(test_reset_without_confirm_leaves_record_unchanged);
    RUN_TEST(test_reset_with_confirm_clears_the_record);
    RUN_TEST(test_reset_only_clears_the_targeted_stage);

    return UNITY_END();
}
