#pragma once

#include <cstdint>

#include "drivers/kv_store/kv_store.h"
#include "settings/network_settings.h"

// Owns the persisted NetworkSettings record and the save -> respond -> AP
// restart sequence (FR-036/037): a save takes effect only after
// `applies_in_s` (letting the UI show the drop-connection warning first),
// while N2kService/ImuTask continue uninterrupted -- restarting the AP never
// touches the N2K/heading tasks. Pure logic driven by an externally-supplied
// monotonic time, so it's native-testable without a Clock/WiFi dependency.
class SettingsService
{
public:
    explicit SettingsService(KeyValueStore &store) : store_(store) {}

    // Loads the persisted SSID, or a fixed factory default if none is saved
    // yet.
    void init();

    const settings::NetworkSettings &current() const { return current_; }

    // Validates and persists `new_settings`; returns false (nothing saved
    // or scheduled) if the SSID fails FR-035 validation. On success,
    // schedules an AP restart `applies_in_s` seconds from `now_s`.
    bool save(const settings::NetworkSettings &new_settings, float now_s, uint32_t applies_in_s);

    // Resets to the factory-default SSID and schedules an immediate (0 s)
    // AP restart -- the network-recovery path (FR-038).
    void resetToDefault(float now_s);

    // True once a scheduled AP restart's due time has arrived; the caller
    // (AppTask) then actually restarts the AP and calls
    // clearPendingRestart().
    bool isRestartDue(float now_s) const;
    void clearPendingRestart();
    bool hasPendingRestart() const { return restart_pending_; }

    static const char *factoryDefaultSsid() { return "LowranceCompass"; }

private:
    KeyValueStore &store_;
    settings::NetworkSettings current_{};
    bool restart_pending_ = false;
    float restart_due_at_s_ = 0.0f;
};
