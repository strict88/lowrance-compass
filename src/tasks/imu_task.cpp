#include "imu_task.h"

#include <Arduino.h>
#include <esp_task_wdt.h>

#include "heading/pipeline.h"
#include "heading/smoothing.h"
#include "pin_config.h"
#include "services/diag_log.h"
#include "tasks/shared_state.h"
#include "thresholds.h"

namespace imu_task
{

namespace
{

constexpr uint32_t kStackSize = 8192;
constexpr UBaseType_t kPriority = configMAX_PRIORITIES - 2;  // high priority
constexpr BaseType_t kCoreId = 1;
constexpr uint32_t kWaitTimeoutMs = 50;  // fallback poll if an INT edge is missed
constexpr uint32_t kLogThrottleMs = 1000;
constexpr uint32_t kReconnectRetryIntervalMs = 3000;  // how often to retry init() while disconnected

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;

TaskHandle_t g_task_handle = nullptr;
ImuDriver *g_driver = nullptr;

void IRAM_ATTR onImuInterrupt()
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    if (g_task_handle != nullptr)
    {
        vTaskNotifyGiveFromISR(g_task_handle, &higher_priority_task_woken);
    }
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

const char *invalidReasonName(heading::InvalidReason reason)
{
    switch (reason)
    {
        case heading::InvalidReason::kSensorAccuracyLow:
            return "SENSOR_ACCURACY_LOW";
        case heading::InvalidReason::kSensorDisconnected:
            return "SENSOR_DISCONNECTED";
        case heading::InvalidReason::kSensorNotCalibrated:
        case heading::InvalidReason::kNone:
        default:
            return "SENSOR_NOT_CALIBRATED";
    }
}

void taskFn(void * /*param*/)
{
    esp_task_wdt_add(nullptr);

    heading::HeadingSmoother smoother(thresholds::kHeadingSmoothStationaryAlpha, thresholds::kHeadingSmoothTurningAlpha,
                                       thresholds::kHeadingSmoothTurnThresholdDegS * kDegToRad);

    ImuReport last_report;  // persists across cycles with no fresh report
    bool was_connected = true;
    bool was_valid = false;
    uint32_t last_log_ms = 0;
    uint32_t last_reconnect_attempt_ms = 0;

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(kWaitTimeoutMs));
        esp_task_wdt_reset();

        while (g_driver->readReport(last_report))
        {
            // Drain to the most recent report this cycle.
        }

        bool connected = g_driver->isConnected();
        if (connected != was_connected)
        {
            diag_log::Line("IMU").token(connected ? "reconnected" : "disconnected").emit();
            was_connected = connected;
        }

        // Self-heal: retry init() periodically while disconnected, so a
        // sensor that starts absent (e.g. not yet wired up) or that drops
        // off the bus comes back automatically once it's actually present,
        // with no reboot required. init() is the interface method (rather
        // than ImuDriverBno08x::hardResetAndReinit(), which isn't part of
        // ImuDriver) so this stays driver-agnostic/fake-able.
        if (!connected)
        {
            uint32_t now_ms = millis();
            if (now_ms - last_reconnect_attempt_ms >= kReconnectRetryIntervalMs)
            {
                last_reconnect_attempt_ms = now_ms;
                g_driver->init();
            }
        }

        shared_state::HeadingCorrectionInputs corrections = shared_state::getHeadingCorrectionInputs();

        heading::PipelineInput input;
        input.raw_quat = heading::Quaternion{last_report.quat_w, last_report.quat_x, last_report.quat_y, last_report.quat_z};
        input.raw_gyro_z_rad_s = last_report.gyro_z_rad_s;
        input.sensor_connected = connected;
        input.live_accuracy = {last_report.mag_accuracy, last_report.accel_accuracy, last_report.gyro_accuracy};
        input.active_stage = corrections.active_stage;
        input.saved_profile = corrections.saved_profile;
        input.level_reference = corrections.level_reference;
        input.mounting_offset = corrections.mounting_offset;
        input.deviation_correction = corrections.deviation_correction;

        heading::HeadingReading result = heading::computeHeadingReading(input);
        result.heading_rad = smoother.update(result.heading_rad, result.rate_of_turn_rad_s);

        shared_state::publishHeadingReading(result);

        shared_state::RawImuSample raw_sample;
        raw_sample.raw_quat = input.raw_quat;
        raw_sample.gyro_x_rad_s = last_report.gyro_x_rad_s;
        raw_sample.gyro_y_rad_s = last_report.gyro_y_rad_s;
        raw_sample.gyro_z_rad_s = last_report.gyro_z_rad_s;
        raw_sample.mag_accuracy = last_report.mag_accuracy;
        raw_sample.accel_accuracy = last_report.accel_accuracy;
        raw_sample.gyro_accuracy = last_report.gyro_accuracy;
        raw_sample.connected = connected;
        raw_sample.monotonic_ms = millis();
        shared_state::publishRawImuSample(raw_sample);

        if (result.valid != was_valid)
        {
            if (result.valid)
            {
                diag_log::Line("IMU").kv("valid", 1L).emit();
            }
            else
            {
                diag_log::Line("IMU").kv("valid", 0L).kv("reason", invalidReasonName(result.reason_if_invalid)).emit();
            }
            was_valid = result.valid;
        }

        uint32_t now_ms = millis();
        if (now_ms - last_log_ms >= kLogThrottleMs)
        {
            last_log_ms = now_ms;
            diag_log::Line("IMU")
                .kv("hdg", static_cast<double>(result.heading_rad * kRadToDeg))
                .kv("mag", static_cast<int>(input.live_accuracy.mag_accuracy))
                .kv("accel", static_cast<int>(input.live_accuracy.accel_accuracy))
                .kv("gyro", static_cast<int>(input.live_accuracy.gyro_accuracy))
                .kv("valid", result.valid ? 1L : 0L)
                .emit();
        }
    }
}

}  // namespace

void start(ImuDriver &driver)
{
    g_driver = &driver;
    driver.init();

    attachInterrupt(digitalPinToInterrupt(pins::kImuInt), onImuInterrupt, RISING);

    xTaskCreatePinnedToCore(taskFn, "ImuTask", kStackSize, nullptr, kPriority, &g_task_handle, kCoreId);
}

}  // namespace imu_task
