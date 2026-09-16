#pragma once

#include "drivers/clock/clock.h"
#include "services/calibration_service.h"

// AppTask: Core 0. Owns CalibrationService/SettingsService state,
// persistence, and WebSocket broadcast; reads UI commands from
// shared_state's command queue (posted by web_api's HTTP handlers) and
// replies synchronously via each command's embedded semaphore/result
// buffer. `calibration_service`/`clock` must outlive the task, which never
// returns.
namespace app_task
{
void start(CalibrationService &calibration_service, Clock &clock);
}
