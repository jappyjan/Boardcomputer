#include "config_manager.hpp"
#include "eeprom_manager.hpp"
#include "logger.hpp"

ConfigManager::ConfigManager(BoardComputer *computer, EEPROMManager *eeprom)
    : computer(computer), eeprom(eeprom), config(), eepromInitialized(false)
{
    eepromInitialized = false;
}

bool ConfigManager::begin()
{
    LOG.debugf("ConfigManager", "Initializing EEPROM for Config size: %d bytes", sizeof(Config));
    eepromInitialized = eeprom->begin(sizeof(Config));
    if (!eepromInitialized)
    {
        LOG.error("ConfigManager", "Failed to initialize EEPROM");
        return false;
    }

    // Try to read config, if it fails (checksum error), clear EEPROM
    Config testConfig;
    if (!eeprom->read(testConfig))
    {
        LOG.error("ConfigManager", "Invalid data in EEPROM, clearing...");
        eeprom->clear();
    }

    return eepromInitialized;
}

bool ConfigManager::load(const Config &config)
{
    this->configure(config);

    if (!eepromInitialized)
    {
        LOG.error("ConfigManager", "Cannot write to EEPROM - not initialized");
        return false;
    }

    return eeprom->write(config);
}

bool ConfigManager::loadFromEEPROM()
{
    if (!eepromInitialized)
    {
        LOG.error("ConfigManager", "Cannot read from EEPROM - not initialized");
        return false;
    }

    Config config;
    if (!eeprom->read(config))
    {
        LOG.error("ConfigManager", "Failed to load config, using defaults");
        return false;
    }
    return configure(config);
}

bool ConfigManager::loadFromJson(const char *jsonConfig)
{
    Config config = parseJson(jsonConfig);
    return load(config);
}

Config ConfigManager::getConfig()
{
    return config;
}

String ConfigManager::getConfigAsJson()
{
    StaticJsonDocument<2048> doc;
    JsonArray handlers = doc.createNestedArray("handlers");

    for (size_t i = 0; i < config.numHandlers; i++)
    {
        const auto &handler = config.handlers[i];
        JsonObject handlerObj = handlers.createNestedObject();
        handlerObj["type"] = handler.type;
        handlerObj["pin"] = handler.pin;
        handlerObj["channel"] = handler.channel;
        handlerObj["failsafe"] = handler.failsafe;
        handlerObj["threshold"] = handler.threshold;
        handlerObj["operator"] = handler.op;
        handlerObj["inverted"] = handler.inverted;
        handlerObj["min"] = handler.min;
        handlerObj["max"] = handler.max;
        handlerObj["onTime"] = handler.onTime;
        handlerObj["offTime"] = handler.offTime;
    }

    doc["apSsid"] = config.apSsid;
    doc["apPassword"] = config.apPassword;
    doc["keepWebServerRunning"] = config.keepWebServerRunning;

    String output;
    serializeJson(doc, output);
    return output;
}

bool ConfigManager::configure(const Config &config)
{
    LOG.info("ConfigManager", "Starting configuration...");

    // Basic validation
    if (config.numHandlers == 0)
    {
        LOG.warning("ConfigManager", "No handlers configured");
    }

    // Clean up old configuration
    computer->cleanup();

    // Store the new configuration
    this->config = config;

    for (size_t i = 0; i < config.numHandlers; i++)
    {
        const auto &handler = config.handlers[i];
        LOG.debugf("ConfigManager", "Configuring %s handler for pin '%s' on channel %d",
                   handler.type, handler.pin, handler.channel);

        if (strcmp(handler.type, "pwm") == 0)
        {
            configurePWMHandler(handler);
        }
        else if (strcmp(handler.type, "onoff") == 0)
        {
            configureOnOffHandler(handler);
        }
        else if (strcmp(handler.type, "blink") == 0)
        {
            configureBlinkHandler(handler);
        }
        else
        {
            LOG.warningf("ConfigManager", "Unknown handler type '%s'", handler.type);
        }
    }

    LOG.info("ConfigManager", "Configuration complete!");
    return true;
}

void ConfigManager::configurePWMHandler(const HandlerConfig &handlerConfig)
{
    if (!handlerConfig.failsafe)
    {
        LOG.error("ConfigManager", "PWM handler requires 'failsafe' value");
        return;
    }

    auto pinInfo = PIN_MAP.find(handlerConfig.pin);
    if (pinInfo == PIN_MAP.end() || !pinInfo->second.isPWM)
    {
        LOG.errorf("ConfigManager", "Invalid PWM pin: %s (isPWM: %s)",
                   handlerConfig.pin, pinInfo == PIN_MAP.end() ? "unknown" : (pinInfo->second.isPWM ? "yes" : "no"));
        return;
    }

    uint8_t channel = handlerConfig.channel;
    int failsafeValue = handlerConfig.failsafe;
    int min = handlerConfig.min;
    int max = handlerConfig.max;
    bool inverted = handlerConfig.inverted;

    // Add detailed debug logging
    LOG.debugf("ConfigManager", "Configuring PWM handler - Channel: %d, Pin: %s (GPIO%d)",
               channel, handlerConfig.pin, pinInfo->second.pin);

    // Validate channel number
    if (channel < 1 || channel > HIGHEST_CHANNEL_NUMBER)
    {
        LOG.errorf("ConfigManager", "Invalid channel number: %d", channel);
        return;
    }

    // Validate failsafe value is within range
    if (failsafeValue < CHANNEL_MIN || failsafeValue > CHANNEL_MAX)
    {
        LOG.errorf("ConfigManager", "Failsafe value %d is out of range (%d-%d)",
                   failsafeValue, CHANNEL_MIN, CHANNEL_MAX);
        return;
    }

    // Validate min/max values
    if (min >= max)
    {
        LOG.errorf("ConfigManager", "Invalid PWM range: min (%d) must be less than max (%d)", min, max);
        return;
    }

    LOG.debugf("ConfigManager", "PWM Config: Pin=%s(GPIO%d), Channel=%d, Failsafe=%d, Range=%d-%d, Inverted=%s",
               handlerConfig.pin, pinInfo->second.pin, channel, failsafeValue, min, max, inverted ? "yes" : "no");

    auto *handler = new PWMChannelHandler(pinInfo->second.pin, min, max);
    handler->setup(failsafeValue); // Use failsafe as initial value
    handler->setInverted(inverted);

    LOG.debugf("ConfigManager", "Registering PWM handler for channel %d", channel);
    this->computer->onChannelChange(channel, handler, failsafeValue);

    LOG.debug("ConfigManager", "PWM handler registration complete");
}

void ConfigManager::configureOnOffHandler(const HandlerConfig &handlerConfig)
{
    if (!handlerConfig.failsafe)
    {
        LOG.error("ConfigManager", "OnOff handler requires 'failsafe' value");
        return;
    }

    auto pinInfo = PIN_MAP.find(handlerConfig.pin);
    if (pinInfo == PIN_MAP.end())
    {
        LOG.errorf("ConfigManager", "Invalid pin: %s", handlerConfig.pin);
        return;
    }

    uint8_t channel = handlerConfig.channel;
    int failsafeValue = handlerConfig.failsafe;
    auto compareFunc = createThresholdFunction(handlerConfig);

    // Add detailed debug logging
    LOG.debugf("ConfigManager", "Configuring OnOff handler - Channel: %d, Pin: %s (GPIO%d)",
               channel, handlerConfig.pin, pinInfo->second.pin);

    // Validate channel number
    if (channel < 1 || channel > HIGHEST_CHANNEL_NUMBER)
    {
        LOG.errorf("ConfigManager", "Invalid channel number: %d", channel);
        return;
    }

    // Validate failsafe value is within range
    if (failsafeValue < CHANNEL_MIN || failsafeValue > CHANNEL_MAX)
    {
        LOG.errorf("ConfigManager", "Failsafe value %d is out of range (%d-%d)",
                   failsafeValue, CHANNEL_MIN, CHANNEL_MAX);
        return;
    }

    LOG.debugf("ConfigManager", "OnOff Config: Pin=%s(GPIO%d), Failsafe=%d, Threshold=%d, Operator=%s",
               handlerConfig.pin, pinInfo->second.pin, failsafeValue, handlerConfig.threshold, handlerConfig.op);

    auto *handler = new OnOffChannelHandler(pinInfo->second.pin);
    handler->isOnWhen(compareFunc);
    this->computer->onChannelChange(channel, handler, failsafeValue);

    LOG.debug("ConfigManager", "Handler registration complete");
}

void ConfigManager::configureBlinkHandler(const HandlerConfig &handlerConfig)
{
    if (!handlerConfig.failsafe)
    {
        LOG.error("ConfigManager", "Blink handler requires 'failsafe' value");
        return;
    }

    auto pinInfo = PIN_MAP.find(handlerConfig.pin);
    if (pinInfo == PIN_MAP.end())
    {
        LOG.errorf("ConfigManager", "Invalid pin: %s", handlerConfig.pin);
        return;
    }

    uint8_t channel = handlerConfig.channel;
    int failsafeValue = handlerConfig.failsafe;
    int onTime = handlerConfig.onTime;
    int offTime = handlerConfig.offTime;
    auto compareFunc = createThresholdFunction(handlerConfig);

    // Validate failsafe value is within range
    if (failsafeValue < CHANNEL_MIN || failsafeValue > CHANNEL_MAX)
    {
        LOG.errorf("ConfigManager", "Failsafe value %d is out of range (%d-%d)",
                   failsafeValue, CHANNEL_MIN, CHANNEL_MAX);
        return;
    }

    LOG.debugf("ConfigManager", "Blink Config: Pin=%s(GPIO%d), Failsafe=%d, Timing=%dms on, %dms off, Threshold=%d, Operator=%s",
               handlerConfig.pin, pinInfo->second.pin, failsafeValue, onTime, offTime, handlerConfig.threshold, handlerConfig.op);

    auto *handler = new BlinkChannelHandler(pinInfo->second.pin, onTime, offTime);
    handler->isOnWhen(compareFunc);
    this->computer->onChannelChange(channel, handler, failsafeValue);
}

auto ConfigManager::createThresholdFunction(const HandlerConfig &handlerConfig) -> std::function<bool(uint16_t)>
{
    int threshold = handlerConfig.threshold;
    const char *op = handlerConfig.op;

    std::function<bool(uint16_t)> compareFunc;

    if (strcmp(op, "lessThan") == 0)
    {
        compareFunc = [threshold](uint16_t value)
        { return value < threshold; };
    }
    else if (strcmp(op, "greaterThan") == 0)
    {
        compareFunc = [threshold](uint16_t value)
        { return value > threshold; };
    }
    else if (strcmp(op, "equals") == 0)
    {
        compareFunc = [threshold](uint16_t value)
        { return value == threshold; };
    }
    else
    {
        LOG.warningf("ConfigManager", "Unknown operator '%s', defaulting to greaterThan", op);
        compareFunc = [threshold](uint16_t value)
        { return value > threshold; };
    }

    return compareFunc;
}

Config ConfigManager::parseJson(const char *jsonConfig)
{
    Config config;
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, jsonConfig);

    if (error)
    {
        LOG.errorf("ConfigManager", "JSON parsing failed: %s", error.c_str());
        return config;
    }

    LOG.infof("ConfigManager", "JSON parsed successfully");
    JsonArray handlers = doc["handlers"];
    size_t numHandlers = std::min(handlers.size(), static_cast<size_t>(Config::MAX_HANDLERS));
    LOG.infof("ConfigManager", "Found %d handlers to configure", numHandlers);

    config.numHandlers = numHandlers;
    for (size_t i = 0; i < numHandlers; i++)
    {
        JsonObject handler = handlers[i];
        HandlerConfig &handlerConfig = config.handlers[i];

        handlerConfig.setType(handler["type"] | "");
        handlerConfig.setPin(handler["pin"] | "");
        handlerConfig.channel = handler["channel"] | 0;
        handlerConfig.failsafe = handler["failsafe"] | 0;
        handlerConfig.threshold = handler["threshold"] | CHANNEL_MID;
        handlerConfig.setOp(handler["operator"] | "greaterThan");
        handlerConfig.inverted = handler["inverted"] | false;
        handlerConfig.min = handler["min"] | 0;
        handlerConfig.max = handler["max"] | 255;
        handlerConfig.onTime = handler["onTime"] | 300;
        handlerConfig.offTime = handler["offTime"] | 400;
    }

    strncpy(config.apSsid, doc["apSsid"] | "Bordcomputer", sizeof(config.apSsid));
    strncpy(config.apPassword, doc["apPassword"] | "bordcomputer", sizeof(config.apPassword));
    config.keepWebServerRunning = doc["keepWebServerRunning"] | false;

    return config;
}
