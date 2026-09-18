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

    // True once a report has ever arrived and one has arrived within the
    // last kDisconnectTimeoutMs; a wire disconnect (or a chip that stops
    // responding for any other reason) presents as reports simply stopping,
    // not as any single I2C call returning a distinguishable error, so
    // "connected" is derived from report recency rather than the last
    // transaction's own return code.
    bool isConnected() const override;

    // Hard resets the sensor via RST (pin_config.h) and re-initializes it.
    // Called by ImuTask after repeated comms failures.
    bool hardResetAndReinit();

private:
    bool enableReports();

    Adafruit_BNO08x bno_;
    ImuReport latest_{};
    uint32_t last_report_ms_ = 0;

    // False whenever begin_I2C() has never succeeded (no chip present/ACKing,
    // or not yet re-initialized after a hard reset). readReport() must not
    // touch `bno_`'s SH-2/SHTP internals while this is false -- calling into
    // an Adafruit_BNO08x that was never successfully begin()'d crashes
    // (LoadProhibited deep inside the vendor sh2/shtp state machine) instead
    // of failing gracefully.
    bool initialized_ = false;
};
