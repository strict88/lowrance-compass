#pragma once

// The Stage C deviation model (data-model.md §1.3), shared between the
// heading pipeline (which applies an already-fitted curve) and
// calibration/deviation_fit.cpp (which fits one). Kept here, in the earliest
// module that needs it, rather than duplicated.
namespace heading
{

struct DeviationCoefficients
{
    float a = 0.0f;
    float b = 0.0f;
    float c = 0.0f;
    float d = 0.0f;
    float e = 0.0f;
};

// deviation(theta) = A + B*sin(theta) + C*cos(theta) + D*sin(2*theta) + E*cos(2*theta)
// `heading_rad` is the uncorrected (compass) heading the curve was fitted
// against; the result is added to it to produce the corrected heading.
float evaluateDeviationRad(const DeviationCoefficients &coeffs, float heading_rad);

}  // namespace heading
