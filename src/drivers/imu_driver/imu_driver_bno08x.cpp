#include "imu_driver_bno08x.h"

#include <Wire.h>

#include "pin_config.h"
#include "services/diag_log.h"

namespace
{
constexpr uint32_t kRotationVectorIntervalUs = 10000;   // 100 Hz
constexpr uint32_t kCalibratedSensorIntervalUs = 20000;  // 50 Hz
constexpr uint32_t kDisconnectTimeoutMs = 500;  // no report for this long -> SENSOR_DISCONNECTED

// The BNO08x's SA0/ADR pin selects between these two addresses; which one a
// given breakout uses depends on how that pin is strapped on the board, not
// on the chip itself, so both are tried rather than assuming BNO08x's own
// library default (0x4A) is always right.
constexpr uint8_t kBno08xAltI2cAddr = 0x4B;

// Diagnostic only: lists every address that ACKs on the I2C bus, so a "not
// found at 0x4A/0x4B" failure can be told apart from "nothing on the bus at
// all" (power/SDA/SCL wiring) vs. "something's there but at an unexpected
// address" (e.g. PS0/PS1 protocol-select pins not strapped for I2C mode).
void scanI2CBus()
{
    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++)
    {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0)
        {
            diag_log::Line("IMU").token("i2c_scan_found").kv("addr", static_cast<long>(addr)).emit();
            found++;
        }
    }
    if (found == 0)
    {
        diag_log::Line("IMU").token("i2c_scan_empty").emit();
    }
}
}  // namespace

bool ImuDriverBno08x::init()
{
    Wire.begin(pins::kImuSda, pins::kImuScl);
    pinMode(pins::kImuInt, INPUT);
    pinMode(pins::kImuRst, OUTPUT);
    digitalWrite(pins::kImuRst, HIGH);

    last_report_ms_ = millis();
    initialized_ = false;

    if (!bno_.begin_I2C(BNO08x_I2CADDR_DEFAULT, &Wire, pins::kImuRst) &&
        !bno_.begin_I2C(kBno08xAltI2cAddr, &Wire, pins::kImuRst))
    {
        scanI2CBus();
        return false;
    }
    initialized_ = true;

    // Dynamic calibration must be running continuously for the chip to
    // maintain/report High accuracy at all -- not just while Stage A is
    // actively guiding the user through it. Without this, a fresh boot with
    // a perfectly good saved DCD would still show gyro accuracy stuck low
    // indefinitely, since nothing was tracking/refining confidence for it
    // (SC-006: a saved calibration must restore usable accuracy within 10s
    // of boot, not require redoing Stage A every power cycle).
    setCalibrationConfig(/*enable_mag=*/true, /*enable_accel=*/true, /*enable_gyro=*/true);

    return enableReports();
}

bool ImuDriverBno08x::isConnected() const
{
    // Grace period before the first report is covered too: last_report_ms_
    // is seeded to the init() timestamp, so a chip that never reports at
    // all still correctly reports disconnected once the timeout elapses.
    return (millis() - last_report_ms_) < kDisconnectTimeoutMs;
}

bool ImuDriverBno08x::enableReports()
{
    bool ok = true;
    ok = bno_.enableReport(SH2_ROTATION_VECTOR, kRotationVectorIntervalUs) && ok;
    ok = bno_.enableReport(SH2_ACCELEROMETER, kCalibratedSensorIntervalUs) && ok;
    ok = bno_.enableReport(SH2_GYROSCOPE_CALIBRATED, kCalibratedSensorIntervalUs) && ok;
    ok = bno_.enableReport(SH2_MAGNETIC_FIELD_CALIBRATED, kCalibratedSensorIntervalUs) && ok;
    return ok;
}

bool ImuDriverBno08x::readReport(ImuReport &out)
{
    if (!initialized_)
    {
        return false;
    }

    if (bno_.wasReset())
    {
        enableReports();
    }

    sh2_SensorValue_t event;
    if (!bno_.getSensorEvent(&event))
    {
        return false;
    }
    last_report_ms_ = millis();

    bool updated = false;
    switch (event.sensorId)
    {
        case SH2_ROTATION_VECTOR:
            latest_.quat_w = event.un.rotationVector.real;
            latest_.quat_x = event.un.rotationVector.i;
            latest_.quat_y = event.un.rotationVector.j;
            latest_.quat_z = event.un.rotationVector.k;
            updated = true;
            break;
        case SH2_ACCELEROMETER:
            latest_.accel_accuracy = event.status;
            updated = true;
            break;
        case SH2_GYROSCOPE_CALIBRATED:
            latest_.gyro_accuracy = event.status;
            latest_.gyro_x_rad_s = event.un.gyroscope.x;
            latest_.gyro_y_rad_s = event.un.gyroscope.y;
            latest_.gyro_z_rad_s = event.un.gyroscope.z;
            updated = true;
            break;
        case SH2_MAGNETIC_FIELD_CALIBRATED:
            latest_.mag_accuracy = event.status;
            updated = true;
            break;
        default:
            break;
    }

    if (!updated)
    {
        return false;
    }
    out = latest_;
    return true;
}

void ImuDriverBno08x::setCalibrationConfig(bool enable_mag, bool enable_accel, bool enable_gyro)
{
    uint8_t sensors = 0;
    if (enable_accel)
    {
        sensors |= SH2_CAL_ACCEL;
    }
    if (enable_gyro)
    {
        sensors |= SH2_CAL_GYRO;
    }
    if (enable_mag)
    {
        sensors |= SH2_CAL_MAG;
    }
    sh2_setCalConfig(sensors);
}

bool ImuDriverBno08x::saveDcd()
{
    return sh2_saveDcdNow() == SH2_OK;
}

bool ImuDriverBno08x::tare()
{
    if (sh2_setTareNow(SH2_TARE_X | SH2_TARE_Y | SH2_TARE_Z, SH2_TARE_BASIS_ROTATION_VECTOR) != SH2_OK)
    {
        return false;
    }
    return sh2_persistTare() == SH2_OK;
}

bool ImuDriverBno08x::hardResetAndReinit()
{
    digitalWrite(pins::kImuRst, LOW);
    delay(10);
    digitalWrite(pins::kImuRst, HIGH);
    delay(50);
    return init();
}
