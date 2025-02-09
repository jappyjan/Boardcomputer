#pragma once

#include <Arduino.h>
#include <map>
#include <string>

struct PinInfo
{
    uint8_t pin;
    bool isPWM;
};

// ESP32-C3 SuperMini pin mapping
#ifdef BOARD_ESP32C3_SUPERMINI
const std::map<std::string, PinInfo> PIN_MAP = {
    {"STEERING", {6, true}},
    {"THROTTLE", {5, true}},
    {"HEADLIGHT", {9, true}},
    {"BLINKER_LEFT", {10, true}},
    {"BLINKER_RIGHT", {20, true}},
    {"BRAKE_LIGHT", {21, true}},
    {"WINCH_1", {7, true}},
    {"WINCH_2", {8, true}}};

#else
#error "No board type defined! Please select a board type in platformio.ini"
#endif

// Helper function to get available pins for web configurator
inline const std::map<std::string, PinInfo> &getPinMap()
{
    return PIN_MAP;
}
