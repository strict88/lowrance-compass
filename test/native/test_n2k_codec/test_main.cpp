// Shared Unity entry point for every test_*.cpp in this directory --
// PlatformIO compiles all files under one test_dir subdirectory into a
// single binary, so only one main()/setUp()/tearDown() may exist here.
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

// test_pgn_codec.cpp
void test_vessel_heading_round_trip_basic(void);
void test_vessel_heading_range_boundaries(void);
void test_vessel_heading_absent_deviation_variation(void);
void test_rate_of_turn_round_trip_positive_and_negative(void);
void test_attitude_round_trip_all_fields(void);
void test_attitude_absent_fields(void);
void test_cog_sog_round_trip(void);
void test_cog_sog_unavailable(void);
void test_magnetic_variation_round_trip(void);
void test_magnetic_variation_absent_fields(void);

// test_transmit_gate.cpp
void test_valid_heading_is_sent(void);
void test_invalid_heading_is_never_sent(void);
void test_sent_frame_encodes_pgn_127250_and_heading(void);
void test_can_id_pdu2_broadcast_formula(void);
void test_no_frame_sent_when_can_bus_rejects_send(void);

// test_variation_source.cpp
void test_bus_variation_preferred_when_fresh(void);
void test_manual_used_when_bus_never_received(void);
void test_manual_used_when_bus_value_is_stale(void);
void test_bus_value_exactly_at_max_age_still_counts_as_fresh(void);
void test_neither_available_fails(void);

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_vessel_heading_round_trip_basic);
    RUN_TEST(test_vessel_heading_range_boundaries);
    RUN_TEST(test_vessel_heading_absent_deviation_variation);
    RUN_TEST(test_rate_of_turn_round_trip_positive_and_negative);
    RUN_TEST(test_attitude_round_trip_all_fields);
    RUN_TEST(test_attitude_absent_fields);
    RUN_TEST(test_cog_sog_round_trip);
    RUN_TEST(test_cog_sog_unavailable);
    RUN_TEST(test_magnetic_variation_round_trip);
    RUN_TEST(test_magnetic_variation_absent_fields);

    RUN_TEST(test_valid_heading_is_sent);
    RUN_TEST(test_invalid_heading_is_never_sent);
    RUN_TEST(test_sent_frame_encodes_pgn_127250_and_heading);
    RUN_TEST(test_can_id_pdu2_broadcast_formula);
    RUN_TEST(test_no_frame_sent_when_can_bus_rejects_send);

    RUN_TEST(test_bus_variation_preferred_when_fresh);
    RUN_TEST(test_manual_used_when_bus_never_received);
    RUN_TEST(test_manual_used_when_bus_value_is_stale);
    RUN_TEST(test_bus_value_exactly_at_max_age_still_counts_as_fresh);
    RUN_TEST(test_neither_available_fails);

    return UNITY_END();
}
