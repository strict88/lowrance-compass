#pragma once

#include <NMEA2000.h>

#include "can_bus.h"

// Bridges ttlappalainen/NMEA2000's tNMEA2000 abstract transport (CANOpen/
// CANSendFrame/CANGetFrame) onto our own CanBus interface, so the NMEA2000
// message/PGN layer runs over either TwaiNodeBus (real hardware) or
// FakeCanBus (tests) without caring which.
class TNmea2000CanBus : public tNMEA2000
{
public:
    // `can_bus` must outlive this object. `enable_self_test` is bench/no-ACK
    // mode (quickstart.md section 5).
    TNmea2000CanBus(CanBus &can_bus, bool enable_self_test) : can_bus_(can_bus), enable_self_test_(enable_self_test) {}

protected:
    bool CANSendFrame(unsigned long id, unsigned char len, const unsigned char *buf, bool wait_sent) override;
    bool CANOpen() override;
    bool CANGetFrame(unsigned long &id, unsigned char &len, unsigned char *buf) override;

private:
    CanBus &can_bus_;
    bool enable_self_test_;
};
