#pragma once

#include <ArduinoJson.h>

// The guide content model (FR-008 through FR-011): validates the JSON shape
// GET /api/guide serves (contracts/rest-api.md) -- what/why, before-you-start
// checklist, numbered illustrated steps, duration estimate, success
// indicator, failure help -- independent of calibration logic. Kept as one
// content payload so it can be edited without touching procedure logic.
//
// Pure logic, native-testable: this module only validates an already-parsed
// JSON document (or a JSON string, for convenience). Reading
// data/guide/guide_content.json off LittleFS is hardware-only and lives in
// web_api.cpp, which calls parseFromJsonString() with the file's contents.
namespace guide
{

// Parses `json_text` into `out` and validates it has at least a "schema"
// field and a "stages.a" object with the FR-008 fields (before_you_start,
// steps, duration_estimate, success_looks_like, if_it_fails). Stages "b"/"c"
// are optional (added by their own later phases). Returns false (leaving
// `out` in an unspecified state) if parsing fails or required fields are
// missing.
bool parseFromJsonString(const char *json_text, JsonDocument &out);

}  // namespace guide
