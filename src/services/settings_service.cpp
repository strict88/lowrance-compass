#include "settings_service.h"

#include <cstring>

void SettingsService::init(record_envelope::Status *status_out)
{
    settings::NetworkSettings loaded;
    if (settings::loadNetworkSettings(store_, loaded, status_out))
    {
        current_ = loaded;
    }
    else
    {
        std::strncpy(current_.ssid, factoryDefaultSsid(), sizeof(current_.ssid) - 1);
    }
}

bool SettingsService::save(const settings::NetworkSettings &new_settings, float now_s, uint32_t applies_in_s)
{
    if (!settings::validateSsid(new_settings.ssid))
    {
        return false;
    }
    if (!settings::saveNetworkSettings(store_, new_settings))
    {
        return false;
    }
    current_ = new_settings;
    restart_pending_ = true;
    restart_due_at_s_ = now_s + static_cast<float>(applies_in_s);
    return true;
}

void SettingsService::resetToDefault(float now_s)
{
    settings::NetworkSettings defaults;
    std::strncpy(defaults.ssid, factoryDefaultSsid(), sizeof(defaults.ssid) - 1);
    settings::saveNetworkSettings(store_, defaults);
    current_ = defaults;
    restart_pending_ = true;
    restart_due_at_s_ = now_s;
}

bool SettingsService::isRestartDue(float now_s) const
{
    return restart_pending_ && now_s >= restart_due_at_s_;
}

void SettingsService::clearPendingRestart()
{
    restart_pending_ = false;
}
