#include "app_task.h"

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <cstdio>
#include <cstring>

#include "calibration/stage_a.h"
#include "calibration/stage_b.h"
#include "calibration/stage_c.h"
#include "drivers/kv_store/record_envelope.h"
#include "n2k_codec/variation.h"
#include "pin_config.h"
#include "services/diag_log.h"
#include "services/web_api.h"
#include "settings/network_settings.h"
#include "tasks/shared_state.h"
#include "thresholds.h"

namespace app_task
{

namespace
{

constexpr uint32_t kStackSize = 8192;
constexpr UBaseType_t kPriority = configMAX_PRIORITIES - 4;  // lower than ImuTask/N2kTask
constexpr BaseType_t kCoreId = 0;
// How often the main loop polls for a command (and, incidentally, how often
// driveStageA/B/C sample the IMU/GPS). At the old 200ms, Stage A's own
// accuracy check only saw 1 in ~20 IMU reports -- a brief all-High moment
// (mag/accel/gyro simultaneously High, which is what Stage A is waiting
// for) could easily land in one of the 19 reports never sampled, so it kept
// looking "not quite there" indefinitely even once the sensor genuinely was.
constexpr uint32_t kCommandWaitMs = 20;
constexpr const char *kFirmwareVersion = "0.1.0";
constexpr float kRadToDeg = 57.29577951308232f;
constexpr float kDegToRad = 1.0f / kRadToDeg;
constexpr uint32_t kSettingsApplyDelayS = 5;

const char *stageName(CalibrationService::Stage stage)
{
    switch (stage)
    {
        case CalibrationService::Stage::kA:
            return "A";
        case CalibrationService::Stage::kB:
            return "B";
        case CalibrationService::Stage::kC:
            return "C";
        case CalibrationService::Stage::kNone:
        default:
            return "NONE";
    }
}

// FR-046: logs a structured [DATA] line whenever a boot-time record load
// found the record corrupted/incompatible; a no-op on kOk/kAbsent (nothing
// to report -- kAbsent just means "never saved").
void logIfRecordCorrupted(const char *record_name, record_envelope::Status status)
{
    if (status == record_envelope::Status::kCrcMismatch)
    {
        diag_log::logRecordReset(record_name, "crc_mismatch");
    }
    else if (status == record_envelope::Status::kSchemaMismatch)
    {
        diag_log::logRecordReset(record_name, "schema_mismatch");
    }
}

const char *stageCRejectReasonName(calibration::StageCRejectReason reason)
{
    switch (reason)
    {
        case calibration::StageCRejectReason::kResidualTooHigh:
            return "residual_too_high";
        case calibration::StageCRejectReason::kDeviationTooHigh:
            return "deviation_too_high";
        case calibration::StageCRejectReason::kIncompleteCoverage:
            return "incomplete_coverage";
        case calibration::StageCRejectReason::kNone:
        default:
            return "none";
    }
}

void respond(shared_state::AppCommand &cmd, bool success, const char *json)
{
    cmd.success = success;
    if (cmd.result_buf != nullptr && cmd.result_buf_len > 0)
    {
        snprintf(cmd.result_buf, cmd.result_buf_len, "%s", json);
    }
    if (cmd.done_sem != nullptr)
    {
        xSemaphoreGive(cmd.done_sem);
    }
}

struct Context
{
    CalibrationService *calibration_service;
    N2kService *n2k_service;
    ImuDriver *imu_driver;
    KeyValueStore *stage_a_store;
    KeyValueStore *stage_b_store;
    KeyValueStore *stage_c_store;
    SettingsService *settings_service;
    Clock *clock;
};

// Publishes shared_state's HeadingCorrectionInputs from the current
// CalibrationService/StageA/StageB/StageC state, so ImuTask's pipeline sees
// the right active_stage, saved Stage A profile accuracy (for the Stage A
// in-progress freeze, FR-041), the persisted Stage B level/offset
// correction, and the persisted Stage C deviation correction.
void publishCorrectionInputs(const Context &ctx, const calibration::SensorCalibrationProfile *saved_profile,
                              const calibration::InstallationAlignment *saved_alignment,
                              const calibration::DeviationCorrection *saved_deviation)
{
    shared_state::HeadingCorrectionInputs inputs = shared_state::getHeadingCorrectionInputs();

    switch (ctx.calibration_service->activeStage())
    {
        case CalibrationService::Stage::kA:
            inputs.active_stage = heading::ActiveCalibrationStage::kA;
            break;
        case CalibrationService::Stage::kB:
            inputs.active_stage = heading::ActiveCalibrationStage::kB;
            break;
        case CalibrationService::Stage::kC:
            inputs.active_stage = heading::ActiveCalibrationStage::kC;
            break;
        case CalibrationService::Stage::kNone:
        default:
            inputs.active_stage = heading::ActiveCalibrationStage::kNone;
            break;
    }

    inputs.saved_profile.exists = saved_profile != nullptr;
    if (saved_profile != nullptr)
    {
        inputs.saved_profile.mag_accuracy = saved_profile->mag_accuracy;
        inputs.saved_profile.accel_accuracy = saved_profile->accel_accuracy;
        inputs.saved_profile.gyro_accuracy = saved_profile->gyro_accuracy;
    }

    calibration::InstallationAlignment alignment_or_default;
    bool alignment_exists = saved_alignment != nullptr;
    if (alignment_exists)
    {
        alignment_or_default = *saved_alignment;
    }
    calibration::applyInstallationAlignment(alignment_or_default, alignment_exists, inputs.level_reference,
                                             inputs.mounting_offset);

    calibration::DeviationCorrection deviation_or_default;
    bool deviation_exists = saved_deviation != nullptr;
    if (deviation_exists)
    {
        deviation_or_default = *saved_deviation;
    }
    calibration::applyDeviationCorrection(deviation_or_default, deviation_exists, inputs.deviation_correction);

    shared_state::publishHeadingCorrectionInputs(inputs);
}

void publishStageAStatus(const CalibrationService &calibration_service, const calibration::StageA &stage_a,
                          const calibration::SensorCalibrationProfile &saved_profile, bool has_saved_profile)
{
    shared_state::StageAStatusSnapshot status;
    status.persisted_done = has_saved_profile;
    if (has_saved_profile)
    {
        status.saved_mag_accuracy = saved_profile.mag_accuracy;
        status.saved_accel_accuracy = saved_profile.accel_accuracy;
        status.saved_gyro_accuracy = saved_profile.gyro_accuracy;
        snprintf(status.saved_at_iso8601, sizeof(status.saved_at_iso8601), "%s", saved_profile.saved_at_iso8601);
    }

    status.session_active = calibration_service.activeStage() == CalibrationService::Stage::kA;
    if (status.session_active)
    {
        const calibration::StageAProgress &progress = stage_a.progress();
        status.live_mag_accuracy = progress.mag_accuracy;
        status.live_accel_accuracy = progress.accel_accuracy;
        status.live_gyro_accuracy = progress.gyro_accuracy;
        for (int i = 0; i < calibration::kRestPositionCount; ++i)
        {
            status.positions_done[i] = progress.positions_done[i];
        }
        status.rotation_coverage_pct = progress.rotation_coverage_fraction * 100.0f;
    }

    shared_state::publishStageAStatus(status);
}

void publishStageBStatus(const CalibrationService &calibration_service, const calibration::StageB &stage_b,
                          const calibration::InstallationAlignment &saved_alignment, bool has_saved_alignment)
{
    shared_state::StageBStatusSnapshot status;
    status.persisted_done = has_saved_alignment;
    if (has_saved_alignment)
    {
        status.saved_offset_deg = saved_alignment.mounting_offset_rad * kRadToDeg;
        status.saved_method_is_gps =
            saved_alignment.offset_method == static_cast<uint8_t>(calibration::StageBOffsetMethod::kGpsCourse);
        snprintf(status.saved_at_iso8601, sizeof(status.saved_at_iso8601), "%s", saved_alignment.saved_at_iso8601);
    }

    status.session_active = calibration_service.activeStage() == CalibrationService::Stage::kB;
    if (status.session_active)
    {
        calibration::StageBState state = stage_b.state();
        status.level_set = state != calibration::StageBState::kIdle && state != calibration::StageBState::kLevelCapturing;
        status.awaiting_method_choice = state == calibration::StageBState::kLevelSet;
        status.awaiting_bearing_entry = state == calibration::StageBState::kAwaitingBearingEntry;
        status.awaiting_gps_alignment = state == calibration::StageBState::kAwaitingGpsAlignment;
        status.waiting_speed_too_low = stage_b.waitingReason() == calibration::StageBWaitingReason::kSpeedTooLow;
        status.waiting_course_not_steady = stage_b.waitingReason() == calibration::StageBWaitingReason::kCourseNotSteady;
        status.has_preview_offset = state == calibration::StageBState::kOffsetComputed;
        if (status.has_preview_offset)
        {
            status.preview_offset_deg = stage_b.previewOffsetRad() * kRadToDeg;
        }
    }

    shared_state::publishStageBStatus(status);
}

void publishStageCStatus(const CalibrationService &calibration_service, const calibration::StageC &stage_c,
                          const calibration::DeviationCorrection &saved_deviation, bool has_saved_deviation)
{
    shared_state::StageCStatusSnapshot status;
    status.persisted_done = has_saved_deviation;
    if (has_saved_deviation)
    {
        status.saved_max_deviation_deg = saved_deviation.max_deviation_rad * kRadToDeg;
        status.saved_residual_rms_deg = saved_deviation.residual_rms_rad * kRadToDeg;
        status.saved_source_is_gps =
            saved_deviation.source == static_cast<uint8_t>(calibration::StageCSource::kGpsSwing);
        snprintf(status.saved_at_iso8601, sizeof(status.saved_at_iso8601), "%s", saved_deviation.saved_at_iso8601);
    }

    status.session_active = calibration_service.activeStage() == CalibrationService::Stage::kC;
    if (status.session_active)
    {
        status.turns_completed = stage_c.turnsCompleted();
        status.sector_coverage_pct = stage_c.sectorCoveragePct();
        status.turning_too_fast = stage_c.turningTooFast();
        status.awaiting_manual_point = stage_c.state() == calibration::StageCState::kAwaitingManualPoint;
        status.manual_point_index = status.awaiting_manual_point ? stage_c.manualPointIndex() : 0;

        status.has_candidate_result = stage_c.state() == calibration::StageCState::kResultReady;
        if (status.has_candidate_result)
        {
            status.candidate_max_deviation_deg = stage_c.candidateMaxDeviationRad() * kRadToDeg;
            status.candidate_residual_rms_deg = stage_c.candidateResidualRmsRad() * kRadToDeg;
            heading::DeviationCoefficients coeffs = stage_c.candidateCoefficients();
            status.candidate_coefficients[0] = coeffs.a;
            status.candidate_coefficients[1] = coeffs.b;
            status.candidate_coefficients[2] = coeffs.c;
            status.candidate_coefficients[3] = coeffs.d;
            status.candidate_coefficients[4] = coeffs.e;
        }
    }

    shared_state::publishStageCStatus(status);
}

// FR-032 (quoted): "When the user starts a new Stage A calibration attempt or
// resets Stage A, the system MUST indicate to the user that the Stage C
// result may also need to be redone" -- surfaced as a response field plus a
// serial log note whenever a DeviationCorrection is currently saved.
void noteStageCRedoHint(bool has_saved_deviation, char *body, size_t body_len, const char *body_prefix)
{
    if (has_saved_deviation)
    {
        diag_log::Line("CAL").kv("stage", "A").token("note").kv("hint", "stage_c_may_need_redo").emit();
        snprintf(body, body_len, "%s,\"stage_c_may_need_redo\":true}", body_prefix);
    }
    else
    {
        snprintf(body, body_len, "%s}", body_prefix);
    }
}

void handleStageAStart(Context &ctx, calibration::StageA &stage_a, bool has_saved_deviation,
                        shared_state::AppCommand &cmd)
{
    CalibrationService::Stage busy_stage = CalibrationService::Stage::kNone;
    if (!ctx.calibration_service->tryStart(CalibrationService::Stage::kA, busy_stage))
    {
        diag_log::Line("CAL").token("busy").kv("stage", stageName(busy_stage)).kv("rejected", "A").emit();
        char body[160];
        snprintf(body, sizeof(body),
                 "{\"schema\":1,\"error\":{\"code\":\"CALIBRATION_BUSY\",\"message\":\"another calibration procedure "
                 "is already running\"},\"active_stage\":\"%s\"}",
                 stageName(busy_stage));
        respond(cmd, false, body);
        return;
    }

    // Dynamic calibration is enabled once, continuously, at driver init
    // (imu_driver_bno08x.cpp) rather than here -- it needs to be running in
    // normal operation too, not just while Stage A is active.
    float now_s = static_cast<float>(ctx.clock->monotonicMillis()) / 1000.0f;
    stage_a.start(now_s);
    diag_log::Line("CAL").kv("stage", "A").kv("state", "AwaitingStillness").emit();
    char body[96];
    noteStageCRedoHint(has_saved_deviation, body, sizeof(body), "{\"schema\":1,\"started\":true");
    respond(cmd, true, body);
}

void handleStageACancel(Context &ctx, calibration::StageA &stage_a, shared_state::AppCommand &cmd)
{
    stage_a.cancel();
    ctx.calibration_service->cancel(CalibrationService::Stage::kA);
    diag_log::Line("CAL").kv("stage", "A").kv("result", "cancelled").emit();
    web_api::broadcastCalibrationResult("A", "CANCELLED", nullptr);
    respond(cmd, true, "{\"schema\":1,\"cancelled\":true}");
}

void handleStageAReset(Context &ctx, calibration::SensorCalibrationProfile *saved_profile, bool *has_saved_profile,
                        const calibration::InstallationAlignment *saved_alignment,
                        const calibration::DeviationCorrection *saved_deviation, shared_state::AppCommand &cmd)
{
    if (!cmd.confirm)
    {
        respond(cmd, false,
                "{\"schema\":1,\"error\":{\"code\":\"CONFIRM_REQUIRED\",\"message\":\"reset requires "
                "confirm:true\"}}");
        return;
    }

    record_envelope::resetToDefault(*ctx.stage_a_store);
    *has_saved_profile = false;
    *saved_profile = calibration::SensorCalibrationProfile{};
    publishCorrectionInputs(ctx, nullptr, saved_alignment, saved_deviation);

    diag_log::Line("CAL").kv("stage", "A").kv("result", "reset").emit();
    char body[96];
    noteStageCRedoHint(saved_deviation != nullptr, body, sizeof(body), "{\"schema\":1,\"reset\":true");
    respond(cmd, true, body);
}

void handleStageBStart(Context &ctx, calibration::StageB &stage_b, shared_state::AppCommand &cmd)
{
    CalibrationService::Stage busy_stage = CalibrationService::Stage::kNone;
    if (!ctx.calibration_service->tryStart(CalibrationService::Stage::kB, busy_stage))
    {
        diag_log::Line("CAL").token("busy").kv("stage", stageName(busy_stage)).kv("rejected", "B").emit();
        char body[160];
        snprintf(body, sizeof(body),
                 "{\"schema\":1,\"error\":{\"code\":\"CALIBRATION_BUSY\",\"message\":\"another calibration procedure "
                 "is already running\"},\"active_stage\":\"%s\"}",
                 stageName(busy_stage));
        respond(cmd, false, body);
        return;
    }

    float now_s = static_cast<float>(ctx.clock->monotonicMillis()) / 1000.0f;
    stage_b.start(now_s);
    diag_log::Line("CAL").kv("stage", "B").kv("state", "Idle").emit();
    respond(cmd, true, "{\"schema\":1,\"started\":true}");
}

void handleStageBCancel(Context &ctx, calibration::StageB &stage_b, shared_state::AppCommand &cmd)
{
    stage_b.cancel();
    ctx.calibration_service->cancel(CalibrationService::Stage::kB);
    diag_log::Line("CAL").kv("stage", "B").kv("result", "cancelled").emit();
    web_api::broadcastCalibrationResult("B", "CANCELLED", nullptr);
    respond(cmd, true, "{\"schema\":1,\"cancelled\":true}");
}

void handleStageBLevel(Context &ctx, calibration::StageB &stage_b, shared_state::AppCommand &cmd)
{
    if (ctx.calibration_service->activeStage() != CalibrationService::Stage::kB)
    {
        respond(cmd, false,
                "{\"schema\":1,\"error\":{\"code\":\"NOT_ACTIVE\",\"message\":\"Stage B is not the active "
                "procedure\"}}");
        return;
    }
    float now_s = static_cast<float>(ctx.clock->monotonicMillis()) / 1000.0f;
    stage_b.beginLevelCapture(now_s);
    diag_log::Line("CAL").kv("stage", "B").kv("state", "LevelCapturing").emit();
    respond(cmd, true, "{\"schema\":1,\"level_set\":false}");
}

void handleStageBChooseGps(Context &ctx, calibration::StageB &stage_b, shared_state::AppCommand &cmd)
{
    if (stage_b.state() != calibration::StageBState::kLevelSet)
    {
        respond(cmd, false,
                "{\"schema\":1,\"error\":{\"code\":\"WRONG_STATE\",\"message\":\"level must be set first\"}}");
        return;
    }
    bool debug_gps_active = shared_state::getDebugGpsInject().active;
    if (!ctx.n2k_service->cogSogSourcePresent() && !debug_gps_active)
    {
        respond(cmd, false,
                "{\"schema\":1,\"error\":{\"code\":\"NO_COG_SOURCE\",\"message\":\"no COG/SOG source detected on "
                "the bus\"}}");
        return;
    }
    stage_b.chooseGpsCourse();
    diag_log::Line("CAL").kv("stage", "B").kv("state", "AwaitingGpsAlignment").emit();
    respond(cmd, true, "{\"schema\":1,\"awaiting_gps_alignment\":true}");
}

// FR-024: bus-provided variation when fresh, else 0 rad (manual variation
// entry is not yet implemented as its own UI -- a documented gap, not a
// silent wrong answer: logged whenever it's the reason variation is 0).
float resolveVariationOrZero(Context &ctx)
{
    n2k_codec::VariationSource source;
    source.bus_ever_received = ctx.n2k_service->hasBusVariation();
    source.bus_variation_rad = ctx.n2k_service->busVariationRad();
    source.bus_age_s = ctx.n2k_service->variationSourcePresent() ? 0.0f : (thresholds::kStageCReferenceMaxAgeS + 1.0f);

    float out = 0.0f;
    if (n2k_codec::resolveVariation(source, out))
    {
        return out;
    }
    diag_log::Line("CAL").kv("stage", "B").kv("variation", "unavailable").emit();
    return 0.0f;
}

void handleStageBBearing(Context &ctx, calibration::StageB &stage_b, shared_state::AppCommand &cmd)
{
    if (stage_b.state() != calibration::StageBState::kLevelSet &&
        stage_b.state() != calibration::StageBState::kAwaitingBearingEntry)
    {
        respond(cmd, false,
                "{\"schema\":1,\"error\":{\"code\":\"WRONG_STATE\",\"message\":\"level must be set first\"}}");
        return;
    }
    stage_b.chooseKnownBearing();

    float variation_rad = resolveVariationOrZero(ctx);
    float current_heading_rad = shared_state::getHeadingReading().heading_rad;
    float true_bearing_rad = cmd.float_param * kDegToRad;
    stage_b.enterBearing(true_bearing_rad, variation_rad, current_heading_rad);

    if (stage_b.state() != calibration::StageBState::kOffsetComputed)
    {
        respond(cmd, false,
                "{\"schema\":1,\"error\":{\"code\":\"INVALID_STATE\",\"message\":\"could not compute offset\"}}");
        return;
    }

    diag_log::Line("CAL")
        .kv("stage", "B")
        .kv("state", "OffsetComputed")
        .kv("preview_deg", static_cast<double>(stage_b.previewOffsetRad() * kRadToDeg))
        .emit();

    char body[128];
    snprintf(body, sizeof(body), "{\"schema\":1,\"preview_offset_deg\":%.2f}",
             static_cast<double>(stage_b.previewOffsetRad() * kRadToDeg));
    respond(cmd, true, body);
}

void handleStageBApply(Context &ctx, calibration::StageB &stage_b, calibration::InstallationAlignment *saved_alignment,
                        bool *has_saved_alignment, const calibration::SensorCalibrationProfile *saved_profile,
                        const calibration::DeviationCorrection *saved_deviation, shared_state::AppCommand &cmd)
{
    if (stage_b.state() != calibration::StageBState::kOffsetComputed)
    {
        respond(cmd, false,
                "{\"schema\":1,\"error\":{\"code\":\"NO_CANDIDATE\",\"message\":\"no computed offset to apply\"}}");
        return;
    }

    float now_s = static_cast<float>(ctx.clock->monotonicMillis()) / 1000.0f;
    stage_b.accept(now_s);

    calibration::InstallationAlignment alignment;
    char iso8601[32];
    if (!ctx.clock->wallClockIso8601(iso8601, sizeof(iso8601)))
    {
        snprintf(iso8601, sizeof(iso8601), "unavailable");
    }
    snprintf(alignment.saved_at_iso8601, sizeof(alignment.saved_at_iso8601), "%s", iso8601);
    heading::Quaternion level = stage_b.levelReferenceQuat();
    alignment.level_reference_quat[0] = level.w;
    alignment.level_reference_quat[1] = level.x;
    alignment.level_reference_quat[2] = level.y;
    alignment.level_reference_quat[3] = level.z;
    alignment.mounting_offset_rad = stage_b.previewOffsetRad();
    alignment.offset_method = static_cast<uint8_t>(stage_b.offsetMethod());

    bool saved = calibration::saveInstallationAlignment(*ctx.stage_b_store, alignment);
    if (saved)
    {
        *saved_alignment = alignment;
        *has_saved_alignment = true;
        publishCorrectionInputs(ctx, saved_profile, saved_alignment, saved_deviation);
        diag_log::Line("CAL")
            .kv("stage", "B")
            .kv("result", "saved")
            .kv("offset_deg", static_cast<double>(alignment.mounting_offset_rad * kRadToDeg))
            .emit();
    }

    ctx.calibration_service->endActive();
    web_api::broadcastCalibrationResult("B", saved ? "SAVED" : "REJECTED", saved ? nullptr : "save failed");
    respond(cmd, saved, saved ? "{\"schema\":1,\"applied\":true}"
                              : "{\"schema\":1,\"error\":{\"code\":\"SAVE_FAILED\",\"message\":\"could not persist "
                                "the alignment record\"}}");
}

void handleStageBDiscard(Context &ctx, calibration::StageB &stage_b, shared_state::AppCommand &cmd)
{
    if (stage_b.state() != calibration::StageBState::kOffsetComputed)
    {
        respond(cmd, false,
                "{\"schema\":1,\"error\":{\"code\":\"NO_CANDIDATE\",\"message\":\"no computed offset to discard\"}}");
        return;
    }
    stage_b.discard();
    ctx.calibration_service->endActive();
    diag_log::Line("CAL").kv("stage", "B").kv("result", "discarded").emit();
    web_api::broadcastCalibrationResult("B", "CANCELLED", "discarded by user");
    respond(cmd, true, "{\"schema\":1,\"discarded\":true}");
}

void handleStageBReset(Context &ctx, calibration::InstallationAlignment *saved_alignment, bool *has_saved_alignment,
                        const calibration::SensorCalibrationProfile *saved_profile,
                        const calibration::DeviationCorrection *saved_deviation, shared_state::AppCommand &cmd)
{
    if (!cmd.confirm)
    {
        respond(cmd, false,
                "{\"schema\":1,\"error\":{\"code\":\"CONFIRM_REQUIRED\",\"message\":\"reset requires "
                "confirm:true\"}}");
        return;
    }

    record_envelope::resetToDefault(*ctx.stage_b_store);
    *has_saved_alignment = false;
    *saved_alignment = calibration::InstallationAlignment{};
    publishCorrectionInputs(ctx, saved_profile, nullptr, saved_deviation);

    diag_log::Line("CAL").kv("stage", "B").kv("result", "reset").emit();
    respond(cmd, true, "{\"schema\":1,\"reset\":true}");
}

void handleSettingsSave(Context &ctx, shared_state::AppCommand &cmd)
{
    settings::NetworkSettings new_settings;
    snprintf(new_settings.ssid, sizeof(new_settings.ssid), "%s", cmd.string_param);

    if (!settings::validateSsid(new_settings.ssid))
    {
        respond(cmd, false,
                "{\"schema\":1,\"error\":{\"code\":\"SSID_INVALID\",\"message\":\"SSID must be 1-32 characters "
                "with no leading or trailing whitespace\"}}");
        return;
    }

    float now_s = static_cast<float>(ctx.clock->monotonicMillis()) / 1000.0f;
    if (!ctx.settings_service->save(new_settings, now_s, kSettingsApplyDelayS))
    {
        respond(cmd, false,
                "{\"schema\":1,\"error\":{\"code\":\"SAVE_FAILED\",\"message\":\"could not persist settings\"}}");
        return;
    }

    diag_log::Line("WIFI").token("ssid_changed").kv("to", new_settings.ssid).kv("applies_in_s", static_cast<long>(kSettingsApplyDelayS)).emit();
    web_api::broadcastSettingsChanged(new_settings.ssid, kSettingsApplyDelayS);

    char body[128];
    snprintf(body, sizeof(body), "{\"schema\":1,\"ssid\":\"%s\",\"applies_in_s\":%lu}", new_settings.ssid,
             static_cast<unsigned long>(kSettingsApplyDelayS));
    respond(cmd, true, body);
}

void handleNetworkReset(Context &ctx, const char *trigger, shared_state::AppCommand *cmd)
{
    if (cmd != nullptr && !cmd->confirm)
    {
        respond(*cmd, false,
                "{\"schema\":1,\"error\":{\"code\":\"CONFIRM_REQUIRED\",\"message\":\"reset requires "
                "confirm:true\"}}");
        return;
    }

    float now_s = static_cast<float>(ctx.clock->monotonicMillis()) / 1000.0f;
    ctx.settings_service->resetToDefault(now_s);
    diag_log::Line("WIFI").token("network_reset").kv("trigger", trigger).emit();
    web_api::broadcastSettingsChanged(ctx.settings_service->current().ssid, 0);

    if (cmd != nullptr)
    {
        char body[128];
        snprintf(body, sizeof(body), "{\"schema\":1,\"ssid\":\"%s\",\"applies_in_s\":0}",
                 ctx.settings_service->current().ssid);
        respond(*cmd, true, body);
    }
}

void handleStageCStart(Context &ctx, calibration::StageC &stage_c, bool manual, shared_state::AppCommand &cmd)
{
    CalibrationService::Stage busy_stage = CalibrationService::Stage::kNone;
    if (!ctx.calibration_service->tryStart(CalibrationService::Stage::kC, busy_stage))
    {
        diag_log::Line("CAL").token("busy").kv("stage", stageName(busy_stage)).kv("rejected", "C").emit();
        char body[160];
        snprintf(body, sizeof(body),
                 "{\"schema\":1,\"error\":{\"code\":\"CALIBRATION_BUSY\",\"message\":\"another calibration procedure "
                 "is already running\"},\"active_stage\":\"%s\"}",
                 stageName(busy_stage));
        respond(cmd, false, body);
        return;
    }

    float now_s = static_cast<float>(ctx.clock->monotonicMillis()) / 1000.0f;
    stage_c.start(now_s);
    if (manual)
    {
        stage_c.startManualSwing();
        diag_log::Line("CAL").kv("stage", "C").kv("state", "AwaitingManualPoint").kv("point", 1).emit();
    }
    else
    {
        stage_c.startGpsSwing();
        diag_log::Line("CAL").kv("stage", "C").kv("state", "Swinging").emit();
    }
    respond(cmd, true, "{\"schema\":1,\"started\":true}");
}

void handleStageCCancel(Context &ctx, calibration::StageC &stage_c, shared_state::AppCommand &cmd)
{
    stage_c.cancel();
    ctx.calibration_service->cancel(CalibrationService::Stage::kC);
    diag_log::Line("CAL").kv("stage", "C").kv("result", "cancelled").emit();
    web_api::broadcastCalibrationResult("C", "CANCELLED", nullptr);
    respond(cmd, true, "{\"schema\":1,\"cancelled\":true}");
}

void handleStageCManualPoint(Context &ctx, calibration::StageC &stage_c, shared_state::AppCommand &cmd)
{
    if (stage_c.state() != calibration::StageCState::kAwaitingManualPoint)
    {
        respond(cmd, false,
                "{\"schema\":1,\"error\":{\"code\":\"NOT_ACTIVE\",\"message\":\"not awaiting a manual point\"}}");
        return;
    }

    float variation_rad = resolveVariationOrZero(ctx);
    float raw_heading_rad = shared_state::getHeadingReading().compass_heading_rad;
    float true_reference_bearing_rad = cmd.float_param * kDegToRad;
    stage_c.enterManualPoint(true_reference_bearing_rad, raw_heading_rad, variation_rad);

    diag_log::Line("CAL").kv("stage", "C").kv("point_entered", true).emit();

    if (stage_c.state() == calibration::StageCState::kRejected)
    {
        diag_log::Line("CAL")
            .kv("stage", "C")
            .kv("result", "rejected")
            .kv("reason", stageCRejectReasonName(stage_c.rejectReason()))
            .kv("rms_deg", static_cast<double>(stage_c.candidateResidualRmsRad() * kRadToDeg))
            .emit();
        ctx.calibration_service->endActive();
        web_api::broadcastCalibrationResult("C", "REJECTED", stageCRejectReasonName(stage_c.rejectReason()));
    }
    respond(cmd, true, "{\"schema\":1,\"point_recorded\":true}");
}

void handleStageCApply(Context &ctx, calibration::StageC &stage_c, calibration::DeviationCorrection *saved_deviation,
                        bool *has_saved_deviation, const calibration::SensorCalibrationProfile *saved_profile,
                        const calibration::InstallationAlignment *saved_alignment, shared_state::AppCommand &cmd)
{
    if (stage_c.state() != calibration::StageCState::kResultReady)
    {
        respond(cmd, false,
                "{\"schema\":1,\"error\":{\"code\":\"NO_CANDIDATE\",\"message\":\"no computed result to apply\"}}");
        return;
    }

    float now_s = static_cast<float>(ctx.clock->monotonicMillis()) / 1000.0f;
    stage_c.apply(now_s);

    calibration::DeviationCorrection correction;
    char iso8601[32];
    if (!ctx.clock->wallClockIso8601(iso8601, sizeof(iso8601)))
    {
        snprintf(iso8601, sizeof(iso8601), "unavailable");
    }
    snprintf(correction.saved_at_iso8601, sizeof(correction.saved_at_iso8601), "%s", iso8601);
    heading::DeviationCoefficients coeffs = stage_c.candidateCoefficients();
    correction.coefficients[0] = coeffs.a;
    correction.coefficients[1] = coeffs.b;
    correction.coefficients[2] = coeffs.c;
    correction.coefficients[3] = coeffs.d;
    correction.coefficients[4] = coeffs.e;
    correction.max_deviation_rad = stage_c.candidateMaxDeviationRad();
    correction.residual_rms_rad = stage_c.candidateResidualRmsRad();
    correction.source = static_cast<uint8_t>(stage_c.source());

    bool saved = calibration::saveDeviationCorrection(*ctx.stage_c_store, correction);
    if (saved)
    {
        *saved_deviation = correction;
        *has_saved_deviation = true;
        publishCorrectionInputs(ctx, saved_profile, saved_alignment, saved_deviation);
        diag_log::Line("CAL")
            .kv("stage", "C")
            .kv("result", "saved")
            .kv("max_deviation_deg", static_cast<double>(correction.max_deviation_rad * kRadToDeg))
            .kv("rms_deg", static_cast<double>(correction.residual_rms_rad * kRadToDeg))
            .emit();
    }

    ctx.calibration_service->endActive();
    web_api::broadcastCalibrationResult("C", saved ? "SAVED" : "REJECTED", saved ? nullptr : "save failed");
    respond(cmd, saved, saved ? "{\"schema\":1,\"applied\":true}"
                              : "{\"schema\":1,\"error\":{\"code\":\"SAVE_FAILED\",\"message\":\"could not persist "
                                "the deviation record\"}}");
}

void handleStageCDiscard(Context &ctx, calibration::StageC &stage_c, shared_state::AppCommand &cmd)
{
    if (stage_c.state() != calibration::StageCState::kResultReady)
    {
        respond(cmd, false,
                "{\"schema\":1,\"error\":{\"code\":\"NO_CANDIDATE\",\"message\":\"no computed result to discard\"}}");
        return;
    }
    stage_c.discard();
    ctx.calibration_service->endActive();
    diag_log::Line("CAL").kv("stage", "C").kv("result", "discarded").emit();
    web_api::broadcastCalibrationResult("C", "CANCELLED", "discarded by user");
    respond(cmd, true, "{\"schema\":1,\"discarded\":true}");
}

void handleStageCReset(Context &ctx, calibration::DeviationCorrection *saved_deviation, bool *has_saved_deviation,
                        const calibration::SensorCalibrationProfile *saved_profile,
                        const calibration::InstallationAlignment *saved_alignment, shared_state::AppCommand &cmd)
{
    if (!cmd.confirm)
    {
        respond(cmd, false,
                "{\"schema\":1,\"error\":{\"code\":\"CONFIRM_REQUIRED\",\"message\":\"reset requires "
                "confirm:true\"}}");
        return;
    }

    record_envelope::resetToDefault(*ctx.stage_c_store);
    *has_saved_deviation = false;
    *saved_deviation = calibration::DeviationCorrection{};
    publishCorrectionInputs(ctx, saved_profile, saved_alignment, nullptr);

    diag_log::Line("CAL").kv("stage", "C").kv("result", "reset").emit();
    respond(cmd, true, "{\"schema\":1,\"reset\":true}");
}

struct StageState
{
    calibration::StageA *stage_a;
    calibration::SensorCalibrationProfile *saved_profile;
    bool *has_saved_profile;
    calibration::StageB *stage_b;
    calibration::InstallationAlignment *saved_alignment;
    bool *has_saved_alignment;
    calibration::StageC *stage_c;
    calibration::DeviationCorrection *saved_deviation;
    bool *has_saved_deviation;
};

void handleCommand(Context &ctx, StageState &s, shared_state::AppCommand &cmd)
{
    switch (cmd.type)
    {
        case shared_state::AppCommandType::kCalStartA:
            handleStageAStart(ctx, *s.stage_a, *s.has_saved_deviation, cmd);
            break;
        case shared_state::AppCommandType::kCalCancelA:
            handleStageACancel(ctx, *s.stage_a, cmd);
            break;
        case shared_state::AppCommandType::kCalResetA:
            handleStageAReset(ctx, s.saved_profile, s.has_saved_profile, s.saved_alignment, s.saved_deviation, cmd);
            break;
        case shared_state::AppCommandType::kCalStartB:
            handleStageBStart(ctx, *s.stage_b, cmd);
            break;
        case shared_state::AppCommandType::kCalCancelB:
            handleStageBCancel(ctx, *s.stage_b, cmd);
            break;
        case shared_state::AppCommandType::kCalBLevel:
            handleStageBLevel(ctx, *s.stage_b, cmd);
            break;
        case shared_state::AppCommandType::kCalBChooseGps:
            handleStageBChooseGps(ctx, *s.stage_b, cmd);
            break;
        case shared_state::AppCommandType::kCalBBearing:
            handleStageBBearing(ctx, *s.stage_b, cmd);
            break;
        case shared_state::AppCommandType::kCalApplyB:
            handleStageBApply(ctx, *s.stage_b, s.saved_alignment, s.has_saved_alignment, s.saved_profile,
                               s.saved_deviation, cmd);
            break;
        case shared_state::AppCommandType::kCalDiscardB:
            handleStageBDiscard(ctx, *s.stage_b, cmd);
            break;
        case shared_state::AppCommandType::kCalResetB:
            handleStageBReset(ctx, s.saved_alignment, s.has_saved_alignment, s.saved_profile, s.saved_deviation, cmd);
            break;
        case shared_state::AppCommandType::kCalStartCGps:
            handleStageCStart(ctx, *s.stage_c, false, cmd);
            break;
        case shared_state::AppCommandType::kCalStartCManual:
            handleStageCStart(ctx, *s.stage_c, true, cmd);
            break;
        case shared_state::AppCommandType::kCalCancelC:
            handleStageCCancel(ctx, *s.stage_c, cmd);
            break;
        case shared_state::AppCommandType::kCalCManualPoint:
            handleStageCManualPoint(ctx, *s.stage_c, cmd);
            break;
        case shared_state::AppCommandType::kCalApplyC:
            handleStageCApply(ctx, *s.stage_c, s.saved_deviation, s.has_saved_deviation, s.saved_profile,
                               s.saved_alignment, cmd);
            break;
        case shared_state::AppCommandType::kCalDiscardC:
            handleStageCDiscard(ctx, *s.stage_c, cmd);
            break;
        case shared_state::AppCommandType::kCalResetC:
            handleStageCReset(ctx, s.saved_deviation, s.has_saved_deviation, s.saved_profile, s.saved_alignment, cmd);
            break;
        case shared_state::AppCommandType::kSettingsSave:
            handleSettingsSave(ctx, cmd);
            break;
        case shared_state::AppCommandType::kNetworkReset:
            handleNetworkReset(ctx, "http_api", &cmd);
            break;
        default:
            respond(cmd, false,
                    "{\"schema\":1,\"error\":{\"code\":\"NOT_IMPLEMENTED\",\"message\":\"not yet implemented\"}}");
            break;
    }
}

// Feeds the latest raw IMU sample into an active Stage A attempt and
// handles its terminal transitions: Done persists the profile and calls
// ImuDriver::saveDcd() (T062 -- only ever on this transition, never on
// Cancel/TimedOut); TimedOut/Cancelled just end the session.
void driveStageA(Context &ctx, calibration::StageA &stage_a, calibration::SensorCalibrationProfile *saved_profile,
                  bool *has_saved_profile, const calibration::InstallationAlignment *saved_alignment,
                  const calibration::DeviationCorrection *saved_deviation)
{
    if (ctx.calibration_service->activeStage() != CalibrationService::Stage::kA)
    {
        return;
    }

    calibration::StageAState state_before = stage_a.state();
    if (state_before == calibration::StageAState::kIdle || state_before == calibration::StageAState::kDone ||
        state_before == calibration::StageAState::kTimedOut || state_before == calibration::StageAState::kCancelled)
    {
        return;
    }

    shared_state::RawImuSample raw = shared_state::getRawImuSample();
    calibration::StageASample sample;
    sample.raw_quat = raw.raw_quat;
    sample.gyro_x_rad_s = raw.gyro_x_rad_s;
    sample.gyro_y_rad_s = raw.gyro_y_rad_s;
    sample.gyro_z_rad_s = raw.gyro_z_rad_s;
    sample.mag_accuracy = raw.mag_accuracy;
    sample.accel_accuracy = raw.accel_accuracy;
    sample.gyro_accuracy = raw.gyro_accuracy;
    sample.dt_s = kCommandWaitMs / 1000.0f;

    float now_s = static_cast<float>(ctx.clock->monotonicMillis()) / 1000.0f;
    stage_a.update(sample, now_s);

    calibration::StageAState state_after = stage_a.state();
    if (state_after == state_before)
    {
        return;
    }

    int positions_done_count = 0;
    for (bool d : stage_a.progress().positions_done)
    {
        if (d) ++positions_done_count;
    }
    diag_log::Line("CAL").kv("stage", "A").kv("positions", positions_done_count).emit();

    if (state_after == calibration::StageAState::kDone)
    {
        ctx.imu_driver->saveDcd();

        calibration::SensorCalibrationProfile profile;
        char iso8601[32];
        if (!ctx.clock->wallClockIso8601(iso8601, sizeof(iso8601)))
        {
            snprintf(iso8601, sizeof(iso8601), "unavailable");
        }
        snprintf(profile.saved_at_iso8601, sizeof(profile.saved_at_iso8601), "%s", iso8601);
        profile.mag_accuracy = stage_a.progress().mag_accuracy;
        profile.accel_accuracy = stage_a.progress().accel_accuracy;
        profile.gyro_accuracy = stage_a.progress().gyro_accuracy;
        snprintf(profile.firmware_version, sizeof(profile.firmware_version), "%s", kFirmwareVersion);

        bool saved = calibration::saveSensorCalibrationProfile(*ctx.stage_a_store, profile);
        if (saved)
        {
            *saved_profile = profile;
            *has_saved_profile = true;
            diag_log::Line("CAL")
                .kv("stage", "A")
                .kv("result", "saved")
                .kv("mag", static_cast<int>(profile.mag_accuracy))
                .kv("accel", static_cast<int>(profile.accel_accuracy))
                .kv("gyro", static_cast<int>(profile.gyro_accuracy))
                .emit();
        }
        ctx.calibration_service->endActive();
        publishCorrectionInputs(ctx, *has_saved_profile ? saved_profile : nullptr, saved_alignment, saved_deviation);
        web_api::broadcastCalibrationResult("A", saved ? "SAVED" : "REJECTED", saved ? nullptr : "save failed");
    }
    else if (state_after == calibration::StageAState::kTimedOut)
    {
        diag_log::Line("CAL").kv("stage", "A").kv("result", "timeout").emit();
        ctx.calibration_service->endActive();
        publishCorrectionInputs(ctx, *has_saved_profile ? saved_profile : nullptr, saved_alignment, saved_deviation);
        web_api::broadcastCalibrationResult("A", "TIMED_OUT", "calibration timed out before reaching High accuracy");
    }
}

// Feeds live samples into an active Stage B attempt: raw IMU samples while
// LevelCapturing, and COG/SOG (real bus data, or the bench-only debug
// injection) while AwaitingGpsAlignment. OffsetComputed waits for an
// explicit Apply/Discard command, so nothing to drive there.
void driveStageB(Context &ctx, calibration::StageB &stage_b)
{
    if (ctx.calibration_service->activeStage() != CalibrationService::Stage::kB)
    {
        return;
    }

    calibration::StageBState state = stage_b.state();
    float dt_s = kCommandWaitMs / 1000.0f;
    float now_s = static_cast<float>(ctx.clock->monotonicMillis()) / 1000.0f;

    if (state == calibration::StageBState::kLevelCapturing)
    {
        shared_state::RawImuSample raw = shared_state::getRawImuSample();
        stage_b.updateLevelCapture(raw.raw_quat, raw.gyro_x_rad_s, raw.gyro_y_rad_s, raw.gyro_z_rad_s, dt_s);
        if (stage_b.state() == calibration::StageBState::kLevelSet)
        {
            diag_log::Line("CAL").kv("stage", "B").kv("state", "LevelSet").emit();
        }
        return;
    }

    if (state == calibration::StageBState::kAwaitingGpsAlignment)
    {
        shared_state::DebugGpsInject inject = shared_state::getDebugGpsInject();
        float sog_m_s;
        float cog_rad;
        if (inject.active)
        {
            sog_m_s = inject.sog_m_s;
            cog_rad = inject.cog_rad;
        }
        else
        {
            sog_m_s = ctx.n2k_service->lastSogMS();
            cog_rad = ctx.n2k_service->lastCogRad();
        }
        float variation_rad = inject.active ? inject.variation_rad : resolveVariationOrZero(ctx);
        float current_heading_rad = shared_state::getHeadingReading().heading_rad;

        stage_b.updateGpsAlignment(sog_m_s, cog_rad, variation_rad, current_heading_rad, now_s);

        if (stage_b.state() == calibration::StageBState::kOffsetComputed)
        {
            diag_log::Line("CAL")
                .kv("stage", "B")
                .kv("state", "OffsetComputed")
                .kv("preview_deg", static_cast<double>(stage_b.previewOffsetRad() * kRadToDeg))
                .emit();
        }
    }
}

// Feeds live samples into an active Stage C GPS-swing attempt: raw IMU
// samples (heading/turn-rate/accuracy) plus COG/SOG (real bus data, or the
// bench-only debug injection) while Swinging. AwaitingManualPoint and
// ResultReady wait for explicit commands (manual-point / apply / discard),
// so nothing to drive there.
void driveStageC(Context &ctx, calibration::StageC &stage_c)
{
    if (ctx.calibration_service->activeStage() != CalibrationService::Stage::kC)
    {
        return;
    }
    if (stage_c.state() != calibration::StageCState::kSwinging)
    {
        return;
    }

    shared_state::DebugGpsInject inject = shared_state::getDebugGpsInject();
    float sog_m_s;
    float cog_rad;
    float variation_rad;
    bool reference_fresh;
    if (inject.active)
    {
        sog_m_s = inject.sog_m_s;
        cog_rad = inject.cog_rad;
        variation_rad = inject.variation_rad;
        reference_fresh = true;
    }
    else
    {
        sog_m_s = ctx.n2k_service->lastSogMS();
        cog_rad = ctx.n2k_service->lastCogRad();
        variation_rad = resolveVariationOrZero(ctx);
        reference_fresh = ctx.n2k_service->cogSogSourcePresent();
    }
    float reference_age_s = reference_fresh ? 0.0f : (thresholds::kStageCReferenceMaxAgeS + 1.0f);
    float reference_magnetic_heading_rad = cog_rad - variation_rad;

    heading::HeadingReading reading = shared_state::getHeadingReading();
    shared_state::RawImuSample raw = shared_state::getRawImuSample();

    calibration::StageCState state_before = stage_c.state();
    float now_s = static_cast<float>(ctx.clock->monotonicMillis()) / 1000.0f;
    stage_c.updateSwing(reading.compass_heading_rad, reference_magnetic_heading_rad, reading.rate_of_turn_rad_s,
                         sog_m_s, reference_age_s, raw.mag_accuracy, raw.accel_accuracy, raw.gyro_accuracy, now_s);

    diag_log::Line("CAL")
        .kv("stage", "C")
        .kv("sector_coverage", static_cast<double>(stage_c.sectorCoveragePct()))
        .kv("turns", static_cast<double>(stage_c.turnsCompleted()))
        .emit();

    calibration::StageCState state_after = stage_c.state();
    if (state_after == calibration::StageCState::kRejected && state_after != state_before)
    {
        diag_log::Line("CAL")
            .kv("stage", "C")
            .kv("result", "rejected")
            .kv("reason", stageCRejectReasonName(stage_c.rejectReason()))
            .kv("rms_deg", static_cast<double>(stage_c.candidateResidualRmsRad() * kRadToDeg))
            .emit();
        ctx.calibration_service->endActive();
        web_api::broadcastCalibrationResult("C", "REJECTED", stageCRejectReasonName(stage_c.rejectReason()));
    }
}

void taskFn(void *param)
{
    auto *ctx = static_cast<Context *>(param);
    esp_task_wdt_add(nullptr);

    // WiFi/AP MUST come up before web_api::start(): AsyncWebServer::begin()
    // binds a listen socket through lwIP's TCP/IP task, which the ESP32
    // Arduino core only spins up once WiFi.mode()/softAP() first runs.
    // Calling web_api::start() first crashes immediately (NULL semaphore in
    // lwIP's tcpip_api_call -> xQueueSemaphoreTake) since that task doesn't
    // exist yet -- this is what was silently preventing the AP from ever
    // appearing.
    record_envelope::Status settings_load_status = record_envelope::Status::kAbsent;
    ctx->settings_service->init(&settings_load_status);
    logIfRecordCorrupted("network_settings", settings_load_status);
    shared_state::publishCurrentSsid(ctx->settings_service->current().ssid);
    web_api::configureAccessPoint(ctx->settings_service->current().ssid);

    web_api::start(*ctx->calibration_service, *ctx->n2k_service, *ctx->clock);

    pinMode(pins::kBootButton, INPUT_PULLUP);  // read-only, per constitution Principle V -- never driven
    bool boot_button_pressed = false;
    uint32_t boot_button_pressed_since_ms = 0;
    bool boot_button_triggered = false;

    calibration::StageA stage_a;
    calibration::SensorCalibrationProfile saved_profile{};
    record_envelope::Status stage_a_load_status = record_envelope::Status::kAbsent;
    bool has_saved_profile =
        calibration::loadSensorCalibrationProfile(*ctx->stage_a_store, saved_profile, &stage_a_load_status);
    logIfRecordCorrupted("stage_a", stage_a_load_status);

    calibration::StageB stage_b;
    calibration::InstallationAlignment saved_alignment{};
    record_envelope::Status stage_b_load_status = record_envelope::Status::kAbsent;
    bool has_saved_alignment =
        calibration::loadInstallationAlignment(*ctx->stage_b_store, saved_alignment, &stage_b_load_status);
    logIfRecordCorrupted("stage_b", stage_b_load_status);

    calibration::StageC stage_c;
    calibration::DeviationCorrection saved_deviation{};
    record_envelope::Status stage_c_load_status = record_envelope::Status::kAbsent;
    bool has_saved_deviation =
        calibration::loadDeviationCorrection(*ctx->stage_c_store, saved_deviation, &stage_c_load_status);
    logIfRecordCorrupted("stage_c", stage_c_load_status);

    publishCorrectionInputs(*ctx, has_saved_profile ? &saved_profile : nullptr,
                             has_saved_alignment ? &saved_alignment : nullptr,
                             has_saved_deviation ? &saved_deviation : nullptr);
    publishStageAStatus(*ctx->calibration_service, stage_a, saved_profile, has_saved_profile);
    publishStageBStatus(*ctx->calibration_service, stage_b, saved_alignment, has_saved_alignment);
    publishStageCStatus(*ctx->calibration_service, stage_c, saved_deviation, has_saved_deviation);

    StageState s{&stage_a,       &saved_profile,      &has_saved_profile,   &stage_b,        &saved_alignment,
                 &has_saved_alignment, &stage_c, &saved_deviation, &has_saved_deviation};

    for (;;)
    {
        esp_task_wdt_reset();

        shared_state::AppCommand cmd;
        if (shared_state::receiveAppCommand(cmd, kCommandWaitMs))
        {
            ctx->calibration_service->noteClientActivity();
            handleCommand(*ctx, s, cmd);
            publishStageAStatus(*ctx->calibration_service, stage_a, saved_profile, has_saved_profile);
            publishStageBStatus(*ctx->calibration_service, stage_b, saved_alignment, has_saved_alignment);
            publishStageCStatus(*ctx->calibration_service, stage_c, saved_deviation, has_saved_deviation);
        }

        driveStageA(*ctx, stage_a, &saved_profile, &has_saved_profile,
                    has_saved_alignment ? &saved_alignment : nullptr, has_saved_deviation ? &saved_deviation : nullptr);
        driveStageB(*ctx, stage_b);
        driveStageC(*ctx, stage_c);
        publishStageAStatus(*ctx->calibration_service, stage_a, saved_profile, has_saved_profile);
        publishStageBStatus(*ctx->calibration_service, stage_b, saved_alignment, has_saved_alignment);
        publishStageCStatus(*ctx->calibration_service, stage_c, saved_deviation, has_saved_deviation);

        CalibrationService::Stage timed_out = ctx->calibration_service->checkInactivityTimeout();
        if (timed_out != CalibrationService::Stage::kNone)
        {
            diag_log::Line("CAL").kv("stage", stageName(timed_out)).kv("result", "timeout").emit();
            if (timed_out == CalibrationService::Stage::kA)
            {
                stage_a.cancel();
                publishStageAStatus(*ctx->calibration_service, stage_a, saved_profile, has_saved_profile);
            }
            else if (timed_out == CalibrationService::Stage::kB)
            {
                stage_b.cancel();
                publishStageBStatus(*ctx->calibration_service, stage_b, saved_alignment, has_saved_alignment);
            }
            else if (timed_out == CalibrationService::Stage::kC)
            {
                stage_c.cancel();
                publishStageCStatus(*ctx->calibration_service, stage_c, saved_deviation, has_saved_deviation);
            }
        }

        {
            float now_s = static_cast<float>(ctx->clock->monotonicMillis()) / 1000.0f;
            if (ctx->settings_service->isRestartDue(now_s))
            {
                web_api::configureAccessPoint(ctx->settings_service->current().ssid);
                shared_state::publishCurrentSsid(ctx->settings_service->current().ssid);
                diag_log::Line("WIFI").token("ap_restart").kv("ssid", ctx->settings_service->current().ssid).emit();
                ctx->settings_service->clearPendingRestart();
            }
        }

        // FR-038 network-recovery path: hold BOOT for NETWORK_RESET_BOOT_HOLD_S
        // to reset the SSID to factory default. BOOT (GPIO0) is read-only here,
        // active-low (pressed = LOW) -- never driven, per pin_config.h.
        {
            bool pressed_now = digitalRead(pins::kBootButton) == LOW;
            uint32_t now_ms = millis();
            if (pressed_now && !boot_button_pressed)
            {
                boot_button_pressed = true;
                boot_button_pressed_since_ms = now_ms;
                boot_button_triggered = false;
            }
            else if (!pressed_now)
            {
                boot_button_pressed = false;
                boot_button_triggered = false;
            }
            else if (boot_button_pressed && !boot_button_triggered &&
                     (now_ms - boot_button_pressed_since_ms) >=
                         static_cast<uint32_t>(thresholds::kNetworkResetBootHoldS * 1000.0f))
            {
                boot_button_triggered = true;
                handleNetworkReset(*ctx, "boot_hold", nullptr);
            }
        }

        web_api::broadcastStatusIfDue(*ctx->calibration_service, *ctx->n2k_service, *ctx->clock);
    }
}

}  // namespace

void start(CalibrationService &calibration_service, N2kService &n2k_service, ImuDriver &imu_driver,
           KeyValueStore &stage_a_store, KeyValueStore &stage_b_store, KeyValueStore &stage_c_store,
           SettingsService &settings_service, Clock &clock)
{
    static Context ctx{&calibration_service, &n2k_service, &imu_driver,        &stage_a_store,
                        &stage_b_store,       &stage_c_store, &settings_service, &clock};
    xTaskCreatePinnedToCore(taskFn, "AppTask", kStackSize, &ctx, kPriority, nullptr, kCoreId);
}

}  // namespace app_task
