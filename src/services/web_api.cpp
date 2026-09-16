#include "web_api.h"

#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#include "n2k_codec/pgn_codec.h"
#include "tasks/shared_state.h"
#include "thresholds.h"

namespace web_api
{

namespace
{

constexpr float kRadToDeg = 57.29577951308232f;

AsyncWebServer g_server(80);
AsyncWebSocket g_ws("/ws");
uint32_t g_last_broadcast_ms = 0;

const char *stageName(CalibrationService::Stage stage)
{
    switch (stage)
    {
        case CalibrationService::Stage::kA:
            return "A";
        case CalibrationService::Stage::kB:
            return "B";
        case CalibrationService::Stage::kC:
            return "C";
        case CalibrationService::Stage::kNone:
        default:
            return "NONE";
    }
}

const char *busStateName(CanBusState state)
{
    switch (state)
    {
        case CanBusState::kRunning:
            return "RUNNING";
        case CanBusState::kBusOff:
            return "BUS_OFF";
        case CanBusState::kErrorPassive:
            return "ERROR_PASSIVE";
        case CanBusState::kBenchMode:
        default:
            return "BENCH_MODE";
    }
}

const char *invalidReasonName(heading::InvalidReason reason)
{
    switch (reason)
    {
        case heading::InvalidReason::kSensorAccuracyLow:
            return "SENSOR_ACCURACY_LOW";
        case heading::InvalidReason::kSensorDisconnected:
            return "SENSOR_DISCONNECTED";
        case heading::InvalidReason::kSensorNotCalibrated:
            return "SENSOR_NOT_CALIBRATED";
        case heading::InvalidReason::kNone:
        default:
            return nullptr;
    }
}

// Assembles the full GET /api/status body (contracts/rest-api.md), reused
// for the periodic `status` WebSocket message (contracts/websocket.md).
// Stage A/B/C readiness/persisted status is hardcoded NOT_DONE for now --
// wired to the real persisted records by each stage's own later task
// (T061/T095/T119); this is expected at this point in the build
// (tasks.md's Foundational checkpoint: "No calibration stage exists yet").
void buildStatusJson(JsonDocument &doc, CalibrationService &calibration_service, N2kService &n2k_service, Clock &clock)
{
    doc["schema"] = 1;

    // TODO(Phase 4/US2): replace with real readiness derivation
    // (src/calibration/readiness.h) once Stage A exists.
    doc["readiness"] = "NOT_CALIBRATED";

    heading::HeadingReading reading = shared_state::getHeadingReading();
    JsonObject heading_obj = doc["heading"].to<JsonObject>();
    heading_obj["valid"] = reading.valid;
    heading_obj["heading_deg"] = reading.heading_rad * kRadToDeg;
    heading_obj["pitch_deg"] = reading.pitch_rad * kRadToDeg;
    heading_obj["roll_deg"] = reading.roll_rad * kRadToDeg;
    heading_obj["rate_of_turn_deg_s"] = reading.rate_of_turn_rad_s * kRadToDeg;
    const char *reason = invalidReasonName(reading.reason_if_invalid);
    if (reason != nullptr)
    {
        heading_obj["reason_if_invalid"] = reason;
    }
    else
    {
        heading_obj["reason_if_invalid"] = nullptr;
    }

    JsonObject stages = doc["stages"].to<JsonObject>();
    for (const char *stage_key : {"a", "b", "c"})
    {
        JsonObject stage_obj = stages[stage_key].to<JsonObject>();
        stage_obj["state"] = "NOT_DONE";
        stage_obj["saved_at"] = nullptr;
    }

    JsonObject session = doc["session"].to<JsonObject>();
    session["active_stage"] = stageName(calibration_service.activeStage());
    session["sub_state"] = "";
    session["progress"].to<JsonObject>();

    JsonObject n2k = doc["n2k"].to<JsonObject>();
    n2k["bus_state"] = busStateName(n2k_service.busState());
    n2k["tx_pgn_127250_count"] = n2k_service.txPgn127250Count();
    n2k["address"] = n2k_service.sourceAddress();
    n2k["cog_sog_source_present"] = n2k_service.cogSogSourcePresent();
    n2k["variation_source_present"] = n2k_service.variationSourcePresent();

    // TODO(US5/Settings): replace with the real persisted NetworkSettings.ssid.
    doc["settings"]["ssid"] = "LowranceCompass";

    JsonObject system = doc["system"].to<JsonObject>();
    system["firmware_version"] = "0.1.0";
    system["uptime_s"] = clock.monotonicMillis() / 1000;
    system["heap_free"] = ESP.getFreeHeap();
    system["reset_reason"] = "POWERON";  // TODO: thread the real reason through from main.cpp
}

void sendCommandAndRespond(AsyncWebServerRequest *request, shared_state::AppCommandType type)
{
    shared_state::AppCommand cmd;
    cmd.type = type;
    char result_buf[256];
    cmd.result_buf = result_buf;
    cmd.result_buf_len = sizeof(result_buf);
    cmd.done_sem = xSemaphoreCreateBinary();
    if (cmd.done_sem == nullptr)
    {
        request->send(503, "application/json",
                       "{\"schema\":1,\"error\":{\"code\":\"BUSY\",\"message\":\"out of memory\"}}");
        return;
    }

    if (!shared_state::postAppCommand(cmd, 200))
    {
        vSemaphoreDelete(cmd.done_sem);
        request->send(503, "application/json",
                       "{\"schema\":1,\"error\":{\"code\":\"BUSY\",\"message\":\"command queue full\"}}");
        return;
    }

    if (xSemaphoreTake(cmd.done_sem, pdMS_TO_TICKS(2000)) != pdTRUE)
    {
        vSemaphoreDelete(cmd.done_sem);
        request->send(504, "application/json",
                       "{\"schema\":1,\"error\":{\"code\":\"TIMEOUT\",\"message\":\"no response from app task\"}}");
        return;
    }
    vSemaphoreDelete(cmd.done_sem);

    int status = cmd.success ? 200 : 409;
    request->send(status, "application/json", result_buf);
}

void onWsEvent(AsyncWebSocket * /*server*/, AsyncWebSocketClient * /*client*/, AwsEventType /*type*/, void * /*arg*/,
               uint8_t * /*data*/, size_t /*len*/)
{
    // Server-push only (contracts/websocket.md): the UI issues commands over
    // REST, never over the socket, so no inbound message handling is needed
    // here beyond AsyncWebSocket's own connect/disconnect bookkeeping.
}

}  // namespace

void start(CalibrationService &calibration_service, N2kService &n2k_service, Clock &clock)
{
    LittleFS.begin(true);

    g_ws.onEvent(onWsEvent);
    g_server.addHandler(&g_ws);

    g_server.on("/api/status", HTTP_GET, [&calibration_service, &n2k_service, &clock](AsyncWebServerRequest *request) {
        JsonDocument doc;
        buildStatusJson(doc, calibration_service, n2k_service, clock);
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        serializeJson(doc, *response);
        request->send(response);
    });

    g_server.on("/api/calibration/a/start", HTTP_POST, [](AsyncWebServerRequest *request) {
        sendCommandAndRespond(request, shared_state::AppCommandType::kCalStartA);
    });
    g_server.on("/api/calibration/a/cancel", HTTP_POST, [](AsyncWebServerRequest *request) {
        sendCommandAndRespond(request, shared_state::AppCommandType::kCalCancelA);
    });
    g_server.on("/api/calibration/b/start", HTTP_POST, [](AsyncWebServerRequest *request) {
        sendCommandAndRespond(request, shared_state::AppCommandType::kCalStartB);
    });
    g_server.on("/api/calibration/b/cancel", HTTP_POST, [](AsyncWebServerRequest *request) {
        sendCommandAndRespond(request, shared_state::AppCommandType::kCalCancelB);
    });
    g_server.on("/api/calibration/c/start", HTTP_POST, [](AsyncWebServerRequest *request) {
        // TODO(Phase 8/US4): parse the {"mode":"gps"|"manual"} body once
        // Stage C exists; defaults to the GPS-swing sub-flow for now.
        sendCommandAndRespond(request, shared_state::AppCommandType::kCalStartCGps);
    });
    g_server.on("/api/calibration/c/cancel", HTTP_POST, [](AsyncWebServerRequest *request) {
        sendCommandAndRespond(request, shared_state::AppCommandType::kCalCancelC);
    });

    g_server.serveStatic("/", LittleFS, "/www/").setDefaultFile("index.html");

    g_server.begin();
}

void broadcastStatusIfDue(CalibrationService &calibration_service, N2kService &n2k_service, Clock &clock)
{
    bool active = calibration_service.activeStage() != CalibrationService::Stage::kNone;
    float rate_hz = active ? thresholds::kWsStatusRateHzActiveCal : thresholds::kWsStatusRateHzIdle;
    uint32_t interval_ms = static_cast<uint32_t>(1000.0f / rate_hz);

    uint32_t now_ms = clock.monotonicMillis();
    if (now_ms - g_last_broadcast_ms < interval_ms)
    {
        return;
    }
    g_last_broadcast_ms = now_ms;

    if (g_ws.count() == 0)
    {
        return;
    }

    JsonDocument doc;
    buildStatusJson(doc, calibration_service, n2k_service, clock);
    doc["type"] = "status";

    char buf[768];
    size_t len = serializeJson(doc, buf, sizeof(buf));
    g_ws.textAll(buf, len);
}

}  // namespace web_api
