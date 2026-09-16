#include "guide_content.h"

namespace guide
{

namespace
{

bool validateStage(JsonVariantConst stage)
{
    if (stage.isNull() || !stage.is<JsonObjectConst>())
    {
        return false;
    }
    if (!stage["before_you_start"].is<JsonArrayConst>())
    {
        return false;
    }
    if (!stage["steps"].is<JsonArrayConst>())
    {
        return false;
    }
    for (JsonVariantConst step : stage["steps"].as<JsonArrayConst>())
    {
        if (!step["title"].is<const char *>() || !step["detail"].is<const char *>() ||
            !step["illustration_svg_id"].is<const char *>())
        {
            return false;
        }
    }
    if (!stage["duration_estimate"].is<const char *>())
    {
        return false;
    }
    if (!stage["success_looks_like"].is<const char *>())
    {
        return false;
    }
    if (!stage["if_it_fails"].is<JsonArrayConst>())
    {
        return false;
    }
    return true;
}

}  // namespace

bool parseFromJsonString(const char *json_text, JsonDocument &out)
{
    DeserializationError err = deserializeJson(out, json_text);
    if (err)
    {
        return false;
    }

    if (!out["schema"].is<int>())
    {
        return false;
    }

    JsonVariantConst stages = out["stages"];
    if (stages.isNull() || !stages.is<JsonObjectConst>())
    {
        return false;
    }

    return validateStage(stages["a"]);
}

}  // namespace guide
