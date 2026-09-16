#pragma once

// Circular-value-aware smoothing filter (FR-043): reduces stationary noise
// without adding noticeable lag during turns. Pure logic, native-testable.
namespace heading
{

class HeadingSmoother
{
public:
    // `stationary_alpha` (0,1]: exponential-moving-average weight given to a
    // new sample while judged stationary (smaller = more smoothing).
    // `turning_alpha`: weight used instead once |turn_rate_rad_s| exceeds
    // `turn_rate_threshold_rad_s`, so an actual turn is tracked with minimal
    // lag.
    HeadingSmoother(float stationary_alpha, float turning_alpha, float turn_rate_threshold_rad_s);

    // Next update() is taken as-is, with no blending against prior state.
    void reset();

    // Feeds one new heading sample (radians, [0, 2*pi)) and the current
    // rate-of-turn magnitude (rad/s); returns the smoothed heading (radians,
    // [0, 2*pi)).
    float update(float heading_rad, float turn_rate_rad_s);

private:
    bool initialized_ = false;
    float smoothed_x_ = 1.0f;  // unit-vector representation avoids wraparound
    float smoothed_y_ = 0.0f;
    float stationary_alpha_;
    float turning_alpha_;
    float turn_rate_threshold_rad_s_;
};

}  // namespace heading
