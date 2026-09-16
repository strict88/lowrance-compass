#include "tnmea2000_can_bus.h"

#include <cstring>

bool TNmea2000CanBus::CANSendFrame(unsigned long id, unsigned char len, const unsigned char *buf, bool /*wait_sent*/)
{
    CanFrame frame;
    frame.id = static_cast<uint32_t>(id);
    frame.dlc = len > 8 ? 8 : len;
    memcpy(frame.data, buf, frame.dlc);
    return can_bus_.send(frame);
}

bool TNmea2000CanBus::CANOpen()
{
    return can_bus_.init(enable_self_test_);
}

bool TNmea2000CanBus::CANGetFrame(unsigned long &id, unsigned char &len, unsigned char *buf)
{
    CanFrame frame;
    if (!can_bus_.receive(frame))
    {
        return false;
    }
    id = frame.id;
    len = frame.dlc;
    memcpy(buf, frame.data, frame.dlc);
    return true;
}
