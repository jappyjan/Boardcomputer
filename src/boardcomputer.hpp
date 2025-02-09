#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>
#include <AlfredoCRSF.h>
#include <functional>

#include "const.hpp"
#define HIGHEST_CHANNEL_NUMBER 16
#define MAX_HANDLERS_PER_CHANNEL 10 // Maximum number of handlers per channel
#define CHANNEL_MIN 1000
#define CHANNEL_MAX 2000
#define CHANNEL_MID CHANNEL_MIN + ((CHANNEL_MAX - CHANNEL_MIN) / 2)

enum BoardComputerStatus
{
    BoardComputerStatus_UNCONFIGURED,
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

    /**
     * @brief Check if the board computer is receiving valid signals
     * @return true if receiving valid signals, false otherwise
     */
    bool isReceiving() const;

    /**
     * @brief Check if the board computer is in an error state
     * @return true if in error state, false otherwise
     */
    bool hasError() const;

    String getPinMap();

    /**
     * @brief Cleans up all handlers for all channels
     * Deletes handler instances and clears the arrays
     */
    void cleanup();

private:
    void taskHandler();
    IChannelHandler *channelHandlers[HIGHEST_CHANNEL_NUMBER][MAX_HANDLERS_PER_CHANNEL];
    int failSafeChannelValues[HIGHEST_CHANNEL_NUMBER][MAX_HANDLERS_PER_CHANNEL];
    uint8_t handlerCount[HIGHEST_CHANNEL_NUMBER]; // Tracks number of handlers for each channel
    uint16_t lastChannelValues[HIGHEST_CHANNEL_NUMBER];
    AlfredoCRSF crsf;
    HardwareSerial *crsfSerial;
    BoardComputerStatus status;

    // New members for signal tracking
    unsigned long lastValidSignalTime;
    static const unsigned long SIGNAL_TIMEOUT_MS = 1000; // 1 second timeout
    bool errorState;

    void executeChannelHandlers();
    void statusLedTaskHandler(void *pvParameters);
};
