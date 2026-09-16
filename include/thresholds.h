#pragma once

// Named threshold/config constants (data-model.md §1.5). Not persisted --
// tunable without touching calibration/procedure logic (FR-011). Exact
// numeric defaults are implementation choices (data-model.md explicitly
// defers them to research.md/implementation); revisit during bench/on-water
// verification (checklists/manual-verification.md) rather than treating
// these as final.
//
// heading::kHeadingMinAccuracy (src/heading/quality_gate.h) is the one
// exception kept in its own module, since it's foundational to the heading
// pipeline itself rather than calibration/settings-specific.
namespace thresholds
{

// --- Stage A: bench sensor calibration ---
constexpr float kStageAStillnessGyroVarMax = 0.0025f;  // (rad/s)^2
constexpr float kStageAStillnessHoldS = 2.0f;
constexpr float kStageAPositionHoldS = 3.0f;
constexpr float kStageAPositionGravityTolDeg = 15.0f;
constexpr int kStageARotationCoverageBins = 64;
constexpr float kStageARotationCoverageMinFraction = 0.8f;
constexpr float kStageATimeoutS = 300.0f;

// --- Stage B: installation alignment ---
constexpr float kStageBLevelStillWindowS = 3.0f;
constexpr float kStageBLevelGyroVarMax = 0.0025f;  // (rad/s)^2
constexpr float kStageBGpsMinSogMS = 1.5f;          // m/s (~3 kn)
constexpr float kStageBGpsCogSteadyWindowS = 10.0f;
constexpr float kStageBGpsCogSteadyMaxStddevDeg = 5.0f;

// --- Stage C: compass swing ---
constexpr float kStageCMinSogMS = 1.5f;  // m/s
constexpr float kStageCMaxTurnRateDegS = 10.0f;
constexpr float kStageCTurnRateSteadyTolDegS = 3.0f;
constexpr int kStageCSectorCount = 36;  // 10 deg sectors
constexpr int kStageCMinSamplesPerSector = 3;
constexpr int kStageCMinFullTurns = 2;
constexpr float kStageCMaxRmsResidualDeg = 3.0f;
constexpr float kStageCMaxAbsDeviationDeg = 15.0f;
constexpr float kStageCOutlierMadMultiplier = 3.5f;
constexpr float kStageCReferenceMaxAgeS = 2.0f;
constexpr int kStageCManualPointCount = 8;

// --- Settings ---
constexpr int kSsidMinLen = 1;
constexpr int kSsidMaxLen = 32;

// --- Heading smoothing (FR-043) ---
constexpr float kHeadingSmoothStationaryAlpha = 0.05f;
constexpr float kHeadingSmoothTurningAlpha = 0.5f;
constexpr float kHeadingSmoothTurnThresholdDegS = 5.0f;

// --- Session / recovery / UI ---
constexpr float kCalibrationSessionInactivityTimeoutS = 120.0f;
constexpr float kNetworkResetBootHoldS = 10.0f;
constexpr float kWsStatusRateHzIdle = 2.0f;
constexpr float kWsStatusRateHzActiveCal = 5.0f;

}  // namespace thresholds
