#include "pgn_codec.h"

#include <cmath>
#include <cstring>

namespace n2k_codec
{

namespace
{

constexpr float kAngleResolutionRad = 0.0001f;
constexpr float kRateOfTurnResolutionRadS = 3.125e-8f;
constexpr float kSpeedResolutionMS = 0.01f;

constexpr uint16_t kU16Unavailable = 0xFFFF;
constexpr int16_t kI16Unavailable = 0x7FFF;
constexpr uint16_t kAgeOfServiceUnavailable = 0xFFFF;
constexpr int32_t kI32Unavailable = 0x7FFFFFFF;

void putU16LE(uint8_t *buf, int offset, uint16_t value)
{
    buf[offset] = static_cast<uint8_t>(value & 0xFF);
    buf[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

void putI16LE(uint8_t *buf, int offset, int16_t value)
{
    putU16LE(buf, offset, static_cast<uint16_t>(value));
}

void putU32LE(uint8_t *buf, int offset, uint32_t value)
{
    buf[offset] = static_cast<uint8_t>(value & 0xFF);
    buf[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    buf[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    buf[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

void putI32LE(uint8_t *buf, int offset, int32_t value)
{
    putU32LE(buf, offset, static_cast<uint32_t>(value));
}

uint16_t getU16LE(const uint8_t *buf, int offset)
{
    return static_cast<uint16_t>(buf[offset]) | (static_cast<uint16_t>(buf[offset + 1]) << 8);
}

int16_t getI16LE(const uint8_t *buf, int offset)
{
    return static_cast<int16_t>(getU16LE(buf, offset));
}

uint32_t getU32LE(const uint8_t *buf, int offset)
{
    return static_cast<uint32_t>(buf[offset]) | (static_cast<uint32_t>(buf[offset + 1]) << 8) |
           (static_cast<uint32_t>(buf[offset + 2]) << 16) | (static_cast<uint32_t>(buf[offset + 3]) << 24);
}

int32_t getI32LE(const uint8_t *buf, int offset)
{
    return static_cast<int32_t>(getU32LE(buf, offset));
}

uint16_t angleToU16(float rad)
{
    return static_cast<uint16_t>(std::lround(rad / kAngleResolutionRad));
}

float u16ToAngle(uint16_t raw)
{
    return static_cast<float>(raw) * kAngleResolutionRad;
}

int16_t angleToI16(float rad)
{
    return static_cast<int16_t>(std::lround(rad / kAngleResolutionRad));
}

float i16ToAngle(int16_t raw)
{
    return static_cast<float>(raw) * kAngleResolutionRad;
}

}  // namespace

// ---------------------------------------------------------------------------
// 127250 - Vessel Heading
// ---------------------------------------------------------------------------
void encodeVesselHeading127250(const VesselHeading127250 &in, uint8_t out[8])
{
    out[0] = in.sid;
    putU16LE(out, 1, angleToU16(in.heading_rad));
    putI16LE(out, 3, in.has_deviation ? angleToI16(in.deviation_rad) : kI16Unavailable);
    putI16LE(out, 5, in.has_variation ? angleToI16(in.variation_rad) : kI16Unavailable);
    uint8_t reference = static_cast<uint8_t>(in.reference) & 0x03u;
    out[7] = static_cast<uint8_t>(0xFCu | reference);  // bits 2-7 reserved (1)
}

bool decodeVesselHeading127250(const uint8_t in[8], VesselHeading127250 &out)
{
    out.sid = in[0];
    out.heading_rad = u16ToAngle(getU16LE(in, 1));

    int16_t dev_raw = getI16LE(in, 3);
    out.has_deviation = (dev_raw != kI16Unavailable);
    out.deviation_rad = out.has_deviation ? i16ToAngle(dev_raw) : 0.0f;

    int16_t var_raw = getI16LE(in, 5);
    out.has_variation = (var_raw != kI16Unavailable);
    out.variation_rad = out.has_variation ? i16ToAngle(var_raw) : 0.0f;

    out.reference = static_cast<DirectionReference>(in[7] & 0x03u);
    return true;
}

// ---------------------------------------------------------------------------
// 127251 - Rate of Turn
// ---------------------------------------------------------------------------
void encodeRateOfTurn127251(const RateOfTurn127251 &in, uint8_t out[8])
{
    out[0] = in.sid;
    int32_t raw = static_cast<int32_t>(std::lround(in.rate_rad_s / kRateOfTurnResolutionRadS));
    putI32LE(out, 1, raw);
    out[5] = 0xFF;
    out[6] = 0xFF;
    out[7] = 0xFF;
}

bool decodeRateOfTurn127251(const uint8_t in[8], RateOfTurn127251 &out)
{
    out.sid = in[0];
    int32_t raw = getI32LE(in, 1);
    if (raw == kI32Unavailable)
    {
        out.rate_rad_s = 0.0f;
        return false;
    }
    out.rate_rad_s = static_cast<float>(raw) * kRateOfTurnResolutionRadS;
    return true;
}

// ---------------------------------------------------------------------------
// 127257 - Attitude
// ---------------------------------------------------------------------------
void encodeAttitude127257(const Attitude127257 &in, uint8_t out[8])
{
    out[0] = in.sid;
    putI16LE(out, 1, in.has_yaw ? angleToI16(in.yaw_rad) : kI16Unavailable);
    putI16LE(out, 3, in.has_pitch ? angleToI16(in.pitch_rad) : kI16Unavailable);
    putI16LE(out, 5, in.has_roll ? angleToI16(in.roll_rad) : kI16Unavailable);
    out[7] = 0xFF;
}

bool decodeAttitude127257(const uint8_t in[8], Attitude127257 &out)
{
    out.sid = in[0];

    int16_t yaw_raw = getI16LE(in, 1);
    out.has_yaw = (yaw_raw != kI16Unavailable);
    out.yaw_rad = out.has_yaw ? i16ToAngle(yaw_raw) : 0.0f;

    int16_t pitch_raw = getI16LE(in, 3);
    out.has_pitch = (pitch_raw != kI16Unavailable);
    out.pitch_rad = out.has_pitch ? i16ToAngle(pitch_raw) : 0.0f;

    int16_t roll_raw = getI16LE(in, 5);
    out.has_roll = (roll_raw != kI16Unavailable);
    out.roll_rad = out.has_roll ? i16ToAngle(roll_raw) : 0.0f;

    return true;
}

// ---------------------------------------------------------------------------
// 129026 - COG & SOG, Rapid Update
// ---------------------------------------------------------------------------
void encodeCogSogRapid129026(const CogSogRapid129026 &in, uint8_t out[8])
{
    out[0] = in.sid;
    uint8_t reference = static_cast<uint8_t>(in.cog_reference) & 0x03u;
    out[1] = static_cast<uint8_t>(0xFCu | reference);  // bits 2-7 reserved (1)
    putU16LE(out, 2, in.has_cog ? angleToU16(in.cog_rad) : kU16Unavailable);
    putU16LE(out, 4, in.has_sog ? static_cast<uint16_t>(std::lround(in.sog_m_s / kSpeedResolutionMS)) : kU16Unavailable);
    out[6] = 0xFF;
    out[7] = 0xFF;
}

bool decodeCogSogRapid129026(const uint8_t in[8], CogSogRapid129026 &out)
{
    out.sid = in[0];
    out.cog_reference = static_cast<DirectionReference>(in[1] & 0x03u);

    uint16_t cog_raw = getU16LE(in, 2);
    out.has_cog = (cog_raw != kU16Unavailable);
    out.cog_rad = out.has_cog ? u16ToAngle(cog_raw) : 0.0f;

    uint16_t sog_raw = getU16LE(in, 4);
    out.has_sog = (sog_raw != kU16Unavailable);
    out.sog_m_s = out.has_sog ? static_cast<float>(sog_raw) * kSpeedResolutionMS : 0.0f;

    return true;
}

// ---------------------------------------------------------------------------
// 127258 - Magnetic Variation
// ---------------------------------------------------------------------------
void encodeMagneticVariation127258(const MagneticVariation127258 &in, uint8_t out[8])
{
    out[0] = in.sid;
    out[1] = static_cast<uint8_t>(0xF0u | (in.source & 0x0Fu));  // bits 4-7 reserved (1)
    putU16LE(out, 2, in.has_age_of_service ? in.age_of_service_days : kAgeOfServiceUnavailable);
    putI16LE(out, 4, in.has_variation ? angleToI16(in.variation_rad) : kI16Unavailable);
    out[6] = 0xFF;
    out[7] = 0xFF;
}

bool decodeMagneticVariation127258(const uint8_t in[8], MagneticVariation127258 &out)
{
    out.sid = in[0];
    out.source = in[1] & 0x0Fu;

    uint16_t age_raw = getU16LE(in, 2);
    out.has_age_of_service = (age_raw != kAgeOfServiceUnavailable);
    out.age_of_service_days = out.has_age_of_service ? age_raw : 0;

    int16_t var_raw = getI16LE(in, 4);
    out.has_variation = (var_raw != kI16Unavailable);
    out.variation_rad = out.has_variation ? i16ToAngle(var_raw) : 0.0f;

    return true;
}

}  // namespace n2k_codec
