#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include "bordcomputer.hpp"
#include "channel-handlers/pwmChannelHandler.hpp"
#include "channel-handlers/onOffChannelHandler.hpp"
#include "channel-handlers/blinkChannelHandler.hpp"
#include "pin_map.hpp"
#include "eeprom_manager.hpp"
#include "config_versions.hpp"

class ConfigManager
{
public:
    ConfigManager(BoardComputer *computer, EEPROMManager *eeprom);
    bool load(const Config &config);
    bool loadFromJson(const char *jsonConfig);
    Config getConfig();
    String getConfigAsJson();
    bool loadFromEEPROM();
    bool begin();

private:
    BoardComputer *computer;
    EEPROMManager *eeprom;
    Config config;
    bool eepromInitialized;
    bool configure(const Config &config);
    void configurePWMHandler(const HandlerConfig &config);
    void configureOnOffHandler(const HandlerConfig &config);
    void configureBlinkHandler(const HandlerConfig &config);
    std::function<bool(uint16_t)> createThresholdFunction(const HandlerConfig &config);
    Config parseJson(const char *jsonConfig);
};