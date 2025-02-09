#include "boardcomputer.hpp"
#include "board_pin_map.hpp"
#include <ArduinoJson.h>

BoardComputer::BoardComputer(HardwareSerial *crsfSerial) : crsfSerial(crsfSerial), lastValidSignalTime(0), errorState(false)
{
    if (!crsfSerial)
    {
        Serial.println("Error: crsfSerial cannot be null");
        while (true)
        {
            delay(100); // Prevent watchdog reset while halting
        }
    }

    this->status = BoardComputerStatus_UNCONFIGURED;
    memset(lastChannelValues, 0, sizeof(lastChannelValues));

    // Initialize arrays with nullptr/default values
    for (int i = 0; i < HIGHEST_CHANNEL_NUMBER; i++)
    {
        handlerCount[i] = 0;
        for (int j = 0; j < MAX_HANDLERS_PER_CHANNEL; j++)
        {
            channelHandlers[i][j] = nullptr;
            failSafeChannelValues[i][j] = -1;
        }
    }
}

void BoardComputer::start()
{
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    xTaskCreate(
        [](void *pvParameters) -> void
        { static_cast<BoardComputer *>(pvParameters)->statusLedTaskHandler(pvParameters); },
        "statusLedTaskHandler",
        2048,
        this,
        1,
        NULL);

    // Validate crsfSerial before using it
    if (!crsfSerial)
    {
        this->status = BoardComputerStatus_ERROR;
        Serial.println("Invalid crsfSerial configuration - null pointer");
        while (true)
        {
            delay(100); // Prevent watchdog reset while halting
        }
    }

    crsfSerial->begin(CRSF_BAUDRATE, SERIAL_8N1, CRSF_RX_PIN, CRSF_TX_PIN);

    crsf.begin(*crsfSerial);

    // create a new rtos task and execute a lambda that calls run()
    xTaskCreate([](void *pvParameters) -> void
                { static_cast<BoardComputer *>(pvParameters)->taskHandler(); },
                "BoardComputer",
                2048,
                this,
                1,
                NULL);
}

void BoardComputer::taskHandler()
{
    const int loopIntervalMs = 1000 / UPDATE_LOOP_FREQUENCY_HZ;
    TickType_t lastWakeTime = xTaskGetTickCount();

    while (true)
    {
        this->crsf.update();

        if (crsf.isLinkUp())
        {
            this->status = BoardComputerStatus_CRSF_CONNECTED;
        }
        else
        {
            this->status = BoardComputerStatus_CRSF_DISCONNECTED;
        }

        this->executeChannelHandlers();

        // Wait until next interval, taking execution time into account
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(loopIntervalMs));
    }
}

void BoardComputer::onChannelChange(uint8_t channel, IChannelHandler *handler, int failSafeChannelValue)
{
    if (channel >= HIGHEST_CHANNEL_NUMBER)
    {
        Serial.printf("Error: Channel %d exceeds maximum channel number\n", channel);
        this->status = BoardComputerStatus_ERROR;
        while (true)
        {
            delay(100); // Prevent watchdog reset while still halting execution
        }
    }

    if (handlerCount[channel] >= MAX_HANDLERS_PER_CHANNEL)
    {
        Serial.printf("Error: Maximum handlers reached for channel %d\n", channel);
        this->status = BoardComputerStatus_ERROR;
        while (true)
        {
            delay(100); // Prevent watchdog reset while still halting execution
        }
    }

    channelHandlers[channel][handlerCount[channel]] = handler;
    failSafeChannelValues[channel][handlerCount[channel]] = failSafeChannelValue;
    handlerCount[channel]++;
}

void BoardComputer::executeChannelHandlers()
{
    bool hasValidSignal = crsf.isLinkUp() && ((millis() - lastValidSignalTime) < SIGNAL_TIMEOUT_MS);

    for (int channel = 0; channel < HIGHEST_CHANNEL_NUMBER; channel++)
    {
        int currentValue;

        if (hasValidSignal)
        {
            currentValue = crsf.getChannel(channel);
            if (currentValue < CHANNEL_MIN)
            {
                currentValue = CHANNEL_MIN;
            }
            else if (currentValue > CHANNEL_MAX)
            {
                currentValue = CHANNEL_MAX;
            }
            lastValidSignalTime = millis();
            errorState = false;
        }
        else
        {
            // Use failsafe values when no valid signal
            for (int handlerIndex = 0; handlerIndex < handlerCount[channel]; handlerIndex++)
            {
                if (channelHandlers[channel][handlerIndex] == nullptr)
                {
                    continue;
                }

                int failsafeValue = failSafeChannelValues[channel][handlerIndex];
                // If failsafe value is set (not -1), use it
                if (failsafeValue != -1)
                {
                    channelHandlers[channel][handlerIndex]->onChannelChange(failsafeValue);
                }
                else
                {
                    // If no failsafe value set, use center position
                    channelHandlers[channel][handlerIndex]->onChannelChange(CHANNEL_MID);
                }
            }
            continue; // Skip the rest of the loop for this channel
        }

        // Only process value changes when we have a valid signal
        if (currentValue == lastChannelValues[channel])
        {
            continue;
        }

        for (int handlerIndex = 0; handlerIndex < handlerCount[channel]; handlerIndex++)
        {
            if (channelHandlers[channel][handlerIndex] == nullptr)
            {
                continue;
            }

            Serial.printf("channel %d changed to %d\n", channel, currentValue);
            channelHandlers[channel][handlerIndex]->onChannelChange(currentValue);
        }

        lastChannelValues[channel] = currentValue;
    }
}

void BoardComputer::statusLedTaskHandler(void *pvParameters)
{
    BoardComputer *boardComputer = static_cast<BoardComputer *>(pvParameters);

    bool bootingBlinkState = false;
    int brightness = 0;
    bool isBreathingUp = true;

    while (true)
    {
        switch (boardComputer->status)
        {
        case BoardComputerStatus_UNCONFIGURED:
            // blink the led slowly
            analogWrite(STATUS_LED_PIN, bootingBlinkState ? 255 : 0);
            bootingBlinkState = !bootingBlinkState;
            vTaskDelay(pdMS_TO_TICKS(500));
            break;

        case BoardComputerStatus_CRSF_CONNECTED:
            // breath the led slowly
            analogWrite(STATUS_LED_PIN, brightness);
            brightness = (brightness + (isBreathingUp ? 1 : -1));
            if (brightness > 255)
            {
                brightness = 255;
                isBreathingUp = false;
            }
            else if (brightness < 0)
            {
                brightness = 0;
                isBreathingUp = true;
            }
            analogWrite(STATUS_LED_PIN, brightness);
            vTaskDelay(pdMS_TO_TICKS(10));
            break;

        case BoardComputerStatus_CRSF_DISCONNECTED:
            // blink rapidly
            analogWrite(STATUS_LED_PIN, 255);
            vTaskDelay(pdMS_TO_TICKS(150));
            analogWrite(STATUS_LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(150));
            break;

        case BoardComputerStatus_ERROR:
            // double blink rapidly then pause for half a second

            // first blink
            analogWrite(STATUS_LED_PIN, 255);
            vTaskDelay(pdMS_TO_TICKS(100));
            analogWrite(STATUS_LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(100));

            // second blink
            analogWrite(STATUS_LED_PIN, 255);
            vTaskDelay(pdMS_TO_TICKS(100));
            analogWrite(STATUS_LED_PIN, 0);

            // pause
            vTaskDelay(pdMS_TO_TICKS(500));
            break;

        default:
            Serial.printf("unknown %d\n", random(0, 100));
            break;
        }
    }
}

bool BoardComputer::isReceiving() const
{
    return crsf.isLinkUp() && ((millis() - lastValidSignalTime) < SIGNAL_TIMEOUT_MS);
}

bool BoardComputer::hasError() const
{
    return status == BoardComputerStatus_ERROR || !isReceiving() || status == BoardComputerStatus_UNCONFIGURED;
}

String BoardComputer::getPinMap()
{
    StaticJsonDocument<512> doc;

    const auto &pinMap = ::getPinMap(); // Use global scope operator to get the function from board_pin_map.hpp

    // Use traditional iterator instead of structured binding
    for (const auto &pair : pinMap)
    {
        const std::string &name = pair.first;
        const PinInfo &info = pair.second;

        JsonObject pinObj = doc.createNestedObject(name);
        pinObj["pin"] = info.pin;
        pinObj["isPWM"] = info.isPWM;
    }

    String output;
    serializeJson(doc, output);
    return output;
}
