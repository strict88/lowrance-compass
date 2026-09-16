#include <unity.h>

#include <cmath>

#include "calibration/deviation_fit.h"

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kRadToDeg = 180.0f / kPi;

heading::DeviationCoefficients knownCoeffs()
{
    heading::DeviationCoefficients c;
    c.a = 1.0f * (kPi / 180.0f);
    c.b = 3.0f * (kPi / 180.0f);
    c.c = -2.0f * (kPi / 180.0f);
    c.d = 1.5f * (kPi / 180.0f);
    c.e = -1.0f * (kPi / 180.0f);
    return c;
}

// Deterministic pseudo-noise generator (no <random> dependency needed).
float pseudoNoise(int i, float amplitude_rad)
{
    return amplitude_rad * std::sin(static_cast<float>(i) * 12.9898f);
}
}  // namespace

void test_clean_synthetic_data_recovers_known_coefficients(void)
{
    heading::DeviationCoefficients truth = knownCoeffs();
    calibration::SectorAggregate sectors[36];
    for (int i = 0; i < 36; ++i)
    {
        float theta = (2.0f * kPi) * (static_cast<float>(i) / 36.0f);
        sectors[i].has_data = true;
        sectors[i].theta_rad = theta;
        sectors[i].mean_deviation_rad = heading::evaluateDeviationRad(truth, theta);
    }

    calibration::DeviationFitResult result = calibration::fitDeviationCurve(sectors, 36);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, truth.a, result.coefficients.a);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, truth.b, result.coefficients.b);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, truth.c, result.coefficients.c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, truth.d, result.coefficients.d);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, truth.e, result.coefficients.e);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, result.residual_rms_rad * kRadToDeg);
}

void test_noisy_synthetic_data_recovers_coefficients_approximately(void)
{
    heading::DeviationCoefficients truth = knownCoeffs();
    calibration::SectorAggregate sectors[36];
    for (int i = 0; i < 36; ++i)
    {
        float theta = (2.0f * kPi) * (static_cast<float>(i) / 36.0f);
        sectors[i].has_data = true;
        sectors[i].theta_rad = theta;
        float noise = pseudoNoise(i, 0.3f * (kPi / 180.0f));  // +-0.3 deg noise
        sectors[i].mean_deviation_rad = heading::evaluateDeviationRad(truth, theta) + noise;
    }

    calibration::DeviationFitResult result = calibration::fitDeviationCurve(sectors, 36);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, truth.a * kRadToDeg, result.coefficients.a * kRadToDeg);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, truth.b * kRadToDeg, result.coefficients.b * kRadToDeg);
    TEST_ASSERT_TRUE(result.residual_rms_rad * kRadToDeg < 1.0f);
}

void test_fewer_than_five_populated_sectors_fails(void)
{
    calibration::SectorAggregate sectors[36] = {};
    for (int i = 0; i < 4; ++i)
    {
        sectors[i].has_data = true;
        sectors[i].theta_rad = (2.0f * kPi) * (static_cast<float>(i) / 36.0f);
        sectors[i].mean_deviation_rad = 0.0f;
    }

    calibration::DeviationFitResult result = calibration::fitDeviationCurve(sectors, 36);
    TEST_ASSERT_FALSE(result.success);
}

void test_missing_sectors_but_enough_coverage_still_fits(void)
{
    heading::DeviationCoefficients truth = knownCoeffs();
    calibration::SectorAggregate sectors[36] = {};
    // Populate only every third sector (12 of 36), still well-distributed
    // around the circle.
    for (int i = 0; i < 36; i += 3)
    {
        float theta = (2.0f * kPi) * (static_cast<float>(i) / 36.0f);
        sectors[i].has_data = true;
        sectors[i].theta_rad = theta;
        sectors[i].mean_deviation_rad = heading::evaluateDeviationRad(truth, theta);
    }

    calibration::DeviationFitResult result = calibration::fitDeviationCurve(sectors, 36);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, truth.b * kRadToDeg, result.coefficients.b * kRadToDeg);
}

void test_bad_outlier_sector_skews_less_when_isolated(void)
{
    // Sanity check that the fit doesn't crash or blow up with one wildly
    // wrong sector value mixed into otherwise-clean data (sample_filter.cpp
    // is what's responsible for excluding true outliers before they reach
    // the fit; this just proves fitDeviationCurve() itself is well-behaved
    // given imperfect input).
    heading::DeviationCoefficients truth = knownCoeffs();
    calibration::SectorAggregate sectors[36];
    for (int i = 0; i < 36; ++i)
    {
        float theta = (2.0f * kPi) * (static_cast<float>(i) / 36.0f);
        sectors[i].has_data = true;
        sectors[i].theta_rad = theta;
        sectors[i].mean_deviation_rad = heading::evaluateDeviationRad(truth, theta);
    }
    sectors[10].mean_deviation_rad += 20.0f * (kPi / 180.0f);  // one bad sector

    calibration::DeviationFitResult result = calibration::fitDeviationCurve(sectors, 36);

    TEST_ASSERT_TRUE(result.success);
    // Residual should reflect the outlier's disagreement, not be near zero.
    TEST_ASSERT_TRUE(result.residual_rms_rad * kRadToDeg > 0.5f);
}
