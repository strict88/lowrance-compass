#include "app_task.h"

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <cstdio>
#include <cstring>

#include "calibration/stage_a.h"
#include "calibration/stage_b.h"
#include "drivers/kv_store/record_envelope.h"
#include "n2k_codec/variation.h"
#include "services/diag_log.h"
#include "services/web_api.h"
#include "tasks/shared_state.h"

namespace app_task
{

namespace
{

constexpr uint32_t kStackSize = 8192;
constexpr UBaseType_t kPriority = configMAX_PRIORITIES - 4;  // lower than ImuTask/N2kTask
constexpr BaseType_t kCoreId = 0;
constexpr uint32_t kCommandWaitMs = 200;
constexpr const char *kFirmwareVersion = "0.1.0";
constexpr float kRadToDeg = 57.29577951308232f;
constexpr float kDegToRad = 1.0f / kRadToDeg;

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
    Clock *clock;
};

// Publishes shared_state's HeadingCorrectionInputs from the current
// CalibrationService/StageA/StageB state, so ImuTask's pipeline sees the
// right active_stage, saved Stage A profile accuracy (for the Stage A
// in-progress freeze, FR-041), and the persisted Stage B level/offset
// correction.
void publishCorrectionInputs(const Context &ctx, const calibration::SensorCalibrationProfile *saved_profile,
                              const calibration::InstallationAlignment *saved_alignment)
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

void handleStageAStart(Context &ctx, calibration::StageA &stage_a, shared_state::AppCommand &cmd)
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

    float now_s = static_cast<float>(ctx.clock->monotonicMillis()) / 1000.0f;
    stage_a.start(now_s);
    diag_log::Line("CAL").kv("stage", "A").kv("state", "AwaitingStillness").emit();
    respond(cmd, true, "{\"schema\":1,\"started\":true}");
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
                        const calibration::InstallationAlignment *saved_alignment, shared_state::AppCommand &cmd)
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
    publishCorrectionInputs(ctx, nullptr, saved_alignment);

    diag_log::Line("CAL").kv("stage", "A").kv("result", "reset").emit();
    respond(cmd, true, "{\"schema\":1,\"reset\":true}");
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
                        bool *has_saved_alignment, shared_state::AppCommand &cmd)
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
                        const calibration::SensorCalibrationProfile *saved_profile, shared_state::AppCommand &cmd)
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
    publishCorrectionInputs(ctx, saved_profile, nullptr);

    diag_log::Line("CAL").kv("stage", "B").kv("result", "reset").emit();
    respond(cmd, true, "{\"schema\":1,\"reset\":true}");
}

void handleOtherStart(shared_state::AppCommand &cmd)
{
    // Stage C isn't implemented yet (its own later phase).
    respond(cmd, false,
            "{\"schema\":1,\"error\":{\"code\":\"NOT_IMPLEMENTED\",\"message\":\"not yet implemented\"}}");
}

struct StageState
{
    calibration::StageA *stage_a;
    calibration::SensorCalibrationProfile *saved_profile;
    bool *has_saved_profile;
    calibration::StageB *stage_b;
    calibration::InstallationAlignment *saved_alignment;
    bool *has_saved_alignment;
};

void handleCommand(Context &ctx, StageState &s, shared_state::AppCommand &cmd)
{
    switch (cmd.type)
    {
        case shared_state::AppCommandType::kCalStartA:
            handleStageAStart(ctx, *s.stage_a, cmd);
            break;
        case shared_state::AppCommandType::kCalCancelA:
            handleStageACancel(ctx, *s.stage_a, cmd);
            break;
        case shared_state::AppCommandType::kCalResetA:
            handleStageAReset(ctx, s.saved_profile, s.has_saved_profile, s.saved_alignment, cmd);
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
            handleStageBApply(ctx, *s.stage_b, s.saved_alignment, s.has_saved_alignment, cmd);
            break;
        case shared_state::AppCommandType::kCalDiscardB:
            handleStageBDiscard(ctx, *s.stage_b, cmd);
            break;
        case shared_state::AppCommandType::kCalResetB:
            handleStageBReset(ctx, s.saved_alignment, s.has_saved_alignment, s.saved_profile, cmd);
            break;
        case shared_state::AppCommandType::kCalStartCGps:
        case shared_state::AppCommandType::kCalStartCManual:
            handleOtherStart(cmd);
            break;
        case shared_state::AppCommandType::kCalCancelC:
            ctx.calibration_service->cancel(CalibrationService::Stage::kC);
            respond(cmd, true, "{\"schema\":1,\"cancelled\":true}");
            break;
        default:
            // Stage C and Settings/network-reset are wired up by their own
            // later tasks (User Stories 4/5) -- not reachable yet since no
            // HTTP route posts these command types until then.
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
                  bool *has_saved_profile, const calibration::InstallationAlignment *saved_alignment)
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
        publishCorrectionInputs(ctx, *has_saved_profile ? saved_profile : nullptr, saved_alignment);
        web_api::broadcastCalibrationResult("A", saved ? "SAVED" : "REJECTED", saved ? nullptr : "save failed");
    }
    else if (state_after == calibration::StageAState::kTimedOut)
    {
        diag_log::Line("CAL").kv("stage", "A").kv("result", "timeout").emit();
        ctx.calibration_service->endActive();
        publishCorrectionInputs(ctx, *has_saved_profile ? saved_profile : nullptr, saved_alignment);
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

void taskFn(void *param)
{
    auto *ctx = static_cast<Context *>(param);
    esp_task_wdt_add(nullptr);

    web_api::start(*ctx->calibration_service, *ctx->n2k_service, *ctx->clock);

    calibration::StageA stage_a;
    calibration::SensorCalibrationProfile saved_profile{};
    bool has_saved_profile = calibration::loadSensorCalibrationProfile(*ctx->stage_a_store, saved_profile);

    calibration::StageB stage_b;
    calibration::InstallationAlignment saved_alignment{};
    bool has_saved_alignment = calibration::loadInstallationAlignment(*ctx->stage_b_store, saved_alignment);

    publishCorrectionInputs(*ctx, has_saved_profile ? &saved_profile : nullptr,
                             has_saved_alignment ? &saved_alignment : nullptr);
    publishStageAStatus(*ctx->calibration_service, stage_a, saved_profile, has_saved_profile);
    publishStageBStatus(*ctx->calibration_service, stage_b, saved_alignment, has_saved_alignment);

    StageState s{&stage_a, &saved_profile, &has_saved_profile, &stage_b, &saved_alignment, &has_saved_alignment};

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
        }

        driveStageA(*ctx, stage_a, &saved_profile, &has_saved_profile,
                    has_saved_alignment ? &saved_alignment : nullptr);
        driveStageB(*ctx, stage_b);
        publishStageAStatus(*ctx->calibration_service, stage_a, saved_profile, has_saved_profile);
        publishStageBStatus(*ctx->calibration_service, stage_b, saved_alignment, has_saved_alignment);

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
        }

        web_api::broadcastStatusIfDue(*ctx->calibration_service, *ctx->n2k_service, *ctx->clock);
    }
}

}  // namespace

void start(CalibrationService &calibration_service, N2kService &n2k_service, ImuDriver &imu_driver,
           KeyValueStore &stage_a_store, KeyValueStore &stage_b_store, Clock &clock)
{
    static Context ctx{&calibration_service, &n2k_service, &imu_driver, &stage_a_store, &stage_b_store, &clock};
    xTaskCreatePinnedToCore(taskFn, "AppTask", kStackSize, &ctx, kPriority, nullptr, kCoreId);
}

}  // namespace app_task
