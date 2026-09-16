#pragma once

#include <cstdint>

#include "drivers/can_bus/can_bus.h"
#include "heading/pipeline.h"

// The PGN 127250 transmit gate (FR-041, constitution Principle I), factored
// out so it is testable with a FakeCanBus without depending on the full
// ttlappalainen/NMEA2000 stack (test/native/test_n2k_codec/test_transmit_gate.cpp).
// N2kService uses this for 127250 specifically; 127251/127257 go through
// tNMEA2000::SendMsg() normally, since only 127250's gating needs this level
// of isolated testability.
namespace n2k_codec
{

// Standard NMEA 2000 / J1939 29-bit CAN identifier: PDU1 (destination-
// specific, PGN low byte 0) uses `destination`; PDU2 (broadcast, PGN's PDU
// format byte >= 240) ignores it. Matches ttlappalainen/NMEA2000's own
// N2ktoCanID() formula exactly. Returns 0 for an invalid PDU1 PGN (matching
// that same convention).
uint32_t buildCanId(uint8_t priority, uint32_t pgn, uint8_t source_address, uint8_t destination = 0xFF);

// Encodes and sends PGN 127250 via `can_bus` only when `heading.valid`
// (FR-041) -- returns true if a frame was sent, false if skipped (invalid)
// or the send failed.
bool sendVesselHeadingIfValid(CanBus &can_bus, uint8_t source_address, const heading::HeadingReading &heading,
                               bool has_bus_variation, float bus_variation_rad);

}  // namespace n2k_codec
