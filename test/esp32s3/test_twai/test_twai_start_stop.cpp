// On-target Unity test: TWAI start, stop, and (as much as is testable
// without physically faulting the bus) bus-off recovery, in self-test mode.
// Genuine bus-off recovery needs a real bus fault and is an explicit manual
// checklist item (checklists/manual-verification.md), not exercised here.
#include <Arduino.h>
#include <unity.h>

#include "drivers/can_bus/twai_node_bus.h"

void setUp(void) {}
void tearDown(void) {}

void test_self_test_mode_start_and_send(void)
{
    TwaiNodeBus bus;
    TEST_ASSERT_TRUE(bus.init(/*enable_self_test=*/true));
    TEST_ASSERT_TRUE(bus.state() == CanBusState::kBenchMode);

    CanFrame frame;
    frame.id = 0x1FF0000UL | 35UL;  // arbitrary extended ID for this smoke test
    frame.dlc = 8;
    for (int i = 0; i < 8; ++i)
    {
        frame.data[i] = static_cast<uint8_t>(i);
    }

    TEST_ASSERT_TRUE(bus.send(frame));

    bus.deinit();
}

void test_recover_is_safe_when_not_bus_off(void)
{
    TwaiNodeBus bus;
    TEST_ASSERT_TRUE(bus.init(true));
    TEST_ASSERT_FALSE(bus.state() == CanBusState::kBusOff);

    // twai_node_recover() is only valid from bus-off; calling it otherwise
    // must be a safe no-op (returns false), never a crash.
    bool result = bus.recover();
    TEST_ASSERT_FALSE(result);

    bus.deinit();
}

void test_stop_then_restart(void)
{
    TwaiNodeBus bus;
    TEST_ASSERT_TRUE(bus.init(true));
    bus.deinit();

    TEST_ASSERT_TRUE(bus.init(true));
    bus.deinit();
}

void setup()
{
    delay(2000);  // let the serial monitor attach
    UNITY_BEGIN();
    RUN_TEST(test_self_test_mode_start_and_send);
    RUN_TEST(test_recover_is_safe_when_not_bus_off);
    RUN_TEST(test_stop_then_restart);
    UNITY_END();
}

void loop() {}
