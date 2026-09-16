#include <unity.h>

#include <cstring>

#include "../fakes/fake_kv_store.h"
#include "settings/network_settings.h"

// FR-047 (quoted): "the most recently received valid save completes and
// persists, and a client whose save was superseded MUST see the current,
// now-saved value the next time it loads Settings." AppTask processes
// commands serially from one queue, so "concurrent" saves are really two
// sequential saves in receipt order; last-write-wins falls directly out of
// record_envelope's active-slot-pointer design (already covered generically
// in test_record_envelope.cpp) -- this test exercises the same property at
// the NetworkSettings/SSID layer specifically.

void test_second_save_wins_and_first_clients_next_load_sees_it(void)
{
    FakeKeyValueStore store;

    settings::NetworkSettings first;
    std::strncpy(first.ssid, "BoatWifi-A", sizeof(first.ssid) - 1);
    TEST_ASSERT_TRUE(settings::saveNetworkSettings(store, first));

    settings::NetworkSettings second;
    std::strncpy(second.ssid, "BoatWifi-B", sizeof(second.ssid) - 1);
    TEST_ASSERT_TRUE(settings::saveNetworkSettings(store, second));

    settings::NetworkSettings loaded{};
    TEST_ASSERT_TRUE(settings::loadNetworkSettings(store, loaded));
    TEST_ASSERT_EQUAL_STRING("BoatWifi-B", loaded.ssid);
}

void test_an_invalid_second_save_does_not_supersede_the_valid_first(void)
{
    FakeKeyValueStore store;

    settings::NetworkSettings first;
    std::strncpy(first.ssid, "BoatWifi-A", sizeof(first.ssid) - 1);
    TEST_ASSERT_TRUE(settings::saveNetworkSettings(store, first));

    settings::NetworkSettings invalid;
    std::strncpy(invalid.ssid, " leading space", sizeof(invalid.ssid) - 1);
    TEST_ASSERT_FALSE(settings::saveNetworkSettings(store, invalid));

    settings::NetworkSettings loaded{};
    TEST_ASSERT_TRUE(settings::loadNetworkSettings(store, loaded));
    TEST_ASSERT_EQUAL_STRING("BoatWifi-A", loaded.ssid);
}
