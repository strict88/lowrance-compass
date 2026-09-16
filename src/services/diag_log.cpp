#include "diag_log.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

namespace diag_log
{

Line::Line(const char *tag) : len_(0)
{
    buf_[0] = '\0';
    appendRaw("[");
    appendRaw(tag);
    appendRaw("]");
}

void Line::appendSpaceIfNeeded()
{
    if (len_ > 0 && len_ < sizeof(buf_) - 1)
    {
        buf_[len_++] = ' ';
        buf_[len_] = '\0';
    }
}

void Line::appendRaw(const char *s)
{
    size_t remaining = sizeof(buf_) - 1 - len_;
    size_t n = strlen(s);
    if (n > remaining)
    {
        n = remaining;
    }
    memcpy(buf_ + len_, s, n);
    len_ += n;
    buf_[len_] = '\0';
}

Line &Line::token(const char *bare_token)
{
    appendSpaceIfNeeded();
    appendRaw(bare_token);
    return *this;
}

Line &Line::kv(const char *key, long value)
{
    char num[24];
    snprintf(num, sizeof(num), "%ld", value);
    appendSpaceIfNeeded();
    appendRaw(key);
    appendRaw("=");
    appendRaw(num);
    return *this;
}

Line &Line::kv(const char *key, unsigned long value)
{
    char num[24];
    snprintf(num, sizeof(num), "%lu", value);
    appendSpaceIfNeeded();
    appendRaw(key);
    appendRaw("=");
    appendRaw(num);
    return *this;
}

Line &Line::kv(const char *key, int value)
{
    return kv(key, static_cast<long>(value));
}

Line &Line::kv(const char *key, double value, uint8_t decimals)
{
    char num[32];
    snprintf(num, sizeof(num), "%.*f", decimals, value);
    appendSpaceIfNeeded();
    appendRaw(key);
    appendRaw("=");
    appendRaw(num);
    return *this;
}

Line &Line::kv(const char *key, const char *value)
{
    appendSpaceIfNeeded();
    appendRaw(key);
    appendRaw("=");
    appendRaw(value);
    return *this;
}

void Line::emit()
{
    Serial.print(buf_);
    Serial.print('\n');
}

void init()
{
    // Nothing to initialize; Serial is brought up by the caller before this
    // module is used.
}

void logBoot(const char *fw_version, const char *reset_reason, uint32_t heap_free)
{
    Line("SYS")
        .token("boot")
        .kv("fw", fw_version)
        .kv("reset_reason", reset_reason)
        .kv("heap", heap_free)
        .emit();
}

void logHeartbeat(uint32_t heap_free, uint32_t uptime_s)
{
    Line("SYS").kv("heap", heap_free).kv("uptime", uptime_s).emit();
}

void logRecordReset(const char *record_name, const char *reason)
{
    Line("DATA").token("reset").kv("record", record_name).kv("reason", reason).emit();
}

}  // namespace diag_log
