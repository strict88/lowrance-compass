#include "clock.h"

#include <Arduino.h>
#include <cstdio>

namespace
{

// Howard Hinnant's civil_from_days: converts a day count relative to the Unix
// epoch (1970-01-01) into a proleptic-Gregorian (year, month, day). Avoids
// pulling in <ctime>/timezone machinery for a single deterministic
// conversion. https://howardhinnant.github.io/date_algorithms.html
void civilFromDays(int32_t z, int *year, int *month, int *day)
{
    z += 719468;
    const int32_t era = (z >= 0 ? z : z - 146096) / 146097;
    const uint32_t doe = static_cast<uint32_t>(z - era * 146097);
    const uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int32_t y = static_cast<int32_t>(yoe) + era * 400;
    const uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const uint32_t mp = (5 * doy + 2) / 153;
    const uint32_t d = doy - (153 * mp + 2) / 5 + 1;
    const uint32_t m = mp + (mp < 10 ? 3 : -9);
    *year = static_cast<int>(y + (m <= 2 ? 1 : 0));
    *month = static_cast<int>(m);
    *day = static_cast<int>(d);
}

}  // namespace

uint32_t SystemClock::monotonicMillis() const
{
    return millis();
}

bool SystemClock::wallClockIso8601(char *buf, size_t buf_len) const
{
    if (!has_wall_clock_)
    {
        return false;
    }

    int year = 0;
    int month = 0;
    int day = 0;
    civilFromDays(static_cast<int32_t>(days_since_1970_), &year, &month, &day);

    uint32_t total_seconds = static_cast<uint32_t>(seconds_since_midnight_);
    int hour = static_cast<int>(total_seconds / 3600);
    int minute = static_cast<int>((total_seconds / 60) % 60);
    int second = static_cast<int>(total_seconds % 60);

    int written = snprintf(buf, buf_len, "%04d-%02d-%02dT%02d:%02d:%02dZ", year, month, day, hour, minute, second);
    return written > 0 && static_cast<size_t>(written) < buf_len;
}

void SystemClock::setWallClockFromSystemTime(uint16_t days_since_1970, double seconds_since_midnight)
{
    days_since_1970_ = days_since_1970;
    seconds_since_midnight_ = seconds_since_midnight;
    has_wall_clock_ = true;
}
