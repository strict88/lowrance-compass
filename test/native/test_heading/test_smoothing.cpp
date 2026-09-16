#include <unity.h>

#include <cmath>

#include "heading/normalize.h"
#include "heading/smoothing.h"

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;

// Illustrative filter tuning for this test only (production values are
// tuned/named separately when this module is wired into the pipeline).
constexpr float kStationaryAlpha = 0.05f;
constexpr float kTurningAlpha = 0.5f;
constexpr float kTurnThresholdRadS = 5.0f * kDegToRad;
constexpr float kSampleRateHz = 10.0f;
}  // namespace

// SC-003 support: stationary noise stays within a bounded peak-to-peak
// window after the filter settles.
void test_stationary_noise_bounded_over_30s(void)
{
    heading::HeadingSmoother smoother(kStationaryAlpha, kTurningAlpha, kTurnThresholdRadS);

    const int kSamples = static_cast<int>(30.0f * kSampleRateHz);
    const float kTrueHeadingDeg = 100.0f;
    const float kNoiseAmplitudeDeg = 3.0f;

    float min_deg = 1e9f;
    float max_deg = -1e9f;
    const int kSettleSamples = 50;  // skip filter warm-up

    for (int i = 0; i < kSamples; ++i)
    {
        // Deterministic pseudo-noise: sum of two incommensurate sinusoids.
        float noise_deg = kNoiseAmplitudeDeg * (0.6f * std::sin(i * 0.71f) + 0.4f * std::sin(i * 1.93f + 1.0f));
        float noisy_heading_rad = heading::normalizeAngle0To2Pi((kTrueHeadingDeg + noise_deg) * kDegToRad);

        float smoothed_rad = smoother.update(noisy_heading_rad, 0.0f);

        if (i >= kSettleSamples)
        {
            float smoothed_deg = smoothed_rad * kRadToDeg;
            if (smoothed_deg < min_deg) min_deg = smoothed_deg;
            if (smoothed_deg > max_deg) max_deg = smoothed_deg;
        }
    }

    float peak_to_peak_deg = max_deg - min_deg;
    TEST_ASSERT_TRUE_MESSAGE(peak_to_peak_deg <= 2.0f, "smoothed stationary output exceeded the bounded noise window");
}

// SC-003 support: a real turn is tracked with bounded lag, not smoothed away.
void test_turn_tracked_without_excessive_lag(void)
{
    heading::HeadingSmoother smoother(kStationaryAlpha, kTurningAlpha, kTurnThresholdRadS);

    const float kTurnRateDegS = 18.0f;
    const float kDurationS = 10.0f;
    const int kSamples = static_cast<int>(kDurationS * kSampleRateHz);
    const float kDtS = 1.0f / kSampleRateHz;

    // Prime the filter at the starting heading so the settling transient
    // (covered by the stationary test above) doesn't count as "turn lag".
    smoother.update(0.0f, 0.0f);

    float max_lag_deg = 0.0f;
    for (int i = 1; i <= kSamples; ++i)
    {
        float true_heading_deg = kTurnRateDegS * (i * kDtS);
        float true_heading_rad = heading::normalizeAngle0To2Pi(true_heading_deg * kDegToRad);
        float turn_rate_rad_s = kTurnRateDegS * kDegToRad;

        float smoothed_rad = smoother.update(true_heading_rad, turn_rate_rad_s);

        float lag_deg = std::fabs(heading::circularDifference(true_heading_rad, smoothed_rad)) * kRadToDeg;
        if (lag_deg > max_lag_deg)
        {
            max_lag_deg = lag_deg;
        }
    }

    TEST_ASSERT_TRUE_MESSAGE(max_lag_deg <= 5.0f, "turning heading lag exceeded the bounded limit");
}
