#include "network_settings.h"

#include <cstring>

#include "drivers/kv_store/record_envelope.h"

namespace settings
{

bool validateSsid(const char *ssid)
{
    if (ssid == nullptr)
    {
        return false;
    }
    size_t len = std::strlen(ssid);
    if (len < static_cast<size_t>(kSsidMinLen) || len > static_cast<size_t>(kSsidMaxLen))
    {
        return false;
    }
    if (ssid[0] == ' ' || ssid[0] == '\t' || ssid[len - 1] == ' ' || ssid[len - 1] == '\t')
    {
        return false;
    }
    return true;
}

bool saveNetworkSettings(KeyValueStore &store, const NetworkSettings &settings)
{
    if (!validateSsid(settings.ssid))
    {
        return false;
    }
    return record_envelope::save(store, kNetworkSettingsSchemaVersion, reinterpret_cast<const uint8_t *>(&settings),
                                  sizeof(settings));
}

bool loadNetworkSettings(KeyValueStore &store, NetworkSettings &out, record_envelope::Status *status_out)
{
    NetworkSettings loaded;
    auto result = record_envelope::load(store, kNetworkSettingsSchemaVersion, reinterpret_cast<uint8_t *>(&loaded),
                                         sizeof(loaded));
    if (status_out != nullptr)
    {
        *status_out = result.status;
    }
    if (result.status == record_envelope::Status::kCrcMismatch ||
        result.status == record_envelope::Status::kSchemaMismatch)
    {
        record_envelope::resetToDefault(store);  // FR-045; see calibration/stage_a.cpp's equivalent.
    }
    if (result.status != record_envelope::Status::kOk)
    {
        return false;
    }
    out = loaded;
    return true;
}

}  // namespace settings
