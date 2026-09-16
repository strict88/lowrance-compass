#pragma once

#include <cstdint>

// One CAN frame. NMEA 2000 always uses 29-bit extended identifiers.
struct CanFrame
{
    uint32_t id = 0;
    uint8_t dlc = 0;
    uint8_t data[8] = {0, 0, 0, 0, 0, 0, 0, 0};
};

enum class CanBusState
{
    kRunning,
    kBusOff,
    kErrorPassive,
    kBenchMode,  // self-test/no-ACK mode: no other node required on the bus
};

// Hardware-adapter interface over the CAN transceiver/controller. Independent
// of TWAI specifics so it is fake-able in native tests (constitution
// Principle V).
class CanBus
{
public:
    virtual ~CanBus() = default;

    // `enable_self_test`: bench/no-ACK mode, so the device can transmit
    // without another node acknowledging (quickstart.md section 5).
    virtual bool init(bool enable_self_test) = 0;

    // Stops and releases the controller; safe to call init() again
    // afterward.
    virtual void deinit() = 0;

    // Non-blocking transmit; returns false if the frame could not be
    // accepted right now (queue full, bus off, etc.).
    virtual bool send(const CanFrame &frame) = 0;

    // Non-blocking receive; returns true and fills `out` if a frame was
    // available.
    virtual bool receive(CanFrame &out) = 0;

    virtual CanBusState state() const = 0;

    // Attempts bus-off recovery (twai_node_recover() on-target). No-op-safe
    // if not currently bus-off.
    virtual bool recover() = 0;

    virtual uint32_t txErrorCount() const = 0;
    virtual uint32_t rxErrorCount() const = 0;
};
