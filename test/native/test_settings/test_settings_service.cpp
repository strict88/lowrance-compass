#include <unity.h>

#include <cstring>

#include "../fakes/fake_kv_store.h"
#include "services/settings_service.h"

void test_init_without_a_saved_record_uses_factory_default(void)
{
    FakeKeyValueStore store;
    SettingsService service(store);
    service.init();
    TEST_ASSERT_EQUAL_STRING(SettingsService::factoryDefaultSsid(), service.current().ssid);
}

void test_save_schedules_restart_after_applies_in_s_not_immediately(void)
{
    FakeKeyValueStore store;
    SettingsService service(store);
    service.init();

    settings::NetworkSettings new_settings;
    std::strncpy(new_settings.ssid, "MyBoat", sizeof(new_settings.ssid) - 1);

    TEST_ASSERT_TRUE(service.save(new_settings, /*now_s=*/100.0f, /*applies_in_s=*/5));
    TEST_ASSERT_EQUAL_STRING("MyBoat", service.current().ssid);

    TEST_ASSERT_FALSE(service.isRestartDue(104.0f));
    TEST_ASSERT_TRUE(service.isRestartDue(105.0f));
}

void test_invalid_ssid_does_not_schedule_a_restart(void)
{
    FakeKeyValueStore store;
    SettingsService service(store);
    service.init();

    settings::NetworkSettings bad;
    std::strncpy(bad.ssid, " bad", sizeof(bad.ssid) - 1);

    TEST_ASSERT_FALSE(service.save(bad, 0.0f, 5));
    TEST_ASSERT_FALSE(service.hasPendingRestart());
    TEST_ASSERT_EQUAL_STRING(SettingsService::factoryDefaultSsid(), service.current().ssid);
}

void test_clear_pending_restart_stops_it_from_firing_again(void)
{
    FakeKeyValueStore store;
    SettingsService service(store);
    service.init();

    settings::NetworkSettings new_settings;
    std::strncpy(new_settings.ssid, "MyBoat", sizeof(new_settings.ssid) - 1);
    service.save(new_settings, 0.0f, 0);

    TEST_ASSERT_TRUE(service.isRestartDue(0.0f));
    service.clearPendingRestart();
    TEST_ASSERT_FALSE(service.isRestartDue(1000.0f));
}

void test_reset_to_default_restarts_immediately(void)
{
    FakeKeyValueStore store;
    SettingsService service(store);
    service.init();

    settings::NetworkSettings new_settings;
    std::strncpy(new_settings.ssid, "MyBoat", sizeof(new_settings.ssid) - 1);
    service.save(new_settings, 0.0f, 30);
    service.clearPendingRestart();

    service.resetToDefault(50.0f);

    TEST_ASSERT_EQUAL_STRING(SettingsService::factoryDefaultSsid(), service.current().ssid);
    TEST_ASSERT_TRUE(service.isRestartDue(50.0f));
}
