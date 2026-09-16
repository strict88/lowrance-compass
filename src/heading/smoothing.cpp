#include "smoothing.h"

#include <cmath>

#include "normalize.h"

namespace heading
{

HeadingSmoother::HeadingSmoother(float stationary_alpha, float turning_alpha, float turn_rate_threshold_rad_s)
    : stationary_alpha_(stationary_alpha),
      turning_alpha_(turning_alpha),
      turn_rate_threshold_rad_s_(turn_rate_threshold_rad_s)
{
}

void HeadingSmoother::reset()
{
    initialized_ = false;
}

float HeadingSmoother::update(float heading_rad, float turn_rate_rad_s)
{
    float x = std::cos(heading_rad);
    float y = std::sin(heading_rad);

    if (!initialized_)
    {
        smoothed_x_ = x;
        smoothed_y_ = y;
        initialized_ = true;
        return normalizeAngle0To2Pi(heading_rad);
    }

    float alpha = (std::fabs(turn_rate_rad_s) >= turn_rate_threshold_rad_s_) ? turning_alpha_ : stationary_alpha_;
    smoothed_x_ += alpha * (x - smoothed_x_);
    smoothed_y_ += alpha * (y - smoothed_y_);

    return normalizeAngle0To2Pi(std::atan2(smoothed_y_, smoothed_x_));
}

}  // namespace heading
