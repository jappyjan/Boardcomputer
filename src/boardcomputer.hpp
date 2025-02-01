#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>
#include <AlfredoCRSF.h>
#include <functional>

#include "const.hpp"
#define HIGHEST_CHANNEL_NUMBER 16
#define MAX_HANDLERS_PER_CHANNEL 10  // Maximum number of handlers per channel
#define CHANNEL_MIN 1000
#define CHANNEL_MAX 2000
#define CHANNEL_MID CHANNEL_MIN + ((CHANNEL_MAX - CHANNEL_MIN) / 2)

enum BoardComputerStatus {
    BoardComputerStatus_BOOTING,
    BoardComputerStatus_CRSF_DISCONNECTED,
    BoardComputerStatus_CRSF_CONNECTED,
    BoardComputerStatus_ERROR
};

class IChannelHandler
{
public:
    virtual ~IChannelHandler() = default;
    virtual void onChannelChange(uint16_t value) = 0;
};

class BoardComputer
{
public:
    BoardComputer(HardwareSerial *crsfSerial);
    void start();
    void onChannelChange(uint8_t channel, IChannelHandler *handler, int failSafeChannelValue = -1);

private:
    void taskHandler();
    IChannelHandler* channelHandlers[HIGHEST_CHANNEL_NUMBER][MAX_HANDLERS_PER_CHANNEL];
    int failSafeChannelValues[HIGHEST_CHANNEL_NUMBER][MAX_HANDLERS_PER_CHANNEL];
    uint8_t handlerCount[HIGHEST_CHANNEL_NUMBER];  // Tracks number of handlers for each channel
    uint16_t lastChannelValues[HIGHEST_CHANNEL_NUMBER];
    AlfredoCRSF crsf;
    HardwareSerial *crsfSerial;
    BoardComputerStatus status;

    void executeChannelHandlers();
    void statusLedTaskHandler(void *pvParameters);
};
