#include <unity.h>

#include "../fakes/fake_can_bus.h"
#include "n2k_codec/heading_transmit.h"
#include "n2k_codec/pgn_codec.h"

namespace
{

heading::HeadingReading validReading()
{
    heading::HeadingReading r;
    r.heading_rad = 1.0f;
    r.valid = true;
    return r;
}

heading::HeadingReading invalidReading()
{
    heading::HeadingReading r;
    r.heading_rad = 1.0f;
    r.valid = false;
    r.reason_if_invalid = heading::InvalidReason::kSensorAccuracyLow;
    return r;
}

}  // namespace

void test_valid_heading_is_sent(void)
{
    FakeCanBus can_bus;
    bool sent = n2k_codec::sendVesselHeadingIfValid(can_bus, /*source_address=*/35, validReading(), false, 0.0f);

    TEST_ASSERT_TRUE(sent);
    TEST_ASSERT_EQUAL_size_t(1, can_bus.sentFrames().size());
}

void test_invalid_heading_is_never_sent(void)
{
    FakeCanBus can_bus;
    bool sent = n2k_codec::sendVesselHeadingIfValid(can_bus, /*source_address=*/35, invalidReading(), false, 0.0f);

    TEST_ASSERT_FALSE(sent);
    TEST_ASSERT_EQUAL_size_t(0, can_bus.sentFrames().size());
}

void test_sent_frame_encodes_pgn_127250_and_heading(void)
{
    FakeCanBus can_bus;
    heading::HeadingReading r = validReading();
    r.heading_rad = 1.2345f;

    TEST_ASSERT_TRUE(n2k_codec::sendVesselHeadingIfValid(can_bus, 12, r, false, 0.0f));

    TEST_ASSERT_EQUAL_size_t(1, can_bus.sentFrames().size());
    const CanFrame &frame = can_bus.sentFrames()[0];

    uint32_t expected_id = n2k_codec::buildCanId(2, 127250UL, 12);
    TEST_ASSERT_EQUAL_UINT32(expected_id, frame.id);
    TEST_ASSERT_EQUAL_UINT8(8, frame.dlc);

    n2k_codec::VesselHeading127250 decoded;
    TEST_ASSERT_TRUE(n2k_codec::decodeVesselHeading127250(frame.data, decoded));
    TEST_ASSERT_FLOAT_WITHIN(0.0002f, r.heading_rad, decoded.heading_rad);
}

void test_can_id_pdu2_broadcast_formula(void)
{
    // 127250 = 0x1F112; PDU format byte (PGN>>8) = 0xF1 = 241 >= 240 -> PDU2.
    uint32_t id = n2k_codec::buildCanId(2, 127250UL, 0x23);
    uint32_t expected = (static_cast<uint32_t>(2 & 0x7) << 26) | (127250UL << 8) | 0x23u;
    TEST_ASSERT_EQUAL_UINT32(expected, id);
}

void test_no_frame_sent_when_can_bus_rejects_send(void)
{
    FakeCanBus can_bus;
    can_bus.setSendShouldSucceedForTest(false);

    bool sent = n2k_codec::sendVesselHeadingIfValid(can_bus, 35, validReading(), false, 0.0f);

    TEST_ASSERT_FALSE(sent);
}
