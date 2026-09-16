#include "imu_driver_bno08x.h"

#include <Wire.h>

#include "pin_config.h"

namespace
{
constexpr uint32_t kRotationVectorIntervalUs = 10000;   // 100 Hz
constexpr uint32_t kCalibratedSensorIntervalUs = 20000;  // 50 Hz
}  // namespace

bool ImuDriverBno08x::init()
{
    Wire.begin(pins::kImuSda, pins::kImuScl);
    pinMode(pins::kImuInt, INPUT);
    pinMode(pins::kImuRst, OUTPUT);
    digitalWrite(pins::kImuRst, HIGH);

    if (!bno_.begin_I2C(BNO08x_I2CADDR_DEFAULT, &Wire, pins::kImuRst))
    {
        connected_ = false;
        return false;
    }

    connected_ = enableReports();
    return connected_;
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
    if (bno_.wasReset())
    {
        connected_ = enableReports();
    }

    sh2_SensorValue_t event;
    if (!bno_.getSensorEvent(&event))
    {
        return false;
    }
    connected_ = true;

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
