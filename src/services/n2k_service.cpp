#include "n2k_service.h"

#include <Arduino.h>

#include "n2k_codec/heading_transmit.h"
#include "n2k_codec/pgn_codec.h"
#include "services/diag_log.h"
#include "thresholds.h"

namespace
{

// NMEA 2000 default transmission intervals (canboat's TransmissionInterval,
// milliseconds).
constexpr uint32_t kPgn127250IntervalMs = 100;
constexpr uint32_t kPgn127251IntervalMs = 100;
constexpr uint32_t kPgn127257IntervalMs = 1000;

constexpr uint32_t kSourceFreshnessMaxAgeMs = static_cast<uint32_t>(thresholds::kStageCReferenceMaxAgeS * 1000.0f);

constexpr float kRadToDeg = 57.29577951308232f;

// NMEA 2000 device class 40 "Steering and Control surfaces", function 160
// "Heading Sensors" (canboat's DEVICE_CLASS/DEVICE_FUNCTION lookup tables).
constexpr unsigned char kDeviceClass = 40;
constexpr unsigned char kDeviceFunction = 160;
constexpr uint16_t kManufacturerCode = 2046;  // unassigned/proprietary -- no NMEA registration yet

uint32_t uniqueNumberFromChipId()
{
    uint64_t mac = ESP.getEfuseMac();
    return static_cast<uint32_t>(mac & 0x1FFFFFu);  // NMEA 2000 unique number is 21 bits
}

}  // namespace

N2kService::N2kService(CanBus &can_bus, Clock &clock, bool enable_self_test)
    : can_bus_(can_bus), clock_(clock), n2k_(can_bus, enable_self_test), msg_handler_(*this, n2k_)
{
}

bool N2kService::init(const char *firmware_version)
{
    n2k_.SetProductInformation("00000001",          // model serial code
                                100,                 // product code (placeholder pending NMEA registration)
                                "Lowrance Compass",  // model ID
                                firmware_version,    // software version
                                "1.0.0");            // model version

    n2k_.SetDeviceInformation(uniqueNumberFromChipId(), kDeviceFunction, kDeviceClass, kManufacturerCode);

    n2k_.SetMode(tNMEA2000::N2km_ListenAndNode, 35);  // proposed address; final address is claimed automatically
    n2k_.EnableForward(false);

    return n2k_.Open();
}

void N2kService::loop(const heading::HeadingReading &heading)
{
    n2k_.ParseMessages();

    if (n2k_.ReadResetAddressChanged())
    {
        diag_log::Line("N2K").token("address_claimed").kv("addr", static_cast<long>(n2k_.GetN2kSource())).emit();
    }

    CanBusState current_state = can_bus_.state();
    if (!has_logged_bus_state_ || current_state != last_logged_bus_state_)
    {
        const char *state_name = "RUNNING";
        switch (current_state)
        {
            case CanBusState::kRunning:
                state_name = "RUNNING";
                break;
            case CanBusState::kBusOff:
                state_name = "BUS_OFF";
                break;
            case CanBusState::kErrorPassive:
                state_name = "ERROR_PASSIVE";
                break;
            case CanBusState::kBenchMode:
                state_name = "BENCH_MODE";
                break;
        }
        diag_log::Line("N2K").token("bus").kv("state", state_name).emit();
        last_logged_bus_state_ = current_state;
        has_logged_bus_state_ = true;
    }

    uint32_t now_ms = clock_.monotonicMillis();
    transmitHeading(heading, now_ms);
    transmitRateOfTurn(heading, now_ms);
    transmitAttitude(heading, now_ms);
}

void N2kService::transmitHeading(const heading::HeadingReading &heading, uint32_t now_ms)
{
    if (now_ms - last_tx_127250_ms_ < kPgn127250IntervalMs)
    {
        return;
    }
    last_tx_127250_ms_ = now_ms;

    // Deviation is already folded into heading_rad by the pipeline's
    // DeviationCorrection step, so there is no separate live deviation value
    // left to report; n2k_codec::sendVesselHeadingIfValid() leaves it "not
    // available".
    bool sent = n2k_codec::sendVesselHeadingIfValid(can_bus_, n2k_.GetN2kSource(), heading, has_bus_variation_,
                                                     bus_variation_rad_);

    if (sent)
    {
        tx_127250_count_++;
        diag_log::Line("N2K")
            .token("tx")
            .kv("pgn", 127250L)
            .token("ok")
            .kv("hdg", static_cast<double>(heading.heading_rad * kRadToDeg))
            .emit();
    }
    else if (!heading.valid)
    {
        diag_log::Line("N2K").token("tx").kv("pgn", 127250L).token("skipped").kv("reason", "quality_gate").emit();
    }
}

void N2kService::transmitRateOfTurn(const heading::HeadingReading &heading, uint32_t now_ms)
{
    if (now_ms - last_tx_127251_ms_ < kPgn127251IntervalMs)
    {
        return;
    }
    last_tx_127251_ms_ = now_ms;

    n2k_codec::RateOfTurn127251 fields;
    fields.sid = 0xFF;
    fields.rate_rad_s = heading.rate_of_turn_rad_s;

    uint8_t bytes[8];
    n2k_codec::encodeRateOfTurn127251(fields, bytes);

    tN2kMsg msg;
    msg.Init(2, 127251UL, n2k_.GetN2kSource());
    for (uint8_t b : bytes)
    {
        msg.AddByte(b);
    }
    n2k_.SendMsg(msg);
}

void N2kService::transmitAttitude(const heading::HeadingReading &heading, uint32_t now_ms)
{
    if (now_ms - last_tx_127257_ms_ < kPgn127257IntervalMs)
    {
        return;
    }
    last_tx_127257_ms_ = now_ms;

    n2k_codec::Attitude127257 fields;
    fields.sid = 0xFF;
    fields.has_pitch = true;
    fields.pitch_rad = heading.pitch_rad;
    fields.has_roll = true;
    fields.roll_rad = heading.roll_rad;
    // Yaw is left "not available": heading is already carried by 127250.

    uint8_t bytes[8];
    n2k_codec::encodeAttitude127257(fields, bytes);

    tN2kMsg msg;
    msg.Init(3, 127257UL, n2k_.GetN2kSource());
    for (uint8_t b : bytes)
    {
        msg.AddByte(b);
    }
    n2k_.SendMsg(msg);
}

void N2kService::handleIncoming(const tN2kMsg &msg)
{
    if (msg.DataLen < 8)
    {
        return;
    }

    uint32_t now_ms = clock_.monotonicMillis();

    if (msg.PGN == 129026UL)
    {
        n2k_codec::CogSogRapid129026 fields;
        if (n2k_codec::decodeCogSogRapid129026(msg.Data, fields) && (fields.has_cog || fields.has_sog))
        {
            has_cog_sog_ = true;
            last_rx_cog_sog_ms_ = now_ms;
            if (fields.has_sog)
            {
                last_sog_m_s_ = fields.sog_m_s;
            }
            if (fields.has_cog)
            {
                last_cog_rad_ = fields.cog_rad;
            }
            if (now_ms - last_log_129026_ms_ >= 1000)
            {
                last_log_129026_ms_ = now_ms;
                diag_log::Line("N2K")
                    .token("rx")
                    .kv("pgn", 129026L)
                    .kv("sog", static_cast<double>(fields.sog_m_s))
                    .kv("cog", static_cast<double>(fields.cog_rad * kRadToDeg))
                    .emit();
            }
        }
    }
    else if (msg.PGN == 127258UL)
    {
        n2k_codec::MagneticVariation127258 fields;
        if (n2k_codec::decodeMagneticVariation127258(msg.Data, fields) && fields.has_variation)
        {
            has_bus_variation_ = true;
            bus_variation_rad_ = fields.variation_rad;
            last_rx_variation_ms_ = now_ms;
        }
    }
}

bool N2kService::cogSogSourcePresent() const
{
    return has_cog_sog_ && (clock_.monotonicMillis() - last_rx_cog_sog_ms_) <= kSourceFreshnessMaxAgeMs;
}

bool N2kService::variationSourcePresent() const
{
    return has_bus_variation_ && (clock_.monotonicMillis() - last_rx_variation_ms_) <= kSourceFreshnessMaxAgeMs;
}
