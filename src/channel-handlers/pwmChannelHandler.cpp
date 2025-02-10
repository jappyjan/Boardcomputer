#include "pwmChannelHandler.hpp"
#include "logger.hpp"

bool PWMChannelHandler::isGlobalSetupDone = false;

PWMChannelHandler::PWMChannelHandler(uint8_t pin, uint16_t min, uint16_t max) : pin(pin)
{
    pinMode(pin, OUTPUT);
    this->min = min;
    this->max = max;
    this->inverted = false;

    // Configure servo - let ESP32Servo handle timer allocation dynamically
    this->output.setPeriodHertz(50); // Standard 50hz servo
    if (this->output.attach(pin, min, max))
    {
        LOG.debugf("PWMHandler", "Initialized servo on pin %d (range: %d-%d)", pin, min, max);
    }
    else
    {
        LOG.errorf("PWMHandler", "Failed to initialize servo on pin %d", pin);
    }
}

void PWMChannelHandler::setup(uint16_t initialPosition)
{
    // Set initial position
    this->output.writeMicroseconds(initialPosition);
    LOG.debugf("PWMHandler", "Set initial position to %d on pin %d", initialPosition, pin);
}

void PWMChannelHandler::onChannelChange(uint16_t value)
{
    // For servos, we want to use the raw channel value (1000-2000) directly
    // but constrain it to our min/max range
    uint16_t constrainedValue = value;
    if (this->inverted)
    {
        constrainedValue = map(value, CHANNEL_MIN, CHANNEL_MAX, CHANNEL_MAX, CHANNEL_MIN);
    }

    constrainedValue = constrain(constrainedValue, this->min, this->max);

    this->output.writeMicroseconds(constrainedValue);
}

void PWMChannelHandler::setInverted(bool inverted)
{
    LOG.infof("PWMHandler", "Setting pin %d to inverted: %s", this->pin, inverted ? "yes" : "no");
    this->inverted = inverted;
}
