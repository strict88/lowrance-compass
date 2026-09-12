#pragma once

#include <stdint.h>

// BNO08x pinout (see CLAUDE.md - source of truth if this ever changes)
constexpr int IMU_SDA_PIN = 8;
constexpr int IMU_SCL_PIN = 9;
constexpr int IMU_INT_PIN = 15; // active-low, not yet consumed by the driver - see bno08x.cpp
constexpr int IMU_RST_PIN = 16; // active-low

// Brings up I2C, resets the BNO08x, and enables the rotation vector report.
// Returns false if the sensor could not be found/initialized.
bool imuInit();

// Non-blocking poll for a new orientation sample.
// Returns false if no new sample is ready yet.
// headingDeg: magnetic heading, normalized to [0, 360).
// calAccuracy: fused rotation-vector accuracy status, 0 (unreliable) - 3 (high).
bool imuPoll(float &headingDeg, uint8_t &calAccuracy);
