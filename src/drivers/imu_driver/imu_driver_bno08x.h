#pragma once

#include <Adafruit_BNO08x.h>

#include "imu_driver.h"

// ImuDriver over Adafruit_BNO08x 1.2.7 (research.md §2), via I2C on
// pin_config.h's SDA/SCL, with interrupt-driven reads on INT (the caller --
// ImuTask -- attaches the interrupt and calls readReport() when it fires;
// this class itself only performs the I2C transaction, per constitution
// Principle III's ban on blocking calls inside an ISR) and a hard reset on
// RST after a comms fault.
class ImuDriverBno08x : public ImuDriver
{
public:
    bool init() override;
    bool readReport(ImuReport &out) override;
    void setCalibrationConfig(bool enable_mag, bool enable_accel, bool enable_gyro) override;
    bool saveDcd() override;
    bool tare() override;
    bool isConnected() const override { return connected_; }

    // Hard resets the sensor via RST (pin_config.h) and re-initializes it.
    // Called by ImuTask after repeated comms failures.
    bool hardResetAndReinit();

private:
    bool enableReports();

    Adafruit_BNO08x bno_;
    ImuReport latest_{};
    bool connected_ = false;
};
