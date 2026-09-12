#include <Arduino.h>

#include "imu/bno08x.h"

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  if (!imuInit()) {
    Serial.println("BNO08x init failed - check wiring/I2C address");
  }
}

void loop() {
  float headingDeg;
  uint8_t calAccuracy;

  if (imuPoll(headingDeg, calAccuracy)) {
    Serial.printf("heading=%.1f  cal=%u\n", headingDeg, calAccuracy);
  }
}
