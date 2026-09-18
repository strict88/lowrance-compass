#include "shared_state.h"

#include <cstring>

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

SemaphoreHandle_t g_debug_gps_mutex = nullptr;
DebugGpsInject g_debug_gps_inject;

SemaphoreHandle_t g_stage_b_status_mutex = nullptr;
StageBStatusSnapshot g_stage_b_status;

SemaphoreHandle_t g_stage_c_status_mutex = nullptr;
StageCStatusSnapshot g_stage_c_status;

SemaphoreHandle_t g_ssid_mutex = nullptr;
char g_current_ssid[33] = "LowranceCompass";

QueueHandle_t g_app_command_queue = nullptr;
}  // namespace

void init()
{
    g_heading_mutex = xSemaphoreCreateMutex();
    g_correction_inputs_mutex = xSemaphoreCreateMutex();
    g_raw_sample_mutex = xSemaphoreCreateMutex();
    g_stage_a_status_mutex = xSemaphoreCreateMutex();
    g_debug_gps_mutex = xSemaphoreCreateMutex();
    g_stage_b_status_mutex = xSemaphoreCreateMutex();
    g_stage_c_status_mutex = xSemaphoreCreateMutex();
    g_ssid_mutex = xSemaphoreCreateMutex();
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

void setDebugGpsInject(const DebugGpsInject &inject)
{
    xSemaphoreTake(g_debug_gps_mutex, portMAX_DELAY);
    g_debug_gps_inject = inject;
    xSemaphoreGive(g_debug_gps_mutex);
}

DebugGpsInject getDebugGpsInject()
{
    xSemaphoreTake(g_debug_gps_mutex, portMAX_DELAY);
    DebugGpsInject copy = g_debug_gps_inject;
    xSemaphoreGive(g_debug_gps_mutex);
    return copy;
}

void publishStageBStatus(const StageBStatusSnapshot &status)
{
    xSemaphoreTake(g_stage_b_status_mutex, portMAX_DELAY);
    g_stage_b_status = status;
    xSemaphoreGive(g_stage_b_status_mutex);
}

StageBStatusSnapshot getStageBStatus()
{
    xSemaphoreTake(g_stage_b_status_mutex, portMAX_DELAY);
    StageBStatusSnapshot copy = g_stage_b_status;
    xSemaphoreGive(g_stage_b_status_mutex);
    return copy;
}

void publishStageCStatus(const StageCStatusSnapshot &status)
{
    xSemaphoreTake(g_stage_c_status_mutex, portMAX_DELAY);
    g_stage_c_status = status;
    xSemaphoreGive(g_stage_c_status_mutex);
}

StageCStatusSnapshot getStageCStatus()
{
    xSemaphoreTake(g_stage_c_status_mutex, portMAX_DELAY);
    StageCStatusSnapshot copy = g_stage_c_status;
    xSemaphoreGive(g_stage_c_status_mutex);
    return copy;
}

void publishCurrentSsid(const char *ssid)
{
    xSemaphoreTake(g_ssid_mutex, portMAX_DELAY);
    strncpy(g_current_ssid, ssid, sizeof(g_current_ssid) - 1);
    g_current_ssid[sizeof(g_current_ssid) - 1] = '\0';
    xSemaphoreGive(g_ssid_mutex);
}

void getCurrentSsid(char *out, size_t out_len)
{
    xSemaphoreTake(g_ssid_mutex, portMAX_DELAY);
    strncpy(out, g_current_ssid, out_len - 1);
    out[out_len - 1] = '\0';
    xSemaphoreGive(g_ssid_mutex);
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
