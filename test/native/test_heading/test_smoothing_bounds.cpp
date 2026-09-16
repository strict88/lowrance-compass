#include <unity.h>

#include <cmath>

#include "heading/normalize.h"
#include "heading/smoothing.h"

// SC-003's stationary peak-to-peak bound, specifically at the 0/360deg
// wraparound (T029's test_smoothing.cpp covers the general case away from
// the boundary; this covers the case the unit-vector-based smoother design
// exists to handle correctly -- naive linear averaging of a raw angle would
// fail badly right at the wrap).
namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;

constexpr float kStationaryAlpha = 0.05f;
constexpr float kTurningAlpha = 0.5f;
constexpr float kTurnThresholdRadS = 5.0f * kDegToRad;
constexpr float kSampleRateHz = 10.0f;
}  // namespace

void test_stationary_noise_bounded_straddling_the_wrap_boundary(void)
{
    heading::HeadingSmoother smoother(kStationaryAlpha, kTurningAlpha, kTurnThresholdRadS);

    const int kSamples = static_cast<int>(30.0f * kSampleRateHz);
    const float kTrueHeadingDeg = 0.0f;  // sits exactly on the wrap boundary
    const float kNoiseAmplitudeDeg = 3.0f;
    const int kSettleSamples = 50;

    float min_x = 2.0f, max_x = -2.0f;  // track via unit-vector components,
    float min_y = 2.0f, max_y = -2.0f;  // avoiding a false "jump" at 0/360 in degrees

    for (int i = 0; i < kSamples; ++i)
    {
        float noise_deg = kNoiseAmplitudeDeg * (0.6f * std::sin(i * 0.71f) + 0.4f * std::sin(i * 1.93f + 1.0f));
        float noisy_heading_rad = heading::normalizeAngle0To2Pi((kTrueHeadingDeg + noise_deg) * kDegToRad);

        float smoothed_rad = smoother.update(noisy_heading_rad, 0.0f);

        if (i >= kSettleSamples)
        {
            float x = std::cos(smoothed_rad);
            float y = std::sin(smoothed_rad);
            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
        }
    }

    // A peak-to-peak bound in unit-circle space is equivalent to a bound in
    // angle space near the boundary (for a small window), without the false
    // "359 deg to 1 deg is a 358 deg swing" artifact of comparing raw
    // degrees across the wrap.
    float span_deg_x = std::asin(std::fmin(1.0f, (max_x - min_x) / 2.0f)) * 2.0f * kRadToDeg;
    float span_deg_y = std::asin(std::fmin(1.0f, (max_y - min_y) / 2.0f)) * 2.0f * kRadToDeg;

    TEST_ASSERT_TRUE_MESSAGE(span_deg_x <= 2.0f, "smoothed output x-component exceeded the bounded noise window at the wrap");
    TEST_ASSERT_TRUE_MESSAGE(span_deg_y <= 2.0f, "smoothed output y-component exceeded the bounded noise window at the wrap");
}
