#include "onOffChannelHandler.hpp"

OnOffChannelHandler::OnOffChannelHandler(uint8_t pin) : pin(pin)
{
}

void OnOffChannelHandler::isOnWhen(std::function<bool(uint16_t value)> isOn)
{
    this->isOn = isOn;
}

void OnOffChannelHandler::onChannelChange(uint16_t value)
{
    digitalWrite(this->pin, this->isOn(value) ? HIGH : LOW);
}