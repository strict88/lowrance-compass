#pragma once

#include "heading/deviation_curve.h"

// Least-squares fit of deviation(theta) = A + B*sin(theta) + C*cos(theta) +
// D*sin(2*theta) + E*cos(2*theta) (data-model.md §1.3), solving the 5x5
// normal equations with no dynamic allocation. Sector-balanced: each
// populated sector contributes exactly one (mean) data point regardless of
// how many raw samples landed in it, so no single sector's sample count can
// dominate the fit. Pure logic, native-testable.
namespace calibration
{

struct SectorAggregate
{
    bool has_data = false;
    float theta_rad = 0.0f;           // representative heading for this sector (e.g. its mean sample heading)
    float mean_deviation_rad = 0.0f;  // mean of (reference - raw heading) for accepted samples in this sector
};

struct DeviationFitResult
{
    bool success = false;  // false if fewer than 5 populated sectors (underdetermined) or a singular system
    heading::DeviationCoefficients coefficients;
    float residual_rms_rad = 0.0f;
    float max_abs_deviation_rad = 0.0f;  // over a full sweep of the fitted curve, not just the sector points
};

DeviationFitResult fitDeviationCurve(const SectorAggregate *sectors, int sector_count);

}  // namespace calibration
