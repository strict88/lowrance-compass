#include <Arduino.h>
#include <esp_task_wdt.h>

#include "drivers/can_bus/twai_node_bus.h"
#include "drivers/clock/clock.h"
#include "drivers/imu_driver/imu_driver_bno08x.h"
#include "drivers/kv_store/kv_store_nvs.h"
#include "services/calibration_service.h"
#include "services/diag_log.h"
#include "services/n2k_service.h"
#include "services/settings_service.h"
#include "tasks/app_task.h"
#include "tasks/imu_task.h"
#include "tasks/n2k_task.h"
#include "tasks/shared_state.h"

namespace
{

constexpr const char *kFirmwareVersion = "0.1.0";
constexpr uint32_t kTaskWdtTimeoutMs = 5000;
constexpr uint32_t kHeartbeatIntervalMs = 5000;

#ifdef BENCH_MODE
constexpr bool kEnableSelfTest = true;
#else
constexpr bool kEnableSelfTest = false;
#endif

SystemClock g_clock;
ImuDriverBno08x g_imu_driver;
TwaiNodeBus g_can_bus;
N2kService g_n2k_service(g_can_bus, g_clock, kEnableSelfTest);
CalibrationService g_calibration_service(g_clock);
NvsKeyValueStore g_stage_a_store("cal_a");
NvsKeyValueStore g_stage_b_store("cal_b");
NvsKeyValueStore g_settings_store("net");
SettingsService g_settings_service(g_settings_store);

const char *resetReasonName()
{
    switch (esp_reset_reason())
    {
        case ESP_RST_POWERON:
            return "POWERON";
        case ESP_RST_SW:
            return "SW";
        case ESP_RST_PANIC:
            return "PANIC";
        case ESP_RST_INT_WDT:
            return "INT_WDT";
        case ESP_RST_TASK_WDT:
            return "TASK_WDT";
        case ESP_RST_WDT:
            return "WDT";
        case ESP_RST_BROWNOUT:
            return "BROWNOUT";
        case ESP_RST_USB:
            return "USB";
        default:
            return "OTHER";
    }
}

}  // namespace

void setup()
{
    Serial.begin(115200);
    diag_log::init();

    shared_state::init();

    esp_task_wdt_config_t wdt_config{};
    wdt_config.timeout_ms = kTaskWdtTimeoutMs;
    wdt_config.idle_core_mask = 0;
    wdt_config.trigger_panic = true;
    esp_task_wdt_init(&wdt_config);

    g_n2k_service.init(kFirmwareVersion);

    imu_task::start(g_imu_driver);
    n2k_task::start(g_n2k_service);
    app_task::start(g_calibration_service, g_n2k_service, g_imu_driver, g_stage_a_store, g_stage_b_store,
                     g_settings_service, g_clock);

    diag_log::logBoot(kFirmwareVersion, resetReasonName(), ESP.getFreeHeap());
}

void loop()
{
    static uint32_t last_heartbeat_ms = 0;
    uint32_t now_ms = millis();
    if (now_ms - last_heartbeat_ms >= kHeartbeatIntervalMs)
    {
        last_heartbeat_ms = now_ms;
        diag_log::logHeartbeat(ESP.getFreeHeap(), now_ms / 1000);
    }
    delay(100);
}
