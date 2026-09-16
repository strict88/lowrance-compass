#include <unity.h>

#include "guide/guide_content.h"

void setUp(void) {}
void tearDown(void) {}

static const char *kValidJson =
    "{\"schema\":1,\"stages\":{\"a\":{"
    "\"what_and_why\":\"x\","
    "\"before_you_start\":[\"a\",\"b\"],"
    "\"steps\":[{\"title\":\"t\",\"detail\":\"d\",\"illustration_svg_id\":\"stage_a_step1\"}],"
    "\"duration_estimate\":\"3 min\","
    "\"success_looks_like\":\"all high\","
    "\"if_it_fails\":[\"try again\"]"
    "}}}";

static void test_valid_document_parses(void)
{
    JsonDocument doc;
    TEST_ASSERT_TRUE(guide::parseFromJsonString(kValidJson, doc));
    TEST_ASSERT_EQUAL_STRING("3 min", doc["stages"]["a"]["duration_estimate"].as<const char *>());
}

static void test_malformed_json_rejected(void)
{
    JsonDocument doc;
    TEST_ASSERT_FALSE(guide::parseFromJsonString("{not json", doc));
}

static void test_missing_stage_a_rejected(void)
{
    JsonDocument doc;
    TEST_ASSERT_FALSE(guide::parseFromJsonString("{\"schema\":1,\"stages\":{}}", doc));
}

static void test_step_missing_required_field_rejected(void)
{
    JsonDocument doc;
    const char *bad_json =
        "{\"schema\":1,\"stages\":{\"a\":{"
        "\"before_you_start\":[],"
        "\"steps\":[{\"title\":\"t\"}],"
        "\"duration_estimate\":\"3 min\","
        "\"success_looks_like\":\"x\","
        "\"if_it_fails\":[]"
        "}}}";
    TEST_ASSERT_FALSE(guide::parseFromJsonString(bad_json, doc));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_valid_document_parses);
    RUN_TEST(test_malformed_json_rejected);
    RUN_TEST(test_missing_stage_a_rejected);
    RUN_TEST(test_step_missing_required_field_rejected);
    return UNITY_END();
}
