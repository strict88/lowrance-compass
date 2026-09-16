#include "shared_state.h"

namespace shared_state
{

namespace
{
constexpr UBaseType_t kAppCommandQueueDepth = 4;

SemaphoreHandle_t g_heading_mutex = nullptr;
heading::HeadingReading g_heading_reading;

SemaphoreHandle_t g_correction_inputs_mutex = nullptr;
HeadingCorrectionInputs g_correction_inputs;

SemaphoreHandle_t g_raw_sample_mutex = nullptr;
RawImuSample g_raw_sample;

SemaphoreHandle_t g_stage_a_status_mutex = nullptr;
StageAStatusSnapshot g_stage_a_status;

QueueHandle_t g_app_command_queue = nullptr;
}  // namespace

void init()
{
    g_heading_mutex = xSemaphoreCreateMutex();
    g_correction_inputs_mutex = xSemaphoreCreateMutex();
    g_raw_sample_mutex = xSemaphoreCreateMutex();
    g_stage_a_status_mutex = xSemaphoreCreateMutex();
    g_app_command_queue = xQueueCreate(kAppCommandQueueDepth, sizeof(AppCommand));
}

void publishHeadingReading(const heading::HeadingReading &reading)
{
    xSemaphoreTake(g_heading_mutex, portMAX_DELAY);
    g_heading_reading = reading;
    xSemaphoreGive(g_heading_mutex);
}

heading::HeadingReading getHeadingReading()
{
    xSemaphoreTake(g_heading_mutex, portMAX_DELAY);
    heading::HeadingReading copy = g_heading_reading;
    xSemaphoreGive(g_heading_mutex);
    return copy;
}

void publishHeadingCorrectionInputs(const HeadingCorrectionInputs &inputs)
{
    xSemaphoreTake(g_correction_inputs_mutex, portMAX_DELAY);
    g_correction_inputs = inputs;
    xSemaphoreGive(g_correction_inputs_mutex);
}

HeadingCorrectionInputs getHeadingCorrectionInputs()
{
    xSemaphoreTake(g_correction_inputs_mutex, portMAX_DELAY);
    HeadingCorrectionInputs copy = g_correction_inputs;
    xSemaphoreGive(g_correction_inputs_mutex);
    return copy;
}

void publishRawImuSample(const RawImuSample &sample)
{
    xSemaphoreTake(g_raw_sample_mutex, portMAX_DELAY);
    g_raw_sample = sample;
    xSemaphoreGive(g_raw_sample_mutex);
}

RawImuSample getRawImuSample()
{
    xSemaphoreTake(g_raw_sample_mutex, portMAX_DELAY);
    RawImuSample copy = g_raw_sample;
    xSemaphoreGive(g_raw_sample_mutex);
    return copy;
}

void publishStageAStatus(const StageAStatusSnapshot &status)
{
    xSemaphoreTake(g_stage_a_status_mutex, portMAX_DELAY);
    g_stage_a_status = status;
    xSemaphoreGive(g_stage_a_status_mutex);
}

StageAStatusSnapshot getStageAStatus()
{
    xSemaphoreTake(g_stage_a_status_mutex, portMAX_DELAY);
    StageAStatusSnapshot copy = g_stage_a_status;
    xSemaphoreGive(g_stage_a_status_mutex);
    return copy;
}

bool postAppCommand(const AppCommand &cmd, uint32_t timeout_ms)
{
    return xQueueSend(g_app_command_queue, &cmd, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

bool receiveAppCommand(AppCommand &out, uint32_t timeout_ms)
{
    return xQueueReceive(g_app_command_queue, &out, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

}  // namespace shared_state
