#pragma once

#include "boardcomputer.hpp"

class PWMChannelHandler : public IChannelHandler
{
public:
    PWMChannelHandler(uint8_t pin);
    void setup();
    void onChannelChange(uint16_t value) override;

private:
    static bool isGlobalSetupDone;
    uint8_t pin;
    Servo output;
};