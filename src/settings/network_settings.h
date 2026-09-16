#pragma once

#include <cstdint>

#include "drivers/kv_store/kv_store.h"

// SSID validation (FR-035) and the NetworkSettings record (data-model.md
// §1.4). Pure logic, native-testable.
namespace settings
{

constexpr int kSsidMinLen = 1;
constexpr int kSsidMaxLen = 32;

// FR-035 (quoted): "1-32 characters, no leading or trailing whitespace".
bool validateSsid(const char *ssid);

struct NetworkSettings
{
    char ssid[kSsidMaxLen + 1] = {0};
};

constexpr uint8_t kNetworkSettingsSchemaVersion = 1;

bool saveNetworkSettings(KeyValueStore &store, const NetworkSettings &settings);
bool loadNetworkSettings(KeyValueStore &store, NetworkSettings &out);

}  // namespace settings
