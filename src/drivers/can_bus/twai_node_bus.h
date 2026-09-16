#pragma once

#include <esp_twai.h>
#include <esp_twai_onchip.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "can_bus.h"

// CanBus over ESP-IDF 5.5's node-based TWAI API (esp_twai.h /
// esp_twai_onchip.h), per research.md §3: no maintained S3-correct bridge
// exists for ttlappalainen/NMEA2000's own tNMEA2000_esp32, so this talks to
// the hardware directly. Wired to pin_config.h's TWAI TX/RX. RX is
// callback-driven (the node API only allows twai_node_receive_from_isr()
// from inside the on_rx_done ISR callback), so received frames are copied
// into a small FreeRTOS queue there and drained by receive() from task
// context.
class TwaiNodeBus : public CanBus
{
public:
    bool init(bool enable_self_test) override;
    bool send(const CanFrame &frame) override;
    bool receive(CanFrame &out) override;
    CanBusState state() const override { return state_; }
    bool recover() override;
    uint32_t txErrorCount() const override;
    uint32_t rxErrorCount() const override;

private:
    static bool onRxDoneStatic(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx);
    static bool onStateChangeStatic(twai_node_handle_t handle, const twai_state_change_event_data_t *edata, void *user_ctx);
    static bool onErrorStatic(twai_node_handle_t handle, const twai_error_event_data_t *edata, void *user_ctx);

    bool onRxDone(twai_node_handle_t handle);
    bool onStateChange(const twai_state_change_event_data_t *edata);

    twai_node_handle_t node_ = nullptr;
    QueueHandle_t rx_queue_ = nullptr;
    bool self_test_ = false;
    volatile CanBusState state_ = CanBusState::kRunning;
    volatile uint32_t error_event_count_ = 0;
};
