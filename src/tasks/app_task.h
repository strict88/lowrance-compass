#pragma once

#include "drivers/clock/clock.h"
#include "drivers/imu_driver/imu_driver.h"
#include "drivers/kv_store/kv_store.h"
#include "services/calibration_service.h"
#include "services/n2k_service.h"
#include "services/settings_service.h"

// AppTask: Core 0. Owns CalibrationService/SettingsService state,
// persistence, and WebSocket broadcast; reads UI commands from
// shared_state's command queue (posted by web_api's HTTP handlers) and
// replies synchronously via each command's embedded semaphore/result
// buffer. Also drives an active Stage A calibration attempt using the raw
// IMU samples ImuTask publishes (shared_state::getRawImuSample()) -- Stage A
// itself is pure logic (src/calibration/stage_a.h) and doesn't touch
// hardware directly. Arguments must outlive the task, which never returns.
namespace app_task
{
void start(CalibrationService &calibration_service, N2kService &n2k_service, ImuDriver &imu_driver,
           KeyValueStore &stage_a_store, KeyValueStore &stage_b_store, KeyValueStore &stage_c_store,
           SettingsService &settings_service, Clock &clock);
}
