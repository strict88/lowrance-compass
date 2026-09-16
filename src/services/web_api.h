#pragma once

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

}  // namespace web_api
