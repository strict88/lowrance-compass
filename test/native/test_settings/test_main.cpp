// Shared Unity entry point for every test_*.cpp in this directory --
// PlatformIO compiles all files under one test_dir subdirectory into a
// single binary, so only one main()/setUp()/tearDown() may exist here.
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

// test_record_envelope.cpp
void test_round_trip_valid_record(void);
void test_absent_before_any_save(void);
void test_crc_mismatch_resets_only_that_record(void);
void test_schema_mismatch(void);
void test_bad_save_never_overwrites_previous_good_record(void);
void test_second_save_flips_active_slot_and_first_slot_survives(void);
void test_reset_to_default_clears_record(void);

// test_ssid_validation.cpp
void test_empty_string_is_invalid(void);
void test_exactly_one_char_is_valid(void);
void test_exactly_thirty_two_chars_is_valid(void);
void test_thirty_three_chars_is_invalid(void);
void test_leading_space_is_invalid(void);
void test_trailing_space_is_invalid(void);
void test_valid_mixed_content_ssid(void);
void test_null_pointer_is_invalid(void);
void test_internal_space_is_valid(void);

// test_concurrent_save.cpp
void test_second_save_wins_and_first_clients_next_load_sees_it(void);
void test_an_invalid_second_save_does_not_supersede_the_valid_first(void);

// test_settings_service.cpp
void test_init_without_a_saved_record_uses_factory_default(void);
void test_save_schedules_restart_after_applies_in_s_not_immediately(void);
void test_invalid_ssid_does_not_schedule_a_restart(void);
void test_clear_pending_restart_stops_it_from_firing_again(void);
void test_reset_to_default_restarts_immediately(void);

// test_corrupted_record_recovery.cpp
void test_stage_a_wrapper_resets_corrupted_profile(void);
void test_stage_b_wrapper_resets_corrupted_alignment(void);
void test_stage_c_wrapper_resets_corrupted_deviation(void);
void test_network_settings_wrapper_resets_corrupted_ssid(void);

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_round_trip_valid_record);
    RUN_TEST(test_absent_before_any_save);
    RUN_TEST(test_crc_mismatch_resets_only_that_record);
    RUN_TEST(test_schema_mismatch);
    RUN_TEST(test_bad_save_never_overwrites_previous_good_record);
    RUN_TEST(test_second_save_flips_active_slot_and_first_slot_survives);
    RUN_TEST(test_reset_to_default_clears_record);

    RUN_TEST(test_empty_string_is_invalid);
    RUN_TEST(test_exactly_one_char_is_valid);
    RUN_TEST(test_exactly_thirty_two_chars_is_valid);
    RUN_TEST(test_thirty_three_chars_is_invalid);
    RUN_TEST(test_leading_space_is_invalid);
    RUN_TEST(test_trailing_space_is_invalid);
    RUN_TEST(test_valid_mixed_content_ssid);
    RUN_TEST(test_null_pointer_is_invalid);
    RUN_TEST(test_internal_space_is_valid);

    RUN_TEST(test_second_save_wins_and_first_clients_next_load_sees_it);
    RUN_TEST(test_an_invalid_second_save_does_not_supersede_the_valid_first);

    RUN_TEST(test_init_without_a_saved_record_uses_factory_default);
    RUN_TEST(test_save_schedules_restart_after_applies_in_s_not_immediately);
    RUN_TEST(test_invalid_ssid_does_not_schedule_a_restart);
    RUN_TEST(test_clear_pending_restart_stops_it_from_firing_again);
    RUN_TEST(test_reset_to_default_restarts_immediately);

    RUN_TEST(test_stage_a_wrapper_resets_corrupted_profile);
    RUN_TEST(test_stage_b_wrapper_resets_corrupted_alignment);
    RUN_TEST(test_stage_c_wrapper_resets_corrupted_deviation);
    RUN_TEST(test_network_settings_wrapper_resets_corrupted_ssid);

    return UNITY_END();
}
