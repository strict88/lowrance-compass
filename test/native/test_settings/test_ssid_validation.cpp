#include <unity.h>

#include <cstring>

#include "settings/network_settings.h"

// FR-035 (quoted verbatim): "1-32 characters, no leading or trailing
// whitespace".

void test_empty_string_is_invalid(void)
{
    TEST_ASSERT_FALSE(settings::validateSsid(""));
}

void test_exactly_one_char_is_valid(void)
{
    TEST_ASSERT_TRUE(settings::validateSsid("A"));
}

void test_exactly_thirty_two_chars_is_valid(void)
{
    const char *ssid = "12345678901234567890123456789012";  // 32 chars
    TEST_ASSERT_EQUAL_UINT32(32, static_cast<uint32_t>(strlen(ssid)));
    TEST_ASSERT_TRUE(settings::validateSsid(ssid));
}

void test_thirty_three_chars_is_invalid(void)
{
    const char *ssid = "123456789012345678901234567890123";  // 33 chars
    TEST_ASSERT_EQUAL_UINT32(33, static_cast<uint32_t>(strlen(ssid)));
    TEST_ASSERT_FALSE(settings::validateSsid(ssid));
}

void test_leading_space_is_invalid(void)
{
    TEST_ASSERT_FALSE(settings::validateSsid(" MyBoat"));
}

void test_trailing_space_is_invalid(void)
{
    TEST_ASSERT_FALSE(settings::validateSsid("MyBoat "));
}

void test_valid_mixed_content_ssid(void)
{
    TEST_ASSERT_TRUE(settings::validateSsid("Lowrance-Compass_42"));
}

void test_null_pointer_is_invalid(void)
{
    TEST_ASSERT_FALSE(settings::validateSsid(nullptr));
}

void test_internal_space_is_valid(void)
{
    TEST_ASSERT_TRUE(settings::validateSsid("My Boat"));
}
