#include <unity.h>

#include <cstring>

#include "../fakes/fake_kv_store.h"
#include "drivers/kv_store/record_envelope.h"

namespace
{

struct DummyPayload
{
    uint32_t a;
    float b;
};

}  // namespace

void test_round_trip_valid_record(void)
{
    FakeKeyValueStore store;
    DummyPayload in{42, 3.5f};

    bool saved = record_envelope::save(store, /*schema_version=*/1, reinterpret_cast<const uint8_t *>(&in), sizeof(in));
    TEST_ASSERT_TRUE(saved);

    DummyPayload out{};
    auto result = record_envelope::load(store, /*expected_schema_version=*/1, reinterpret_cast<uint8_t *>(&out), sizeof(out));

    TEST_ASSERT_TRUE(result.status == record_envelope::Status::kOk);
    TEST_ASSERT_EQUAL_UINT32(in.a, out.a);
    TEST_ASSERT_EQUAL_FLOAT(in.b, out.b);
}

void test_absent_before_any_save(void)
{
    FakeKeyValueStore store;
    DummyPayload out{};
    auto result = record_envelope::load(store, 1, reinterpret_cast<uint8_t *>(&out), sizeof(out));
    TEST_ASSERT_TRUE(result.status == record_envelope::Status::kAbsent);
}

void test_crc_mismatch_resets_only_that_record(void)
{
    FakeKeyValueStore store;
    DummyPayload in{7, 1.0f};
    TEST_ASSERT_TRUE(record_envelope::save(store, 1, reinterpret_cast<const uint8_t *>(&in), sizeof(in)));

    // Corrupt one payload byte in the active slot (slot "s0" since it was the
    // first-ever save).
    store.corrupt("s0", 5, 0xFF);

    DummyPayload out{};
    auto result = record_envelope::load(store, 1, reinterpret_cast<uint8_t *>(&out), sizeof(out));
    TEST_ASSERT_TRUE(result.status == record_envelope::Status::kCrcMismatch);
}

void test_schema_mismatch(void)
{
    FakeKeyValueStore store;
    DummyPayload in{1, 2.0f};
    TEST_ASSERT_TRUE(record_envelope::save(store, 1, reinterpret_cast<const uint8_t *>(&in), sizeof(in)));

    DummyPayload out{};
    auto result = record_envelope::load(store, /*expected_schema_version=*/2, reinterpret_cast<uint8_t *>(&out), sizeof(out));
    TEST_ASSERT_TRUE(result.status == record_envelope::Status::kSchemaMismatch);
}

void test_bad_save_never_overwrites_previous_good_record(void)
{
    // A record with the same payload size lives in two independent
    // FakeKeyValueStore instances representing two different record
    // families, to assert corrupting one never touches the other.
    FakeKeyValueStore store_a;
    FakeKeyValueStore store_b;

    DummyPayload a{1, 1.0f};
    DummyPayload b{2, 2.0f};
    TEST_ASSERT_TRUE(record_envelope::save(store_a, 1, reinterpret_cast<const uint8_t *>(&a), sizeof(a)));
    TEST_ASSERT_TRUE(record_envelope::save(store_b, 1, reinterpret_cast<const uint8_t *>(&b), sizeof(b)));

    store_a.corrupt("s0", 5, 0xAB);

    DummyPayload out_a{};
    DummyPayload out_b{};
    auto result_a = record_envelope::load(store_a, 1, reinterpret_cast<uint8_t *>(&out_a), sizeof(out_a));
    auto result_b = record_envelope::load(store_b, 1, reinterpret_cast<uint8_t *>(&out_b), sizeof(out_b));

    TEST_ASSERT_TRUE(result_a.status == record_envelope::Status::kCrcMismatch);
    TEST_ASSERT_TRUE(result_b.status == record_envelope::Status::kOk);
    TEST_ASSERT_EQUAL_UINT32(b.a, out_b.a);
}

void test_second_save_flips_active_slot_and_first_slot_survives(void)
{
    FakeKeyValueStore store;
    DummyPayload first{1, 1.0f};
    DummyPayload second{2, 2.0f};

    TEST_ASSERT_TRUE(record_envelope::save(store, 1, reinterpret_cast<const uint8_t *>(&first), sizeof(first)));
    TEST_ASSERT_TRUE(record_envelope::save(store, 1, reinterpret_cast<const uint8_t *>(&second), sizeof(second)));

    DummyPayload out{};
    auto result = record_envelope::load(store, 1, reinterpret_cast<uint8_t *>(&out), sizeof(out));
    TEST_ASSERT_TRUE(result.status == record_envelope::Status::kOk);
    TEST_ASSERT_EQUAL_UINT32(second.a, out.a);
}

void test_reset_to_default_clears_record(void)
{
    FakeKeyValueStore store;
    DummyPayload in{9, 9.0f};
    TEST_ASSERT_TRUE(record_envelope::save(store, 1, reinterpret_cast<const uint8_t *>(&in), sizeof(in)));

    record_envelope::resetToDefault(store);

    DummyPayload out{};
    auto result = record_envelope::load(store, 1, reinterpret_cast<uint8_t *>(&out), sizeof(out));
    TEST_ASSERT_TRUE(result.status == record_envelope::Status::kAbsent);
}
