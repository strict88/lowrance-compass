#pragma once

#include <cstddef>
#include <cstdint>

// Hardware-adapter interface for time. Two independent notions (data-model.md
// §4):
//   - monotonicMillis(): free-running uptime, never affected by wall-clock
//     source changes -- used for timeouts/durations everywhere.
//   - wallClockIso8601(): a "date" for persisted records' saved_at fields,
//     sourced from the last-received NMEA 2000 System Time (PGN 126992).
//     Returns false ("date unavailable") if no bus time has been received
//     since boot.
class Clock
{
public:
    virtual ~Clock() = default;

    virtual uint32_t monotonicMillis() const = 0;

    // On success, writes a "YYYY-MM-DDTHH:MM:SSZ" string (at least 21 bytes
    // including the terminator) into `buf` and returns true. Returns false,
    // leaving `buf` untouched, if no NMEA 2000 System Time has been received
    // since boot.
    virtual bool wallClockIso8601(char *buf, size_t buf_len) const = 0;

    // Called by the N2K receive path whenever a fresh PGN 126992 System Time
    // arrives, so the wall clock can start answering wallClockIso8601().
    virtual void setWallClockFromSystemTime(uint16_t days_since_1970, double seconds_since_midnight) = 0;
};

// Concrete Clock over Arduino's millis() plus a wall clock derived from the
// last NMEA 2000 System Time received. Hardware-only (uses Arduino.h in its
// .cpp) -- native tests use FakeClock instead.
class SystemClock : public Clock
{
public:
    uint32_t monotonicMillis() const override;
    bool wallClockIso8601(char *buf, size_t buf_len) const override;
    void setWallClockFromSystemTime(uint16_t days_since_1970, double seconds_since_midnight) override;

private:
    bool has_wall_clock_ = false;
    uint16_t days_since_1970_ = 0;
    double seconds_since_midnight_ = 0;
};
