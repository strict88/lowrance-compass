#include "twai_node_bus.h"

#include <cstring>

#include "pin_config.h"

namespace
{
constexpr uint32_t kBitrateBps = 250000;  // NMEA 2000 fixed bus speed
constexpr uint32_t kTxQueueDepth = 16;
constexpr UBaseType_t kRxQueueDepth = 32;
}  // namespace

bool TwaiNodeBus::init(bool enable_self_test)
{
    self_test_ = enable_self_test;

    rx_queue_ = xQueueCreate(kRxQueueDepth, sizeof(CanFrame));
    if (rx_queue_ == nullptr)
    {
        return false;
    }

    twai_onchip_node_config_t node_config = {};
    node_config.io_cfg.tx = static_cast<gpio_num_t>(pins::kTwaiTx);
    node_config.io_cfg.rx = static_cast<gpio_num_t>(pins::kTwaiRx);
    node_config.io_cfg.quanta_clk_out = GPIO_NUM_NC;
    node_config.io_cfg.bus_off_indicator = GPIO_NUM_NC;
    node_config.bit_timing.bitrate = kBitrateBps;
    node_config.tx_queue_depth = kTxQueueDepth;
    node_config.intr_priority = 0;
    node_config.fail_retry_cnt = -1;  // retry forever, standard CAN behavior
    node_config.flags.enable_self_test = enable_self_test ? 1 : 0;

    if (twai_new_node_onchip(&node_config, &node_) != ESP_OK)
    {
        return false;
    }

    twai_event_callbacks_t callbacks = {};
    callbacks.on_rx_done = &TwaiNodeBus::onRxDoneStatic;
    callbacks.on_state_change = &TwaiNodeBus::onStateChangeStatic;
    callbacks.on_error = &TwaiNodeBus::onErrorStatic;
    if (twai_node_register_event_callbacks(node_, &callbacks, this) != ESP_OK)
    {
        return false;
    }

    if (twai_node_enable(node_) != ESP_OK)
    {
        return false;
    }

    state_ = enable_self_test ? CanBusState::kBenchMode : CanBusState::kRunning;
    return true;
}

bool TwaiNodeBus::send(const CanFrame &frame)
{
    twai_frame_t tx = {};
    tx.header.id = frame.id;
    tx.header.dlc = frame.dlc;
    tx.header.ide = 1;  // NMEA 2000 always uses 29-bit extended identifiers
    tx.buffer = const_cast<uint8_t *>(frame.data);
    tx.buffer_len = frame.dlc;

    return twai_node_transmit(node_, &tx, 0) == ESP_OK;  // 0 = non-blocking
}

bool TwaiNodeBus::receive(CanFrame &out)
{
    return xQueueReceive(rx_queue_, &out, 0) == pdTRUE;  // 0 = non-blocking
}

bool TwaiNodeBus::recover()
{
    return twai_node_recover(node_) == ESP_OK;
}

uint32_t TwaiNodeBus::txErrorCount() const
{
    twai_node_status_t status = {};
    if (twai_node_get_info(node_, &status, nullptr) != ESP_OK)
    {
        return 0;
    }
    return status.tx_error_count;
}

uint32_t TwaiNodeBus::rxErrorCount() const
{
    twai_node_status_t status = {};
    if (twai_node_get_info(node_, &status, nullptr) != ESP_OK)
    {
        return 0;
    }
    return status.rx_error_count;
}

bool TwaiNodeBus::onRxDone(twai_node_handle_t handle)
{
    uint8_t buf[8];
    twai_frame_t rx_frame = {};
    rx_frame.buffer = buf;
    rx_frame.buffer_len = sizeof(buf);

    if (twai_node_receive_from_isr(handle, &rx_frame) != ESP_OK)
    {
        return false;
    }

    CanFrame frame;
    frame.id = rx_frame.header.id;
    frame.dlc = static_cast<uint8_t>(rx_frame.header.dlc > 8 ? 8 : rx_frame.header.dlc);
    memcpy(frame.data, buf, frame.dlc);

    BaseType_t higher_priority_task_woken = pdFALSE;
    xQueueSendFromISR(rx_queue_, &frame, &higher_priority_task_woken);
    return higher_priority_task_woken == pdTRUE;
}

bool TwaiNodeBus::onStateChange(const twai_state_change_event_data_t *edata)
{
    if (edata->new_sta == TWAI_ERROR_BUS_OFF)
    {
        state_ = CanBusState::kBusOff;
    }
    else if (edata->new_sta == TWAI_ERROR_PASSIVE)
    {
        state_ = CanBusState::kErrorPassive;
    }
    else
    {
        // ACTIVE or WARNING: healthy enough to communicate.
        state_ = self_test_ ? CanBusState::kBenchMode : CanBusState::kRunning;
    }
    return false;
}

bool TwaiNodeBus::onRxDoneStatic(twai_node_handle_t handle, const twai_rx_done_event_data_t * /*edata*/, void *user_ctx)
{
    return static_cast<TwaiNodeBus *>(user_ctx)->onRxDone(handle);
}

bool TwaiNodeBus::onStateChangeStatic(twai_node_handle_t /*handle*/, const twai_state_change_event_data_t *edata,
                                       void *user_ctx)
{
    return static_cast<TwaiNodeBus *>(user_ctx)->onStateChange(edata);
}

bool TwaiNodeBus::onErrorStatic(twai_node_handle_t /*handle*/, const twai_error_event_data_t * /*edata*/, void *user_ctx)
{
    auto *self = static_cast<TwaiNodeBus *>(user_ctx);
    self->error_event_count_ = self->error_event_count_ + 1;
    return false;
}
