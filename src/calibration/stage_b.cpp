#include "stage_b.h"

#include <cmath>
#include <cstring>

#include "drivers/kv_store/record_envelope.h"
#include "heading/normalize.h"

namespace calibration
{

namespace
{

constexpr float kPi = 3.14159265358979323846f;
constexpr float kRadToDeg = 180.0f / kPi;

// Circular mean and standard deviation (radians) of a set of angles, via the
// standard mean-resultant-length formula. An R near 0 (samples spread all
// the way around the circle) reports the maximum possible spread (pi).
void computeCircularStats(const float *angles_rad, int count, float &mean_rad_out, float &stddev_rad_out)
{
    double sum_x = 0.0;
    double sum_y = 0.0;
    for (int i = 0; i < count; ++i)
    {
        sum_x += std::cos(angles_rad[i]);
        sum_y += std::sin(angles_rad[i]);
    }
    double mean_x = sum_x / count;
    double mean_y = sum_y / count;
    double r = std::sqrt(mean_x * mean_x + mean_y * mean_y);

    mean_rad_out = static_cast<float>(std::atan2(mean_y, mean_x));
    if (r < 1e-6)
    {
        stddev_rad_out = kPi;
    }
    else
    {
        stddev_rad_out = static_cast<float>(std::sqrt(-2.0 * std::log(r)));
    }
}

}  // namespace

bool saveInstallationAlignment(KeyValueStore &store, const InstallationAlignment &alignment)
{
    return record_envelope::save(store, kInstallationAlignmentSchemaVersion,
                                  reinterpret_cast<const uint8_t *>(&alignment), sizeof(alignment));
}

bool loadInstallationAlignment(KeyValueStore &store, InstallationAlignment &out,
                                record_envelope::Status *status_out)
{
    InstallationAlignment loaded;
    auto result = record_envelope::load(store, kInstallationAlignmentSchemaVersion,
                                         reinterpret_cast<uint8_t *>(&loaded), sizeof(loaded));
    if (status_out != nullptr)
    {
        *status_out = result.status;
    }
    if (result.status == record_envelope::Status::kCrcMismatch ||
        result.status == record_envelope::Status::kSchemaMismatch)
    {
        record_envelope::resetToDefault(store);  // FR-045; see stage_a.cpp's loadSensorCalibrationProfile.
    }
    if (result.status != record_envelope::Status::kOk)
    {
        return false;
    }
    out = loaded;
    return true;
}

void applyInstallationAlignment(const InstallationAlignment &alignment, bool exists,
                                 heading::LevelReference &level_reference_out,
                                 heading::MountingOffset &mounting_offset_out)
{
    if (!exists)
    {
        level_reference_out = heading::LevelReference{};
        mounting_offset_out = heading::MountingOffset{};
        return;
    }

    level_reference_out.has_level = true;
    level_reference_out.level_reference_quat =
        heading::Quaternion{alignment.level_reference_quat[0], alignment.level_reference_quat[1],
                             alignment.level_reference_quat[2], alignment.level_reference_quat[3]};

    mounting_offset_out.has_offset = true;
    mounting_offset_out.mounting_offset_rad = alignment.mounting_offset_rad;
}

void StageB::start(float now_s)
{
    (void)now_s;
    state_ = StageBState::kIdle;
    waiting_reason_ = StageBWaitingReason::kNone;
    accum_w_ = accum_x_ = accum_y_ = accum_z_ = 0.0;
    accum_count_ = 0;
    level_held_s_ = 0.0f;
    gps_sample_count_ = 0;
    gps_write_index_ = 0;
    offset_method_ = StageBOffsetMethod::kNone;
    candidate_offset_rad_ = 0.0f;
}

void StageB::cancel()
{
    state_ = StageBState::kCancelled;
}

void StageB::beginLevelCapture(float now_s)
{
    if (state_ != StageBState::kIdle)
    {
        return;
    }
    (void)now_s;
    accum_w_ = accum_x_ = accum_y_ = accum_z_ = 0.0;
    accum_count_ = 0;
    level_held_s_ = 0.0f;
    state_ = StageBState::kLevelCapturing;
}

void StageB::updateLevelCapture(const heading::Quaternion &raw_quat, float gyro_x_rad_s, float gyro_y_rad_s,
                                 float gyro_z_rad_s, float dt_s)
{
    if (state_ != StageBState::kLevelCapturing)
    {
        return;
    }

    float gyro_sq = gyro_x_rad_s * gyro_x_rad_s + gyro_y_rad_s * gyro_y_rad_s + gyro_z_rad_s * gyro_z_rad_s;
    if (gyro_sq > thresholds::kStageBLevelGyroVarMax)
    {
        accum_w_ = accum_x_ = accum_y_ = accum_z_ = 0.0;
        accum_count_ = 0;
        level_held_s_ = 0.0f;
        return;
    }

    accum_w_ += raw_quat.w;
    accum_x_ += raw_quat.x;
    accum_y_ += raw_quat.y;
    accum_z_ += raw_quat.z;
    accum_count_++;
    level_held_s_ += dt_s;

    if (level_held_s_ >= thresholds::kStageBLevelStillWindowS && accum_count_ > 0)
    {
        heading::Quaternion avg{static_cast<float>(accum_w_ / accum_count_), static_cast<float>(accum_x_ / accum_count_),
                                 static_cast<float>(accum_y_ / accum_count_), static_cast<float>(accum_z_ / accum_count_)};
        level_reference_quat_ = heading::normalizeQuaternion(avg);
        state_ = StageBState::kLevelSet;
    }
}

void StageB::chooseKnownBearing()
{
    if (state_ != StageBState::kLevelSet)
    {
        return;
    }
    state_ = StageBState::kAwaitingBearingEntry;
}

void StageB::chooseGpsCourse()
{
    if (state_ != StageBState::kLevelSet)
    {
        return;
    }
    gps_sample_count_ = 0;
    gps_write_index_ = 0;
    waiting_reason_ = StageBWaitingReason::kNone;
    state_ = StageBState::kAwaitingGpsAlignment;
}

void StageB::enterBearing(float true_bearing_rad, float variation_rad, float current_heading_rad)
{
    if (state_ != StageBState::kAwaitingBearingEntry)
    {
        return;
    }
    float magnetic_bearing_rad = true_bearing_rad - variation_rad;
    candidate_offset_rad_ = heading::circularDifference(magnetic_bearing_rad, current_heading_rad);
    offset_method_ = StageBOffsetMethod::kKnownBearing;
    state_ = StageBState::kOffsetComputed;
}

void StageB::updateGpsAlignment(float sog_m_s, float cog_rad, float variation_rad, float current_heading_rad,
                                 float now_s)
{
    if (state_ != StageBState::kAwaitingGpsAlignment)
    {
        return;
    }

    if (sog_m_s < thresholds::kStageBGpsMinSogMS)
    {
        waiting_reason_ = StageBWaitingReason::kSpeedTooLow;
        return;
    }

    gps_cog_rad_[gps_write_index_] = cog_rad;
    gps_timestamp_s_[gps_write_index_] = now_s;
    gps_write_index_ = (gps_write_index_ + 1) % kMaxGpsSamples;
    if (gps_sample_count_ < kMaxGpsSamples)
    {
        gps_sample_count_++;
    }

    float window_start_s = now_s - thresholds::kStageBGpsCogSteadyWindowS;
    float valid_cogs[kMaxGpsSamples];
    int valid_count = 0;
    float oldest_in_window_s = now_s;
    for (int i = 0; i < gps_sample_count_; ++i)
    {
        if (gps_timestamp_s_[i] >= window_start_s)
        {
            valid_cogs[valid_count++] = gps_cog_rad_[i];
            if (gps_timestamp_s_[i] < oldest_in_window_s)
            {
                oldest_in_window_s = gps_timestamp_s_[i];
            }
        }
    }

    if (valid_count < 2 || (now_s - oldest_in_window_s) < thresholds::kStageBGpsCogSteadyWindowS)
    {
        waiting_reason_ = StageBWaitingReason::kNone;  // still filling the window
        return;
    }

    float mean_cog_rad = 0.0f;
    float stddev_rad = 0.0f;
    computeCircularStats(valid_cogs, valid_count, mean_cog_rad, stddev_rad);
    float stddev_deg = stddev_rad * kRadToDeg;

    if (stddev_deg > thresholds::kStageBGpsCogSteadyMaxStddevDeg)
    {
        waiting_reason_ = StageBWaitingReason::kCourseNotSteady;
        return;
    }

    float magnetic_cog_rad = mean_cog_rad - variation_rad;
    candidate_offset_rad_ = heading::circularDifference(magnetic_cog_rad, current_heading_rad);
    offset_method_ = StageBOffsetMethod::kGpsCourse;
    waiting_reason_ = StageBWaitingReason::kNone;
    state_ = StageBState::kOffsetComputed;
}

void StageB::accept(float now_s)
{
    (void)now_s;
    if (state_ != StageBState::kOffsetComputed)
    {
        return;
    }
    state_ = StageBState::kSaved;
}

void StageB::discard()
{
    if (state_ != StageBState::kOffsetComputed)
    {
        return;
    }
    state_ = StageBState::kIdle;
}

}  // namespace calibration
