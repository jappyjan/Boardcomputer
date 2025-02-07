#pragma once

#include "boardcomputer.hpp"

#define PWM_MIN 1000
#define PWM_MAX 2000
#define PWM_CENTER (PWM_MAX - PWM_MIN) / 2

// Define the type for the mapping function
typedef int (*PWMMappingFunction)(uint16_t channelValue, uint16_t min, uint16_t max);

class PWMChannelHandler : public IChannelHandler
{
public:
    PWMChannelHandler(uint8_t pin, uint16_t min = PWM_MIN, uint16_t max = PWM_MAX);
    void setup(uint16_t initialPosition = PWM_MIN);
    void onChannelChange(uint16_t value) override;
    void setInverted(bool inverted);

private:
    static bool isGlobalSetupDone;
    uint8_t pin;
    Servo output;
    uint16_t min;
    uint16_t max;
    bool inverted;
};
