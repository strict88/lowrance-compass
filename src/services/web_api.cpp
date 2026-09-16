#include "web_api.h"

#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#include "calibration/readiness.h"
#include "guide/guide_content.h"
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
// Stage A is wired to its real persisted/live status via shared_state;
// Stages B/C are still hardcoded NOT_DONE, wired up by their own later tasks
// (T095/T119).
void buildStatusJson(JsonDocument &doc, CalibrationService &calibration_service, N2kService &n2k_service, Clock &clock)
{
    doc["schema"] = 1;

    shared_state::StageAStatusSnapshot stage_a_status = shared_state::getStageAStatus();
    shared_state::StageBStatusSnapshot stage_b_status = shared_state::getStageBStatus();
    heading::HeadingReading reading = shared_state::getHeadingReading();

    calibration::ReadinessInput readiness_input;
    readiness_input.stage_a_done = stage_a_status.persisted_done;
    readiness_input.stage_b_done = stage_b_status.persisted_done;
    // TODO(Phase 8): stage_c_done once Stage C exists.
    readiness_input.heading_currently_withheld = !reading.valid;
    switch (calibration::deriveReadiness(readiness_input))
    {
        case calibration::Readiness::kReady:
            doc["readiness"] = "READY";
            break;
        case calibration::Readiness::kUsableIncomplete:
            doc["readiness"] = "USABLE_INCOMPLETE";
            break;
        case calibration::Readiness::kNotCalibrated:
        default:
            doc["readiness"] = "NOT_CALIBRATED";
            break;
    }
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
    JsonObject stage_a_obj = stages["a"].to<JsonObject>();
    if (stage_a_status.persisted_done)
    {
        stage_a_obj["state"] = "DONE";
        JsonObject quality = stage_a_obj["quality"].to<JsonObject>();
        quality["mag"] = stage_a_status.saved_mag_accuracy;
        quality["accel"] = stage_a_status.saved_accel_accuracy;
        quality["gyro"] = stage_a_status.saved_gyro_accuracy;
        stage_a_obj["saved_at"] = stage_a_status.saved_at_iso8601;
    }
    else
    {
        stage_a_obj["state"] = "NOT_DONE";
        stage_a_obj["saved_at"] = nullptr;
    }
    JsonObject stage_b_obj = stages["b"].to<JsonObject>();
    if (stage_b_status.persisted_done)
    {
        stage_b_obj["state"] = "DONE";
        stage_b_obj["offset_deg"] = stage_b_status.saved_offset_deg;
        stage_b_obj["method"] = stage_b_status.saved_method_is_gps ? "GPS_COURSE" : "KNOWN_BEARING";
        stage_b_obj["saved_at"] = stage_b_status.saved_at_iso8601;
    }
    else
    {
        stage_b_obj["state"] = "NOT_DONE";
        stage_b_obj["saved_at"] = nullptr;
    }
    // TODO(Phase 8): stage "c" once Stage C exists.
    JsonObject stage_c_obj = stages["c"].to<JsonObject>();
    stage_c_obj["state"] = "NOT_DONE";
    stage_c_obj["saved_at"] = nullptr;

    JsonObject session = doc["session"].to<JsonObject>();
    session["active_stage"] = stageName(calibration_service.activeStage());
    session["sub_state"] = "";
    JsonObject progress = doc["session"]["progress"].to<JsonObject>();
    if (stage_a_status.session_active)
    {
        progress["mag_acc"] = stage_a_status.live_mag_accuracy;
        progress["accel_acc"] = stage_a_status.live_accel_accuracy;
        progress["gyro_acc"] = stage_a_status.live_gyro_accuracy;
        JsonArray positions = progress["positions_done"].to<JsonArray>();
        static const char *kPositionNames[6] = {"POS_X", "NEG_X", "POS_Y", "NEG_Y", "POS_Z", "NEG_Z"};
        for (int i = 0; i < 6; ++i)
        {
            if (stage_a_status.positions_done[i])
            {
                positions.add(kPositionNames[i]);
            }
        }
        progress["rotation_coverage_pct"] = stage_a_status.rotation_coverage_pct;
    }
    else if (stage_b_status.session_active)
    {
        progress["level_set"] = stage_b_status.level_set;
        progress["awaiting_method_choice"] = stage_b_status.awaiting_method_choice;
        progress["awaiting_bearing_entry"] = stage_b_status.awaiting_bearing_entry;
        progress["awaiting_gps_alignment"] = stage_b_status.awaiting_gps_alignment;
        if (stage_b_status.waiting_speed_too_low)
        {
            progress["waiting_reason"] = "SPEED_TOO_LOW";
        }
        else if (stage_b_status.waiting_course_not_steady)
        {
            progress["waiting_reason"] = "COURSE_NOT_STEADY";
        }
        else
        {
            progress["waiting_reason"] = nullptr;
        }
        if (stage_b_status.has_preview_offset)
        {
            progress["preview_offset_deg"] = stage_b_status.preview_offset_deg;
        }
        else
        {
            progress["preview_offset_deg"] = nullptr;
        }
    }

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

void sendConfirmCommandAndRespond(AsyncWebServerRequest *request, JsonVariant &json, shared_state::AppCommandType type)
{
    shared_state::AppCommand cmd;
    cmd.type = type;
    cmd.confirm = json["confirm"] | false;
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

    request->send(cmd.success ? 200 : 400, "application/json", result_buf);
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

    g_server.on("/api/guide", HTTP_GET, [](AsyncWebServerRequest *request) {
        static const char *kGuidePath = "/guide/guide_content.json";
        File file = LittleFS.open(kGuidePath, "r");
        if (!file)
        {
            request->send(500, "application/json",
                          "{\"schema\":1,\"error\":{\"code\":\"GUIDE_UNAVAILABLE\",\"message\":\"guide content file "
                          "missing\"}}");
            return;
        }

        String contents = file.readString();
        file.close();

        JsonDocument doc;
        if (!guide::parseFromJsonString(contents.c_str(), doc))
        {
            request->send(500, "application/json",
                          "{\"schema\":1,\"error\":{\"code\":\"GUIDE_UNAVAILABLE\",\"message\":\"guide content file is "
                          "malformed\"}}");
            return;
        }

        request->send(LittleFS, kGuidePath, "application/json");
    });

    g_server.on("/api/calibration/a/start", HTTP_POST, [](AsyncWebServerRequest *request) {
        sendCommandAndRespond(request, shared_state::AppCommandType::kCalStartA);
    });
    g_server.on("/api/calibration/a/cancel", HTTP_POST, [](AsyncWebServerRequest *request) {
        sendCommandAndRespond(request, shared_state::AppCommandType::kCalCancelA);
    });
    g_server.on("/api/calibration/a/reset", HTTP_POST, [](AsyncWebServerRequest *request, JsonVariant &json) {
        sendConfirmCommandAndRespond(request, json, shared_state::AppCommandType::kCalResetA);
    });
    g_server.on("/api/calibration/b/start", HTTP_POST, [](AsyncWebServerRequest *request) {
        sendCommandAndRespond(request, shared_state::AppCommandType::kCalStartB);
    });
    g_server.on("/api/calibration/b/cancel", HTTP_POST, [](AsyncWebServerRequest *request) {
        sendCommandAndRespond(request, shared_state::AppCommandType::kCalCancelB);
    });
    g_server.on("/api/calibration/b/level", HTTP_POST, [](AsyncWebServerRequest *request) {
        sendCommandAndRespond(request, shared_state::AppCommandType::kCalBLevel);
    });
    g_server.on("/api/calibration/b/gps-course", HTTP_POST, [](AsyncWebServerRequest *request) {
        sendCommandAndRespond(request, shared_state::AppCommandType::kCalBChooseGps);
    });
    g_server.on("/api/calibration/b/bearing", HTTP_POST, [](AsyncWebServerRequest *request, JsonVariant &json) {
        shared_state::AppCommand cmd;
        cmd.type = shared_state::AppCommandType::kCalBBearing;
        cmd.float_param = json["bearing_deg"] | 0.0f;
        char result_buf[256];
        cmd.result_buf = result_buf;
        cmd.result_buf_len = sizeof(result_buf);
        cmd.done_sem = xSemaphoreCreateBinary();
        if (cmd.done_sem == nullptr)
        {
            request->send(503, "application/json", "{\"schema\":1,\"error\":{\"code\":\"BUSY\",\"message\":\"out of memory\"}}");
            return;
        }
        if (!shared_state::postAppCommand(cmd, 200))
        {
            vSemaphoreDelete(cmd.done_sem);
            request->send(503, "application/json", "{\"schema\":1,\"error\":{\"code\":\"BUSY\",\"message\":\"command queue full\"}}");
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
        request->send(cmd.success ? 200 : 400, "application/json", result_buf);
    });
    g_server.on("/api/calibration/b/apply", HTTP_POST, [](AsyncWebServerRequest *request) {
        sendCommandAndRespond(request, shared_state::AppCommandType::kCalApplyB);
    });
    g_server.on("/api/calibration/b/discard", HTTP_POST, [](AsyncWebServerRequest *request) {
        sendCommandAndRespond(request, shared_state::AppCommandType::kCalDiscardB);
    });
    g_server.on("/api/calibration/b/reset", HTTP_POST, [](AsyncWebServerRequest *request, JsonVariant &json) {
        sendConfirmCommandAndRespond(request, json, shared_state::AppCommandType::kCalResetB);
    });

#ifdef DEBUG_GPS_INJECT
    // Bench-only synthetic GPS input (quickstart.md section 6). Only
    // compiled in when explicitly built with -D DEBUG_GPS_INJECT=1 -- never
    // reachable in a release build, so it can never be reachable on a boat.
    g_server.on("/api/debug/gps-inject", HTTP_POST, [](AsyncWebServerRequest *request, JsonVariant &json) {
        shared_state::DebugGpsInject inject;
        inject.active = true;
        inject.sog_m_s = (json["sog_kn"] | 0.0f) * 0.514444f;
        inject.cog_rad = (json["cog_deg"] | 0.0f) * (3.14159265358979323846f / 180.0f);
        inject.variation_rad = (json["variation_deg"] | 0.0f) * (3.14159265358979323846f / 180.0f);
        shared_state::setDebugGpsInject(inject);
        request->send(200, "application/json", "{\"schema\":1,\"injected\":true}");
    });
#endif

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

void broadcastCalibrationResult(const char *stage, const char *outcome, const char *reason)
{
    if (g_ws.count() == 0)
    {
        return;
    }

    JsonDocument doc;
    doc["type"] = "calibration_result";
    doc["schema"] = 1;
    doc["stage"] = stage;
    doc["outcome"] = outcome;
    doc["reason"] = reason != nullptr ? reason : nullptr;
    doc["result"].to<JsonObject>();

    char buf[256];
    size_t len = serializeJson(doc, buf, sizeof(buf));
    g_ws.textAll(buf, len);
}

}  // namespace web_api
