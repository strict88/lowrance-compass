// On-target Unity test (T127): using real NVS storage (not the native fake),
// pre-populate four independent record namespaces with valid records, then
// corrupt one via a direct putBytes() overwrite (equivalent to on-flash
// corruption -- record_envelope::load() cannot distinguish "corrupted before
// this boot" from "corrupted just now", so no literal power-cycle is needed
// to exercise the same code path data-model.md sec5 describes for boot-time
// loading), and confirm only the corrupted record resets to default while
// the other three -- each pre-populated with valid data -- are left
// untouched (FR-045/046).
#include <Arduino.h>
#include <unity.h>

#include <cstring>

#include "drivers/kv_store/kv_store_nvs.h"
#include "drivers/kv_store/record_envelope.h"

namespace
{

struct DummyPayload
{
    uint32_t a;
    float b;
};

// Distinct namespaces from the production records ("cal_a"/"cal_b"/"cal_c"/
// "net") so this test never touches a real saved calibration/setting.
NvsKeyValueStore g_store_1("t_rec1");
NvsKeyValueStore g_store_2("t_rec2");
NvsKeyValueStore g_store_3("t_rec3");
NvsKeyValueStore g_store_4("t_rec4");

void resetAllFour()
{
    record_envelope::resetToDefault(g_store_1);
    record_envelope::resetToDefault(g_store_2);
    record_envelope::resetToDefault(g_store_3);
    record_envelope::resetToDefault(g_store_4);
}

}  // namespace

void setUp(void) {}
void tearDown(void) {}

void test_four_independent_records_round_trip_on_real_nvs(void)
{
    resetAllFour();

    DummyPayload p1{1, 1.0f}, p2{2, 2.0f}, p3{3, 3.0f}, p4{4, 4.0f};
    TEST_ASSERT_TRUE(record_envelope::save(g_store_1, 1, reinterpret_cast<const uint8_t *>(&p1), sizeof(p1)));
    TEST_ASSERT_TRUE(record_envelope::save(g_store_2, 1, reinterpret_cast<const uint8_t *>(&p2), sizeof(p2)));
    TEST_ASSERT_TRUE(record_envelope::save(g_store_3, 1, reinterpret_cast<const uint8_t *>(&p3), sizeof(p3)));
    TEST_ASSERT_TRUE(record_envelope::save(g_store_4, 1, reinterpret_cast<const uint8_t *>(&p4), sizeof(p4)));

    DummyPayload out{};
    TEST_ASSERT_TRUE(record_envelope::load(g_store_1, 1, reinterpret_cast<uint8_t *>(&out), sizeof(out)).status ==
                      record_envelope::Status::kOk);
    TEST_ASSERT_EQUAL_UINT32(1u, out.a);
}

void test_corrupting_one_record_resets_only_that_one_others_survive(void)
{
    resetAllFour();

    DummyPayload p1{10, 1.0f}, p2{20, 2.0f}, p3{30, 3.0f}, p4{40, 4.0f};
    TEST_ASSERT_TRUE(record_envelope::save(g_store_1, 1, reinterpret_cast<const uint8_t *>(&p1), sizeof(p1)));
    TEST_ASSERT_TRUE(record_envelope::save(g_store_2, 1, reinterpret_cast<const uint8_t *>(&p2), sizeof(p2)));
    TEST_ASSERT_TRUE(record_envelope::save(g_store_3, 1, reinterpret_cast<const uint8_t *>(&p3), sizeof(p3)));
    TEST_ASSERT_TRUE(record_envelope::save(g_store_4, 1, reinterpret_cast<const uint8_t *>(&p4), sizeof(p4)));

    // Corrupt only store 2's active slot ("s0", since it was the first-ever
    // save for that namespace) by overwriting one payload byte directly.
    g_store_2.begin(false);
    uint8_t buf[64];
    size_t len = g_store_2.getBytesLength("s0");
    TEST_ASSERT_TRUE(len > 0 && len <= sizeof(buf));
    g_store_2.getBytes("s0", buf, len);
    buf[5] ^= 0xFF;  // flip a payload byte -> CRC no longer matches
    g_store_2.putBytes("s0", buf, len);
    g_store_2.end();

    DummyPayload out1{}, out2{}, out3{}, out4{};
    auto r1 = record_envelope::load(g_store_1, 1, reinterpret_cast<uint8_t *>(&out1), sizeof(out1));
    auto r2 = record_envelope::load(g_store_2, 1, reinterpret_cast<uint8_t *>(&out2), sizeof(out2));
    auto r3 = record_envelope::load(g_store_3, 1, reinterpret_cast<uint8_t *>(&out3), sizeof(out3));
    auto r4 = record_envelope::load(g_store_4, 1, reinterpret_cast<uint8_t *>(&out4), sizeof(out4));

    TEST_ASSERT_TRUE(r1.status == record_envelope::Status::kOk);
    TEST_ASSERT_EQUAL_UINT32(10u, out1.a);
    TEST_ASSERT_TRUE(r2.status == record_envelope::Status::kCrcMismatch);
    TEST_ASSERT_TRUE(r3.status == record_envelope::Status::kOk);
    TEST_ASSERT_EQUAL_UINT32(30u, out3.a);
    TEST_ASSERT_TRUE(r4.status == record_envelope::Status::kOk);
    TEST_ASSERT_EQUAL_UINT32(40u, out4.a);

    // Explicitly reset the corrupted one (as the boot-time load*() wrappers
    // do internally) and confirm it now reads back as absent, while an
    // unrelated record is still completely unaffected.
    record_envelope::resetToDefault(g_store_2);
    auto r2_after = record_envelope::load(g_store_2, 1, reinterpret_cast<uint8_t *>(&out2), sizeof(out2));
    TEST_ASSERT_TRUE(r2_after.status == record_envelope::Status::kAbsent);
    auto r3_after = record_envelope::load(g_store_3, 1, reinterpret_cast<uint8_t *>(&out3), sizeof(out3));
    TEST_ASSERT_TRUE(r3_after.status == record_envelope::Status::kOk);
    TEST_ASSERT_EQUAL_UINT32(30u, out3.a);

    resetAllFour();
}

void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_four_independent_records_round_trip_on_real_nvs);
    RUN_TEST(test_corrupting_one_record_resets_only_that_one_others_survive);
    UNITY_END();
}

void loop() {}
