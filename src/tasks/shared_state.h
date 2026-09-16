#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include <cstddef>

#include "heading/pipeline.h"

// The only two cross-task data paths in this firmware (constitution
// Principle III: no unprotected globals):
//   1. A mutex-protected HeadingReading snapshot: ImuTask publishes, N2kTask
//      and AppTask (for the web API/WebSocket) read.
//   2. AppTask's command queue: web_api's HTTP handlers (running in the
//      async web server's own task context) enqueue a command and block on
//      its embedded semaphore for a synchronous-looking HTTP response, while
//      AppTask itself performs the actual calibration/settings state change.
namespace shared_state
{

void init();

void publishHeadingReading(const heading::HeadingReading &reading);
heading::HeadingReading getHeadingReading();

// Everything the heading pipeline needs besides the live IMU report itself:
// the persisted Stage B/C corrections (identity/zero when NOT_DONE), the
// in-progress calibration session's active stage, and the persisted Stage A
// profile's accuracy (for the Stage A in-progress freeze, FR-041). AppTask
// (which owns CalibrationService and the persisted records) publishes this
// whenever it changes; ImuTask reads it once per pipeline tick.
struct HeadingCorrectionInputs
{
    heading::LevelReference level_reference;
    heading::MountingOffset mounting_offset;
    heading::DeviationCorrectionInput deviation_correction;
    heading::ActiveCalibrationStage active_stage = heading::ActiveCalibrationStage::kNone;
    heading::SavedCalibrationAccuracy saved_profile;
};

void publishHeadingCorrectionInputs(const HeadingCorrectionInputs &inputs);
HeadingCorrectionInputs getHeadingCorrectionInputs();

// The raw IMU sample behind the latest HeadingReading, published alongside
// it so AppTask can drive an active Stage A calibration attempt (which needs
// the raw rotation vector/gyro/accuracy, not the corrected/gated heading)
// without a second hardware dependency. `connected` mirrors
// ImuDriver::isConnected() at publish time.
struct RawImuSample
{
    heading::Quaternion raw_quat;
    float gyro_x_rad_s = 0.0f;
    float gyro_y_rad_s = 0.0f;
    float gyro_z_rad_s = 0.0f;
    uint8_t mag_accuracy = 0;
    uint8_t accel_accuracy = 0;
    uint8_t gyro_accuracy = 0;
    bool connected = true;
    uint32_t monotonic_ms = 0;
};

void publishRawImuSample(const RawImuSample &sample);
RawImuSample getRawImuSample();

// A web_api-facing snapshot of Stage A's persisted + live status
// (contracts/rest-api.md's stages.a / session.progress.a shapes),
// published by AppTask whenever it changes. Kept decoupled from
// calibration/stage_a.h's own types so shared_state doesn't need to know
// about calibration internals.
struct StageAStatusSnapshot
{
    bool persisted_done = false;
    uint8_t saved_mag_accuracy = 0;
    uint8_t saved_accel_accuracy = 0;
    uint8_t saved_gyro_accuracy = 0;
    char saved_at_iso8601[32] = {0};

    bool session_active = false;
    uint8_t live_mag_accuracy = 0;
    uint8_t live_accel_accuracy = 0;
    uint8_t live_gyro_accuracy = 0;
    bool positions_done[6] = {false, false, false, false, false, false};
    float rotation_coverage_pct = 0.0f;
};

void publishStageAStatus(const StageAStatusSnapshot &status);
StageAStatusSnapshot getStageAStatus();

enum class AppCommandType
{
    kCalStartA,
    kCalStartB,
    kCalStartCGps,
    kCalStartCManual,
    kCalCancelA,
    kCalCancelB,
    kCalCancelC,
    kCalApplyB,
    kCalApplyC,
    kCalDiscardB,
    kCalDiscardC,
    kCalResetA,
    kCalResetB,
    kCalResetC,
    kCalBLevel,
    kCalBChooseGps,
    kCalBBearing,
    kCalCManualPoint,
    kSettingsSave,
    kNetworkReset,
};

// A queued UI command plus a synchronous-response bridge: the caller
// (web_api, a different task) fills `done_sem`/`result_buf` and blocks on
// `done_sem` after posting; AppTask processes the command, writes a JSON
// body into `result_buf` (caller-owned, at least `result_buf_len` bytes),
// sets `success`, and gives `done_sem`.
struct AppCommand
{
    AppCommandType type = AppCommandType::kCalStartA;

    float float_param = 0.0f;     // bearing_deg / reference_heading_deg
    bool confirm = false;         // reset / network-reset confirmation
    bool bool_param = false;      // e.g. GPS vs manual already encoded in type; reserved for future use
    char string_param[33] = {0};  // ssid (max 32 chars + terminator)

    SemaphoreHandle_t done_sem = nullptr;
    char *result_buf = nullptr;
    size_t result_buf_len = 0;
    bool success = false;
};

bool postAppCommand(const AppCommand &cmd, uint32_t timeout_ms);
bool receiveAppCommand(AppCommand &out, uint32_t timeout_ms);

// Bench-only synthetic GPS input for exercising Stage B's GPS-course method
// and Stage C's swing without a real GPS source on the bus (quickstart.md
// section 6). Only reachable when the firmware is built with
// -D DEBUG_GPS_INJECT=1 (never in a release build, so it can never be
// reachable on a boat) -- web_api.cpp only registers the route under that
// same guard.
struct DebugGpsInject
{
    bool active = false;
    float sog_m_s = 0.0f;
    float cog_rad = 0.0f;
    float variation_rad = 0.0f;
};

void setDebugGpsInject(const DebugGpsInject &inject);
DebugGpsInject getDebugGpsInject();

// A web_api-facing snapshot of Stage B's persisted + live status
// (contracts/rest-api.md's stages.b / session.progress.b shapes).
struct StageBStatusSnapshot
{
    bool persisted_done = false;
    float saved_offset_deg = 0.0f;
    bool saved_method_is_gps = false;
    char saved_at_iso8601[32] = {0};

    bool session_active = false;
    bool level_set = false;
    bool awaiting_method_choice = false;
    bool awaiting_bearing_entry = false;
    bool awaiting_gps_alignment = false;
    bool waiting_speed_too_low = false;
    bool waiting_course_not_steady = false;
    bool has_preview_offset = false;
    float preview_offset_deg = 0.0f;
};

void publishStageBStatus(const StageBStatusSnapshot &status);
StageBStatusSnapshot getStageBStatus();

// The current SSID, published by AppTask (which owns SettingsService)
// whenever it changes, for web_api's GET /api/status.settings.ssid (its
// route handlers run in a different task context than AppTask).
void publishCurrentSsid(const char *ssid);
void getCurrentSsid(char *out, size_t out_len);

}  // namespace shared_state
