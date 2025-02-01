#pragma once

#include "boardcomputer.hpp"

class BlinkChannelHandler : public IChannelHandler
{
public:
    BlinkChannelHandler(uint8_t pin, uint16_t onDurationMs, uint16_t offDurationMs);
    void isOnWhen(std::function<bool(uint16_t value)> isOn);
    void onChannelChange(uint16_t value);

private:
    uint8_t pin;
    std::function<bool(uint16_t value)> isOn;
    uint16_t onDurationMs;
    uint16_t offDurationMs;
    bool isBlinking;
    TaskHandle_t blinkTaskHandle;
    void blinkTaskHandler(void *pvParameters);
};
