#include "stage_c.h"

#include <cmath>
#include <cstring>

#include "calibration/sample_filter.h"
#include "drivers/kv_store/record_envelope.h"
#include "heading/normalize.h"

namespace calibration
{

namespace
{

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;
constexpr float kRadToDeg = 180.0f / kPi;

}  // namespace

bool saveDeviationCorrection(KeyValueStore &store, const DeviationCorrection &correction)
{
    return record_envelope::save(store, kDeviationCorrectionSchemaVersion,
                                  reinterpret_cast<const uint8_t *>(&correction), sizeof(correction));
}

bool loadDeviationCorrection(KeyValueStore &store, DeviationCorrection &out)
{
    DeviationCorrection loaded;
    auto result = record_envelope::load(store, kDeviationCorrectionSchemaVersion,
                                         reinterpret_cast<uint8_t *>(&loaded), sizeof(loaded));
    if (result.status != record_envelope::Status::kOk)
    {
        return false;
    }
    out = loaded;
    return true;
}

void applyDeviationCorrection(const DeviationCorrection &correction, bool exists,
                               heading::DeviationCorrectionInput &out)
{
    if (!exists)
    {
        out = heading::DeviationCorrectionInput{};
        return;
    }
    out.has_correction = true;
    out.coefficients = heading::DeviationCoefficients{correction.coefficients[0], correction.coefficients[1],
                                                        correction.coefficients[2], correction.coefficients[3],
                                                        correction.coefficients[4]};
}

void StageC::start(float now_s)
{
    (void)now_s;
    state_ = StageCState::kIdle;
    reject_reason_ = StageCRejectReason::kNone;
    for (int i = 0; i < kSectorCount; ++i)
    {
        sector_sample_count_[i] = 0;
    }
    has_last_heading_ = false;
    last_heading_rad_ = 0.0f;
    cumulative_rotation_rad_ = 0.0f;
    last_turn_rate_rad_s_ = 0.0f;
    turning_too_fast_ = false;
    manual_point_index_ = 0;
    for (int i = 0; i < 8; ++i)
    {
        manual_point_confirmed_[i] = false;
    }
    candidate_coefficients_ = heading::DeviationCoefficients{};
    candidate_max_deviation_rad_ = 0.0f;
    candidate_residual_rms_rad_ = 0.0f;
}

void StageC::cancel()
{
    state_ = StageCState::kCancelled;
}

void StageC::startGpsSwing()
{
    if (state_ != StageCState::kIdle)
    {
        return;
    }
    source_ = StageCSource::kGpsSwing;
    for (int i = 0; i < kSectorCount; ++i)
    {
        sector_sample_count_[i] = 0;
    }
    has_last_heading_ = false;
    cumulative_rotation_rad_ = 0.0f;
    last_turn_rate_rad_s_ = 0.0f;
    turning_too_fast_ = false;
    state_ = StageCState::kSwinging;
}

void StageC::startManualSwing()
{
    if (state_ != StageCState::kIdle)
    {
        return;
    }
    source_ = StageCSource::kManual8pt;
    for (int i = 0; i < 8; ++i)
    {
        manual_point_confirmed_[i] = false;
    }
    manual_point_index_ = 1;
    state_ = StageCState::kAwaitingManualPoint;
}

int StageC::sectorIndexForHeading(float heading_rad) const
{
    float wrapped = std::fmod(heading_rad, kTwoPi);
    if (wrapped < 0.0f)
    {
        wrapped += kTwoPi;
    }
    int index = static_cast<int>(wrapped / (kTwoPi / static_cast<float>(kSectorCount)));
    if (index < 0)
    {
        index = 0;
    }
    if (index >= kSectorCount)
    {
        index = kSectorCount - 1;
    }
    return index;
}

bool StageC::allSectorsCovered() const
{
    for (int i = 0; i < kSectorCount; ++i)
    {
        if (sector_sample_count_[i] < thresholds::kStageCMinSamplesPerSector)
        {
            return false;
        }
    }
    return true;
}

float StageC::turnsCompleted() const
{
    return std::fabs(cumulative_rotation_rad_) / kTwoPi;
}

float StageC::sectorCoveragePct() const
{
    int covered = 0;
    for (int i = 0; i < kSectorCount; ++i)
    {
        if (sector_sample_count_[i] >= thresholds::kStageCMinSamplesPerSector)
        {
            covered++;
        }
    }
    return 100.0f * static_cast<float>(covered) / static_cast<float>(kSectorCount);
}

void StageC::updateSwing(float raw_heading_rad, float reference_magnetic_heading_rad, float turn_rate_rad_s,
                          float sog_m_s, float reference_age_s, uint8_t mag_accuracy, uint8_t accel_accuracy,
                          uint8_t gyro_accuracy, float now_s)
{
    (void)now_s;
    if (state_ != StageCState::kSwinging)
    {
        return;
    }

    if (has_last_heading_)
    {
        cumulative_rotation_rad_ += heading::circularDifference(raw_heading_rad, last_heading_rad_);
    }
    last_heading_rad_ = raw_heading_rad;
    has_last_heading_ = true;

    SwingSampleInput sample_input;
    sample_input.sog_m_s = sog_m_s;
    sample_input.turn_rate_rad_s = turn_rate_rad_s;
    sample_input.turn_rate_prev_rad_s = last_turn_rate_rad_s_;
    sample_input.reference_age_s = reference_age_s;
    sample_input.mag_accuracy = mag_accuracy;
    sample_input.accel_accuracy = accel_accuracy;
    sample_input.gyro_accuracy = gyro_accuracy;
    last_turn_rate_rad_s_ = turn_rate_rad_s;

    SampleRejectReason reason = evaluateSample(sample_input);
    turning_too_fast_ = (reason == SampleRejectReason::kTurnTooFastOrUneven);
    if (reason != SampleRejectReason::kAccepted)
    {
        return;
    }

    int sector = sectorIndexForHeading(raw_heading_rad);
    float deviation_rad = heading::circularDifference(reference_magnetic_heading_rad, raw_heading_rad);
    if (sector_sample_count_[sector] < kMaxSamplesPerSector)
    {
        sector_deviations_rad_[sector][sector_sample_count_[sector]] = deviation_rad;
    }
    sector_sample_count_[sector]++;

    if (allSectorsCovered() && turnsCompleted() >= thresholds::kStageCMinFullTurns)
    {
        state_ = StageCState::kEvaluating;
        evaluate();
    }
}

void StageC::stopSwing()
{
    if (state_ != StageCState::kSwinging)
    {
        return;
    }
    state_ = StageCState::kEvaluating;
    evaluate();
}

void StageC::enterManualPoint(float true_reference_bearing_rad, float raw_heading_rad, float variation_rad)
{
    if (state_ != StageCState::kAwaitingManualPoint)
    {
        return;
    }
    int idx = manual_point_index_ - 1;
    if (idx < 0 || idx >= kManualPointCount)
    {
        return;
    }

    float magnetic_bearing_rad = true_reference_bearing_rad - variation_rad;
    manual_raw_heading_rad_[idx] = raw_heading_rad;
    manual_deviation_rad_[idx] = heading::circularDifference(magnetic_bearing_rad, raw_heading_rad);
    manual_point_confirmed_[idx] = true;

    manual_point_index_++;
    if (manual_point_index_ > kManualPointCount)
    {
        state_ = StageCState::kEvaluating;
        evaluate();
    }
}

void StageC::evaluate()
{
    SectorAggregate aggregates[kSectorCount];
    for (int i = 0; i < kSectorCount; ++i)
    {
        aggregates[i] = SectorAggregate{};
    }

    bool full_coverage = false;

    if (source_ == StageCSource::kManual8pt)
    {
        int confirmed = 0;
        for (int i = 0; i < kManualPointCount && i < kSectorCount; ++i)
        {
            if (!manual_point_confirmed_[i])
            {
                continue;
            }
            aggregates[i].has_data = true;
            aggregates[i].theta_rad = manual_raw_heading_rad_[i];
            aggregates[i].mean_deviation_rad = manual_deviation_rad_[i];
            confirmed++;
        }
        full_coverage = (confirmed == kManualPointCount);
    }
    else
    {
        for (int i = 0; i < kSectorCount; ++i)
        {
            int n = sector_sample_count_[i] < kMaxSamplesPerSector ? sector_sample_count_[i] : kMaxSamplesPerSector;
            if (n == 0)
            {
                continue;
            }
            bool accepted[kMaxSamplesPerSector];
            rejectOutliers(sector_deviations_rad_[i], n, accepted);

            double sum = 0.0;
            int kept = 0;
            for (int j = 0; j < n; ++j)
            {
                if (accepted[j])
                {
                    sum += sector_deviations_rad_[i][j];
                    kept++;
                }
            }
            if (kept == 0)
            {
                continue;
            }
            aggregates[i].has_data = true;
            aggregates[i].theta_rad = (kTwoPi) * (static_cast<float>(i) + 0.5f) / static_cast<float>(kSectorCount);
            aggregates[i].mean_deviation_rad = static_cast<float>(sum / kept);
        }
        full_coverage = allSectorsCovered() && turnsCompleted() >= thresholds::kStageCMinFullTurns;
    }

    DeviationFitResult fit = fitDeviationCurve(aggregates, kSectorCount);

    if (!full_coverage || !fit.success)
    {
        reject_reason_ = StageCRejectReason::kIncompleteCoverage;
        state_ = StageCState::kRejected;
        return;
    }

    float rms_deg = fit.residual_rms_rad * kRadToDeg;
    float max_deg = fit.max_abs_deviation_rad * kRadToDeg;

    if (rms_deg > thresholds::kStageCMaxRmsResidualDeg)
    {
        reject_reason_ = StageCRejectReason::kResidualTooHigh;
        state_ = StageCState::kRejected;
        return;
    }
    if (max_deg > thresholds::kStageCMaxAbsDeviationDeg)
    {
        reject_reason_ = StageCRejectReason::kDeviationTooHigh;
        state_ = StageCState::kRejected;
        return;
    }

    candidate_coefficients_ = fit.coefficients;
    candidate_residual_rms_rad_ = fit.residual_rms_rad;
    candidate_max_deviation_rad_ = fit.max_abs_deviation_rad;
    reject_reason_ = StageCRejectReason::kNone;
    state_ = StageCState::kResultReady;
}

void StageC::apply(float now_s)
{
    (void)now_s;
    if (state_ != StageCState::kResultReady)
    {
        return;
    }
    state_ = StageCState::kSaved;
}

void StageC::discard()
{
    if (state_ != StageCState::kResultReady)
    {
        return;
    }
    state_ = StageCState::kIdle;
}

}  // namespace calibration
