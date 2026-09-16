#include "n2k_task.h"

#include <Arduino.h>
#include <esp_task_wdt.h>

#include "tasks/shared_state.h"

namespace n2k_task
{

namespace
{
constexpr uint32_t kStackSize = 8192;
constexpr UBaseType_t kPriority = configMAX_PRIORITIES - 2;  // high priority
constexpr BaseType_t kCoreId = 1;
constexpr uint32_t kLoopIntervalMs = 20;  // well above the 100 ms 127250/127251 tx interval

void taskFn(void *param)
{
    auto *service = static_cast<N2kService *>(param);
    esp_task_wdt_add(nullptr);

    TickType_t last_wake = xTaskGetTickCount();
    for (;;)
    {
        esp_task_wdt_reset();
        heading::HeadingReading reading = shared_state::getHeadingReading();
        service->loop(reading);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(kLoopIntervalMs));
    }
}

}  // namespace

void start(N2kService &service)
{
    xTaskCreatePinnedToCore(taskFn, "N2kTask", kStackSize, &service, kPriority, nullptr, kCoreId);
}

}  // namespace n2k_task
