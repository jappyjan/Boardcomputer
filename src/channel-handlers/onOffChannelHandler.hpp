#pragma once

#include "../boardcomputer.hpp"

class OnOffChannelHandler : public IChannelHandler
{
public:
    OnOffChannelHandler(uint8_t pin);
    void isOnWhen(std::function<bool(uint16_t value)> isOn);
    void onChannelChange(uint16_t value);

private:
    uint8_t pin;
    std::function<bool(uint16_t value)> isOn;
};
