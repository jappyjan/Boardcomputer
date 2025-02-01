#include "pwmChannelHandler.hpp"

bool PWMChannelHandler::isGlobalSetupDone = false;


PWMChannelHandler::PWMChannelHandler(uint8_t pin, uint16_t min, uint16_t max) : pin(pin)
{
    pinMode(pin, OUTPUT);

    this->min = min;
    this->max = max;
}

void PWMChannelHandler::setup(uint16_t initialPosition)
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
    this->output.write(initialPosition);
    this->output.attach(this->pin);
    this->output.write(initialPosition);
}

void PWMChannelHandler::onChannelChange(uint16_t value)
{
    int outputValue = map(value, CHANNEL_MIN, CHANNEL_MAX, this->min, this->max);
    this->output.write(outputValue);
}