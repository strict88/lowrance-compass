#pragma once

#include <cstdio>
#include <cstring>

#include "drivers/clock/clock.h"

// In-memory Clock for native tests: monotonic time and wall-clock date are
// both set explicitly by the test, never derived from a real clock source.
class FakeClock : public Clock
{
public:
    uint32_t monotonicMillis() const override { return millis_; }

    bool wallClockIso8601(char *buf, size_t buf_len) const override
    {
        if (!has_wall_clock_)
        {
            return false;
        }
        int written = snprintf(buf, buf_len, "%s", iso8601_);
        return written > 0 && static_cast<size_t>(written) < buf_len;
    }

    void setWallClockFromSystemTime(uint16_t /*days_since_1970*/, double /*seconds_since_midnight*/) override
    {
        // Not used by native tests directly; use setIso8601ForTest() instead
        // so tests can assert on an exact, human-readable string.
    }

    void setMillisForTest(uint32_t ms) { millis_ = ms; }

    void setIso8601ForTest(const char *iso8601)
    {
        snprintf(iso8601_, sizeof(iso8601_), "%s", iso8601);
        has_wall_clock_ = true;
    }

    void clearWallClockForTest() { has_wall_clock_ = false; }

private:
    uint32_t millis_ = 0;
    bool has_wall_clock_ = false;
    char iso8601_[32] = {0};
};
