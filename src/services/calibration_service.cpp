#include "calibration_service.h"

#include "thresholds.h"

bool CalibrationService::tryStart(Stage stage, Stage &busy_stage_out)
{
    if (active_stage_ != Stage::kNone)
    {
        busy_stage_out = active_stage_;
        return false;
    }
    active_stage_ = stage;
    last_client_activity_ms_ = clock_.monotonicMillis();
    return true;
}

void CalibrationService::cancel(Stage stage)
{
    if (active_stage_ == stage)
    {
        active_stage_ = Stage::kNone;
    }
}

void CalibrationService::endActive()
{
    active_stage_ = Stage::kNone;
}

void CalibrationService::noteClientActivity()
{
    last_client_activity_ms_ = clock_.monotonicMillis();
}

CalibrationService::Stage CalibrationService::checkInactivityTimeout()
{
    if (active_stage_ == Stage::kNone)
    {
        return Stage::kNone;
    }

    uint32_t elapsed_ms = clock_.monotonicMillis() - last_client_activity_ms_;
    uint32_t timeout_ms = static_cast<uint32_t>(thresholds::kCalibrationSessionInactivityTimeoutS * 1000.0f);
    if (elapsed_ms < timeout_ms)
    {
        return Stage::kNone;
    }

    Stage timed_out_stage = active_stage_;
    active_stage_ = Stage::kNone;
    return timed_out_stage;
}
