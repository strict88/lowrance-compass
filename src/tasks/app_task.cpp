#include "app_task.h"

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <cstdio>
#include <cstring>

#include "calibration/stage_a.h"
#include "drivers/kv_store/record_envelope.h"
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
    Clock *clock;
};

// Publishes shared_state's HeadingCorrectionInputs from the current
// CalibrationService/StageA state, so ImuTask's pipeline sees the right
// active_stage and saved-profile accuracy (for the Stage A in-progress
// freeze, FR-041).
void publishCorrectionInputs(const Context &ctx, const calibration::SensorCalibrationProfile *saved_profile)
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

    if (saved_profile != nullptr)
    {
        inputs.saved_profile.exists = true;
        inputs.saved_profile.mag_accuracy = saved_profile->mag_accuracy;
        inputs.saved_profile.accel_accuracy = saved_profile->accel_accuracy;
        inputs.saved_profile.gyro_accuracy = saved_profile->gyro_accuracy;
    }

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
                        shared_state::AppCommand &cmd)
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
    publishCorrectionInputs(ctx, nullptr);

    diag_log::Line("CAL").kv("stage", "A").kv("result", "reset").emit();
    respond(cmd, true, "{\"schema\":1,\"reset\":true}");
}

void handleOtherStart(shared_state::AppCommand &cmd)
{
    // Stage B/C aren't implemented yet (their own later phases) -- report
    // busy against NONE so the UI gets a clear "not ready" signal rather
    // than a silent failure.
    respond(cmd, false,
            "{\"schema\":1,\"error\":{\"code\":\"NOT_IMPLEMENTED\",\"message\":\"not yet implemented\"}}");
}

void handleCommand(Context &ctx, calibration::StageA &stage_a, calibration::SensorCalibrationProfile *saved_profile,
                    bool *has_saved_profile, shared_state::AppCommand &cmd)
{
    switch (cmd.type)
    {
        case shared_state::AppCommandType::kCalStartA:
            handleStageAStart(ctx, stage_a, cmd);
            break;
        case shared_state::AppCommandType::kCalCancelA:
            handleStageACancel(ctx, stage_a, cmd);
            break;
        case shared_state::AppCommandType::kCalResetA:
            handleStageAReset(ctx, saved_profile, has_saved_profile, cmd);
            break;
        case shared_state::AppCommandType::kCalStartB:
        case shared_state::AppCommandType::kCalStartCGps:
        case shared_state::AppCommandType::kCalStartCManual:
            handleOtherStart(cmd);
            break;
        case shared_state::AppCommandType::kCalCancelB:
            ctx.calibration_service->cancel(CalibrationService::Stage::kB);
            respond(cmd, true, "{\"schema\":1,\"cancelled\":true}");
            break;
        case shared_state::AppCommandType::kCalCancelC:
            ctx.calibration_service->cancel(CalibrationService::Stage::kC);
            respond(cmd, true, "{\"schema\":1,\"cancelled\":true}");
            break;
        default:
            // Stage B/C apply/discard/level/bearing/manual-point and
            // Settings/network-reset are wired up by their own later tasks
            // (User Stories 3/4/5) -- not reachable yet since no HTTP route
            // posts these command types until then.
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
                  bool *has_saved_profile)
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

    diag_log::Line("CAL")
        .kv("stage", "A")
        .kv("positions", static_cast<int>([&]() {
                int n = 0;
                for (bool d : stage_a.progress().positions_done)
                {
                    if (d) ++n;
                }
                return n;
            }()))
        .emit();

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
        publishCorrectionInputs(ctx, *has_saved_profile ? saved_profile : nullptr);
        web_api::broadcastCalibrationResult("A", saved ? "SAVED" : "REJECTED", saved ? nullptr : "save failed");
    }
    else if (state_after == calibration::StageAState::kTimedOut)
    {
        diag_log::Line("CAL").kv("stage", "A").kv("result", "timeout").emit();
        ctx.calibration_service->endActive();
        publishCorrectionInputs(ctx, *has_saved_profile ? saved_profile : nullptr);
        web_api::broadcastCalibrationResult("A", "TIMED_OUT", "calibration timed out before reaching High accuracy");
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
    publishCorrectionInputs(*ctx, has_saved_profile ? &saved_profile : nullptr);
    publishStageAStatus(*ctx->calibration_service, stage_a, saved_profile, has_saved_profile);

    for (;;)
    {
        esp_task_wdt_reset();

        shared_state::AppCommand cmd;
        if (shared_state::receiveAppCommand(cmd, kCommandWaitMs))
        {
            ctx->calibration_service->noteClientActivity();
            handleCommand(*ctx, stage_a, &saved_profile, &has_saved_profile, cmd);
            publishStageAStatus(*ctx->calibration_service, stage_a, saved_profile, has_saved_profile);
        }

        driveStageA(*ctx, stage_a, &saved_profile, &has_saved_profile);
        publishStageAStatus(*ctx->calibration_service, stage_a, saved_profile, has_saved_profile);

        CalibrationService::Stage timed_out = ctx->calibration_service->checkInactivityTimeout();
        if (timed_out != CalibrationService::Stage::kNone)
        {
            diag_log::Line("CAL").kv("stage", stageName(timed_out)).kv("result", "timeout").emit();
            if (timed_out == CalibrationService::Stage::kA)
            {
                stage_a.cancel();
                publishStageAStatus(*ctx->calibration_service, stage_a, saved_profile, has_saved_profile);
            }
        }

        web_api::broadcastStatusIfDue(*ctx->calibration_service, *ctx->n2k_service, *ctx->clock);
    }
}

}  // namespace

void start(CalibrationService &calibration_service, N2kService &n2k_service, ImuDriver &imu_driver,
           KeyValueStore &stage_a_store, Clock &clock)
{
    static Context ctx{&calibration_service, &n2k_service, &imu_driver, &stage_a_store, &clock};
    xTaskCreatePinnedToCore(taskFn, "AppTask", kStackSize, &ctx, kPriority, nullptr, kCoreId);
}

}  // namespace app_task
