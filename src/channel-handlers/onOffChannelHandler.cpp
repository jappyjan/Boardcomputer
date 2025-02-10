#include "onOffChannelHandler.hpp"
#include "logger.hpp"

OnOffChannelHandler::OnOffChannelHandler(uint8_t pin) : pin(pin)
{
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW); // Initialize to OFF state
    LOG.debugf("OnOffHandler", "Initialized on pin %d", pin);
}

void OnOffChannelHandler::isOnWhen(std::function<bool(uint16_t value)> isOn)
{
    this->isOn = isOn;
}

void OnOffChannelHandler::onChannelChange(uint16_t value)
{
    bool shouldBeOn = this->isOn(value);
    digitalWrite(pin, shouldBeOn ? HIGH : LOW);
    LOG.debugf("OnOffHandler", "Pin %d set to %s (value: %d)",
               pin, shouldBeOn ? "ON" : "OFF", value);
}