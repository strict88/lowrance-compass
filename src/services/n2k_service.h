#pragma once

#include <NMEA2000.h>

#include "drivers/can_bus/can_bus.h"
#include "drivers/can_bus/tnmea2000_can_bus.h"
#include "drivers/clock/clock.h"
#include "heading/pipeline.h"

// Wraps ttlappalainen/NMEA2000 + CanBus: configures the base node identity
// (product/config info, address claim, ISO request/PGN list/heartbeat --
// all handled by the library once Open()/ParseMessages() run, per
// constitution Principle II), transmits 127250/127251/127257 from the
// current HeadingReading at NMEA 2000 default rates (127250 skipped per
// FR-041 whenever HeadingReading.valid is false), and parses incoming
// 129026/127258 into a bus-status snapshot. (129029 GNSS Position Data also
// carries a variation field per plan.md's "optionally 129029" -- deferred;
// 127258 alone already satisfies FR-024's bus-variation-when-available
// requirement.)
class N2kService
{
public:
    N2kService(CanBus &can_bus, Clock &clock, bool enable_self_test);

    bool init(const char *firmware_version);

    // Drives address claim / incoming message parsing and transmits at NMEA
    // 2000 default rates. Call on schedule from N2kTask.
    void loop(const heading::HeadingReading &heading);

    CanBusState busState() const { return can_bus_.state(); }
    uint32_t txPgn127250Count() const { return tx_127250_count_; }
    uint8_t sourceAddress() const { return n2k_.GetN2kSource(); }

    bool cogSogSourcePresent() const;
    bool variationSourcePresent() const;
    bool hasBusVariation() const { return has_bus_variation_; }
    float busVariationRad() const { return bus_variation_rad_; }

private:
    class IncomingMsgHandler : public tNMEA2000::tMsgHandler
    {
    public:
        IncomingMsgHandler(N2kService &owner, tNMEA2000 &n2k) : tNMEA2000::tMsgHandler(0, &n2k), owner_(owner) {}
        void HandleMsg(const tN2kMsg &msg) override { owner_.handleIncoming(msg); }

    private:
        N2kService &owner_;
    };

    void transmitHeading(const heading::HeadingReading &heading, uint32_t now_ms);
    void transmitRateOfTurn(const heading::HeadingReading &heading, uint32_t now_ms);
    void transmitAttitude(const heading::HeadingReading &heading, uint32_t now_ms);
    void handleIncoming(const tN2kMsg &msg);

    CanBus &can_bus_;
    Clock &clock_;
    TNmea2000CanBus n2k_;
    IncomingMsgHandler msg_handler_;

    uint32_t last_tx_127250_ms_ = 0;
    uint32_t last_tx_127251_ms_ = 0;
    uint32_t last_tx_127257_ms_ = 0;
    uint32_t tx_127250_count_ = 0;

    bool has_logged_bus_state_ = false;
    CanBusState last_logged_bus_state_ = CanBusState::kRunning;

    uint32_t last_rx_cog_sog_ms_ = 0;
    bool has_cog_sog_ = false;

    uint32_t last_rx_variation_ms_ = 0;
    bool has_bus_variation_ = false;
    float bus_variation_rad_ = 0.0f;
};
