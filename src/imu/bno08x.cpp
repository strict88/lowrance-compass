#include "imu/bno08x.h"

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BNO08x.h>
#include <math.h>

namespace {

// BNO08x default I2C address is 0x4A, 0x4B if the board's ADR/PS0 pin is
// strapped high - confirmed on this hardware via the I2C scan below, which
// found the sensor answering only at 0x4B, not the 0x4A
// Adafruit_BNO08x::begin_I2C() tries by default.
constexpr uint8_t BNO08X_I2C_ADDR = 0x4B;
constexpr uint32_t ROTATION_VECTOR_INTERVAL_US = 50000; // 50ms -> 20Hz

Adafruit_BNO08x bno08x(IMU_RST_PIN);
sh2_SensorValue_t sensorEvent;

bool enableRotationVectorReport() {
  return bno08x.enableReport(SH2_ROTATION_VECTOR, ROTATION_VECTOR_INTERVAL_US);
}

// Standard quaternion -> yaw conversion for the BNO08x rotation vector
// (same formula as Adafruit's own example sketches), yaw in radians.
float quaternionToYawRad(float qr, float qi, float qj, float qk) {
  float sqr = qr * qr;
  float sqi = qi * qi;
  float sqj = qj * qj;
  float sqk = qk * qk;
  return atan2f(2.0f * (qi * qj + qk * qr), (sqi - sqj - sqk + sqr));
}

// Diagnostic only: lists every address that ACKs on the I2C bus. Useful
// while the BNO08x isn't being found at its expected 0x4A/0x4B, to tell a
// dead bus (nothing found - power/wiring) apart from a live device at an
// unexpected address (found, but not 0x4A/0x4B - e.g. PS0/PS1 protocol-select
// pins not strapped for I2C mode on a bare module).
void scanI2CBus() {
  Serial.println("I2C scan:");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  device found at 0x%02X\n", addr);
      found++;
    }
  }
  if (found == 0) {
    Serial.println("  no devices found");
  }
}

} // namespace

bool imuInit() {
  Wire.begin(IMU_SDA_PIN, IMU_SCL_PIN);

  // Wired per the pinout's "enable pullup" note. Adafruit_BNO08x does not
  // itself use the INT pin (only the reset pin, via the constructor above) -
  // this is a placeholder until/unless interrupt-driven reads are added.
  pinMode(IMU_INT_PIN, INPUT_PULLUP);
  if (!bno08x.begin_I2C(BNO08X_I2C_ADDR)) {
    scanI2CBus();
    return false;
  }

  return enableRotationVectorReport();
}

bool imuPoll(float &headingDeg, uint8_t &calAccuracy) {
  if (bno08x.wasReset()) {
    // The BNO08x drops all enabled reports across an internal reset -
    // without re-enabling here, output silently stops after any reset.
    enableRotationVectorReport();
  }

  if (!bno08x.getSensorEvent(&sensorEvent)) {
    return false;
  }

  if (sensorEvent.sensorId != SH2_ROTATION_VECTOR) {
    return false;
  }

  float yawRad = quaternionToYawRad(
      sensorEvent.un.rotationVector.real,
      sensorEvent.un.rotationVector.i,
      sensorEvent.un.rotationVector.j,
      sensorEvent.un.rotationVector.k);

  float yawDeg = yawRad * RAD_TO_DEG;
  if (yawDeg < 0) {
    yawDeg += 360.0f;
  }

  headingDeg = yawDeg;
  calAccuracy = sensorEvent.status; // 0 (unreliable) - 3 (high), fused-orientation accuracy
  return true;
}
