#include "normalize.h"

#include <cmath>

namespace heading
{

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;
}  // namespace

float normalizeAngle0To2Pi(float angle_rad)
{
    float a = std::fmod(angle_rad, kTwoPi);
    if (a < 0.0f)
    {
        a += kTwoPi;
    }
    if (a < 0.0f || a >= kTwoPi)
    {
        // Guards against fmod's floating-point edge cases so the result is
        // always strictly within [0, 2*pi).
        a = 0.0f;
    }
    return a;
}

float normalizeAnglePi(float angle_rad)
{
    float a = normalizeAngle0To2Pi(angle_rad);
    if (a > kPi)
    {
        a -= kTwoPi;
    }
    return a;
}

float circularDifference(float a_rad, float b_rad)
{
    return normalizeAnglePi(a_rad - b_rad);
}

}  // namespace heading
