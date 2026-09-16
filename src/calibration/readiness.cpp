#include "readiness.h"

namespace calibration
{

Readiness deriveReadiness(const ReadinessInput &input)
{
    if (input.heading_currently_withheld)
    {
        return Readiness::kNotCalibrated;
    }
    if (input.stage_a_done && input.stage_b_done && input.stage_c_done)
    {
        return Readiness::kReady;
    }
    if (input.stage_a_done)
    {
        return Readiness::kUsableIncomplete;
    }
    return Readiness::kNotCalibrated;
}

}  // namespace calibration
