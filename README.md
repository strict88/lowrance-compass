# Lowrance Compass

An ESP32-S3 NMEA 2000 heading sensor: BNO08x IMU calibration, installation
alignment, and compass-swing deviation correction, exposed through a
phone-first Wi-Fi web UI. See `specs/001-calibration-guided-setup/` for the
full spec/plan/tasks and `quickstart.md` for build/flash/test instructions.

## NMEA 2000 installation (electrical)

Read this before wiring the device to a boat's NMEA 2000 backbone.

- **Power from the backbone through a proper regulator.** Do not tap the
  backbone's 12 V supply directly into the device's own regulator without
  verifying it tolerates the bus's voltage range and transients; use a
  regulator/converter rated for marine NMEA 2000 power (per NMEA 2000 /
  IEC 61162-3 supply requirements).
- **Declare a Load Equivalency Number (LEN)** for this device in its product
  information (one LEN = 50 mA) so the backbone's power budget can be
  calculated correctly alongside every other node on the bus.
- **No extra 120 Ω termination on a drop cable.** The bus is terminated only
  at its two physical ends (the backbone itself), never on a drop cable to
  this device. If the CAN transceiver module (VP230) you're using ships with
  an on-board 120 Ω termination resistor/jumper, **remove it** before
  installing on a drop cable — leaving it in place mis-terminates the bus and
  can degrade or disrupt every other instrument sharing it.

This device must behave as a well-mannered NMEA 2000 node (address claim,
product/configuration info, ISO request handling, PGN list, heartbeat) —
see the constitution's NMEA 2000 Compliance principle
(`.specify/memory/constitution.md`) and `specs/001-calibration-guided-setup/plan.md`'s
Constitution Check for how this is implemented and verified.
