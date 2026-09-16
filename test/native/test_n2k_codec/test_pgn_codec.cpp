#include <unity.h>

#include <cmath>

#include "n2k_codec/pgn_codec.h"

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kAngleEps = 0.0002f;  // just over one LSB (0.0001 rad)
}  // namespace

void test_vessel_heading_round_trip_basic(void)
{
    n2k_codec::VesselHeading127250 in;
    in.sid = 5;
    in.heading_rad = 1.2345f;
    in.has_deviation = true;
    in.deviation_rad = -0.05f;
    in.has_variation = true;
    in.variation_rad = 0.08f;
    in.reference = n2k_codec::DirectionReference::kMagnetic;

    uint8_t buf[8];
    n2k_codec::encodeVesselHeading127250(in, buf);

    n2k_codec::VesselHeading127250 out;
    TEST_ASSERT_TRUE(n2k_codec::decodeVesselHeading127250(buf, out));

    TEST_ASSERT_EQUAL_UINT8(5, out.sid);
    TEST_ASSERT_FLOAT_WITHIN(kAngleEps, in.heading_rad, out.heading_rad);
    TEST_ASSERT_TRUE(out.has_deviation);
    TEST_ASSERT_FLOAT_WITHIN(kAngleEps, in.deviation_rad, out.deviation_rad);
    TEST_ASSERT_TRUE(out.has_variation);
    TEST_ASSERT_FLOAT_WITHIN(kAngleEps, in.variation_rad, out.variation_rad);
    TEST_ASSERT_TRUE(out.reference == n2k_codec::DirectionReference::kMagnetic);
}

void test_vessel_heading_range_boundaries(void)
{
    n2k_codec::VesselHeading127250 in;
    in.heading_rad = 0.0f;

    uint8_t buf[8];
    n2k_codec::encodeVesselHeading127250(in, buf);
    n2k_codec::VesselHeading127250 out;
    TEST_ASSERT_TRUE(n2k_codec::decodeVesselHeading127250(buf, out));
    TEST_ASSERT_FLOAT_WITHIN(kAngleEps, 0.0f, out.heading_rad);

    in.heading_rad = 2.0f * kPi - 0.0001f;  // just under 2*pi
    n2k_codec::encodeVesselHeading127250(in, buf);
    TEST_ASSERT_TRUE(n2k_codec::decodeVesselHeading127250(buf, out));
    TEST_ASSERT_FLOAT_WITHIN(kAngleEps, in.heading_rad, out.heading_rad);
}

void test_vessel_heading_absent_deviation_variation(void)
{
    n2k_codec::VesselHeading127250 in;
    in.heading_rad = 0.5f;
    in.has_deviation = false;
    in.has_variation = false;
    in.reference = n2k_codec::DirectionReference::kTrue;

    uint8_t buf[8];
    n2k_codec::encodeVesselHeading127250(in, buf);
    n2k_codec::VesselHeading127250 out;
    TEST_ASSERT_TRUE(n2k_codec::decodeVesselHeading127250(buf, out));

    TEST_ASSERT_FALSE(out.has_deviation);
    TEST_ASSERT_FALSE(out.has_variation);
    TEST_ASSERT_TRUE(out.reference == n2k_codec::DirectionReference::kTrue);
}

void test_rate_of_turn_round_trip_positive_and_negative(void)
{
    n2k_codec::RateOfTurn127251 in;
    in.sid = 9;
    in.rate_rad_s = 0.25f;

    uint8_t buf[8];
    n2k_codec::encodeRateOfTurn127251(in, buf);
    n2k_codec::RateOfTurn127251 out;
    TEST_ASSERT_TRUE(n2k_codec::decodeRateOfTurn127251(buf, out));
    TEST_ASSERT_EQUAL_UINT8(9, out.sid);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, in.rate_rad_s, out.rate_rad_s);

    in.rate_rad_s = -0.25f;
    n2k_codec::encodeRateOfTurn127251(in, buf);
    TEST_ASSERT_TRUE(n2k_codec::decodeRateOfTurn127251(buf, out));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, in.rate_rad_s, out.rate_rad_s);

    in.rate_rad_s = 0.0f;
    n2k_codec::encodeRateOfTurn127251(in, buf);
    TEST_ASSERT_TRUE(n2k_codec::decodeRateOfTurn127251(buf, out));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, out.rate_rad_s);
}

void test_attitude_round_trip_all_fields(void)
{
    n2k_codec::Attitude127257 in;
    in.sid = 3;
    in.has_yaw = true;
    in.yaw_rad = 1.0f;
    in.has_pitch = true;
    in.pitch_rad = -0.2f;
    in.has_roll = true;
    in.roll_rad = kPi;  // upper boundary (-pi, pi]

    uint8_t buf[8];
    n2k_codec::encodeAttitude127257(in, buf);
    n2k_codec::Attitude127257 out;
    TEST_ASSERT_TRUE(n2k_codec::decodeAttitude127257(buf, out));

    TEST_ASSERT_TRUE(out.has_yaw);
    TEST_ASSERT_FLOAT_WITHIN(kAngleEps, in.yaw_rad, out.yaw_rad);
    TEST_ASSERT_TRUE(out.has_pitch);
    TEST_ASSERT_FLOAT_WITHIN(kAngleEps, in.pitch_rad, out.pitch_rad);
    TEST_ASSERT_TRUE(out.has_roll);
    TEST_ASSERT_FLOAT_WITHIN(kAngleEps, in.roll_rad, out.roll_rad);
}

void test_attitude_absent_fields(void)
{
    n2k_codec::Attitude127257 in;  // all has_* false

    uint8_t buf[8];
    n2k_codec::encodeAttitude127257(in, buf);
    n2k_codec::Attitude127257 out;
    TEST_ASSERT_TRUE(n2k_codec::decodeAttitude127257(buf, out));

    TEST_ASSERT_FALSE(out.has_yaw);
    TEST_ASSERT_FALSE(out.has_pitch);
    TEST_ASSERT_FALSE(out.has_roll);
}

void test_cog_sog_round_trip(void)
{
    n2k_codec::CogSogRapid129026 in;
    in.sid = 1;
    in.cog_reference = n2k_codec::DirectionReference::kTrue;
    in.has_cog = true;
    in.cog_rad = 1.5f;
    in.has_sog = true;
    in.sog_m_s = 3.6f;

    uint8_t buf[8];
    n2k_codec::encodeCogSogRapid129026(in, buf);
    n2k_codec::CogSogRapid129026 out;
    TEST_ASSERT_TRUE(n2k_codec::decodeCogSogRapid129026(buf, out));

    TEST_ASSERT_TRUE(out.cog_reference == n2k_codec::DirectionReference::kTrue);
    TEST_ASSERT_TRUE(out.has_cog);
    TEST_ASSERT_FLOAT_WITHIN(kAngleEps, in.cog_rad, out.cog_rad);
    TEST_ASSERT_TRUE(out.has_sog);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, in.sog_m_s, out.sog_m_s);
}

void test_cog_sog_unavailable(void)
{
    n2k_codec::CogSogRapid129026 in;  // has_cog=false, has_sog=false

    uint8_t buf[8];
    n2k_codec::encodeCogSogRapid129026(in, buf);
    n2k_codec::CogSogRapid129026 out;
    TEST_ASSERT_TRUE(n2k_codec::decodeCogSogRapid129026(buf, out));

    TEST_ASSERT_FALSE(out.has_cog);
    TEST_ASSERT_FALSE(out.has_sog);
}

void test_magnetic_variation_round_trip(void)
{
    n2k_codec::MagneticVariation127258 in;
    in.sid = 2;
    in.source = 7;
    in.has_age_of_service = true;
    in.age_of_service_days = 100;
    in.has_variation = true;
    in.variation_rad = -0.14f;

    uint8_t buf[8];
    n2k_codec::encodeMagneticVariation127258(in, buf);
    n2k_codec::MagneticVariation127258 out;
    TEST_ASSERT_TRUE(n2k_codec::decodeMagneticVariation127258(buf, out));

    TEST_ASSERT_EQUAL_UINT8(2, out.sid);
    TEST_ASSERT_EQUAL_UINT8(7, out.source);
    TEST_ASSERT_TRUE(out.has_age_of_service);
    TEST_ASSERT_EQUAL_UINT16(100, out.age_of_service_days);
    TEST_ASSERT_TRUE(out.has_variation);
    TEST_ASSERT_FLOAT_WITHIN(kAngleEps, in.variation_rad, out.variation_rad);
}

void test_magnetic_variation_absent_fields(void)
{
    n2k_codec::MagneticVariation127258 in;
    in.source = 0;

    uint8_t buf[8];
    n2k_codec::encodeMagneticVariation127258(in, buf);
    n2k_codec::MagneticVariation127258 out;
    TEST_ASSERT_TRUE(n2k_codec::decodeMagneticVariation127258(buf, out));

    TEST_ASSERT_FALSE(out.has_age_of_service);
    TEST_ASSERT_FALSE(out.has_variation);
}
