#pragma once

#include <cstddef>
#include <cstdint>

// Structured serial log line writer implementing the grammar in
// specs/001-calibration-guided-setup/contracts/serial-log.md:
//
//   [TAG] token key1=value1 key2=value2 ...
//
// One line per event, newline-terminated, ASCII only. `token` is an optional
// bare event-name (e.g. "boot", "disconnected", "busy") with no `=`; keys and
// values never contain `=` or whitespace. Hardware-only (uses Serial) --
// excluded from the native build.
namespace diag_log
{

// Fluent single-line builder. Not thread-safe: build and emit() a line
// entirely from one task before another task starts its own line, per the
// tag ownership below (ImuTask -> IMU, N2kTask -> N2K, AppTask -> SYS/CAL/
// WIFI/DATA).
class Line
{
public:
    explicit Line(const char *tag);

    Line &token(const char *bare_token);
    Line &kv(const char *key, long value);
    Line &kv(const char *key, unsigned long value);
    Line &kv(const char *key, int value);
    Line &kv(const char *key, double value, uint8_t decimals = 1);
    Line &kv(const char *key, const char *value);

    // Writes the assembled line followed by '\n' to Serial.
    void emit();

private:
    char buf_[192];
    size_t len_;

    void appendRaw(const char *s);
    void appendSpaceIfNeeded();
};

// Call once after Serial is up. Emits nothing itself.
void init();

// [SYS] boot fw=<fw_version> reset_reason=<reset_reason> heap=<heap_free>
void logBoot(const char *fw_version, const char *reset_reason, uint32_t heap_free);

// [SYS] heap=<heap_free> uptime=<uptime_s>
void logHeartbeat(uint32_t heap_free, uint32_t uptime_s);

// [DATA] reset record=<record_name> reason=<crc_mismatch|schema_mismatch>
void logRecordReset(const char *record_name, const char *reason);

}  // namespace diag_log
