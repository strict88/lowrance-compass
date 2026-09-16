#include "deviation_curve.h"

#include <cmath>

namespace heading
{

float evaluateDeviationRad(const DeviationCoefficients &coeffs, float heading_rad)
{
    return coeffs.a + coeffs.b * std::sin(heading_rad) + coeffs.c * std::cos(heading_rad) +
           coeffs.d * std::sin(2.0f * heading_rad) + coeffs.e * std::cos(2.0f * heading_rad);
}

}  // namespace heading
