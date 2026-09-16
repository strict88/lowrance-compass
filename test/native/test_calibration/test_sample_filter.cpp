#include <unity.h>

#include "calibration/sample_filter.h"
#include "thresholds.h"

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;

calibration::SwingSampleInput goodSample()
{
    calibration::SwingSampleInput s;
    s.sog_m_s = thresholds::kStageCMinSogMS + 1.0f;
    s.turn_rate_rad_s = 2.0f * kDegToRad;
    s.turn_rate_prev_rad_s = 2.0f * kDegToRad;
    s.reference_age_s = 0.1f;
    s.mag_accuracy = 3;
    s.accel_accuracy = 3;
    s.gyro_accuracy = 3;
    return s;
}
}  // namespace

void test_accepted_baseline_sample(void)
{
    TEST_ASSERT_TRUE(calibration::evaluateSample(goodSample()) == calibration::SampleRejectReason::kAccepted);
}

void test_too_slow_is_rejected(void)
{
    calibration::SwingSampleInput s = goodSample();
    s.sog_m_s = thresholds::kStageCMinSogMS * 0.5f;
    TEST_ASSERT_TRUE(calibration::evaluateSample(s) == calibration::SampleRejectReason::kTooSlow);
}

void test_turn_rate_too_fast_is_rejected(void)
{
    calibration::SwingSampleInput s = goodSample();
    s.turn_rate_rad_s = (thresholds::kStageCMaxTurnRateDegS + 5.0f) * kDegToRad;
    s.turn_rate_prev_rad_s = s.turn_rate_rad_s;
    TEST_ASSERT_TRUE(calibration::evaluateSample(s) == calibration::SampleRejectReason::kTurnTooFastOrUneven);
}

void test_turn_rate_uneven_is_rejected(void)
{
    calibration::SwingSampleInput s = goodSample();
    s.turn_rate_rad_s = 2.0f * kDegToRad;
    s.turn_rate_prev_rad_s = (2.0f + thresholds::kStageCTurnRateSteadyTolDegS + 5.0f) * kDegToRad;
    TEST_ASSERT_TRUE(calibration::evaluateSample(s) == calibration::SampleRejectReason::kTurnTooFastOrUneven);
}

void test_stale_reference_is_rejected(void)
{
    calibration::SwingSampleInput s = goodSample();
    s.reference_age_s = thresholds::kStageCReferenceMaxAgeS + 1.0f;
    TEST_ASSERT_TRUE(calibration::evaluateSample(s) == calibration::SampleRejectReason::kReferenceStale);
}

void test_low_accuracy_is_rejected(void)
{
    calibration::SwingSampleInput s = goodSample();
    s.mag_accuracy = 2;
    TEST_ASSERT_TRUE(calibration::evaluateSample(s) == calibration::SampleRejectReason::kLowSensorAccuracy);

    s = goodSample();
    s.accel_accuracy = 1;
    TEST_ASSERT_TRUE(calibration::evaluateSample(s) == calibration::SampleRejectReason::kLowSensorAccuracy);

    s = goodSample();
    s.gyro_accuracy = 0;
    TEST_ASSERT_TRUE(calibration::evaluateSample(s) == calibration::SampleRejectReason::kLowSensorAccuracy);
}

void test_mad_outlier_rejection_flags_the_clear_outlier(void)
{
    // 10 samples clustered near 2.0 deg deviation, one wild outlier at 40 deg.
    float deviations[11];
    for (int i = 0; i < 10; ++i)
    {
        deviations[i] = (2.0f + 0.1f * static_cast<float>(i % 3 - 1)) * kDegToRad;
    }
    deviations[10] = 40.0f * kDegToRad;

    bool accepted[11] = {false};
    calibration::rejectOutliers(deviations, 11, accepted);

    for (int i = 0; i < 10; ++i)
    {
        TEST_ASSERT_TRUE_MESSAGE(accepted[i], "clustered sample was incorrectly rejected");
    }
    TEST_ASSERT_FALSE_MESSAGE(accepted[10], "wild outlier was not rejected");
}

void test_mad_outlier_rejection_accepts_all_when_no_spread(void)
{
    float deviations[5] = {1.0f * kDegToRad, 1.0f * kDegToRad, 1.0f * kDegToRad, 1.0f * kDegToRad, 1.0f * kDegToRad};
    bool accepted[5] = {false};
    calibration::rejectOutliers(deviations, 5, accepted);
    for (int i = 0; i < 5; ++i)
    {
        TEST_ASSERT_TRUE(accepted[i]);
    }
}
