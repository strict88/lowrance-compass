#pragma once

// Feeds GET /api/status.readiness (contracts/rest-api.md). Pure logic,
// native-testable.
namespace calibration
{

enum class Readiness
{
    kNotCalibrated,
    kUsableIncomplete,
    kReady,
};

struct ReadinessInput
{
    bool stage_a_done = false;
    bool stage_b_done = false;
    bool stage_c_done = false;

    // FR-001 (quoted): "This summary MUST also reflect, in real time and
    // independently of any stage's persisted status, whenever heading is
    // currently withheld per FR-041 ..., even while the underlying stage
    // still reads 'Done'." Since the readiness enum has only three values,
    // a currently-withheld heading takes priority over persisted status and
    // reports kNotCalibrated -- the same value a genuinely never-calibrated
    // device shows, both meaning "don't trust this heading right now".
    bool heading_currently_withheld = false;
};

Readiness deriveReadiness(const ReadinessInput &input);

}  // namespace calibration
