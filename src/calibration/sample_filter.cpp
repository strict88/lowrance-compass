#include "sample_filter.h"

#include <cmath>

#include "heading/quality_gate.h"
#include "thresholds.h"

namespace calibration
{

namespace
{

constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;

void insertionSort(float *arr, int n)
{
    for (int i = 1; i < n; ++i)
    {
        float key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key)
        {
            arr[j + 1] = arr[j];
            --j;
        }
        arr[j + 1] = key;
    }
}

float median(const float *sorted, int n)
{
    if (n % 2 == 1)
    {
        return sorted[n / 2];
    }
    return 0.5f * (sorted[n / 2 - 1] + sorted[n / 2]);
}

}  // namespace

SampleRejectReason evaluateSample(const SwingSampleInput &input)
{
    if (input.mag_accuracy < heading::kHeadingMinAccuracy || input.accel_accuracy < heading::kHeadingMinAccuracy ||
        input.gyro_accuracy < heading::kHeadingMinAccuracy)
    {
        return SampleRejectReason::kLowSensorAccuracy;
    }
    if (input.reference_age_s > thresholds::kStageCReferenceMaxAgeS)
    {
        return SampleRejectReason::kReferenceStale;
    }
    if (input.sog_m_s < thresholds::kStageCMinSogMS)
    {
        return SampleRejectReason::kTooSlow;
    }

    float turn_rate_deg_s = std::fabs(input.turn_rate_rad_s) * kRadToDeg;
    if (turn_rate_deg_s > thresholds::kStageCMaxTurnRateDegS)
    {
        return SampleRejectReason::kTurnTooFastOrUneven;
    }

    float turn_rate_change_deg_s = std::fabs(input.turn_rate_rad_s - input.turn_rate_prev_rad_s) * kRadToDeg;
    if (turn_rate_change_deg_s > thresholds::kStageCTurnRateSteadyTolDegS)
    {
        return SampleRejectReason::kTurnTooFastOrUneven;
    }

    return SampleRejectReason::kAccepted;
}

void rejectOutliers(const float *deviations_rad, int count, bool *accepted_out)
{
    if (count <= 0)
    {
        return;
    }

    int n = count > kMaxOutlierCheckSamples ? kMaxOutlierCheckSamples : count;

    float sorted[kMaxOutlierCheckSamples];
    for (int i = 0; i < n; ++i)
    {
        sorted[i] = deviations_rad[i];
    }
    insertionSort(sorted, n);
    float med = median(sorted, n);

    float abs_dev[kMaxOutlierCheckSamples];
    for (int i = 0; i < n; ++i)
    {
        abs_dev[i] = std::fabs(deviations_rad[i] - med);
    }
    float sorted_ad[kMaxOutlierCheckSamples];
    for (int i = 0; i < n; ++i)
    {
        sorted_ad[i] = abs_dev[i];
    }
    insertionSort(sorted_ad, n);
    float mad = median(sorted_ad, n);

    for (int i = 0; i < count; ++i)
    {
        if (i >= n)
        {
            accepted_out[i] = false;
            continue;
        }
        if (mad < 1e-9f)
        {
            accepted_out[i] = true;  // no spread at all -- nothing is an outlier
            continue;
        }
        float dev_from_median = std::fabs(deviations_rad[i] - med);
        accepted_out[i] = dev_from_median <= (thresholds::kStageCOutlierMadMultiplier * mad);
    }
}

}  // namespace calibration
