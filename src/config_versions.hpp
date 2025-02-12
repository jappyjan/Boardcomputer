#pragma once

#include <Arduino.h>
#include <type_traits>

struct HandlerConfig
{
    char type[16];
    char pin[16];
    char op[16];
    uint8_t channel;
    int32_t failsafe;  // Make explicit size
    int32_t threshold; // Make explicit size
    int32_t min;       // Make explicit size
    int32_t max;       // Make explicit size
    int32_t onTime;    // Make explicit size
    int32_t offTime;   // Make explicit size
    bool inverted;
    uint8_t padding[3]; // Add explicit padding to align structure

    // Initialize all fields in constructor
    HandlerConfig()
    {
        memset(type, 0, sizeof(type));
        memset(pin, 0, sizeof(pin));
        memset(op, 0, sizeof(op));
        channel = 0;
        failsafe = 0;
        threshold = CHANNEL_MID;
        min = 0;
        max = 255;
        onTime = 300;
        offTime = 400;
        inverted = false;
        memset(padding, 0, sizeof(padding));
    }

    // Helper function to safely set strings
    void setType(const char *t)
    {
        strncpy(type, t, sizeof(type) - 1);
        type[sizeof(type) - 1] = '\0';
    }
    void setPin(const char *p)
    {
        strncpy(pin, p, sizeof(pin) - 1);
        pin[sizeof(pin) - 1] = '\0';
    }
    void setOp(const char *o)
    {
        strncpy(op, o, sizeof(op) - 1);
        op[sizeof(op) - 1] = '\0';
    }
};

namespace ConfigVersions
{

    struct ConfigV1
    {
        static constexpr size_t MAX_HANDLERS = 20;
        uint32_t numHandlers;
        HandlerConfig handlers[MAX_HANDLERS];
        char apSsid[32];
        char apPassword[32];
        bool keepWebServerRunning;

        ConfigV1()
        {
            numHandlers = 0;
            strncpy(apSsid, "Bordcomputer", sizeof(apSsid) - 1);
            strncpy(apPassword, "bordcomputer", sizeof(apPassword) - 1);
            apSsid[sizeof(apSsid) - 1] = '\0';
            apPassword[sizeof(apPassword) - 1] = '\0';
            keepWebServerRunning = false;
        }
    };

}

using Config = ConfigVersions::ConfigV1;

static_assert(std::is_standard_layout<Config>::value, "Config must be standard layout for EEPROM storage");