#include "boardcomputer.hpp"

BoardComputer::BoardComputer()
{
    memset(lastChannelValues, 0, sizeof(lastChannelValues));
    memset(currentChannelValues, 0, sizeof(currentChannelValues));
}

void BoardComputer::start()
{
    // create a new rtos task and execute a lambda that calls run()
    xTaskCreate([](void *pvParameters) -> void
                { static_cast<BoardComputer *>(pvParameters)->taskHandler(); },
                "BoardComputer", 1024, this, 1, NULL);
}

void BoardComputer::taskHandler()
{
    const int loopIntervalMs = 1000 / UPDATE_LOOP_FREQUENCY_HZ;
    TickType_t lastWakeTime = xTaskGetTickCount();

    while (true)
    {
        this->updateChannelValues();
        this->executeChannelHandlers();

        // Wait until next interval, taking execution time into account
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(loopIntervalMs));
    }
}

void BoardComputer::onChannelChange(uint8_t channel, IChannelHandler *handler)
{
    channelHandlers[channel].push_back(handler);
}

bool forward = true;
void BoardComputer::updateChannelValues()
{
    // copy current channel values to last channel values
    memcpy(lastChannelValues, currentChannelValues, sizeof(currentChannelValues));
    // read current channel values from rx
    // TODO: add rx reading
    // mocked for now
    for (int i = 0; i < CHANNEL_COUNT; i++)
    {
        // slowly change the value of the channel
        int nextChannelValue = currentChannelValues[i];
        if (forward)
        {
            nextChannelValue += 1;
        }
        else
        {
            nextChannelValue -= 1;
        }

        if (nextChannelValue < 0)
        {
            nextChannelValue = 0;
            forward = true;
        }

        if (nextChannelValue > 1023)
        {
            nextChannelValue = 1023;
            forward = false;
        }

        currentChannelValues[i] = nextChannelValue;
    }
}

void BoardComputer::executeChannelHandlers()
{
    for (int i = 0; i < CHANNEL_COUNT; i++)
    {
        for (IChannelHandler *handler : channelHandlers[i])
        {
            bool didChange = currentChannelValues[i] != lastChannelValues[i];
            if (didChange)
            {
                handler->onChannelChange(currentChannelValues[i]);
            }
        }
    }
}
