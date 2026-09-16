#include <unity.h>

#include <cmath>

#include "heading/normalize.h"

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;
constexpr float kEps = 1e-4f;
}  // namespace

void test_zero_stays_zero(void)
{
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, heading::normalizeAngle0To2Pi(0.0f));
}

void test_exactly_two_pi_wraps_to_zero(void)
{
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, heading::normalizeAngle0To2Pi(kTwoPi));
}

void test_small_negative_wraps_near_two_pi(void)
{
    // -0.1 rad -> just under 2*pi.
    float result = heading::normalizeAngle0To2Pi(-0.1f);
    TEST_ASSERT_FLOAT_WITHIN(kEps, kTwoPi - 0.1f, result);
}

void test_value_past_two_pi_wraps_into_range(void)
{
    float result = heading::normalizeAngle0To2Pi(kTwoPi + 0.1f);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.1f, result);
}

void test_large_multiple_wraps_correctly(void)
{
    float result = heading::normalizeAngle0To2Pi(10.0f * kTwoPi + 1.2345f);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 1.2345f, result);
}

void test_large_negative_multiple_wraps_correctly(void)
{
    float result = heading::normalizeAngle0To2Pi(-10.0f * kTwoPi - 1.2345f);
    TEST_ASSERT_FLOAT_WITHIN(kEps, kTwoPi - 1.2345f, result);
}

void test_normalize_pi_range_positive(void)
{
    TEST_ASSERT_FLOAT_WITHIN(kEps, kPi - 0.1f, heading::normalizeAnglePi(kPi - 0.1f));
}

void test_normalize_pi_range_wraps_negative(void)
{
    // 3*pi/2 should wrap to -pi/2.
    float result = heading::normalizeAnglePi(1.5f * kPi);
    TEST_ASSERT_FLOAT_WITHIN(kEps, -0.5f * kPi, result);
}

void test_normalize_pi_boundary_stays_positive_pi(void)
{
    TEST_ASSERT_FLOAT_WITHIN(kEps, kPi, heading::normalizeAnglePi(kPi));
}

void test_circular_difference_across_zero_boundary(void)
{
    // 359 deg vs 1 deg should differ by -2 deg, not +358 deg.
    float a = heading::normalizeAngle0To2Pi(359.0f * kPi / 180.0f);
    float b = heading::normalizeAngle0To2Pi(1.0f * kPi / 180.0f);
    float diff_deg = heading::circularDifference(a, b) * 180.0f / kPi;
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -2.0f, diff_deg);
}

void test_circular_difference_symmetry(void)
{
    float a = 0.2f;
    float b = 6.0f;  // close to 2*pi, wraps around near zero
    float diff_ab = heading::circularDifference(a, b);
    float diff_ba = heading::circularDifference(b, a);
    TEST_ASSERT_FLOAT_WITHIN(kEps, -diff_ab, diff_ba);
}
