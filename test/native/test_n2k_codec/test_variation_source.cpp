#include <unity.h>

#include "n2k_codec/variation.h"
#include "thresholds.h"

void test_bus_variation_preferred_when_fresh(void)
{
    n2k_codec::VariationSource source;
    source.bus_ever_received = true;
    source.bus_variation_rad = -0.14f;
    source.bus_age_s = 0.5f;
    source.manual_available = true;
    source.manual_variation_rad = 0.2f;

    float out = 0.0f;
    TEST_ASSERT_TRUE(n2k_codec::resolveVariation(source, out));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -0.14f, out);
}

void test_manual_used_when_bus_never_received(void)
{
    n2k_codec::VariationSource source;
    source.manual_available = true;
    source.manual_variation_rad = 0.2f;

    float out = 0.0f;
    TEST_ASSERT_TRUE(n2k_codec::resolveVariation(source, out));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.2f, out);
}

void test_manual_used_when_bus_value_is_stale(void)
{
    n2k_codec::VariationSource source;
    source.bus_ever_received = true;
    source.bus_variation_rad = -0.14f;
    source.bus_age_s = thresholds::kStageCReferenceMaxAgeS + 1.0f;
    source.manual_available = true;
    source.manual_variation_rad = 0.2f;

    float out = 0.0f;
    TEST_ASSERT_TRUE(n2k_codec::resolveVariation(source, out));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.2f, out);
}

void test_bus_value_exactly_at_max_age_still_counts_as_fresh(void)
{
    n2k_codec::VariationSource source;
    source.bus_ever_received = true;
    source.bus_variation_rad = -0.14f;
    source.bus_age_s = thresholds::kStageCReferenceMaxAgeS;

    float out = 0.0f;
    TEST_ASSERT_TRUE(n2k_codec::resolveVariation(source, out));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -0.14f, out);
}

void test_neither_available_fails(void)
{
    n2k_codec::VariationSource source;
    float out = 0.0f;
    TEST_ASSERT_FALSE(n2k_codec::resolveVariation(source, out));
}
