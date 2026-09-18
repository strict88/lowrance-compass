// Shared Unity entry point for every test_*.cpp in this directory --
// PlatformIO compiles all files under one test_dir subdirectory into a
// single binary, so only one main()/setUp()/tearDown() may exist here.
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

// test_normalize.cpp
void test_zero_stays_zero(void);
void test_exactly_two_pi_wraps_to_zero(void);
void test_small_negative_wraps_near_two_pi(void);
void test_value_past_two_pi_wraps_into_range(void);
void test_large_multiple_wraps_correctly(void);
void test_large_negative_multiple_wraps_correctly(void);
void test_normalize_pi_range_positive(void);
void test_normalize_pi_range_wraps_negative(void);
void test_normalize_pi_boundary_stays_positive_pi(void);
void test_circular_difference_across_zero_boundary(void);
void test_circular_difference_symmetry(void);

// test_pipeline.cpp
void test_raw_quat_only_yields_matching_heading(void);
void test_level_reference_applied_before_offset(void);
void test_mounting_offset_adds_after_level(void);
void test_deviation_evaluated_after_offset_not_before(void);
void test_result_normalized_into_0_360(void);
void test_identity_corrections_when_stages_not_done(void);

// test_quality_gate.cpp
void test_all_high_passes(void);
void test_mag_below_high_still_valid_with_warning_reason(void);
void test_accel_below_high_still_valid(void);
void test_gyro_below_high_still_valid(void);
void test_never_calibrated_reason(void);
void test_disconnected_overrides_everything(void);

// test_quality_gate_stage_a_inflight.cpp
void test_live_drop_during_stage_a_does_not_invalidate_when_profile_exists(void);
void test_no_freeze_without_a_saved_profile(void);
void test_freeze_does_not_apply_to_stage_b(void);
void test_freeze_does_not_apply_to_stage_c(void);
void test_freeze_does_not_apply_when_no_stage_active(void);

// test_smoothing.cpp
void test_stationary_noise_bounded_over_30s(void);
void test_turn_tracked_without_excessive_lag(void);

// test_wraparound_e2e.cpp
void test_wraparound_from_below(void);
void test_wraparound_via_mounting_offset_push_past_360(void);
void test_wraparound_via_mounting_offset_pull_below_zero(void);
void test_continuity_stepping_through_the_boundary(void);
void test_exactly_zero_and_exactly_360_both_normalize_to_zero(void);

// test_smoothing_bounds.cpp
void test_stationary_noise_bounded_straddling_the_wrap_boundary(void);

// test_single_source.cpp
void test_two_independent_readers_of_the_same_published_value_see_identical_fields(void);
void test_computation_is_deterministic_for_the_same_input(void);

// test_pipeline_with_stage_b.cpp
void test_absent_installation_alignment_yields_identity_zero(void);
void test_present_installation_alignment_carries_through_to_the_pipeline(void);
void test_nonidentity_level_reference_is_applied(void);

// test_pipeline_with_stage_c.cpp
void test_absent_deviation_correction_yields_zero_curve(void);
void test_present_deviation_correction_is_applied_at_compass_heading(void);
void test_deviation_correction_matches_evaluateDeviationRad_directly(void);

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_zero_stays_zero);
    RUN_TEST(test_exactly_two_pi_wraps_to_zero);
    RUN_TEST(test_small_negative_wraps_near_two_pi);
    RUN_TEST(test_value_past_two_pi_wraps_into_range);
    RUN_TEST(test_large_multiple_wraps_correctly);
    RUN_TEST(test_large_negative_multiple_wraps_correctly);
    RUN_TEST(test_normalize_pi_range_positive);
    RUN_TEST(test_normalize_pi_range_wraps_negative);
    RUN_TEST(test_normalize_pi_boundary_stays_positive_pi);
    RUN_TEST(test_circular_difference_across_zero_boundary);
    RUN_TEST(test_circular_difference_symmetry);

    RUN_TEST(test_raw_quat_only_yields_matching_heading);
    RUN_TEST(test_level_reference_applied_before_offset);
    RUN_TEST(test_mounting_offset_adds_after_level);
    RUN_TEST(test_deviation_evaluated_after_offset_not_before);
    RUN_TEST(test_result_normalized_into_0_360);
    RUN_TEST(test_identity_corrections_when_stages_not_done);

    RUN_TEST(test_all_high_passes);
    RUN_TEST(test_mag_below_high_still_valid_with_warning_reason);
    RUN_TEST(test_accel_below_high_still_valid);
    RUN_TEST(test_gyro_below_high_still_valid);
    RUN_TEST(test_never_calibrated_reason);
    RUN_TEST(test_disconnected_overrides_everything);

    RUN_TEST(test_live_drop_during_stage_a_does_not_invalidate_when_profile_exists);
    RUN_TEST(test_no_freeze_without_a_saved_profile);
    RUN_TEST(test_freeze_does_not_apply_to_stage_b);
    RUN_TEST(test_freeze_does_not_apply_to_stage_c);
    RUN_TEST(test_freeze_does_not_apply_when_no_stage_active);

    RUN_TEST(test_stationary_noise_bounded_over_30s);
    RUN_TEST(test_turn_tracked_without_excessive_lag);

    RUN_TEST(test_wraparound_from_below);
    RUN_TEST(test_wraparound_via_mounting_offset_push_past_360);
    RUN_TEST(test_wraparound_via_mounting_offset_pull_below_zero);
    RUN_TEST(test_continuity_stepping_through_the_boundary);
    RUN_TEST(test_exactly_zero_and_exactly_360_both_normalize_to_zero);

    RUN_TEST(test_stationary_noise_bounded_straddling_the_wrap_boundary);

    RUN_TEST(test_two_independent_readers_of_the_same_published_value_see_identical_fields);
    RUN_TEST(test_computation_is_deterministic_for_the_same_input);

    RUN_TEST(test_absent_installation_alignment_yields_identity_zero);
    RUN_TEST(test_present_installation_alignment_carries_through_to_the_pipeline);
    RUN_TEST(test_nonidentity_level_reference_is_applied);

    RUN_TEST(test_absent_deviation_correction_yields_zero_curve);
    RUN_TEST(test_present_deviation_correction_is_applied_at_compass_heading);
    RUN_TEST(test_deviation_correction_matches_evaluateDeviationRad_directly);

    return UNITY_END();
}
