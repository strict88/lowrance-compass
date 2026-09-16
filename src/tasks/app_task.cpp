#include "app_task.h"

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <cstdio>

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

CalibrationService::Stage stageForStartCommand(shared_state::AppCommandType type)
{
    switch (type)
    {
        case shared_state::AppCommandType::kCalStartA:
            return CalibrationService::Stage::kA;
        case shared_state::AppCommandType::kCalStartB:
            return CalibrationService::Stage::kB;
        case shared_state::AppCommandType::kCalStartCGps:
        case shared_state::AppCommandType::kCalStartCManual:
            return CalibrationService::Stage::kC;
        default:
            return CalibrationService::Stage::kNone;
    }
}

CalibrationService::Stage stageForCancelCommand(shared_state::AppCommandType type)
{
    switch (type)
    {
        case shared_state::AppCommandType::kCalCancelA:
            return CalibrationService::Stage::kA;
        case shared_state::AppCommandType::kCalCancelB:
            return CalibrationService::Stage::kB;
        case shared_state::AppCommandType::kCalCancelC:
            return CalibrationService::Stage::kC;
        default:
            return CalibrationService::Stage::kNone;
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

void handleStart(CalibrationService &calibration_service, shared_state::AppCommand &cmd)
{
    CalibrationService::Stage stage = stageForStartCommand(cmd.type);
    CalibrationService::Stage busy_stage = CalibrationService::Stage::kNone;

    if (calibration_service.tryStart(stage, busy_stage))
    {
        diag_log::Line("CAL").kv("stage", stageName(stage)).kv("state", "Started").emit();
        respond(cmd, true, "{\"schema\":1,\"started\":true}");
        return;
    }

    diag_log::Line("CAL").token("busy").kv("stage", stageName(busy_stage)).kv("rejected", stageName(stage)).emit();

    char body[160];
    snprintf(body, sizeof(body),
             "{\"schema\":1,\"error\":{\"code\":\"CALIBRATION_BUSY\",\"message\":\"another calibration procedure is "
             "already running\"},\"active_stage\":\"%s\"}",
             stageName(busy_stage));
    respond(cmd, false, body);
}

void handleCancel(CalibrationService &calibration_service, shared_state::AppCommand &cmd)
{
    CalibrationService::Stage stage = stageForCancelCommand(cmd.type);
    calibration_service.cancel(stage);
    diag_log::Line("CAL").kv("stage", stageName(stage)).kv("result", "cancelled").emit();
    respond(cmd, true, "{\"schema\":1,\"cancelled\":true}");
}

void handleCommand(CalibrationService &calibration_service, shared_state::AppCommand &cmd)
{
    switch (cmd.type)
    {
        case shared_state::AppCommandType::kCalStartA:
        case shared_state::AppCommandType::kCalStartB:
        case shared_state::AppCommandType::kCalStartCGps:
        case shared_state::AppCommandType::kCalStartCManual:
            handleStart(calibration_service, cmd);
            break;
        case shared_state::AppCommandType::kCalCancelA:
        case shared_state::AppCommandType::kCalCancelB:
        case shared_state::AppCommandType::kCalCancelC:
            handleCancel(calibration_service, cmd);
            break;
        default:
            // Stage B/C apply/discard/reset/level/bearing/manual-point and
            // Settings/network-reset are wired up by their own later tasks
            // (User Stories 3/4/5) -- not reachable yet since no HTTP route
            // posts these command types until then.
            respond(cmd, false,
                    "{\"schema\":1,\"error\":{\"code\":\"NOT_IMPLEMENTED\",\"message\":\"not yet implemented\"}}");
            break;
    }
}

struct Context
{
    CalibrationService *calibration_service;
    N2kService *n2k_service;
    Clock *clock;
};

void taskFn(void *param)
{
    auto *ctx = static_cast<Context *>(param);
    esp_task_wdt_add(nullptr);

    web_api::start(*ctx->calibration_service, *ctx->n2k_service, *ctx->clock);

    for (;;)
    {
        esp_task_wdt_reset();

        shared_state::AppCommand cmd;
        if (shared_state::receiveAppCommand(cmd, kCommandWaitMs))
        {
            ctx->calibration_service->noteClientActivity();
            handleCommand(*ctx->calibration_service, cmd);
        }

        CalibrationService::Stage timed_out = ctx->calibration_service->checkInactivityTimeout();
        if (timed_out != CalibrationService::Stage::kNone)
        {
            diag_log::Line("CAL").kv("stage", stageName(timed_out)).kv("result", "timeout").emit();
        }

        web_api::broadcastStatusIfDue(*ctx->calibration_service, *ctx->n2k_service, *ctx->clock);
    }
}

}  // namespace

void start(CalibrationService &calibration_service, N2kService &n2k_service, Clock &clock)
{
    static Context ctx{&calibration_service, &n2k_service, &clock};
    xTaskCreatePinnedToCore(taskFn, "AppTask", kStackSize, &ctx, kPriority, nullptr, kCoreId);
}

}  // namespace app_task
