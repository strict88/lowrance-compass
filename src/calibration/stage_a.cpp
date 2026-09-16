#include "stage_a.h"

#include <cmath>

namespace calibration
{

namespace
{

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;

// Rotation-coverage bins are laid out as an 8x8 (latitude x longitude) grid
// over the unit sphere; kept in sync with thresholds::kStageARotationCoverageBins.
constexpr int kGridDim = 8;
static_assert(kGridDim * kGridDim == thresholds::kStageARotationCoverageBins,
              "rotation coverage grid dimensions must multiply to kStageARotationCoverageBins");

// The six body-frame half-axis unit vectors, in RestPosition enum order.
const heading::Vec3 kAxisVectors[kRestPositionCount] = {
    {1.0f, 0.0f, 0.0f},   // kPosX
    {-1.0f, 0.0f, 0.0f},  // kNegX
    {0.0f, 1.0f, 0.0f},   // kPosY
    {0.0f, -1.0f, 0.0f},  // kNegY
    {0.0f, 0.0f, 1.0f},   // kPosZ
    {0.0f, 0.0f, -1.0f},  // kNegZ
};

float dot(const heading::Vec3 &a, const heading::Vec3 &b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

}  // namespace

void StageA::start(float now_s)
{
    state_ = StageAState::kAwaitingStillness;
    progress_ = StageAProgress{};
    started_at_s_ = now_s;
    stillness_held_s_ = 0.0f;
    position_current_index_ = -1;
    position_current_held_s_ = 0.0f;
    for (bool &visited : rotation_bins_visited_)
    {
        visited = false;
    }
}

void StageA::cancel()
{
    state_ = StageAState::kCancelled;
}

void StageA::updateStillness(const StageASample &sample)
{
    float gyro_sq = sample.gyro_x_rad_s * sample.gyro_x_rad_s + sample.gyro_y_rad_s * sample.gyro_y_rad_s +
                    sample.gyro_z_rad_s * sample.gyro_z_rad_s;
    if (gyro_sq <= thresholds::kStageAStillnessGyroVarMax)
    {
        stillness_held_s_ += sample.dt_s;
    }
    else
    {
        stillness_held_s_ = 0.0f;
    }

    if (stillness_held_s_ >= thresholds::kStageAStillnessHoldS)
    {
        state_ = StageAState::kAwaitingPositions;
    }
}

void StageA::updatePositions(const StageASample &sample)
{
    // Gravity direction in body frame: rotate the world "down" vector
    // (0,0,-1) into body frame using the inverse (conjugate) of the
    // body->world rotation vector.
    heading::Vec3 gravity_body = heading::rotate(heading::conjugate(sample.raw_quat), heading::Vec3{0.0f, 0.0f, -1.0f});

    int best_index = -1;
    float best_dot = -2.0f;
    for (int i = 0; i < kRestPositionCount; ++i)
    {
        float d = dot(gravity_body, kAxisVectors[i]);
        if (d > best_dot)
        {
            best_dot = d;
            best_index = i;
        }
    }

    float tolerance_cos = std::cos(thresholds::kStageAPositionGravityTolDeg * kDegToRad);
    bool within_tolerance = best_dot >= tolerance_cos;

    if (!within_tolerance || best_index != position_current_index_)
    {
        position_current_index_ = within_tolerance ? best_index : -1;
        position_current_held_s_ = 0.0f;
    }
    else
    {
        position_current_held_s_ += sample.dt_s;
        if (position_current_held_s_ >= thresholds::kStageAPositionHoldS)
        {
            progress_.positions_done[position_current_index_] = true;
        }
    }

    bool all_done = true;
    for (bool done : progress_.positions_done)
    {
        all_done = all_done && done;
    }
    if (all_done)
    {
        state_ = StageAState::kAwaitingRotation;
    }
}

void StageA::updateRotationCoverage(const StageASample &sample)
{
    // Rotate the body +X axis into world frame and bin its direction on the
    // unit sphere; visiting enough distinct bins is the rotation-coverage
    // proxy (data-model.md's "sphere-surface bins").
    heading::Vec3 world_dir = heading::rotate(sample.raw_quat, heading::Vec3{1.0f, 0.0f, 0.0f});

    float mag = std::sqrt(world_dir.x * world_dir.x + world_dir.y * world_dir.y + world_dir.z * world_dir.z);
    if (mag > 1e-6f)
    {
        float theta = std::acos(std::fmax(-1.0f, std::fmin(1.0f, world_dir.z / mag)));  // [0, pi]
        float phi = std::atan2(world_dir.y, world_dir.x);                               // (-pi, pi]

        int lat_bin = static_cast<int>((theta / kPi) * kGridDim);
        if (lat_bin >= kGridDim) lat_bin = kGridDim - 1;
        if (lat_bin < 0) lat_bin = 0;

        int lon_bin = static_cast<int>(((phi + kPi) / (2.0f * kPi)) * kGridDim);
        if (lon_bin >= kGridDim) lon_bin = kGridDim - 1;
        if (lon_bin < 0) lon_bin = 0;

        rotation_bins_visited_[lat_bin * kGridDim + lon_bin] = true;
    }

    int visited_count = 0;
    for (bool visited : rotation_bins_visited_)
    {
        if (visited) ++visited_count;
    }
    progress_.rotation_coverage_fraction =
        static_cast<float>(visited_count) / static_cast<float>(thresholds::kStageARotationCoverageBins);

    if (progress_.rotation_coverage_fraction >= thresholds::kStageARotationCoverageMinFraction)
    {
        state_ = StageAState::kEvaluating;
    }
}

void StageA::evaluate(float now_s)
{
    constexpr uint8_t kRequiredAccuracy = 3;  // High -- fixed, not tunable (data-model.md §1.5)
    bool all_high = progress_.mag_accuracy >= kRequiredAccuracy && progress_.accel_accuracy >= kRequiredAccuracy &&
                     progress_.gyro_accuracy >= kRequiredAccuracy;

    if (all_high)
    {
        state_ = StageAState::kDone;
        return;
    }

    if (now_s - started_at_s_ >= thresholds::kStageATimeoutS)
    {
        state_ = StageAState::kTimedOut;
    }
}

void StageA::update(const StageASample &sample, float now_s)
{
    progress_.mag_accuracy = sample.mag_accuracy;
    progress_.accel_accuracy = sample.accel_accuracy;
    progress_.gyro_accuracy = sample.gyro_accuracy;

    switch (state_)
    {
        case StageAState::kAwaitingStillness:
            updateStillness(sample);
            break;
        case StageAState::kAwaitingPositions:
            updatePositions(sample);
            break;
        case StageAState::kAwaitingRotation:
            updateRotationCoverage(sample);
            break;
        case StageAState::kEvaluating:
            evaluate(now_s);
            break;
        case StageAState::kIdle:
        case StageAState::kDone:
        case StageAState::kTimedOut:
        case StageAState::kCancelled:
        default:
            break;
    }
}

}  // namespace calibration
