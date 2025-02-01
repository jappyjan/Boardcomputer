#include "blinkChannelHandler.hpp"

BlinkChannelHandler::BlinkChannelHandler(uint8_t pin, uint16_t onDurationMs, uint16_t offDurationMs)
{
    this->pin = pin;
    this->onDurationMs = onDurationMs;
    this->offDurationMs = offDurationMs;
    this->isBlinking = false;
    this->blinkTaskHandle = NULL;
    pinMode(pin, OUTPUT);
}

void BlinkChannelHandler::isOnWhen(std::function<bool(uint16_t value)> isOn)
{
    this->isOn = isOn;
}

void BlinkChannelHandler::onChannelChange(uint16_t value)
{
    bool shouldBlink = this->isOn(value);
    
    if (shouldBlink != this->isBlinking) {
        this->isBlinking = shouldBlink;
        
        if (shouldBlink) {
            // Start blink task if not already running
            if (this->blinkTaskHandle == NULL) {
                xTaskCreate(
                    [](void *pvParameters) -> void
                    { static_cast<BlinkChannelHandler *>(pvParameters)->blinkTaskHandler(pvParameters); },
                    "BlinkTask",
                    1000,
                    this,
                    1,
                    &this->blinkTaskHandle
                );
            }
        } else {
            // Stop blink task if running
            if (this->blinkTaskHandle != NULL) {
                vTaskDelete(this->blinkTaskHandle);
                this->blinkTaskHandle = NULL;
                digitalWrite(this->pin, LOW); // Ensure LED is off when stopping
            }
        }
    }
}

void BlinkChannelHandler::blinkTaskHandler(void *pvParameters)
{
    BlinkChannelHandler *handler = (BlinkChannelHandler *)pvParameters;
    while (true)
    {
        digitalWrite(handler->pin, HIGH);
        vTaskDelay(pdMS_TO_TICKS(handler->onDurationMs));
        digitalWrite(handler->pin, LOW);
        vTaskDelay(pdMS_TO_TICKS(handler->offDurationMs));
    }
}