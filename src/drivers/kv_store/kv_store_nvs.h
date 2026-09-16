#pragma once

#include <Preferences.h>

#include "kv_store.h"

// KeyValueStore backed by ESP32 NVS via the Arduino Preferences API. One
// instance per record namespace (e.g. "cal_a", "cal_b", "cal_c", "netcfg").
class NvsKeyValueStore : public KeyValueStore
{
public:
    explicit NvsKeyValueStore(const char *ns) : ns_(ns) {}

    bool begin(bool read_only) override;
    void end() override;

    size_t putBytes(const char *key, const void *value, size_t len) override;
    size_t getBytesLength(const char *key) override;
    size_t getBytes(const char *key, void *buf, size_t max_len) override;

    bool putUChar(const char *key, uint8_t value) override;
    uint8_t getUChar(const char *key, uint8_t default_value) override;

    bool remove(const char *key) override;

private:
    const char *ns_;
    Preferences prefs_;
};
