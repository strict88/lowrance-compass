#include "deviation_fit.h"

#include <cmath>

namespace calibration
{

namespace
{

constexpr float kPi = 3.14159265358979323846f;

// Gaussian elimination with partial pivoting for a 5x5 system a*x = b.
// Returns false if the system is singular (within tolerance).
bool solve5x5(double a[5][5], double b[5], double x[5])
{
    for (int col = 0; col < 5; ++col)
    {
        int pivot_row = col;
        double max_val = std::fabs(a[col][col]);
        for (int row = col + 1; row < 5; ++row)
        {
            if (std::fabs(a[row][col]) > max_val)
            {
                max_val = std::fabs(a[row][col]);
                pivot_row = row;
            }
        }
        if (max_val < 1e-12)
        {
            return false;
        }
        if (pivot_row != col)
        {
            for (int k = 0; k < 5; ++k)
            {
                double tmp = a[col][k];
                a[col][k] = a[pivot_row][k];
                a[pivot_row][k] = tmp;
            }
            double tmp_b = b[col];
            b[col] = b[pivot_row];
            b[pivot_row] = tmp_b;
        }

        for (int row = col + 1; row < 5; ++row)
        {
            double factor = a[row][col] / a[col][col];
            for (int k = col; k < 5; ++k)
            {
                a[row][k] -= factor * a[col][k];
            }
            b[row] -= factor * b[col];
        }
    }

    for (int row = 4; row >= 0; --row)
    {
        double sum = b[row];
        for (int k = row + 1; k < 5; ++k)
        {
            sum -= a[row][k] * x[k];
        }
        x[row] = sum / a[row][row];
    }
    return true;
}

}  // namespace

DeviationFitResult fitDeviationCurve(const SectorAggregate *sectors, int sector_count)
{
    DeviationFitResult result;

    double ata[5][5] = {{0}};
    double atb[5] = {0, 0, 0, 0, 0};
    int populated = 0;

    for (int i = 0; i < sector_count; ++i)
    {
        if (!sectors[i].has_data)
        {
            continue;
        }
        double th = sectors[i].theta_rad;
        double f[5] = {1.0, std::sin(th), std::cos(th), std::sin(2.0 * th), std::cos(2.0 * th)};
        double y = sectors[i].mean_deviation_rad;
        for (int r = 0; r < 5; ++r)
        {
            atb[r] += f[r] * y;
            for (int c = 0; c < 5; ++c)
            {
                ata[r][c] += f[r] * f[c];
            }
        }
        ++populated;
    }

    if (populated < 5)
    {
        result.success = false;
        return result;
    }

    double x[5] = {0, 0, 0, 0, 0};
    if (!solve5x5(ata, atb, x))
    {
        result.success = false;
        return result;
    }

    result.coefficients.a = static_cast<float>(x[0]);
    result.coefficients.b = static_cast<float>(x[1]);
    result.coefficients.c = static_cast<float>(x[2]);
    result.coefficients.d = static_cast<float>(x[3]);
    result.coefficients.e = static_cast<float>(x[4]);

    double sq_sum = 0.0;
    for (int i = 0; i < sector_count; ++i)
    {
        if (!sectors[i].has_data)
        {
            continue;
        }
        float pred = heading::evaluateDeviationRad(result.coefficients, sectors[i].theta_rad);
        double resid = static_cast<double>(pred) - sectors[i].mean_deviation_rad;
        sq_sum += resid * resid;
    }
    result.residual_rms_rad = static_cast<float>(std::sqrt(sq_sum / populated));

    float max_abs = 0.0f;
    constexpr int kSweepSteps = 360;
    for (int i = 0; i < kSweepSteps; ++i)
    {
        float th = (2.0f * kPi) * (static_cast<float>(i) / static_cast<float>(kSweepSteps));
        float v = std::fabs(heading::evaluateDeviationRad(result.coefficients, th));
        if (v > max_abs)
        {
            max_abs = v;
        }
    }
    result.max_abs_deviation_rad = max_abs;

    result.success = true;
    return result;
}

}  // namespace calibration
