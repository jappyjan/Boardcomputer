#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>
#include <map>

#include "const.hpp"
#define CHANNEL_COUNT 16
#define CHANNEL_MIN 0
#define CHANNEL_MAX 1023
#define CHANNEL_MID (CHANNEL_MAX - CHANNEL_MIN) / 2

class IChannelHandler
{
public:
    virtual ~IChannelHandler() = default;
    virtual void onChannelChange(uint16_t value) = 0;
};

class BoardComputer
{
public:
    BoardComputer();
    void start();
    void onChannelChange(uint8_t channel, IChannelHandler *handler);

private:
    void taskHandler();
    std::map<uint8_t, std::vector<IChannelHandler *>> channelHandlers;
    uint16_t lastChannelValues[CHANNEL_COUNT];
    uint16_t currentChannelValues[CHANNEL_COUNT];

    void updateChannelValues();
    void executeChannelHandlers();
};
