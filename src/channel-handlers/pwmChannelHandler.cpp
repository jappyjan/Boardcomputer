#include "pwmChannelHandler.hpp"

bool PWMChannelHandler::isGlobalSetupDone = false;

PWMChannelHandler::PWMChannelHandler(uint8_t pin) : pin(pin)
{
}

void PWMChannelHandler::setup()
{
    if (!PWMChannelHandler::isGlobalSetupDone)
    {
        PWMChannelHandler::isGlobalSetupDone = true;
        ESP32PWM::allocateTimer(0);
        ESP32PWM::allocateTimer(1);
        ESP32PWM::allocateTimer(2);
        ESP32PWM::allocateTimer(3);
    }

    this->output.setPeriodHertz(50);
    this->output.attach(this->pin);
}

void PWMChannelHandler::onChannelChange(uint16_t value)
{
    int outputValue = map(value, CHANNEL_MIN, CHANNEL_MAX, 0, 180);
    this->output.write(outputValue);
}
