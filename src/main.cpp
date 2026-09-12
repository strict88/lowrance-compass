#include <Arduino.h>

#include "imu/bno08x.h"

namespace {
constexpr uint32_t IMU_RETRY_INTERVAL_MS = 2000;
bool imuReady = false;
uint32_t lastRetryMs = 0;
} // namespace

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  imuReady = imuInit();
  if (!imuReady) {
    Serial.println("BNO08x init failed - check wiring/I2C address");
  }
}

void loop() {
  if (!imuReady) {
    // Sensor calls (getSensorEvent etc.) assume a successful begin_I2C() -
    // calling them after a failed init crashes the SH2 library. Retry
    // periodically instead of ever touching the library in that state.
    if (millis() - lastRetryMs >= IMU_RETRY_INTERVAL_MS) {
      lastRetryMs = millis();
      Serial.println("Retrying BNO08x init...");
      imuReady = imuInit();
    }
    return;
  }

  float headingDeg;
  uint8_t calAccuracy;

  if (imuPoll(headingDeg, calAccuracy)) {
    Serial.printf("heading=%.1f  cal=%u\n", headingDeg, calAccuracy);
  }
}
