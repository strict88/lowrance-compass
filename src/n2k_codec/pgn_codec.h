#pragma once

#include <cstdint>

// Pure encode/decode for the PGNs this feature transmits/receives, plus unit
// conversion to/from radians and rad/s (constitution Principle I). Field
// layouts (byte offsets, resolutions, "not available" sentinels) match the
// canonical NMEA 2000 definitions (canboat's public PGN database):
//   127250 Vessel Heading, 127251 Rate of Turn, 127257 Attitude,
//   129026 COG & SOG Rapid Update, 127258 Magnetic Variation.
// Every message here is a fixed 8-byte single-frame PGN -- no fast-packet
// framing needed. Pure logic, native-testable.
namespace n2k_codec
{

constexpr uint8_t kSidNotAvailable = 0xFF;

enum class DirectionReference : uint8_t
{
    kTrue = 0,
    kMagnetic = 1,
    kError = 2,
    kUnavailable = 3,
};

// ---------------------------------------------------------------------------
// PGN 127250 - Vessel Heading
// ---------------------------------------------------------------------------
struct VesselHeading127250
{
    uint8_t sid = kSidNotAvailable;
    float heading_rad = 0.0f;  // [0, 2*pi), always present

    bool has_deviation = false;
    float deviation_rad = 0.0f;  // (-pi, pi]

    bool has_variation = false;
    float variation_rad = 0.0f;  // (-pi, pi]

    DirectionReference reference = DirectionReference::kMagnetic;
};

void encodeVesselHeading127250(const VesselHeading127250 &in, uint8_t out[8]);
bool decodeVesselHeading127250(const uint8_t in[8], VesselHeading127250 &out);

// ---------------------------------------------------------------------------
// PGN 127251 - Rate of Turn
// ---------------------------------------------------------------------------
struct RateOfTurn127251
{
    uint8_t sid = kSidNotAvailable;
    float rate_rad_s = 0.0f;
};

void encodeRateOfTurn127251(const RateOfTurn127251 &in, uint8_t out[8]);
bool decodeRateOfTurn127251(const uint8_t in[8], RateOfTurn127251 &out);

// ---------------------------------------------------------------------------
// PGN 127257 - Attitude
// ---------------------------------------------------------------------------
struct Attitude127257
{
    uint8_t sid = kSidNotAvailable;

    bool has_yaw = false;
    float yaw_rad = 0.0f;  // (-pi, pi]

    bool has_pitch = false;
    float pitch_rad = 0.0f;  // (-pi, pi]

    bool has_roll = false;
    float roll_rad = 0.0f;  // (-pi, pi]
};

void encodeAttitude127257(const Attitude127257 &in, uint8_t out[8]);
bool decodeAttitude127257(const uint8_t in[8], Attitude127257 &out);

// ---------------------------------------------------------------------------
// PGN 129026 - COG & SOG, Rapid Update
// ---------------------------------------------------------------------------
struct CogSogRapid129026
{
    uint8_t sid = kSidNotAvailable;
    DirectionReference cog_reference = DirectionReference::kTrue;

    bool has_cog = false;
    float cog_rad = 0.0f;  // [0, 2*pi)

    bool has_sog = false;
    float sog_m_s = 0.0f;  // >= 0
};

void encodeCogSogRapid129026(const CogSogRapid129026 &in, uint8_t out[8]);
bool decodeCogSogRapid129026(const uint8_t in[8], CogSogRapid129026 &out);

// ---------------------------------------------------------------------------
// PGN 127258 - Magnetic Variation
// ---------------------------------------------------------------------------
struct MagneticVariation127258
{
    uint8_t sid = kSidNotAvailable;
    uint8_t source = 0;  // MAGNETIC_VARIATION lookup enum, 4 bits (0-13; 15=unavailable)

    bool has_age_of_service = false;
    uint16_t age_of_service_days = 0;

    bool has_variation = false;
    float variation_rad = 0.0f;  // (-pi, pi]
};

void encodeMagneticVariation127258(const MagneticVariation127258 &in, uint8_t out[8]);
bool decodeMagneticVariation127258(const uint8_t in[8], MagneticVariation127258 &out);

}  // namespace n2k_codec
