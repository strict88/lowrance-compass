#pragma once

#include <cstdint>

#include "calibration/deviation_fit.h"
#include "drivers/kv_store/kv_store.h"
#include "heading/pipeline.h"
#include "thresholds.h"

// Stage C state machine (data-model.md §3.3/§3.4):
//
//   Idle -Start swing-> Swinging -stop/full coverage-> Evaluating
//   Evaluating -passes acceptance-> ResultReady    Evaluating -otherwise-> Rejected -> Idle
//   ResultReady -Apply-> Saved    ResultReady -Discard-> Idle
//   Idle -Start manual swing-> AwaitingPoint(1..8) -8th point-> Evaluating
//   (any state) -Cancel-> Cancelled
//
// Pure logic, native-testable, driven entirely by fed-in samples and a
// monotonic clock value -- no direct hardware/N2K dependency (the caller
// resolves magnetic variation via n2k_codec::resolveVariation() first, same
// as Stage B).
namespace calibration
{

enum class StageCState
{
    kIdle,
    kSwinging,
    kAwaitingManualPoint,
    kEvaluating,
    kResultReady,
    kRejected,
    kSaved,
    kCancelled,
};

enum class StageCSource : uint8_t
{
    kGpsSwing = 0,
    kManual8pt = 1,
};

enum class StageCRejectReason
{
    kNone,
    kResidualTooHigh,
    kDeviationTooHigh,
    kIncompleteCoverage,
};

// The persisted DeviationCorrection record (data-model.md §1.3).
struct DeviationCorrection
{
    char saved_at_iso8601[32] = {0};
    float coefficients[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};  // A, B, C, D, E
    float max_deviation_rad = 0.0f;
    float residual_rms_rad = 0.0f;
    uint8_t source = static_cast<uint8_t>(StageCSource::kGpsSwing);
};

constexpr uint8_t kDeviationCorrectionSchemaVersion = 1;

bool saveDeviationCorrection(KeyValueStore &store, const DeviationCorrection &correction);
bool loadDeviationCorrection(KeyValueStore &store, DeviationCorrection &out);

// Converts a loaded (or absent) DeviationCorrection into the heading
// pipeline's correction input -- zero curve when `exists` is false
// (data-model.md §6).
void applyDeviationCorrection(const DeviationCorrection &correction, bool exists,
                               heading::DeviationCorrectionInput &out);

class StageC
{
public:
    void start(float now_s);
    void cancel();

    void startGpsSwing();     // kIdle -> kSwinging
    void startManualSwing();  // kIdle -> kAwaitingManualPoint(1)

    // Feeds one sample while kSwinging. `raw_heading_rad` is the current
    // (pre-deviation-correction) compass heading; `reference_magnetic_heading_rad`
    // is the GPS-course-derived magnetic reference (COG - variation) for
    // this instant. Turn tracking (for STAGE_C_MIN_FULL_TURNS) always
    // updates from `raw_heading_rad`, regardless of sample acceptance.
    void updateSwing(float raw_heading_rad, float reference_magnetic_heading_rad, float turn_rate_rad_s,
                      float sog_m_s, float reference_age_s, uint8_t mag_accuracy, uint8_t accel_accuracy,
                      uint8_t gyro_accuracy, float now_s);

    void stopSwing();  // kSwinging -> kEvaluating (user-requested early stop)

    // kAwaitingManualPoint: records this point's deviation (true bearing
    // converted to magnetic via `variation_rad`, compared against the
    // caller-supplied current raw heading) and advances to the next point,
    // or to kEvaluating after the 8th.
    void enterManualPoint(float true_reference_bearing_rad, float raw_heading_rad, float variation_rad);

    void apply(float now_s);  // kResultReady -> kSaved (persistence is the caller's job)
    void discard();           // kResultReady -> kIdle

    StageCState state() const { return state_; }
    StageCRejectReason rejectReason() const { return reject_reason_; }
    StageCSource source() const { return source_; }
    int manualPointIndex() const { return manual_point_index_; }  // 1..8 while kAwaitingManualPoint
    bool turningTooFast() const { return turning_too_fast_; }
    float turnsCompleted() const;
    float sectorCoveragePct() const;

    const heading::DeviationCoefficients &candidateCoefficients() const { return candidate_coefficients_; }
    float candidateMaxDeviationRad() const { return candidate_max_deviation_rad_; }
    float candidateResidualRmsRad() const { return candidate_residual_rms_rad_; }

private:
    static constexpr int kSectorCount = thresholds::kStageCSectorCount;
    static constexpr int kMaxSamplesPerSector = 8;
    static constexpr int kManualPointCount = thresholds::kStageCManualPointCount;

    void evaluate();
    bool allSectorsCovered() const;
    int sectorIndexForHeading(float heading_rad) const;

    StageCState state_ = StageCState::kIdle;
    StageCRejectReason reject_reason_ = StageCRejectReason::kNone;
    StageCSource source_ = StageCSource::kGpsSwing;

    float sector_deviations_rad_[kSectorCount][kMaxSamplesPerSector] = {{0}};
    int sector_sample_count_[kSectorCount] = {0};

    bool has_last_heading_ = false;
    float last_heading_rad_ = 0.0f;
    float cumulative_rotation_rad_ = 0.0f;
    float last_turn_rate_rad_s_ = 0.0f;
    bool turning_too_fast_ = false;

    int manual_point_index_ = 0;  // 1..8 while active
    float manual_raw_heading_rad_[8] = {0};
    float manual_deviation_rad_[8] = {0};
    bool manual_point_confirmed_[8] = {false};

    heading::DeviationCoefficients candidate_coefficients_{};
    float candidate_max_deviation_rad_ = 0.0f;
    float candidate_residual_rms_rad_ = 0.0f;
};

}  // namespace calibration
