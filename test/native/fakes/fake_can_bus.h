#pragma once

#include <deque>
#include <vector>

#include "drivers/can_bus/can_bus.h"

// In-memory CanBus for native tests: sent frames are captured for
// inspection, and frames to "receive" are queued explicitly by the test.
class FakeCanBus : public CanBus
{
public:
    bool init(bool enable_self_test) override
    {
        initialized_ = true;
        self_test_ = enable_self_test;
        state_ = enable_self_test ? CanBusState::kBenchMode : CanBusState::kRunning;
        return true;
    }

    bool send(const CanFrame &frame) override
    {
        if (!send_should_succeed_)
        {
            return false;
        }
        sent_frames_.push_back(frame);
        return true;
    }

    bool receive(CanFrame &out) override
    {
        if (rx_queue_.empty())
        {
            return false;
        }
        out = rx_queue_.front();
        rx_queue_.pop_front();
        return true;
    }

    CanBusState state() const override { return state_; }

    bool recover() override
    {
        if (state_ == CanBusState::kBusOff)
        {
            state_ = self_test_ ? CanBusState::kBenchMode : CanBusState::kRunning;
        }
        return true;
    }

    uint32_t txErrorCount() const override { return tx_error_count_; }
    uint32_t rxErrorCount() const override { return rx_error_count_; }

    // Test-only helpers.
    const std::vector<CanFrame> &sentFrames() const { return sent_frames_; }
    void queueReceivedFrame(const CanFrame &frame) { rx_queue_.push_back(frame); }
    void setStateForTest(CanBusState state) { state_ = state; }
    void setSendShouldSucceedForTest(bool ok) { send_should_succeed_ = ok; }
    void setErrorCountsForTest(uint32_t tx, uint32_t rx)
    {
        tx_error_count_ = tx;
        rx_error_count_ = rx;
    }
    bool wasInitialized() const { return initialized_; }

private:
    bool initialized_ = false;
    bool self_test_ = false;
    bool send_should_succeed_ = true;
    CanBusState state_ = CanBusState::kRunning;
    uint32_t tx_error_count_ = 0;
    uint32_t rx_error_count_ = 0;
    std::vector<CanFrame> sent_frames_;
    std::deque<CanFrame> rx_queue_;
};
