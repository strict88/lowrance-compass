#pragma once

#include <cstdint>

#include "drivers/clock/clock.h"
#include "services/calibration_service.h"
#include "services/n2k_service.h"

// ESPAsyncWebServer bootstrap: serves LittleFS static assets from
// data/www/, JSON REST endpoints (contracts/rest-api.md) via ArduinoJson v7,
// and a /ws WebSocket endpoint (contracts/websocket.md). State-changing
// requests are bridged to AppTask via shared_state's command queue; this
// module never mutates CalibrationService/N2kService state directly.
namespace web_api
{

// Starts the async web server (call once from setup()/AppTask init).
// References must outlive the server.
void start(CalibrationService &calibration_service, N2kService &n2k_service, Clock &clock);

// (Re)configures the device's Wi-Fi Access Point with `ssid` (open, no
// password -- out of scope for this feature per data-model.md §1.4). Call
// once at boot with the loaded SSID, and again whenever SettingsService's
// scheduled restart comes due.
void configureAccessPoint(const char *ssid);

// Sends a `status` WebSocket message to every connected client if
// WS_STATUS_RATE_HZ_IDLE/ACTIVE_CAL's interval has elapsed. Call
// periodically from AppTask's loop (never from ImuTask/N2kTask, per
// constitution Principle III).
void broadcastStatusIfDue(CalibrationService &calibration_service, N2kService &n2k_service, Clock &clock);

// Sends a one-shot `calibration_result` WebSocket message (contracts/
// websocket.md) when a stage reaches a terminal or result-ready state.
// `outcome` in {"SAVED","REJECTED","CANCELLED","TIMED_OUT"}; `reason` may be
// nullptr.
void broadcastCalibrationResult(const char *stage, const char *outcome, const char *reason);

// Sends a `settings_changed` WebSocket message (contracts/websocket.md) to
// every connected client, including the one that made the change, so a
// second open browser tab reflects it immediately rather than waiting for
// the next periodic `status` broadcast.
void broadcastSettingsChanged(const char *ssid, uint32_t applies_in_s);

}  // namespace web_api
