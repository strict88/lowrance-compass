#include "heading_transmit.h"

#include "pgn_codec.h"

namespace n2k_codec
{

uint32_t buildCanId(uint8_t priority, uint32_t pgn, uint8_t source_address, uint8_t destination)
{
    uint8_t pdu_format = static_cast<uint8_t>(pgn >> 8);
    if (pdu_format < 240)
    {
        // PDU1 (destination-specific): PGN's low byte must be 0.
        if ((pgn & 0xFFu) != 0)
        {
            return 0;
        }
        return (static_cast<uint32_t>(priority & 0x7u) << 26) | (pgn << 8) | (static_cast<uint32_t>(destination) << 8) |
               static_cast<uint32_t>(source_address);
    }
    // PDU2 (broadcast).
    return (static_cast<uint32_t>(priority & 0x7u) << 26) | (pgn << 8) | static_cast<uint32_t>(source_address);
}

bool sendVesselHeadingIfValid(CanBus &can_bus, uint8_t source_address, const heading::HeadingReading &heading,
                               bool has_bus_variation, float bus_variation_rad)
{
    if (!heading.valid)
    {
        return false;
    }

    VesselHeading127250 fields;
    fields.sid = kSidNotAvailable;
    fields.heading_rad = heading.heading_rad;
    fields.has_variation = has_bus_variation;
    fields.variation_rad = bus_variation_rad;
    fields.reference = DirectionReference::kMagnetic;

    CanFrame frame;
    frame.id = buildCanId(/*priority=*/2, /*pgn=*/127250UL, source_address);
    frame.dlc = 8;
    encodeVesselHeading127250(fields, frame.data);

    return can_bus.send(frame);
}

}  // namespace n2k_codec
