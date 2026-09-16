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

    return UNITY_END();
}
