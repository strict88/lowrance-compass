#pragma once

#include <cstdint>

// One IMU report: the rotation-vector quaternion plus per-sensor accuracy
// (SH-2 accuracy enum 0-3: 0=Unreliable, 1=Low, 2=Medium, 3=High) and raw
// gyro, independent of any specific chip so it is fake-able in native tests.
struct ImuReport
{
    float quat_w = 1.0f;
    float quat_x = 0.0f;
    float quat_y = 0.0f;
    float quat_z = 0.0f;

    uint8_t mag_accuracy = 0;
    uint8_t accel_accuracy = 0;
    uint8_t gyro_accuracy = 0;

    // rad/s, body frame -- used for rate-of-turn and stillness detection.
    float gyro_x_rad_s = 0.0f;
    float gyro_y_rad_s = 0.0f;
    float gyro_z_rad_s = 0.0f;
};

// Hardware-adapter interface over the IMU. Independent of Adafruit_BNO08x so
// it is fake-able in native tests (constitution Principle V).
class ImuDriver
{
public:
    virtual ~ImuDriver() = default;

    virtual bool init() = 0;

    // Polls for a new sensor report. Returns true and fills `out` if a fresh
    // report was available; returns false (leaving `out` untouched)
    // otherwise. Never blocks.
    virtual bool readReport(ImuReport &out) = 0;

    // Configures which of the three dynamic-calibration sub-algorithms
    // (magnetometer, accelerometer, gyroscope) are active on-chip.
    virtual void setCalibrationConfig(bool enable_mag, bool enable_accel, bool enable_gyro) = 0;

    // Persists the sensor's current Dynamic Calibration Data to its own
    // on-chip flash (SH-2 "save DCD"). Only ever called on a successful
    // Stage A completion (never on cancel/timeout) -- see stage_a.cpp.
    virtual bool saveDcd() = 0;

    // Zeroes the current orientation as the reference ("tare").
    virtual bool tare() = 0;

    virtual bool isConnected() const = 0;
};
