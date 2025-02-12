#include "bordcomputer.hpp"
#include "pin_map.hpp"
#include <ArduinoJson.h>

#include "logger.hpp"

BoardComputer::BoardComputer(HardwareSerial *crsfSerial) : crsfSerial(crsfSerial), lastValidSignalTime(0), errorState(false)
{
    if (!crsfSerial)
    {
        LOG.error("BoardComputer", "crsfSerial cannot be null");
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
    LOG.info("BoardComputer", "Initializing board computer");

    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);
    LOG.debug("BoardComputer", "Status LED initialized");

    // LED task - lowest priority
    xTaskCreate(
        [](void *pvParameters) -> void
        { static_cast<BoardComputer *>(pvParameters)->statusLedTaskHandler(pvParameters); },
        "statusLedTask",
        4096,
        this,
        1, // Lowest priority
        NULL);
    LOG.debug("BoardComputer", "Status LED task created");

    if (!crsfSerial)
    {
        this->status = BoardComputerStatus_ERROR;
        LOG.error("BoardComputer", "Invalid crsfSerial configuration - null pointer");
        while (true)
        {
            delay(100);
        }
    }

    LOG.infof("BoardComputer", "Configuring CRSF serial on pins RX:%d, TX:%d", CRSF_RX_PIN, CRSF_TX_PIN);
    crsfSerial->begin(CRSF_BAUDRATE, SERIAL_8N1, CRSF_RX_PIN, CRSF_TX_PIN);
    LOG.debug("BoardComputer", "Serial configuration complete");

    crsf.begin(*crsfSerial);
    LOG.debug("BoardComputer", "CRSF protocol initialized");

    // Main task - higher priority
    xTaskCreate([](void *pvParameters) -> void
                { static_cast<BoardComputer *>(pvParameters)->taskHandler(); },
                "BoardComputer",
                8192,
                this,
                2, // Higher priority than LED task
                NULL);
    LOG.debug("BoardComputer", "Main board computer task created");
}

void BoardComputer::taskHandler()
{
    const int loopIntervalMs = 1000 / UPDATE_LOOP_FREQUENCY_HZ;
    TickType_t lastWakeTime = xTaskGetTickCount();
    unsigned long lastDebugTime = 0;
    const unsigned long DEBUG_INTERVAL = 1000; // Print debug info every second

    LOG.info("BoardComputer", "Starting CRSF task handler");
    LOG.infof("BoardComputer", "CRSF configured on Serial0 - RX: %d, TX: %d @ %d baud",
              CRSF_RX_PIN, CRSF_TX_PIN, CRSF_BAUDRATE);

    while (true)
    {
        unsigned long currentTime = millis();

        // Update CRSF and immediately check link status
        this->crsf.update();
        if (crsf.isLinkUp())
        {
            lastValidSignalTime = currentTime; // Update timestamp when link is up

            if (this->status != BoardComputerStatus_CRSF_CONNECTED)
            {
                LOG.info("BoardComputer", "CRSF link established");
                this->status = BoardComputerStatus_CRSF_CONNECTED;
            }
        }
        else
        {
            if (this->status != BoardComputerStatus_CRSF_DISCONNECTED)
            {
                LOG.info("BoardComputer", "CRSF link lost");
                this->status = BoardComputerStatus_CRSF_DISCONNECTED;
            }
        }

        // Debug logging every second
        if (currentTime - lastDebugTime >= DEBUG_INTERVAL)
        {
            lastDebugTime = currentTime;
        }

        this->executeChannelHandlers();

        // Wait until next interval, taking execution time into account
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(loopIntervalMs));
    }
}

void BoardComputer::onChannelChange(uint8_t channel, IChannelHandler *handler, int failSafeChannelValue)
{
    // Convert 1-based channel number to 0-based index
    uint8_t channelIndex = channel - 1;

    LOG.debugf("BoardComputer", "Registering handler for channel %d (index %d)",
               channel, channelIndex);

    if (channelIndex >= HIGHEST_CHANNEL_NUMBER)
    {
        LOG.errorf("BoardComputer", "Channel %d exceeds maximum channel number", channel);
        this->status = BoardComputerStatus_ERROR;
        while (true)
        {
            delay(100); // Prevent watchdog reset while still halting execution
        }
    }

    if (handlerCount[channelIndex] >= MAX_HANDLERS_PER_CHANNEL)
    {
        LOG.errorf("BoardComputer", "Maximum handlers reached for channel %d", channel);
        this->status = BoardComputerStatus_ERROR;
        while (true)
        {
            delay(100); // Prevent watchdog reset while still halting execution
        }
    }

    channelHandlers[channelIndex][handlerCount[channelIndex]] = handler;
    failSafeChannelValues[channelIndex][handlerCount[channelIndex]] = failSafeChannelValue;
    handlerCount[channelIndex]++;
}

void BoardComputer::executeChannelHandlers()
{
    unsigned long currentTime = millis();
    bool hasValidSignal = crsf.isLinkUp() && ((currentTime - lastValidSignalTime) < SIGNAL_TIMEOUT_MS);

    if (!hasValidSignal)
    {
        static unsigned long lastTimeoutLog = 0;
        if (currentTime - lastTimeoutLog >= 5000)
        { // Log timeout every second
            LOG.warningf("BoardComputer", "Signal timeout - Last valid: %lums ago, Link: %s, Status: %d",
                         currentTime - lastValidSignalTime,
                         crsf.isLinkUp() ? "UP" : "DOWN",
                         this->status);
            lastTimeoutLog = currentTime;
        }
    }

    for (int channel = 0; channel < HIGHEST_CHANNEL_NUMBER; channel++)
    {
        int currentValue;

        if (hasValidSignal)
        {
            currentValue = crsf.getChannel(channel + 1);
            if (currentValue < CHANNEL_MIN)
            {
                currentValue = CHANNEL_MIN;
            }
            else if (currentValue > CHANNEL_MAX)
            {
                currentValue = CHANNEL_MAX;
            }
            lastValidSignalTime = currentTime;
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
            LOG.warningf("BoardComputer", "unknown status reached: %d", boardComputer->status);
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

    const auto &pinMap = ::getPinMap(); // Use global scope operator to get the function from pin_map.hpp

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

void BoardComputer::cleanup()
{
    for (uint8_t channel = 0; channel < HIGHEST_CHANNEL_NUMBER; channel++)
    {
        for (uint8_t i = 0; i < handlerCount[channel]; i++)
        {
            if (channelHandlers[channel][i])
            {
                delete channelHandlers[channel][i];
                channelHandlers[channel][i] = nullptr;
                failSafeChannelValues[channel][i] = -1;
            }
        }
        handlerCount[channel] = 0;
    }
}
