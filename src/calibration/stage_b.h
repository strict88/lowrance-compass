#pragma once

#include <cstdint>

#include "drivers/kv_store/kv_store.h"
#include "drivers/kv_store/record_envelope.h"
#include "heading/pipeline.h"
#include "heading/quaternion.h"
#include "thresholds.h"

// Stage B state machine (data-model.md §3.2):
//
//   Idle -"Set level"-> LevelCapturing -window complete-> LevelSet
//   LevelSet -known bearing-> AwaitingBearingEntry -entered-> OffsetComputed
//   LevelSet -GPS course-> AwaitingGpsAlignment -criteria met-> OffsetComputed
//   OffsetComputed -Accept-> Saved      OffsetComputed -Discard-> Idle
//   (any state) -Cancel-> Cancelled
//
// Pure logic, native-testable, driven entirely by fed-in samples and a
// monotonic clock value -- no direct hardware/N2K dependency (the caller
// resolves magnetic variation via n2k_codec::resolveVariation() first).
namespace calibration
{

enum class StageBState
{
    kIdle,
    kLevelCapturing,
    kLevelSet,
    kAwaitingBearingEntry,
    kAwaitingGpsAlignment,
    kOffsetComputed,
    kSaved,
    kCancelled,
};

enum class StageBWaitingReason
{
    kNone,
    kSpeedTooLow,
    kCourseNotSteady,
};

enum class StageBOffsetMethod : uint8_t
{
    kNone = 0,
    kKnownBearing = 1,
    kGpsCourse = 2,
};

// The persisted InstallationAlignment record (data-model.md §1.2).
struct InstallationAlignment
{
    char saved_at_iso8601[32] = {0};
    float level_reference_quat[4] = {1.0f, 0.0f, 0.0f, 0.0f};  // w,x,y,z
    float mounting_offset_rad = 0.0f;                          // (-pi, pi]
    uint8_t offset_method = static_cast<uint8_t>(StageBOffsetMethod::kNone);
};

constexpr uint8_t kInstallationAlignmentSchemaVersion = 1;

bool saveInstallationAlignment(KeyValueStore &store, const InstallationAlignment &alignment);
// See stage_a.h's loadSensorCalibrationProfile for `status_out`'s purpose.
bool loadInstallationAlignment(KeyValueStore &store, InstallationAlignment &out,
                                record_envelope::Status *status_out = nullptr);

// Converts a loaded (or absent) InstallationAlignment into the heading
// pipeline's correction inputs. `exists=false` (Stage B NOT_DONE) yields
// identity/zero -- data-model.md §6: "an uncalibrated Stage B or C
// contributes an identity/zero correction rather than blocking the
// pipeline." Pure logic, native-testable.
void applyInstallationAlignment(const InstallationAlignment &alignment, bool exists,
                                 heading::LevelReference &level_reference_out,
                                 heading::MountingOffset &mounting_offset_out);

class StageB
{
public:
    void start(float now_s);
    void cancel();

    // "Set level" pressed: begins averaging attitude over
    // STAGE_B_LEVEL_STILL_WINDOW_S. No-op unless in kIdle.
    void beginLevelCapture(float now_s);

    // Feeds one sample while in kLevelCapturing. A gyro variance excursion
    // above STAGE_B_LEVEL_GYRO_VAR_MAX resets the averaging window (mirrors
    // Stage A's stillness check).
    void updateLevelCapture(const heading::Quaternion &raw_quat, float gyro_x_rad_s, float gyro_y_rad_s,
                             float gyro_z_rad_s, float dt_s);

    // kLevelSet -> kAwaitingBearingEntry / kAwaitingGpsAlignment.
    void chooseKnownBearing();
    void chooseGpsCourse();

    // kAwaitingBearingEntry -> kOffsetComputed. FR-018 (quoted): "A true
    // bearing entered this way MUST be converted to magnetic, using the
    // same magnetic-variation source as FR-024 ..., before it is compared
    // against the device's own magnetic heading reading." `current_heading_rad`
    // is the compass's own current (post-level, pre-offset) reading of the
    // bow direction.
    void enterBearing(float true_bearing_rad, float variation_rad, float current_heading_rad);

    // kAwaitingGpsAlignment: feed one COG/SOG sample (FR-019). Only accepts
    // samples while `sog_m_s >= STAGE_B_GPS_MIN_SOG`; advances to
    // kOffsetComputed once course has been steady (stddev <=
    // STAGE_B_GPS_COG_STEADY_MAX_STDDEV_DEG) over
    // STAGE_B_GPS_COG_STEADY_WINDOW_S. `cog_rad`/`variation_rad` are true
    // COG and the resolved variation; offset is computed from magnetic COG
    // (cog_rad - variation_rad) against `current_heading_rad`.
    void updateGpsAlignment(float sog_m_s, float cog_rad, float variation_rad, float current_heading_rad,
                             float now_s);

    void accept(float now_s);  // kOffsetComputed -> kSaved (persistence is the caller's job)
    void discard();            // kOffsetComputed -> kIdle

    StageBState state() const { return state_; }
    StageBWaitingReason waitingReason() const { return waiting_reason_; }
    float previewOffsetRad() const { return candidate_offset_rad_; }
    const heading::Quaternion &levelReferenceQuat() const { return level_reference_quat_; }
    StageBOffsetMethod offsetMethod() const { return offset_method_; }

private:
    static constexpr int kMaxGpsSamples = 64;

    StageBState state_ = StageBState::kIdle;
    StageBWaitingReason waiting_reason_ = StageBWaitingReason::kNone;

    // Level capture accumulator.
    double accum_w_ = 0.0, accum_x_ = 0.0, accum_y_ = 0.0, accum_z_ = 0.0;
    int accum_count_ = 0;
    float level_held_s_ = 0.0f;
    heading::Quaternion level_reference_quat_{};

    // GPS-course sample ring buffer.
    float gps_cog_rad_[kMaxGpsSamples] = {0};
    float gps_timestamp_s_[kMaxGpsSamples] = {0};
    int gps_sample_count_ = 0;
    int gps_write_index_ = 0;

    StageBOffsetMethod offset_method_ = StageBOffsetMethod::kNone;
    float candidate_offset_rad_ = 0.0f;
};

}  // namespace calibration
