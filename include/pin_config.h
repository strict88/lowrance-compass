#pragma once

// Single source of truth for every pin assignment on this board (constitution
// Principle V: Hardware Abstraction and Configuration). See
// specs/001-calibration-guided-setup/quickstart.md "Wiring" for the source
// table. All six deliberately avoid the ESP32-S3-N16R8 reserved ranges:
// flash/PSRAM 26-37, native USB 19/20, strapping 0/3/45/46.

namespace pins
{

// BNO08x IMU, I2C bus.
constexpr int kImuSda = 8;   // avoids 26-37 (flash/PSRAM octal bus)
constexpr int kImuScl = 9;   // avoids 26-37 (flash/PSRAM octal bus)
constexpr int kImuInt = 15;  // drives the interrupt-driven ImuTask design
constexpr int kImuRst = 16;  // hard sensor reset after a comms fault

// NMEA 2000 backbone via VP230 CAN transceiver, TWAI controller.
constexpr int kTwaiTx = 4;  // to VP230 TXD; avoids 26-37, 19/20, 0/3/45/46
constexpr int kTwaiRx = 5;  // from VP230 RXD; avoids 26-37, 19/20, 0/3/45/46

// Network-recovery long-press (FR-038). Reuses the board's BOOT
// button/strapping pin (GPIO0) as a plain input read only after boot
// completes -- never driven. This is the one documented constitution
// deviation; see plan.md "Complexity Tracking" for the justification.
constexpr int kBootButton = 0;

}  // namespace pins
