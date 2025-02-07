#include "pwmChannelHandler.hpp"

bool PWMChannelHandler::isGlobalSetupDone = false;

PWMChannelHandler::PWMChannelHandler(uint8_t pin, uint16_t min, uint16_t max) : pin(pin)
{
    pinMode(pin, OUTPUT);

    this->min = min;
    this->max = max;
    this->inverted = false;
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
    int finalMin = this->inverted ? CHANNEL_MAX : CHANNEL_MIN;
    int finalMax = this->inverted ? CHANNEL_MIN : CHANNEL_MAX;

    int outputValue = map(value, finalMin, finalMax, this->min, this->max);
    if (outputValue < this->min)
    {
        outputValue = this->min;
    }
    else if (outputValue > this->max)
    {
        outputValue = this->max;
    }

    this->output.writeMicroseconds(outputValue);
}

void PWMChannelHandler::setInverted(bool inverted)
{
    Serial.printf("Setting pin %d to inverted: %d\n", this->pin, inverted);
    this->inverted = inverted;
}
